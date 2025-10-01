#ifndef OBJECT_HIGHLIGHTER
#define OBJECT_HIGHLIGHTER

#include "DataStructs.h"
#include "ThreadSafeQ.h"
#include "VideoProcessor.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stop_token>
#include <vector>

#include "opencv2/tracking.hpp"

static const std::string sSaveWindowTitle{"Saving..."};

class ObjectHighlighter : public VideoProcessor
{
public:
    ObjectHighlighter() = default;
    void playVideo() override;
    void writerSettings(const std::string &outputPath, const std::string &format);

private:
    struct PlaybackState
    {
        ThreadSafeQ<Frame> processorQueue{8};
        ThreadSafeQ<Frame> writerQueue{8};

        std::stop_source stopSource;
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