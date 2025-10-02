#ifndef CONTROL_NODE
#define CONTROL_NODE

#include "DataStructs.h"
#include "ThreadPool.h"

#include <mutex>
#include <string>
#include <thread>

#include "opencv2/highgui.hpp"

class ControlNode
{
private:
    // Stop source for thread management
    std::stop_source mStopSource;

    // Video capture
    cv::VideoCapture mCap;
    mutable std::mutex mCapMutex;

    // Object trackers
    std::vector<ObjectTracker> mTrackers;
    mutable std::mutex mTrackersMutex;

    // Frame generation counter
    std::atomic<uint32_t> mGeneration{0};

    // Save control
    std::atomic<uint32_t> mSaveState{0};
    uint32_t mReturnIndex{0};

    // Thread pool
    ThreadPool mThreadPool;

public:
    ControlNode(cv::VideoCapture cap) : mCap(std::move(cap)), mThreadPool(4, mStopSource.get_token()) {}
    ~ControlNode() = default;

    // Delete copy and move constructors and assignment operators
    ControlNode(const ControlNode &) = delete;
    ControlNode &operator=(const ControlNode &) = delete;
    ControlNode(ControlNode &&other) = delete;
    ControlNode &operator=(ControlNode &&other) = delete;

    // Generation access and thread management

    // Returns a reference to the stop source for thread management
    std::stop_source &stopSourceGet() { return mStopSource; }
    // Wait until the generation is different from value
    void generationWait(uint32_t value) const;
    // Get the current generation value
    uint32_t generationGet() const;

    // Capture functions

    // Open the video capture with the given filename
    // Returns true if successful, false otherwise
    bool capOpen(const cv::String &filename);
    // Check if the video capture is opened
    bool capIsOpened() const;
    // Read the next frame from the video capture into the provided image
    // Returns true if successful, false otherwise
    bool capRead(cv::OutputArray image);
    // Set a property of the video capture
    // Returns true if successful, false otherwise
    bool capSet(int propId, double value);
    // Get a property of the video capture
    double capGet(int propId) const;
    // Read the next frame and get its metadata
    // Returns true if successful, false otherwise
    bool capReadAndGet(Frame &frame);
    // Release the video capture
    void capRelease();

    // Tracker functions

    // Add new trackers and rewind to a specific frame index
    void trackersPushBackAndRewind(std::vector<ObjectTracker> &&trackers, int rewindIndex);
    // Update all trackers with the given frame and draw their bounding boxes
    // Returns true if the frame generation matched, false otherwise
    bool trackersUpdateAndDraw(const Frame &frame);

    // Output functions

    // Set the saving state (1 to start saving, 0 to stop) and optionally a return index
    // Return index is given when starting to save (value = 1)
    // When stopping saving (value = 0), the video capture will return to this index
    void setIsSaving(uint32_t value, uint32_t returnIndex = 0);
    // Get the current saving state (1 if saving, 0 if not)
    bool isSaving() const;
};

#endif
