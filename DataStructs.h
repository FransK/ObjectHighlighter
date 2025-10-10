#ifndef DATA_STRUCTS
#define DATA_STRUCTS

#include "opencv2/highgui.hpp"
#include "opencv2/tracking.hpp"

// Frame structure to hold image and metadata
struct Frame
{
    int idx;
    uint32_t generation;
    cv::Mat image;
};

// Object tracker structure to hold tracker instance and bounding box
struct ObjectTracker
{
    cv::Ptr<cv::Tracker> tracker;
    cv::Rect box;
    bool active{true};
};

#endif