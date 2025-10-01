#ifndef VIDEO_PROCESSOR
#define VIDEO_PROCESSOR

#include <string>

#include "opencv2/highgui.hpp"

#include "ControlNode.h"

class VideoProcessor
{
public:
    VideoProcessor() = default;
    void displayInfo();
    void loadVideo(const std::string &videoPath);
    virtual void playVideo();
    void rewindVideo(int time);

protected:
    ControlNode mControlNode{cv::VideoCapture()};
};

#endif