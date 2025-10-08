#ifndef VIDEO_PROCESSOR
#define VIDEO_PROCESSOR

#include <string>

#include "opencv2/highgui.hpp"

#include "ControlNode.h"

// Main window title
static const std::string sMainTitle = "Video Processor";

class VideoProcessor
{
public:
    VideoProcessor() : mControlNode(std::make_shared<ControlNode>(cv::VideoCapture())) {}
    virtual ~VideoProcessor() = default;
    // Delete copy and move constructors and assignment operators
    // This avoids issues with ControlNode's VideoCapture,
    // mutexes, and ThreadPool not being safely movable or copyable.
    VideoProcessor(const VideoProcessor &) = delete;
    VideoProcessor &operator=(const VideoProcessor &) = delete;
    VideoProcessor(VideoProcessor &&other) = delete;
    VideoProcessor &operator=(VideoProcessor &&other) = delete;

    // Video functions
    void displayInfo();
    bool loadVideo(const std::string &videoPath);
    virtual void playVideo();
    void rewindVideo(int time);

protected:
    // Control node for managing video capture and processing
    // across multiple threads
    std::shared_ptr<ControlNode> mControlNode;
};

#endif