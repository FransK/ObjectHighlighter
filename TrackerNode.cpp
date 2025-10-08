#include "TrackerNode.h"

#include "opencv2/imgproc.hpp"

std::optional<Frame> TrackerNode::getFrame(std::stop_token st)
{
    return mInputQueue->waitAndPop(st);
}

void TrackerNode::updateFrame(Frame &frame)
{
    if (frame.image.empty())
    {
        return;
    }

    mControlNode->trackersUpdateAndDraw(frame);
}

void TrackerNode::passFrame(const Frame &frame, std::stop_token st)
{
    mOutputQueue->push(frame, st);
}