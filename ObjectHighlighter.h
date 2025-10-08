#ifndef OBJECT_HIGHLIGHTER
#define OBJECT_HIGHLIGHTER

#include "DataStructs.h"
#include "ThreadSafeQueue.h"
#include "VideoProcessor.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
#include <vector>

#include "opencv2/tracking.hpp"

// Window title for saving frames
constexpr int sProcessorQueueSize{8};
constexpr int sWriterQueueSize{8};

class ObjectHighlighter : public VideoProcessor
{
public:
    ObjectHighlighter() = default;
    ~ObjectHighlighter() override = default;
    // Delete copy and move constructors and assignment operators
    ObjectHighlighter(const ObjectHighlighter &) = delete;
    ObjectHighlighter &operator=(const ObjectHighlighter &) = delete;
    ObjectHighlighter(ObjectHighlighter &&other) = delete;
    ObjectHighlighter &operator=(ObjectHighlighter &&other) = delete;

    void playVideo() override;
    void writerSettings(const std::string &outputPath, const std::string &format);

private:
    std::string mOutputPath;
    std::string mFormat;
    cv::VideoWriter mVideoWriter;
};

#endif