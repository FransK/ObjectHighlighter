#ifndef DATA_STRUCTS
#define DATA_STRUCTS

#include "opencv2/highgui.hpp"
#include "opencv2/tracking.hpp"

struct Frame
{
    int idx;
    uint32_t generation;
    cv::Mat image;
};

struct ObjectTracker
{
    cv::Ptr<cv::Tracker> tracker;
    cv::Rect box;
};

#endif