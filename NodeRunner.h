#ifndef NODE_RUNNER_H
#define NODE_RUNNER_H

#include "ControlNode.h"
#include "DataStructs.h"
#include "Node.h"
#include "ThreadSafeQueue.h"

#include <thread>
#include <stop_token>

template <Node NodeType>
class NodeRunner
{
private:
    NodeType mNodeLogic;
    std::shared_ptr<ControlNode> mControlNode;
    std::jthread mWorker;

    void run()
    {
        std::stop_token st = mControlNode->stopSourceGet().get_token();
        while (!st.stop_requested())
        {
            // Get a frame to process
            std::optional<Frame> frameOpt = mNodeLogic.getFrame(st);

            if (!frameOpt.has_value())
            {
                continue;
            }

            Frame frame = std::move(*frameOpt);

            if (frame.generation != mControlNode->generationGet())
            {
                continue;
            }

            // Process the frame using the node logic
            mNodeLogic.updateFrame(frame);

            // Do something with the updated frame
            mNodeLogic.passFrame(frame, st);
        }
    }

public:
    NodeRunner(NodeType &&nodeLogic,
               std::shared_ptr<ControlNode> controlNode)
        : mNodeLogic(std::move(nodeLogic)),
          mControlNode(controlNode)
    {
    }
    ~NodeRunner() = default;

    NodeRunner(const NodeRunner &) = delete;
    NodeRunner operator=(const NodeRunner &) = delete;
    NodeRunner(NodeRunner &&) = delete;
    NodeRunner operator=(NodeRunner &&) = delete;

    void start()
    {
        mWorker = std::jthread([this](std::stop_token st)
                               { run(); });
    }
};

#endif