#include "ObjectHighlighter.h"

#include <chrono>
#include <iostream>
#include <thread>

#include "opencv2/features2d.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/tracking.hpp"

using std::cout;
using std::endl;

void ObjectHighlighter::playVideo()
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    {
        PlaybackState state;

        std::jthread readerThread(&ObjectHighlighter::captureFrames, this, std::ref(state));
        std::jthread processorThread(&ObjectHighlighter::updateTrackers, this, std::ref(state));
        std::jthread writerThread(&ObjectHighlighter::drawFrames, this, std::ref(state));
    }

    cv::destroyAllWindows();
}

void ObjectHighlighter::captureFrames(PlaybackState &state)
{
    Frame myFrame;

    // Use a generation counter to invalidate queues
    uint64_t localGen = state.processorGen;

    // Use stop token to exit by user input
    std::stop_token st = state.stopSource.get_token();

    while (!st.stop_requested())
    {
        bool ok;
        {
            // lock the VideoCapture object when reading or changing position
            std::scoped_lock lock(state.capMutex);
            ok = mCap.read(myFrame.frame);
            if (ok)
            {
                myFrame.idx = mCap.get(cv::CAP_PROP_POS_FRAMES) - 1;
                state.processorQueue.push(myFrame);
            }
        }
        if (!ok)
        {
            // Last frame to indicate reading done
            myFrame.idx = -1;
            myFrame.frame = cv::Mat();
            state.processorQueue.push(myFrame);

            cout << "Finished capturing frames." << endl;

            // Pause until either shut down or invalidation
            // If we recieve a stop request, move past wait
            std::stop_callback cb(st, [&]
                                  { state.processorGen += 1;
                                    state.processorGen.notify_one(); });

            state.processorGen.wait(localGen);

            if (st.stop_requested())
            {
                break;
            }

            localGen = state.processorGen;
        }
    }
    cout << "capture thread exiting." << endl;
}

void ObjectHighlighter::updateTrackers(PlaybackState &state)
{
    std::stop_token st = state.stopSource.get_token();

    while (!st.stop_requested())
    {
        // Uses thread safe queue to wait and grab the next frame
        // If interrupted with stop token, returns nullopt
        std::optional<Frame> myFrameOpt = state.processorQueue.waitAndPop(st);

        if (!myFrameOpt.has_value())
        {
            break;
        }

        Frame myFrame = std::move(*myFrameOpt);

        // Check to see if done processing
        if (myFrame.idx != -1)
        {
            // Lock to prevent adding new trackers while iterating trackers
            std::scoped_lock lock(state.trackersMutex);
            if (state.trackerInvalid)
            {
                state.trackerInvalid = false;
                continue;
            }
            trackOnFrame(myFrame);
        }

        // cout << "Tracked frame: " << myFrame.idx << endl;
        state.writerQueue.push(myFrame);
    }

    cout << "update thread exiting." << endl;
}

void ObjectHighlighter::drawFrames(PlaybackState &state)
{
    std::stop_token st = state.stopSource.get_token();

    while (!st.stop_requested())
    {
        std::optional<Frame> myFrameOpt = state.writerQueue.waitAndPop(st);

        if (!myFrameOpt.has_value())
        {
            // Stop requested
            break;
        }

        Frame myFrame = std::move(*myFrameOpt);

        if (myFrame.idx == -1)
        {
            // We have displayed all the frames
            state.stopSource.request_stop();
            cv::waitKey(0);
            break;
        }

        // Displays the video to the user
        cout << "****Showing frame: " << myFrame.idx << endl;
        cv::imshow(sWindowTitle, myFrame.frame);

        int key = cv::waitKey(1);
        if (!handlePlaybackInput(key, state, myFrame))
        {
            state.stopSource.request_stop();
            break;
        }
    }

    cv::destroyWindow(sWindowTitle);
    cout << "drawing thread exiting." << endl;
}

void ObjectHighlighter::selectObjects(Frame &myFrame)
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    std::vector<cv::Rect> boundingBoxes;
    cout << "Selecting on frame: " << myFrame.idx << endl;
    cv::selectROIs("Video", myFrame.frame, boundingBoxes);

    for (auto bbox : boundingBoxes)
    {
        Highlight hl;
        hl.frame = myFrame.idx;
        hl.box = bbox;
        mHighlights.push_back(hl);

        ObjectTracker ot;
        cv::Ptr<cv::Tracker> tracker = cv::TrackerKCF::create();
        tracker->init(myFrame.frame, bbox);
        ot.box = bbox;
        ot.tracker = tracker;
        mTrackers.push_back(ot);
    }

    // Sort highlights by frame number
    std::sort(mHighlights.begin(), mHighlights.end(), [](Highlight a, Highlight b)
              { return a.frame < b.frame; });
}

