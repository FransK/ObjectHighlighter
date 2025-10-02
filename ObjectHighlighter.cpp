#include "ObjectHighlighter.h"

#include <iostream>
#include <thread>

#include "opencv2/features2d.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/tracking.hpp"

using std::cout;
using std::endl;

// Set the output path and format for the video writer
void ObjectHighlighter::writerSettings(const std::string &outputPath, const std::string &format)
{
    mOutputPath = outputPath;
    mFormat = format;
}

// Play the video with object highlighting and saving capabilities
void ObjectHighlighter::playVideo()
{
    // Check if video is loaded
    if (!mControlNode.capIsOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    {
        // Struct to hold the state of the playback pipeline
        PlaybackState state;

        // Start the threads for capturing, processing, and drawing frames
        std::jthread readerThread(&ObjectHighlighter::captureFrames, this, std::ref(state));
        std::jthread processorThread(&ObjectHighlighter::updateTrackers, this, std::ref(state));
        std::jthread writerThread(&ObjectHighlighter::drawFrames, this, std::ref(state));
    }

    // Ensure all OpenCV windows are closed
    cv::destroyAllWindows();
}

void ObjectHighlighter::captureFrames(PlaybackState &state)
{
    Frame frame;

    // Use stop token to exit by user input
    std::stop_token st = mControlNode.stopSourceGet().get_token();

    // Capture frames until stop is requested
    while (!st.stop_requested())
    {
        // Read the next frame and its metadata
        // If reading fails, frame.idx will be -1
        bool ok = mControlNode.capReadAndGet(frame);

        // Push the frame to the processing queue
        // If the queue is full, wait until space is available or stop is requested
        // If interrupted with stop token, the frame is not added
        state.processorQueue.push(frame, mControlNode.stopSourceGet().get_token());

        // If reading succeded, continue capturing
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

    // Use stop token to exit by user input
    std::stop_token st = mControlNode.stopSourceGet().get_token();

    while (!st.stop_requested())
    {
        // Uses thread safe queue to wait and grab the next frame
        // If interrupted with stop token, returns nullopt
        std::optional<Frame> frameOpt = state.processorQueue.waitAndPop(st);

        // If the stop token was triggered or the queue was cleared, continue
        if (!frameOpt.has_value())
        {
            // Stop token or queue cleared
            continue;
        }

        // Move the frame out of the optional
        frame = std::move(*frameOpt);

        // Check to see if done processing
        if (frame.idx != -1)
        {
            // Update all trackers and draw their bounding boxes
            // If the frame generation does not match, clear the writer queue and continue
            if (!mControlNode.trackersUpdateAndDraw(frame))
            {
                // Frame is not the right generation, continue
                state.writerQueue.clear();
                continue;
            }
        }

        // Push the processed frame to the writer queue
        // If the queue is full, wait until space is available or stop is requested
        // If interrupted with stop token, the frame is not added
        state.writerQueue.push(frame, mControlNode.stopSourceGet().get_token());
    }

    cout << "update thread exiting." << endl;
}

void ObjectHighlighter::drawFrames(PlaybackState &state)
{
    // Use stop token to exit by user input
    std::stop_token st = mControlNode.stopSourceGet().get_token();

    while (!st.stop_requested())
    {
        // Uses thread safe queue to wait and grab the next frame
        // If interrupted with stop token, returns nullopt
        std::optional<Frame> frameOpt = state.writerQueue.waitAndPop(st);

        // If the stop token was triggered or the queue was cleared, continue
        if (!frameOpt.has_value())
        {
            // Stop token or queue cleared
            continue;
        }

        // Move the frame out of the optional
        Frame frame = std::move(*frameOpt);

        // If control is in save mode, just save the frame and move on
        if (mControlNode.isSaving())
        {
            // Only save frames of the correct generation
            if (frame.generation != mControlNode.generationGet())
            {
                continue;
            }

            // Check for end of saving signal
            if (frame.idx == -1)
            {
                // Finished saving, return to main drawing
                mControlNode.setIsSaving(0);
                cv::destroyWindow(sSaveWindowTitle);
                continue;
            }

            // Show the saving window
            cv::imshow(sSaveWindowTitle, frame.image);
            cv::waitKey(1);

            // Write the frame to the video writer
            mVideoWriter.write(frame.image);
            continue;
        }

        // Check for end of video signal
        if (frame.idx == -1)
        {
            // We have displayed all the frames, end program
            mControlNode.stopSourceGet().request_stop();
            cv::waitKey(0);

            // Release the video capture
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
            // User requested to quit
            mControlNode.stopSourceGet().request_stop();
            break;
        }
    }

    // Ensure all OpenCV windows are closed
    cv::destroyAllWindows();

    cout << "drawing thread exiting." << endl;
}

// Allow the user to select objects to track in the given frame
void ObjectHighlighter::selectObjects(Frame &frame)
{
    // Check if video is loaded
    if (!mControlNode.capIsOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    // Let the user select multiple ROIs
    std::vector<cv::Rect> boundingBoxes;
    cv::selectROIs(sMainTitle, frame.image, boundingBoxes);

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
    mControlNode.trackersPushBackAndRewind(std::move(trackers), frame.idx);
}

// Handle user input during playback
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
            // Start saving from the current frame
            mControlNode.setIsSaving(1, frame.idx);
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

// Load the video writer with the given output path and fourcc format
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

    // Check if the writer was opened successfully
    return mVideoWriter.isOpened();
}

// Capture and save a single frame with highlighted objects
void ObjectHighlighter::captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame)
{
    cv::imwrite(outputPath, frame);
}
