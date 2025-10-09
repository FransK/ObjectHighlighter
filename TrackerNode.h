#ifndef TRACKER_NODE
#define TRACKER_NODE

#include "ControlNode.h"
#include "DataStructs.h"
#include "Node.h"
#include "ThreadSafeQueue.h"

class TrackerNode
{
private:
    std::shared_ptr<ControlNode> mControlNode;
    std::shared_ptr<ThreadSafeQueue<Frame>> mInputQueue;
    std::shared_ptr<ThreadSafeQueue<Frame>> mOutputQueue;

public:
    TrackerNode(std::shared_ptr<ControlNode> controlNode,
                std::shared_ptr<ThreadSafeQueue<Frame>> inputQueue,
                std::shared_ptr<ThreadSafeQueue<Frame>> outputQueue)
        : mControlNode(controlNode),
          mInputQueue(inputQueue),
          mOutputQueue(outputQueue) {}
    ~TrackerNode() = default;

    TrackerNode(const TrackerNode &) = delete;
    TrackerNode operator=(const TrackerNode &) = delete;

    TrackerNode(TrackerNode &&) noexcept = default;
    TrackerNode &operator=(TrackerNode &&) noexcept = default;

    // Node concept methods
    std::optional<Frame> getFrame(std::stop_token st);
    void updateFrame(Frame &f);
    void passFrame(const Frame &f, std::stop_token st);
};

#endif