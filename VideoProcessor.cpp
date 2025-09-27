#include "VideoProcessor.h"

#include <algorithm>
#include <iostream>

using std::cout;
using std::endl;

void VideoProcessor::displayInfo()
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    int fps = static_cast<int>(mCap.get(cv::CAP_PROP_FPS));
    int frames = static_cast<int>(mCap.get(cv::CAP_PROP_FRAME_COUNT));
    int width = static_cast<int>(mCap.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(mCap.get(cv::CAP_PROP_FRAME_HEIGHT));

    cout << "FPS: " << fps << endl;
    cout << "Frame count: " << frames << endl;
    cout << "Duration: " << frames / fps << "s" << endl;
    cout << "Resolution: " << width << " x " << height << endl;
}

void VideoProcessor::loadVideo(const std::string &videoPath)
{
    mCap.open(videoPath);
}

void VideoProcessor::playVideo()
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    cv::Mat frame;
    while (mCap.read(frame))
    {
        cv::imshow("Video", frame);

        int key = cv::waitKey(1);
        if (key == 'q')
        {
            break;
        }
        else if (key == 'p')
        {
            cv::waitKey(0);
        }
        else if (key == 'r')
        {
            this->rewindVideo(-1);
        }
        else if (key == 'z')
        {
            this->rewindVideo(290);
        }
    }

    cv::destroyAllWindows();
}

void VideoProcessor::rewindVideo(int frames)
{
    if (!mCap.isOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    if (frames < 0)
    {
        mCap.set(cv::CAP_PROP_POS_FRAMES, 0);
        return;
    }

    // Subtract 1 because POS_FRAMES is the next frame read
    int currentFrame = static_cast<int>(mCap.get(cv::CAP_PROP_POS_FRAMES)) - 1;
    mCap.set(cv::CAP_PROP_POS_FRAMES, std::max(currentFrame - frames, 0));
}
