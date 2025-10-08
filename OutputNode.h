#ifndef OUTPUT_NODE_H
#define OUTPUT_NODE_H

#include "ControlNode.h"
#include "DataStructs.h"
#include "ThreadSafeQueue.h"

#include <optional>
#include <stop_token>
#include <string>

#include "opencv2/videoio.hpp"

constexpr int cFramesToRewind = 10 * 30; // Assuming 30 FPS, rewind 10 seconds

class OutputNode
{
private:
    cv::VideoWriter mVideoWriter;
    std::string mWindowName;
    std::string mSaveWindowName{"Saving..."};
    std::string mOutputPath;
    std::string mFormat{"mp4v"};
    std::shared_ptr<ControlNode> mControlNode;
    std::shared_ptr<ThreadSafeQueue<Frame>> mInputQueue;
    // No output queue needed for OutputNode

    bool handlePlaybackInput(int key, const Frame &frame);
    void selectObjects(const Frame &frame);
    void rewindVideo(int frameCount);
    bool loadWriter(const std::string &outputPath, const std::string &fourcc);
    void captureFrameWithHighlights(const std::string &filename, const cv::Mat &image);

public:
    OutputNode(const std::string &windowName,
               const std::string &outputPath,
               const std::string &format,
               std::shared_ptr<ControlNode> controlNode,
               std::shared_ptr<ThreadSafeQueue<Frame>> inputQueue)
        : mWindowName(windowName),
          mOutputPath(outputPath),
          mFormat(format),
          mControlNode(controlNode),
          mInputQueue(inputQueue) {}
    ~OutputNode() = default;

    OutputNode(const OutputNode &) = delete;
    OutputNode &operator=(const OutputNode &) = delete;
    OutputNode(OutputNode &&) = default;
    OutputNode &operator=(OutputNode &&) = default;

    // Node concept methods
    std::optional<Frame> getFrame(std::stop_token st);
    void updateFrame(Frame &frame);
    void passFrame(const Frame &frame, std::stop_token st);
};

#endif