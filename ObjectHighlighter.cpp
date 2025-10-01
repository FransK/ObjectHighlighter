#include "ObjectHighlighter.h"

#include <iostream>
#include <thread>

#include "opencv2/features2d.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/tracking.hpp"

using std::cout;
using std::endl;

void ObjectHighlighter::writerSettings(const std::string &outputPath, const std::string &format)
{
    mOutputPath = outputPath;
    mFormat = format;
}

void ObjectHighlighter::playVideo()
{
    if (!mControlNode.capIsOpened())
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
    Frame frame;

    // Use stop token to exit by user input
    std::stop_token st = mControlNode.stopSourceGet().get_token();

    while (!st.stop_requested())
    {
        bool ok = mControlNode.capReadAndGet(frame);
        state.processorQueue.push(frame, mControlNode.stopSourceGet().get_token());

        if (ok)
        {
            continue;
        }

        cout << "Finished capturing frames." << endl;

        // Wait until the generation changes
        uint32_t localGen = mControlNode.generationGet();
        mControlNode.generationWait(localGen);
    }

    cout << "capture thread exiting." << endl;
}

void ObjectHighlighter::updateTrackers(PlaybackState &state)
{
    Frame frame;
    std::stop_token st = mControlNode.stopSourceGet().get_token();

    while (!st.stop_requested())
    {
        // Uses thread safe queue to wait and grab the next frame
        // If interrupted with stop token, returns nullopt
        std::optional<Frame> frameOpt = state.processorQueue.waitAndPop(st);

        if (!frameOpt.has_value())
        {
            // Stop token or queue cleared
            continue;
        }

        frame = std::move(*frameOpt);

        // Check to see if done processing
        if (frame.idx != -1)
        {
            if (!mControlNode.trackersUpdateAndDraw(frame))
            {
                // Frame is not the right generation, continue
                state.writerQueue.clear();
                continue;
            }
        }

        state.writerQueue.push(frame, mControlNode.stopSourceGet().get_token());
    }

    cout << "update thread exiting." << endl;
}

void ObjectHighlighter::drawFrames(PlaybackState &state)
{
    std::stop_token st = mControlNode.stopSourceGet().get_token();

    while (!st.stop_requested())
    {
        std::optional<Frame> frameOpt = state.writerQueue.waitAndPop(st);

        if (!frameOpt.has_value())
        {
            // Stop token or queue cleared
            continue;
        }

        Frame frame = std::move(*frameOpt);

        // If control is in save mode, just save the frame and move on
        if (mControlNode.isSaving())
        {
            if (frame.generation != mControlNode.generationGet())
            {
                continue;
            }

            if (frame.idx == -1)
            {
                // Finished saving, return to main drawing
                mControlNode.setIsSaving(0);
                cv::destroyWindow(sSaveWindowTitle);
                continue;
            }

            cv::imshow(sSaveWindowTitle, frame.image);
            cv::waitKey(1);

            mVideoWriter.write(frame.image);
            continue;
        }

        if (frame.idx == -1)
        {
            // We have displayed all the frames, end program
            mControlNode.stopSourceGet().request_stop();
            cv::waitKey(0);
            mControlNode.capRelease();
            break;
        }

        // Only show frames of the correct generation
        if (frame.generation != mControlNode.generationGet())
        {
            continue;
        }

        // Displays the video to the user
        cv::imshow(sMainTitle, frame.image);

        // Allow user to interact with currently shown frame
        int key = cv::waitKey(1);
        if (!handlePlaybackInput(key, state, frame))
        {
            mControlNode.stopSourceGet().request_stop();
            break;
        }
    }

    cv::destroyWindow(sMainTitle);
    cout << "drawing thread exiting." << endl;
}

void ObjectHighlighter::selectObjects(Frame &frame)
{
    if (!mControlNode.capIsOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    std::vector<cv::Rect> boundingBoxes;
    cv::selectROIs(sMainTitle, frame.image, boundingBoxes);

    if (boundingBoxes.empty())
    {
        return;
    }

    std::vector<ObjectTracker> trackers;
    trackers.reserve(boundingBoxes.size());
    for (const auto &bbox : boundingBoxes)
    {
        ObjectTracker ot;
        cv::Ptr<cv::Tracker> tracker = cv::TrackerKCF::create();
        tracker->init(frame.image, bbox);
        ot.box = bbox;
        ot.tracker = tracker;
        trackers.push_back(std::move(ot));
    }

    mControlNode.trackersPushBackAndRewind(std::move(trackers), frame.idx);
}

bool ObjectHighlighter::handlePlaybackInput(int key, ObjectHighlighter::PlaybackState &state, Frame &frame)
{
    if (key == 'p')
    {
        selectObjects(frame);
    }
    else if (key == 'q')
    {
        return false;
    }
    else if (key == 'r')
    {
        rewindVideo(-1);
    }
    else if (key == 'z')
    {
        rewindVideo(290);
    }
    else if (key == 's')
    {
        if (!loadWriter(mOutputPath, mFormat))
        {
            std::cerr << "Error: Could not open video writer: " << mOutputPath;
            std::cerr << " with format: " << mFormat << std::endl;
        }
        else
        {
            mControlNode.setIsSaving(1, frame.idx);
        }
    }
    else if (key == 'o')
    {
        std::string filename = "frame_" + std::to_string(frame.idx) + ".jpg";
        captureFrameWithHighlights(filename, frame.image);
    }

    return true;
}

bool ObjectHighlighter::loadWriter(const std::string &outputPath, const std::string &fourcc)
{
    // Default to mp4v codec
    int fourccFormat = cv::VideoWriter::fourcc('m', 'p', '4', 'v');

    // Check if fourcc has exactly 4 characters
    if (fourcc.length() == 4)
    {
        fourccFormat = cv::VideoWriter::fourcc(
            fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
    }
    else
    {
        std::cerr << "Invalid fourcc length, using default mp4v" << std::endl;
    }

    // Initialize the VideoWriter for save functions
    if (!outputPath.empty())
    {
        mVideoWriter = cv::VideoWriter(outputPath,
                                       fourccFormat,
                                       mControlNode.capGet(cv::CAP_PROP_FPS),
                                       cv::Size(mControlNode.capGet(cv::CAP_PROP_FRAME_WIDTH), mControlNode.capGet(cv::CAP_PROP_FRAME_HEIGHT)));
    }

    return mVideoWriter.isOpened();
}

void ObjectHighlighter::captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame)
{
    cv::imwrite(outputPath, frame);
}
