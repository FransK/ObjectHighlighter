#ifndef OBJECT_HIGHLIGHTER
#define OBJECT_HIGHLIGHTER

#include "VideoProcessor.h"

#include "opencv2/tracking.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>

class ObjectHighlighter : public VideoProcessor
{
public:
    struct Highlight
    {
        uint frame;
        cv::Rect box;
    };

    struct ObjectTracker
    {
        cv::Ptr<cv::Tracker> tracker;
        cv::Rect box;
    };

    ObjectHighlighter() = default;
    void captureFrameWithHighlights(const std::string &outputPath, const cv::Mat &frame);
    void objectSelection(cv::Mat &frame);
    void playVideo() override;
    void saveVideoWithHighlights(const std::string &outputPath, const std::string &format);

private:
    struct PlaybackState
    {
        std::queue<cv::Mat> inputFrames{};
        std::queue<cv::Mat> processedFrames{};

        std::mutex inputMutex;
        std::mutex processedMutex;

        std::condition_variable inputCv;
        std::condition_variable processedCv;

        std::atomic<bool> readingFinished{false};
        std::atomic<bool> shuttingDown{false};
        std::atomic<bool> stateInvalid{false};
        std::atomic<bool> processingFinished{false};
    };

    struct SaveVideoState
    {
        std::queue<cv::Mat> inputFrames{};
        std::queue<cv::Mat> processedFrames{};

        std::mutex inputMutex;
        std::mutex processedMutex;

        std::condition_variable inputCv;
        std::condition_variable processedCv;

        std::atomic<bool> readingFinished{false};
        std::atomic<bool> processingFinished{false};
    };

    std::vector<Highlight> mHighlights; // TODO Further optimization: Add index into highlights for rewinding
    std::vector<ObjectTracker> mTrackers;

    void captureFrames(PlaybackState &state);
    void updateTrackers(PlaybackState &state);
    void drawFrames(PlaybackState &state);

    void readFrames(SaveVideoState &state);
    void drawHighlights(SaveVideoState &state);
    void writeFrames(const std::string &outputPath, const std::string &format, SaveVideoState &state);

    bool handlePlaybackInput(int key, cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &hlIter);
    void drawHighlightsOnFrame(cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &hlIter);
    void trackOnFrame(cv::Mat &frame);
};

#endif