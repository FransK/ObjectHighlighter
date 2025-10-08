#ifndef READER_NODE_H
#define READER_NODE_H

#include "ControlNode.h"
#include "DataStructs.h"
#include "ThreadSafeQueue.h"

#include "opencv2/videoio.hpp"

class ReaderNode
{
private:
    std::shared_ptr<ControlNode> mControlNode;
    std::shared_ptr<ThreadSafeQueue<Frame>> mOutputQueue;
    // No input queue needed for ReaderNode

public:
    ReaderNode(std::shared_ptr<ControlNode> controlNode,
               std::shared_ptr<ThreadSafeQueue<Frame>> outputQueue)
        : mControlNode(controlNode),
          mOutputQueue(outputQueue) {}
    ~ReaderNode() = default;

    ReaderNode(const ReaderNode &) = delete;
    ReaderNode &operator=(const ReaderNode &) = delete;
    ReaderNode(ReaderNode &&other) = default;
    ReaderNode &operator=(ReaderNode &&other) = default;

    // Node concept methods
    std::optional<Frame> getFrame(std::stop_token st);
    void updateFrame(Frame &frame);
    void passFrame(const Frame &frame, std::stop_token st);
};

#endif