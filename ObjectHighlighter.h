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
static const std::string sSaveWindowTitle{"Saving..."};

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
    struct PlaybackState
    {
        ThreadSafeQueue<Frame> processorQueue{8};
        ThreadSafeQueue<Frame> writerQueue{8};
    };

    std::string mOutputPath;
    std::string mFormat;
    cv::VideoWriter mVideoWriter;

    // Playback pipeline
    void captureFrames(PlaybackState &state);
    void updateTrackers(PlaybackState &state);
    void drawFrames(PlaybackState &state);

    bool handlePlaybackInput(int key, PlaybackState &state, Frame &myFrame);
    void selectObjects(Frame &frame);
    bool loadWriter(const std::string &outputPath, const std::string &fourcc);
    void captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame);
};

#endif