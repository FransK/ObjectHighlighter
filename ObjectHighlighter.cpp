#include <iostream>

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
    for (auto h : mHighlights)
    {
        cout << h.frame << endl;
    }
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

    std::vector<Highlight>::iterator hlIter;
    bool anyHighlights = !mHighlights.empty();
    if (anyHighlights)
    {
        hlIter = mHighlights.begin();
    }

    while (mCap.read(frame))
    {
        if (anyHighlights && hlIter->frame <= framec)
        {
            while (hlIter->frame < framec && hlIter != mHighlights.end())
            {
                ++hlIter;
            }

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
            anyHighlights = !mHighlights.empty();
            if (anyHighlights)
            {
                hlIter = mHighlights.begin();
                std::string outputPath = "img_" + std::to_string(framec) + ".jpg";
                captureFrameWithHighlights(outputPath, frame);
            }
        }
        else if (key == 'r')
        {
            rewindVideo(-1);
            framec = 0;
            if (anyHighlights)
            {
                hlIter = mHighlights.begin();
            }
        }
        else if (key == 'z')
        {
            rewindVideo(290);
            framec = static_cast<int>(mCap.get(cv::CAP_PROP_POS_FRAMES));
            if (anyHighlights)
            {
                hlIter = mHighlights.begin();
            }
        }
        else if (key == 's')
        {
            cv::destroyAllWindows();
            saveVideoWithHighlights("output.mp4", "ignored");
            break;
        }

        ++framec;
    }

    cv::destroyAllWindows();
}

void ObjectHighlighter::saveVideoWithHighlights(const std::string &outputPath, const std::string &format)
{
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

    std::vector<Highlight>::iterator hlIter;
    bool anyHighlights = !mHighlights.empty();
    if (anyHighlights)
    {
        hlIter = mHighlights.begin();
    }

    cv::Mat frame;
    uint framec = 0;
    while (mCap.read(frame))
    {
        if (anyHighlights && hlIter->frame <= framec)
        {
            while (hlIter->frame < framec)
            {
                ++hlIter;
            }

            while (hlIter->frame == framec)
            {
                cv::rectangle(frame, hlIter->box, cv::Scalar(0, 255, 0));
                ++hlIter;
            }
        }

        writer.write(frame);

        ++framec;
    }
}

void ObjectHighlighter::captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame)
{
    cv::imwrite(outputPath, frame);
}
