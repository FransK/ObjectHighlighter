#include "DataStructs.h"
#include "NodeRunner.h"
#include "ObjectHighlighter.h"
#include "ReaderNode.h"
#include "TrackerNode.h"
#include "OutputNode.h"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>

using std::cout;
using std::endl;

// Set the output path and format for the video writer
void ObjectHighlighter::writerSettings(const std::string &outputPath, const std::string &format)
{
    mOutputPath = outputPath;
    mFormat = format;
}

// Play the video with object highlighting and saving capabilities
void ObjectHighlighter::playVideo()
{
    // Check if video is loaded
    if (!mControlNode->capIsOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    auto readerTrackerQueue = std::make_shared<ThreadSafeQueue<Frame>>(8);
    auto trackerWriterQueue = std::make_shared<ThreadSafeQueue<Frame>>(8);

    auto readerNode = NodeRunner<ReaderNode>(ReaderNode(mControlNode, readerTrackerQueue),
                                             mControlNode);
    auto trackerNode = NodeRunner<TrackerNode>(TrackerNode(mControlNode, readerTrackerQueue, trackerWriterQueue),
                                               mControlNode);
    auto outputNode = NodeRunner<OutputNode>(OutputNode(sMainTitle, mOutputPath, mFormat, mControlNode, trackerWriterQueue),
                                             mControlNode);

    readerNode.start();
    trackerNode.start();
    outputNode.start();

    // Wait for processing to complete (e.g., when stop is requested)
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;

    std::stop_callback callback(mControlNode->stopSourceGet().get_token(), [&]()
                                {
                                    {
                                        std::scoped_lock lock(mtx);
                                        done = true;
                                    }
                                    cv.notify_all(); });

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&done]
                { return done; });
    }

    // Ensure all OpenCV windows are closed
    cv::destroyAllWindows();
}