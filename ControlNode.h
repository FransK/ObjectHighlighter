#ifndef CONTROL_NODE
#define CONTROL_NODE

#include "DataStructs.h"

#include <mutex>
#include <string>
#include <thread>

#include "opencv2/highgui.hpp"

class ControlNode
{
private:
    cv::VideoCapture mCap;
    mutable std::mutex mCapMutex;

    std::vector<ObjectTracker> mTrackers;
    mutable std::mutex mTrackersMutex;

    std::atomic<uint32_t> mGeneration{0};

    // Save control
    std::atomic<uint32_t> mSaveState{0};
    uint32_t mReturnIndex{0};

public:
    ControlNode(cv::VideoCapture cap) : mCap(std::move(cap)) {}
    ~ControlNode() = default;
    ControlNode(const ControlNode &) = delete;
    ControlNode &operator=(const ControlNode &) = delete;
    ControlNode(ControlNode &&other) = delete;
    ControlNode &operator=(ControlNode &&other) = delete;

    // Generation access
    void generationWait(uint32_t value) const;
    uint32_t generationGet() const;

    // Capture functions
    bool capOpen(const cv::String &filename);
    bool capIsOpened() const;
    bool capRead(cv::OutputArray image);
    bool capSet(int propId, double value);
    double capGet(int propId) const;
    bool capReadAndGet(Frame &frame);
    void capRelease();

    // Tracker functions
    void trackersPushBackAndRewind(std::vector<ObjectTracker> &&trackers, int rewindIndex);
    bool trackersUpdateAndDraw(const Frame &frame);

    // Output functions
    void setIsSaving(uint32_t value, uint32_t returnIndex = 0);
    bool isSaving() const;
};

#endif
