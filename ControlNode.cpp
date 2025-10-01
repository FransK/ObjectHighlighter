#include "ControlNode.h"

#include <chrono>
#include <iostream>

void ControlNode::generationWait(uint32_t value) const
{
    mGeneration.wait(value);
}

uint32_t ControlNode::generationGet() const
{
    return mGeneration.load();
}

bool ControlNode::capOpen(const cv::String &filename)
{
    std::scoped_lock lock(mCapMutex);
    mGeneration.fetch_add(1);
    bool ok = mCap.open(filename);
    mGeneration.notify_all();
    return ok;
}

bool ControlNode::capIsOpened() const
{
    std::scoped_lock lock(mCapMutex);
    return mCap.isOpened();
}

bool ControlNode::capRead(cv::OutputArray image)
{
    std::scoped_lock lock(mCapMutex);
    return mCap.read(image);
}

bool ControlNode::capSet(int propId, double value)
{
    std::scoped_lock lock(mCapMutex);
    mGeneration.fetch_add(1);
    bool ok = mCap.set(propId, value);
    mGeneration.notify_all();
    return ok;
}

double ControlNode::capGet(int propId) const
{
    std::scoped_lock lock(mCapMutex);
    return mCap.get(propId);
}

bool ControlNode::capReadAndGet(Frame &frame)
{
    std::scoped_lock lock(mCapMutex);
    frame.generation = mGeneration;
    if (mCap.read(frame.image))
    {
        frame.idx = mCap.get(cv::CAP_PROP_POS_FRAMES) - 1;
        return true;
    }
    frame.image = cv::Mat();
    frame.idx = -1;
    return false;
}

void ControlNode::capRelease()
{
    std::scoped_lock lock(mCapMutex);
    mCap.release();
    mGeneration.fetch_add(1);
    mGeneration.notify_all();
}

void ControlNode::trackersPushBackAndRewind(std::vector<ObjectTracker> &&trackers, int rewindIndex)
{
    std::scoped_lock lock(mCapMutex, mTrackersMutex);
    mTrackers.reserve(mTrackers.size() + trackers.size());
    mTrackers.insert(mTrackers.end(),
                     std::make_move_iterator(trackers.begin()),
                     std::make_move_iterator(trackers.end()));

    mCap.set(cv::CAP_PROP_POS_FRAMES, rewindIndex);
    mGeneration.fetch_add(1);
    mGeneration.notify_all();
}

bool ControlNode::trackersUpdateAndDraw(const Frame &frame)
{
    std::scoped_lock lock(mTrackersMutex);
    if (mGeneration.load() != frame.generation)
    {
        std::cout << "Frame generation (" << frame.generation << ") did not match ControlNode generation (" << mGeneration << ")" << std::endl;
        return false;
    }

    cv::Mat overlay = frame.image.clone();

    auto currentTime = std::chrono::high_resolution_clock::now();

    for (auto &tracker : mTrackers)
    {
        mThreadPool.submit([&tracker, &frame, &overlay]
                           {
            if (tracker.tracker->update(frame.image, tracker.box))
            {
                cv::rectangle(overlay, tracker.box, cv::Scalar(0, 255, 0), cv::FILLED);
            } });
    }

    mThreadPool.waitAll(std::chrono::milliseconds(100));

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - currentTime);
    std::cout << "Tracking and drawing took " << duration.count() << " microseconds." << std::endl;

    const double alpha = 0.30;
    cv::addWeighted(overlay, alpha, frame.image, 1.0 - alpha, 0.0, frame.image);

    return true;
}

void ControlNode::setIsSaving(uint32_t value, uint32_t returnIndex)
{
    if (value == mSaveState)
    {
        return;
    }

    std::scoped_lock lock(mCapMutex, mTrackersMutex);
    if (value == 1)
    {
        mReturnIndex = returnIndex;
        mCap.set(cv::CAP_PROP_POS_FRAMES, 0);
    }
    else
    {
        mCap.set(cv::CAP_PROP_POS_FRAMES, mReturnIndex);
    }
    mSaveState.fetch_xor(1);
    mGeneration.fetch_add(1);
    mSaveState.notify_all();
    mGeneration.notify_all();
}

bool ControlNode::isSaving() const
{
    return mSaveState.load() == 1;
}
