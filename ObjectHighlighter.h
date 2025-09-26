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
    std::vector<Highlight> mHighlights; // TODO Further optimization: Add index into highlights for rewinding
    std::vector<ObjectTracker> mTrackers;

    std::queue<cv::Mat> mInputFrames;
    std::queue<cv::Mat> mProcessedFrames;

    std::mutex mInputMutex;
    std::mutex mProcessedMutex;

    std::condition_variable mInputCv;
    std::condition_variable mProcessedCv;

    std::atomic<bool> mReadingFinished{false};
    std::atomic<bool> mProcessingFinished{false};

    void readFrames();
    void drawHighlights();
    void writeFrames(const std::string &outputPath, const std::string &format);

    bool handlePlaybackInput(int key, cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &hlIter);
    void drawHighlightsOnFrame(cv::Mat &frame, uint framec, std::vector<Highlight>::iterator &hlIter);
    void trackOnFrame(cv::Mat &frame);
};

#endif