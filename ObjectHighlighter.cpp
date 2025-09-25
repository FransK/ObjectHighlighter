#include <chrono>
#include <iostream>
#include <thread>

#include "opencv2/features2d.hpp"
#include "opencv2/imgproc.hpp"

#include "ObjectHighlighter.h"

using std::cout;
using std::endl;

void ObjectHighlighter::objectSelection(cv::Mat &frame)
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    std::vector<cv::Rect> boundingBoxes;
    cv::selectROIs("Video", frame, boundingBoxes);

    for (auto bbox : boundingBoxes)
    {
        Highlight hl;
        hl.frame = static_cast<int>(mCap.get(cv::CAP_PROP_POS_FRAMES));
        hl.box = bbox;
        mHighlights.push_back(hl);
        cout << "Added highlight: " << hl.frame << endl;

        cv::rectangle(frame, bbox, cv::Scalar(0, 255, 0));
    }

    // Sort highlights by frame number
    std::sort(mHighlights.begin(), mHighlights.end(), [](Highlight a, Highlight b)
              { return a.frame < b.frame; });

    cout << "Highlights sorted." << endl;
}

void ObjectHighlighter::playVideo()
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    cv::Mat frame;
    uint framec = 0;

    auto hlIter = mHighlights.begin();

    auto resetState = [&]()
    {
        framec = static_cast<uint>(mCap.get(cv::CAP_PROP_POS_FRAMES));
        hlIter = mHighlights.begin();
        while (hlIter != mHighlights.end() && hlIter->frame < framec)
        {
            ++hlIter;
        }
    };

    while (mCap.read(frame))
    {
        if (!mHighlights.empty())
        {
            while (hlIter->frame == framec && hlIter != mHighlights.end())
            {
                cout << "Drawing rectangle on frame: " << framec << endl;
                cv::rectangle(frame, hlIter->box, cv::Scalar(0, 255, 0));
                ++hlIter;
            }
        }

        cv::imshow("Video", frame);

        int key = cv::waitKey(35);

        if (key == 'q')
        {
            break;
        }
        else if (key == 'p')
        {
            objectSelection(frame);
            resetState();
            std::string outputPath = "img_" + std::to_string(framec) + ".jpg";
            captureFrameWithHighlights(outputPath, frame);
        }
        else if (key == 'r')
        {
            rewindVideo(-1);
            resetState();
        }
        else if (key == 'z')
        {
            rewindVideo(290);
            resetState();
        }
        else if (key == 's')
        {
            cout << "Saving video parallel." << endl;
            saveVideoWithHighlights2("output2.mp4", "ignored");
            cout << "Saving video serial." << endl;
            saveVideoWithHighlights("output.mp4", "ignored");
            cout << "Video saved." << endl;
            cv::waitKey(0);
        }

        ++framec;
    }

    cv::destroyAllWindows();
}

