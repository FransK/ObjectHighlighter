#include <chrono>
#include <iostream>
#include <thread>

#include "opencv2/features2d.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/tracking.hpp"

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

    int currentFrame = static_cast<int>(mCap.get(cv::CAP_PROP_POS_FRAMES)) - 1;

    for (auto bbox : boundingBoxes)
    {
        Highlight hl;
        hl.frame = currentFrame;
        hl.box = bbox;
        mHighlights.push_back(hl);

        ObjectTracker ot;
        cv::Ptr<cv::Tracker> tracker = cv::TrackerKCF::create();
        tracker->init(frame, bbox);
        ot.box = bbox;
        ot.tracker = tracker;
        mTrackers.push_back(ot);

        cv::rectangle(frame, bbox, cv::Scalar(0, 255, 0));
    }

    // Sort highlights by frame number
    std::sort(mHighlights.begin(), mHighlights.end(), [](Highlight a, Highlight b)
              { return a.frame < b.frame; });
}

void ObjectHighlighter::playVideo()
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    PlaybackState state;
    {
        std::jthread readerThread(&ObjectHighlighter::captureFrames, this, std::ref(state));
        std::jthread processorThread(&ObjectHighlighter::updateTrackers, this, std::ref(state));
        std::jthread writerThread(&ObjectHighlighter::drawFrames, this, std::ref(state));
    }

    // uint currentFrame = 0;
    // auto iter = mHighlights.begin();

    // cv::Mat frame;
    // while (mCap.read(frame))
    // {
    //     drawHighlightsOnFrame(frame, currentFrame, iter);
    //     trackOnFrame(frame);

    //     cv::imshow("Video", frame);

    //     int key = cv::waitKey(1);

    //     if (!handlePlaybackInput(key, frame, currentFrame, iter))
    //     {
    //         break;
    //     }

    //     // Update currentFrame to next frame
    //     currentFrame = static_cast<uint>(mCap.get(cv::CAP_PROP_POS_FRAMES));
    // }

    cv::destroyAllWindows();
}

void ObjectHighlighter::captureFrames(PlaybackState &state)
{
    cv::Mat frame;
    uint64_t localGen = state.preProcessorGen.load(std::memory_order_acquire);

    while (!state.shuttingDown.load(std::memory_order_acquire))
    {
        uint64_t newGen = state.preProcessorGen.load(std::memory_order_acquire);
        if (newGen != localGen)
        {
            {
                std::lock_guard<std::mutex> lock(state.processorMutex);
                state.readingFinished.store(false, std::memory_order_release);
                state.processorFrames = {};
            }
            localGen = newGen;
            state.processorGen.fetch_add(1, std::memory_order_release);

            // Notify the processors in case they are done processing
            // and need to restart on new data
            state.processorCv.notify_one();
        }

        bool ok;
        {
            std::lock_guard<std::mutex> lock(state.preProcessorMutex);
            ok = mCap.read(frame);
        }
        if (ok)
        {
            {
                std::lock_guard<std::mutex> lock(state.processorMutex);
                state.processorFrames.push(frame.clone());
            }

            // Notify processors of new frames to process
            state.processorCv.notify_one();
        }
        else
        {
            state.readingFinished.store(true, std::memory_order_release);
            state.processorCv.notify_one();
            cout << "Finished capturing frames." << endl;

            // Pause until either shut down or invalidation
            std::unique_lock lock(state.preProcessorMutex);
            state.preProcessorCv.wait(lock, [&]
                                      { return state.preProcessorGen != localGen || state.shuttingDown; });

            lock.unlock();
        }
    }
    cout << "capture thread exiting." << endl;
}

