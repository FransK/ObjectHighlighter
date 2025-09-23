#ifndef VIDEO_PROCESSOR
#define VIDEO_PROCESSOR

#include "opencv2/highgui.hpp"
#include <string>

class VideoProcessor
{
public:
    VideoProcessor() = default;
    void displayInfo();
    void loadVideo(const std::string &videoPath);
    virtual void playVideo();
    void rewindVideo(int time);

protected:
    cv::VideoCapture mCap;
};

#endif