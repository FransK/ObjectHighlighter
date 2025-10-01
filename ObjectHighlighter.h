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

static const std::string sWindowTitle{"Video"};

class ObjectHighlighter : public VideoProcessor
{
public:
    ObjectHighlighter() = default;
    void playVideo() override;
    void selectObjects(Frame &frame);
    // void saveVideoWithHighlights(const std::string &outputPath, const std::string &format);
    void captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame);

private:
    struct PlaybackState
    {
        ThreadSafeQ<Frame> processorQueue{8};
        ThreadSafeQ<Frame> writerQueue{8};

        std::stop_source stopSource;
    };

    cv::VideoWriter mVideoWriter;

    // Playback pipeline
    void captureFrames(PlaybackState &state);
    void updateTrackers(PlaybackState &state);
    void drawFrames(PlaybackState &state);

    bool handlePlaybackInput(int key, PlaybackState &state, Frame &myFrame);
};

#endif