void ObjectHighlighter::trackOnFrame(Frame &myFrame)
{
    if (mTrackers.empty())
    {
        return;
    }

    // Draw all highlights for the current frame
    for (auto &tracker : mTrackers)
    {
        if (tracker.tracker->update(myFrame.frame, tracker.box))
        {
            cv::rectangle(myFrame.frame, tracker.box, cv::Scalar(0, 255, 0));
        }
    }
}

bool ObjectHighlighter::handlePlaybackInput(int key, ObjectHighlighter::PlaybackState &state, Frame &myFrame)
{
    if (key == 'p')
    {
        {
            std::scoped_lock lock(state.trackersMutex);
            selectObjects(myFrame);
            state.writerQueue.clear();
            state.trackerInvalid = true;
            {
                // Rewind to the current frame and throw out the items from the queues
                // This may rewind BEFORE the myFrame.idx. That is okay.
                std::scoped_lock lock(state.capMutex);
                mCap.set(cv::CAP_PROP_POS_FRAMES, myFrame.idx);
                state.processorQueue.clear();
            }
        }
        // If the reader had finished, need to notify to wake
        state.processorGen += 1;
        state.processorGen.notify_one();
    }
    else if (key == 'q')
    {
        return false;
    }
    else if (key == 'r')
    {
        {
            std::scoped_lock lock(state.capMutex);
            rewindVideo(-1);
            state.processorQueue.clear();
            state.writerQueue.clear();
        }
        // If the reader had finished, need to notify to wake
        state.processorGen += 1;
        state.processorGen.notify_one();
    }
    else if (key == 'z')
    {
        {
            std::scoped_lock lock(state.capMutex);
            rewindVideo(290);
            state.processorQueue.clear();
            state.writerQueue.clear();
        }
        // If the reader had finished, need to notify to wake
        state.processorGen += 1;
        state.processorGen.notify_one();
    }
    else if (key == 's')
    {
        cout << "Saving video." << endl;
        std::scoped_lock lock(state.capMutex, state.trackersMutex);
        saveVideoWithHighlights("output.mp4", "ignored");
        cout << "Video saved." << endl;
    }

    return true;
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
        {
            std::scoped_lock processorLock(state.processorMutex);
            state.processorQueue.push(frame.clone());
        }
        state.processorCv.notify_one();
    }

    state.preProcessingFinished = true;
    state.processorCv.notify_all();
}

void ObjectHighlighter::drawHighlights(ObjectHighlighter::SaveVideoState &state)
{
    auto iter = mHighlights.begin();
    uint framec = 0;

    while (true)
    {
        std::unique_lock processorLock(state.processorMutex);
        state.processorCv.wait(processorLock, [&]
                               { return !state.processorQueue.empty() || state.preProcessingFinished; });

        if (state.processorQueue.empty() && state.preProcessingFinished)
        {
            break;
        }

        cv::Mat frame = state.processorQueue.front();
        state.processorQueue.pop();
        processorLock.unlock();

        if (!mHighlights.empty())
        {
            while (iter != mHighlights.end() && iter->frame < framec)
            {
                ++iter;
            }
            while (iter != mHighlights.end() && iter->frame == framec)
            {
                cv::rectangle(frame, iter->box, cv::Scalar(0, 255, 0));
                ++iter;
            }
        }

        {
            std::scoped_lock processedLock(state.writerMutex);
            state.writerQueue.push(frame);
        }
        state.writerCv.notify_one();

        ++framec;
    }

    state.processingFinished = true;
    state.writerCv.notify_all();
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
        std::unique_lock writerLock(state.writerMutex);
        state.writerCv.wait(writerLock, [&]
                            { return !state.writerQueue.empty() || state.processingFinished; });

        if (state.writerQueue.empty() && state.processingFinished)
        {
            break;
        }

        cv::Mat frame = state.writerQueue.front();
        state.writerQueue.pop();
        writerLock.unlock();

        writer.write(frame);
    }
}

void ObjectHighlighter::captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame)
{
    cv::imwrite(outputPath, frame);
}
