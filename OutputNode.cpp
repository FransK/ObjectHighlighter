#include "OutputNode.h"

#include <algorithm>
#include <iostream>

#include "opencv2/highgui.hpp"

std::optional<Frame> OutputNode::getFrame(std::stop_token st)
{
    return mInputQueue->waitAndPop(st);
}

void OutputNode::updateFrame(Frame &frame)
{
}

void OutputNode::passFrame(const Frame &frame, std::stop_token st)
{
    // If control is in save mode, just save the frame and move on
    if (mControlNode->isSaving())
    {
        // Check for end of saving signal
        if (frame.idx == -1)
        {
            // Finished saving, return to main drawing
            mControlNode->setIsSaving(0);
            cv::destroyWindow(mSaveWindowName);
            return;
        }

        // Show the saving window
        cv::imshow(mSaveWindowName, frame.image);
        cv::waitKey(1);

        // Write the frame to the video writer
        mVideoWriter.write(frame.image);
        return;
    }

    // Check for end of video signal
    if (frame.idx == -1)
    {
        // We have displayed all the frames, end program
        mControlNode->stopSourceGet().request_stop();
        cv::waitKey(0);

        // Release the video capture
        mControlNode->capRelease();
        return;
    }

    // Displays the video to the user
    cv::imshow(mWindowName, frame.image);

    // Allow user to interact with currently shown frame
    int key = cv::waitKey(1);
    if (!handlePlaybackInput(key, frame))
    {
        // User requested to quit
        mControlNode->stopSourceGet().request_stop();

        // Release the video capture
        mControlNode->capRelease();
    }
}

// Handle user input during playback
bool OutputNode::handlePlaybackInput(int key, const Frame &frame)
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
        rewindVideo(cFramesToRewind);
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
            // Start saving from the current frame
            mControlNode->setIsSaving(1, frame.idx);
        }
    }
    else if (key == 'o')
    {
        // Capture the current frame with highlights
        std::string filename = "frame_" + std::to_string(frame.idx) + ".jpg";
        captureFrameWithHighlights(filename, frame.image);
    }

    return true;
}

// Allow the user to select objects to track in the given frame
void OutputNode::selectObjects(const Frame &frame)
{
    // Check if video is loaded
    if (!mControlNode->capIsOpened())
    {
        return;
    }

    // Let the user select multiple ROIs
    std::vector<cv::Rect> boundingBoxes;
    cv::selectROIs(mWindowName, frame.image, boundingBoxes);

    // If no boxes were selected, return
    if (boundingBoxes.empty())
    {
        return;
    }

    // Create a tracker for each selected bounding box
    std::vector<ObjectTracker> trackers;
    trackers.reserve(boundingBoxes.size());
    for (const auto &bbox : boundingBoxes)
    {
        ObjectTracker ot;

        // Create a KCF tracker
        cv::Ptr<cv::Tracker> tracker = cv::TrackerKCF::create();

        // Initialize the tracker with the current frame and bounding box
        tracker->init(frame.image, bbox);

        // Store the tracker and bounding box
        ot.box = bbox;
        ot.tracker = tracker;
        trackers.push_back(std::move(ot));
    }

    // Push the new trackers to the control node and rewind to the current frame
    mControlNode->trackersPushBackAndRewind(std::move(trackers), frame.idx);
}

// Rewind the video by the given number of frames
void OutputNode::rewindVideo(int frameCount)
{
    if (!mControlNode->capIsOpened())
    {
        return;
    }

    // If frames is negative, rewind to the start
    if (frameCount < 0)
    {
        mControlNode->capSet(cv::CAP_PROP_POS_FRAMES, 0);
        return;
    }

    // Subtract 1 because POS_FRAMES is the next frame to be read
    int currentFrame = mControlNode->capGet(cv::CAP_PROP_POS_FRAMES) - 1;

    // Set the new frame position, ensuring it doesn't go below 0
    mControlNode->capSet(cv::CAP_PROP_POS_FRAMES, std::max(currentFrame - frameCount, 0));
}

// Load the video writer with the given output path and fourcc format
bool OutputNode::loadWriter(const std::string &outputPath, const std::string &fourcc)
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
                                       mControlNode->capGet(cv::CAP_PROP_FPS),
                                       cv::Size(mControlNode->capGet(cv::CAP_PROP_FRAME_WIDTH), mControlNode->capGet(cv::CAP_PROP_FRAME_HEIGHT)));
    }

    // Check if the writer was opened successfully
    return mVideoWriter.isOpened();
}

// Capture and save a single frame with highlighted objects
void OutputNode::captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame)
{
    cv::imwrite(outputPath, frame);
}
