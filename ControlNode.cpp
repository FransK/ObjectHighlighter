#include "ControlNode.h"

#include <chrono>
#include <iostream>

void ControlNode::generationWait(uint32_t value) const
{
    // Wait until the generation is different from value
    mGeneration.wait(value);
}

uint32_t ControlNode::generationGet() const
{
    // Get the current generation value
    return mGeneration.load();
}

bool ControlNode::capOpen(const cv::String &filename)
{
    std::scoped_lock lock(mCapMutex);

    // Open the video capture with the given filename
    bool ok = mCap.open(filename);

    // If opened successfully, increment the generation and notify all waiting threads
    if (ok)
    {
        mGeneration.fetch_add(1);
        mGeneration.notify_all();
    }

    // Return whether the operation was successful
    return ok;
}

bool ControlNode::capIsOpened() const
{
    std::scoped_lock lock(mCapMutex);

    // Check if the video capture is opened
    return mCap.isOpened();
}

bool ControlNode::capRead(cv::OutputArray image)
{
    std::scoped_lock lock(mCapMutex);

    // Read the next frame from the video capture into the provided image
    return mCap.read(image);
}

bool ControlNode::capSet(int propId, double value)
{
    std::scoped_lock lock(mCapMutex);

    // Set a property of the video capture
    bool ok = mCap.set(propId, value);

    // If set successfully, increment the generation and notify all waiting threads
    if (ok)
    {
        mGeneration.fetch_add(1);
        mGeneration.notify_all();
    }

    // Return whether the operation was successful
    return ok;
}

double ControlNode::capGet(int propId) const
{
    std::scoped_lock lock(mCapMutex);

    // Get a property of the video capture
    return mCap.get(propId);
}

bool ControlNode::capReadAndGet(Frame &frame)
{
    std::scoped_lock lock(mCapMutex);

    // Set the frame generation
    frame.generation = mGeneration;

    // Read the next frame
    if (mCap.read(frame.image))
    {
        // Set the frame index (0-based)
        frame.idx = mCap.get(cv::CAP_PROP_POS_FRAMES) - 1;
        return true;
    }

    // If reading failed, set idx to -1 and image to empty
    frame.image = cv::Mat();
    frame.idx = -1;

    return false;
}

void ControlNode::capRelease()
{
    // Lock the capture mutex to ensure thread safety
    // then release the capture
    std::scoped_lock lock(mCapMutex);
    mCap.release();

    // Increment the generation and notify all waiting threads
    mGeneration.fetch_add(1);
    mGeneration.notify_all();
}

void ControlNode::trackersPushBackAndRewind(std::vector<ObjectTracker> &&trackers, int rewindIndex)
{
    std::scoped_lock lock(mCapMutex, mTrackersMutex);

    // Add new trackers to the existing list
    mTrackers.reserve(mTrackers.size() + trackers.size());
    mTrackers.insert(mTrackers.end(),
                     std::make_move_iterator(trackers.begin()),
                     std::make_move_iterator(trackers.end()));

    // Rewind the video capture to the specified frame index
    mCap.set(cv::CAP_PROP_POS_FRAMES, rewindIndex);

    // Increment the generation and notify all waiting threads
    mGeneration.fetch_add(1);
    mGeneration.notify_all();
}

bool ControlNode::trackersUpdateAndDraw(const Frame &frame)
{
    // Create an overlay to draw the filled rectangles
    // This is work that can be done outside the lock

    {
        std::scoped_lock lock(mTrackersMutex);

        // If the frame generation does not match, return false
        // This might occur if we had queued items from a previous generation
        if (mGeneration.load() != frame.generation)
        {
            return false;
        }

        // Update each tracker in parallel using the thread pool
        for (auto &tracker : mTrackers)
        {
            mThreadPool.submit([&tracker, &frame]
                               {
            // Update the tracker with the current frame
            // If successful, draw the bounding box on the overlay
            if (tracker.tracker->update(frame.image, tracker.box))
            {
                cv::Mat roi = frame.image(tracker.box);
                roi.convertTo(roi, roi.type(), 0.7);           // scale existing pixels
                roi += cv::Scalar(0, 255 * 0.3, 0.0);          // add green contribution
            } });
        }
    } // Release the lock before waiting for threads to finish

    // Wait for all tracker updates to complete with a timeout
    if (!mThreadPool.waitAll(std::chrono::milliseconds(100)))
    {
        std::cout << "Warning: Not all tracker updates completed within the timeout." << std::endl;
    }

    // Frame generation was correct and processing is done
    return true;
}

void ControlNode::setIsSaving(uint32_t value, uint32_t returnIndex)
{
    // If the value is the same as the current state, do nothing
    if (value == mSaveState)
    {
        return;
    }

    // Lock both mutexes to ensure thread safety
    {
        std::scoped_lock lock(mCapMutex, mTrackersMutex);
        if (value == 1)
        {
            // Start saving: store the return index and rewind to frame 0
            mReturnIndex = returnIndex;
            mCap.set(cv::CAP_PROP_POS_FRAMES, 0);
        }
        else
        {
            // Stop saving: rewind to the stored return index
            mCap.set(cv::CAP_PROP_POS_FRAMES, mReturnIndex);
        }

        // Toggle the saving state and increment the generation
        mSaveState.fetch_xor(1);
        mGeneration.fetch_add(1);
    }

    // Notify all waiting threads about the state change
    mSaveState.notify_all();
    mGeneration.notify_all();
}

bool ControlNode::isSaving() const
{
    // Return true if currently saving (mSaveState == 1), false otherwise
    return mSaveState.load() == 1;
}