void ObjectHighlighter::saveVideoWithHighlights(const std::string &outputPath, const std::string &format)
{
    auto start = std::chrono::high_resolution_clock::now();

    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    cv::VideoWriter writer = cv::VideoWriter(outputPath,
                                             cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                                             mCap.get(cv::CAP_PROP_FPS),
                                             cv::Size(mCap.get(cv::CAP_PROP_FRAME_WIDTH), mCap.get(cv::CAP_PROP_FRAME_HEIGHT)));

    if (!writer.isOpened())
    {
        cout << "Writer not opened." << endl;
        return;
    }

    mCap.set(cv::CAP_PROP_POS_FRAMES, 0);

    cv::Mat frame;

    if (!mHighlights.empty())
    {
        auto hlIter = mHighlights.begin();
        uint framec = 0;

        while (mCap.read(frame))
        {
            while (hlIter->frame < framec && hlIter != mHighlights.end())
            {
                ++hlIter;
            }
            while (hlIter->frame == framec && hlIter != mHighlights.end())
            {
                cv::rectangle(frame, hlIter->box, cv::Scalar(0, 255, 0));
                ++hlIter;
            }

            writer.write(frame);

            ++framec;
        }
    }
    else
    {
        while (mCap.read(frame))
        {
            writer.write(frame);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    cout << "Save video time: " << duration.count() << "s" << endl;
}

void ObjectHighlighter::saveVideoWithHighlights2(const std::string &outputPath, const std::string &format)
{
    auto start = std::chrono::high_resolution_clock::now();

    mReadingFinished = false;
    mProcessingFinished = false;
    mInputFrames = {};
    mProcessedFrames = {};

    {
        std::jthread readerThread(&ObjectHighlighter::readFrames, this);
        std::jthread processorThread(&ObjectHighlighter::drawHighlights, this);
        std::jthread writerThread(&ObjectHighlighter::writeFrames, this, outputPath, format);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    cout << "Save video time: " << duration.count() << "s" << endl;
}

void ObjectHighlighter::readFrames()
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    mCap.set(cv::CAP_PROP_POS_FRAMES, 0);

    cv::Mat frame;
    while (mCap.read(frame))
    {
        std::unique_lock lock(mInputMutex);
        mInputFrames.push(frame.clone());
        lock.unlock();
        mInputCv.notify_one();
    }

    std::unique_lock lock(mInputMutex);
    mReadingFinished = true;
    mInputCv.notify_all();
}

void ObjectHighlighter::drawHighlights()
{
    if (!mHighlights.empty())
    {
        auto iter = mHighlights.begin();
        uint framec = 0;

        while (true)
        {
            std::unique_lock inputLock(mInputMutex);
            mInputCv.wait(inputLock, [this]
                          { return !mInputFrames.empty() || mReadingFinished; });

            if (mInputFrames.empty() && mReadingFinished)
            {
                break;
            }

            cv::Mat frame = mInputFrames.front();
            mInputFrames.pop();
            inputLock.unlock();

            while (iter != mHighlights.end() && iter->frame < framec)
            {
                ++iter;
            }
            while (iter != mHighlights.end() && iter->frame == framec)
            {
                cv::rectangle(frame, iter->box, cv::Scalar(0, 255, 0));
                ++iter;
            }

            std::unique_lock processedLock(mProcessedMutex);
            mProcessedFrames.push(frame);
            processedLock.unlock();
            mProcessedCv.notify_one();

            ++framec;
        }
    }
    else
    {
        while (true)
        {
            std::unique_lock inputLock(mInputMutex);
            mInputCv.wait(inputLock, [this]
                          { return !mInputFrames.empty() || mReadingFinished; });

            if (mInputFrames.empty() && mReadingFinished)
            {
                break;
            }

            cv::Mat frame = mInputFrames.front();
            mInputFrames.pop();
            inputLock.unlock();

            std::unique_lock processedLock(mProcessedMutex);
            mProcessedFrames.push(frame);
            processedLock.unlock();
            mProcessedCv.notify_one();
        }
    }

    std::unique_lock lock(mProcessedMutex);
    mProcessingFinished = true;
    mProcessedCv.notify_all();
}

void ObjectHighlighter::writeFrames(const std::string &outputPath, const std::string &format)
{
    cv::VideoWriter writer = cv::VideoWriter(outputPath,
                                             cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                                             mCap.get(cv::CAP_PROP_FPS),
                                             cv::Size(mCap.get(cv::CAP_PROP_FRAME_WIDTH), mCap.get(cv::CAP_PROP_FRAME_HEIGHT)));

    if (!writer.isOpened())
    {
        cout << "Writer not opened." << endl;
        return;
    }

    while (true)
    {
        std::unique_lock lock(mProcessedMutex);
        mProcessedCv.wait(lock, [this]
                          { return !mProcessedFrames.empty() || mProcessingFinished; });

        if (mProcessedFrames.empty() && mProcessingFinished)
        {
            break;
        }

        cv::Mat frame = mProcessedFrames.front();
        mProcessedFrames.pop();
        lock.unlock();

        writer.write(frame);
    }
}

void ObjectHighlighter::captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame)
{
    cv::imwrite(outputPath, frame);
}