void ObjectHighlighter::updateTrackers(PlaybackState &state)
{
    uint64_t localGen = state.processorGen.load(std::memory_order_acquire);

    while (!state.shuttingDown.load(std::memory_order_acquire))
    {
        while (!state.shuttingDown.load(std::memory_order_acquire))
        {
            std::unique_lock inputLock(state.processorMutex);
            state.processorCv.wait(inputLock, [&]
                                   { return !state.processorFrames.empty() ||
                                            state.readingFinished ||
                                            state.shuttingDown; });

            if (state.shuttingDown)
            {
                break;
            }

            if (state.processorFrames.empty() && state.readingFinished)
            {
                state.processingFinished.store(true, std::memory_order_release);
                state.writerCv.notify_one();
                cout << "Finished updating trackers." << endl;
                break;
            }

            uint64_t newGen = state.processorGen.load(std::memory_order_acquire);
            if (newGen != localGen)
            {
                {
                    std::lock_guard<std::mutex> lock(state.writerMutex);
                    state.processingFinished.store(false, std::memory_order_release);
                    state.writerFrames = {};
                }
                localGen = newGen;
            }

            cv::Mat frame = state.processorFrames.front();
            state.processorFrames.pop();
            inputLock.unlock();

            trackOnFrame(frame);

            {
                std::lock_guard<std::mutex> lock(state.writerMutex);
                state.writerFrames.push(frame);
            }
            state.writerCv.notify_one();
        }

        if (state.shuttingDown)
        {
            break;
        }

        // Pause until either shut down or invalidation
        std::unique_lock outputLock(state.processorMutex);
        state.processorCv.wait(outputLock, [&]
                               { return state.processorGen != localGen || state.shuttingDown; });

        uint64_t newGen = state.processorGen.load(std::memory_order_acquire);
        if (newGen != localGen)
        {
            {
                std::unique_lock lock(state.writerMutex);
                state.processingFinished.store(false, std::memory_order_release);
                state.writerFrames = {};
            }
            localGen = newGen;
        }
        outputLock.unlock();
    }

    cout << "update thread exiting." << endl;
}

void ObjectHighlighter::drawFrames(PlaybackState &state)
{
    while (!state.shuttingDown.load(std::memory_order_acquire))
    {
        std::unique_lock lock(state.writerMutex);
        state.writerCv.wait(lock, [&]
                            { return !state.writerFrames.empty() ||
                                     state.processingFinished ||
                                     state.shuttingDown; });

        if (state.shuttingDown)
        {
            break;
        }

        if (state.writerFrames.empty() && state.processingFinished)
        {
            state.requestShutdown();
            break;
        }

        cv::Mat frame = state.writerFrames.front();
        state.writerFrames.pop();
        lock.unlock();

        cv::imshow("Video", frame);

        int key = cv::waitKey(1);

        if (key == 'p')
        {
            {
                std::lock_guard<std::mutex> preLock(state.preProcessorMutex);
                objectSelection(frame);
            }
            state.preProcessorGen.fetch_add(1, std::memory_order_acquire);
            state.preProcessorCv.notify_one();
        }
        else if (key == 'q')
        {
            state.requestShutdown();
            break;
        }
        else if (key == 'r')
        {
            {
                std::lock_guard<std::mutex> lock(state.preProcessorMutex);
                rewindVideo(-1);
            }
            state.preProcessorGen.fetch_add(1, std::memory_order_acquire);
            state.preProcessorCv.notify_one();
        }
    }

    cv::destroyWindow("Video");
    cout << "drawing thread exiting." << endl;
}

bool ObjectHighlighter::handlePlaybackInput(int key, cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &iter)
{
    auto resetHighlightIterator = [&]()
    {
        // Sets the highlight iterator to the next set of highlights
        uint nextFrame = static_cast<uint>(mCap.get(cv::CAP_PROP_POS_FRAMES));
        iter = std::lower_bound(mHighlights.begin(), mHighlights.end(), nextFrame,
                                [](const Highlight &h, uint f)
                                { return h.frame < f; });
    };

    if (key == 'q')
    {
        return false; // Stop playback
    }
    else if (key == 'p')
    {
        objectSelection(frame);
        resetHighlightIterator();
        std::string outputPath = "img_" + std::to_string(framec) + ".jpg";
        captureFrameWithHighlights(outputPath, frame);
    }
    else if (key == 'r')
    {
        rewindVideo(-1);
        resetHighlightIterator();
    }
    else if (key == 'z')
    {
        rewindVideo(290); // 10s
        resetHighlightIterator();
    }
    else if (key == 's')
    {
        cout << "Saving video." << endl;
        saveVideoWithHighlights("output.mp4", "ignored");
        cout << "Video saved." << endl;
    }

    return true; // Continue playback
}

