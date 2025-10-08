#include "ReaderNode.h"

std::optional<Frame> ReaderNode::getFrame(std::stop_token st)
{
    Frame frame;
    mControlNode->capReadAndGet(frame);
    return frame;
}

void ReaderNode::updateFrame(Frame &frame)
{
}

void ReaderNode::passFrame(const Frame &frame, std::stop_token st)
{
    mOutputQueue->push(frame, st);

    if (frame.idx == -1)
    {
        // End of video signal, wait for generation change
        mControlNode->generationWait(frame.generation);
    }
}