void ObjectHighlighter::drawHighlightsOnFrame(cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &iter)
{
    if (mHighlights.empty())
    {
        return;
    }

    // Advance iterator to the current frame
    while (iter != mHighlights.end() && iter->frame < framec)
    {
        ++iter;
    }
    // Draw all highlights for the current frame
    while (iter != mHighlights.end() && iter->frame == framec)
    {
        cv::rectangle(frame, iter->box, cv::Scalar(0, 255, 0));
        ++iter;
    }
}

void ObjectHighlighter::trackOnFrame(cv::Mat &frame)
{
    if (mTrackers.empty())
    {
        return;
    }

    // Draw all highlights for the current frame
    for (auto tracker : mTrackers)
    {
        if (tracker.tracker->update(frame, tracker.box))
        {
            cv::rectangle(frame, tracker.box, cv::Scalar(0, 255, 0));
        }
    }
}

void ObjectHighlighter::saveVideoWithHighlights(const std::string &outputPath, const std::string &format)
{
    auto start = std::chrono::high_resolution_clock::now();

    double currentPos = mCap.get(cv::CAP_PROP_POS_FRAMES);

    SaveVideoState saveState;

    // Scope for multi-threaded pipeline
    {
        std::jthread readerThread(&ObjectHighlighter::readFrames, this, std::ref(saveState));
        std::jthread processorThread(&ObjectHighlighter::drawHighlights, this, std::ref(saveState));
        std::jthread writerThread(&ObjectHighlighter::writeFrames, this, outputPath, format, std::ref(saveState));
    }

    mCap.set(cv::CAP_PROP_POS_FRAMES, currentPos);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    cout << "Save video time: " << duration.count() << "s" << endl;
}

void ObjectHighlighter::readFrames(ObjectHighlighter::SaveVideoState &state)
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
        std::unique_lock lock(state.inputMutex);
        state.inputFrames.push(frame.clone());
        lock.unlock();
        state.inputCv.notify_one();
    }

    state.readingFinished = true;
    state.inputCv.notify_all();
}

void ObjectHighlighter::drawHighlights(ObjectHighlighter::SaveVideoState &state)
{
    if (!mHighlights.empty())
    {
        auto iter = mHighlights.begin();
        uint framec = 0;

        while (true)
        {
            std::unique_lock inputLock(state.inputMutex);
            state.inputCv.wait(inputLock, [&]
                               { return !state.inputFrames.empty() || state.readingFinished; });

            if (state.inputFrames.empty() && state.readingFinished)
            {
                break;
            }

            cv::Mat frame = state.inputFrames.front();
            state.inputFrames.pop();
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

            std::unique_lock processedLock(state.processedMutex);
            state.processedFrames.push(frame);
            processedLock.unlock();
            state.processedCv.notify_one();

            ++framec;
        }
    }
    else
    {
        while (true)
        {
            std::unique_lock inputLock(state.inputMutex);
            state.inputCv.wait(inputLock, [&]
                               { return !state.inputFrames.empty() || state.readingFinished; });

            if (state.inputFrames.empty() && state.readingFinished)
            {
                break;
            }

            cv::Mat frame = state.inputFrames.front();
            state.inputFrames.pop();
            inputLock.unlock();

            std::unique_lock processedLock(state.processedMutex);
            state.processedFrames.push(frame);
            processedLock.unlock();
            state.processedCv.notify_one();
        }
    }

    state.processingFinished = true;
    state.processedCv.notify_all();
}

void ObjectHighlighter::writeFrames(const std::string &outputPath, const std::string &format, ObjectHighlighter::SaveVideoState &state)
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
        std::unique_lock lock(state.processedMutex);
        state.processedCv.wait(lock, [&]
                               { return !state.processedFrames.empty() || state.processingFinished; });

        if (state.processedFrames.empty() && state.processingFinished)
        {
            break;
        }

        cv::Mat frame = state.processedFrames.front();
        state.processedFrames.pop();
        lock.unlock();

        writer.write(frame);
    }
}

void ObjectHighlighter::captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame)
{
    cv::imwrite(outputPath, frame);
}
