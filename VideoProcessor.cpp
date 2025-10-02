#include "VideoProcessor.h"

#include <algorithm>
#include <iostream>

using std::cout;
using std::endl;

// Display video information such as FPS, frame count, duration, and resolution
void VideoProcessor::displayInfo()
{
    // Check if video is loaded
    if (!mControlNode.capIsOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    // Retrieve video properties
    int fps = static_cast<int>(mControlNode.capGet(cv::CAP_PROP_FPS));
    int frames = static_cast<int>(mControlNode.capGet(cv::CAP_PROP_FRAME_COUNT));
    int width = static_cast<int>(mControlNode.capGet(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(mControlNode.capGet(cv::CAP_PROP_FRAME_HEIGHT));

    // Display video information
    cout << "FPS: " << fps << endl;
    cout << "Frame count: " << frames << endl;
    cout << "Duration: " << frames / fps << "s" << endl;
    cout << "Resolution: " << width << " x " << height << endl;
}

// Load a video from the specified path
bool VideoProcessor::loadVideo(const std::string &videoPath)
{
    // Open the video file using the control node
    return mControlNode.capOpen(videoPath);
}

// Play the loaded video, allowing for pausing and rewinding
void VideoProcessor::playVideo()
{
    // Check if video is loaded
    if (!mControlNode.capIsOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    // Frame display loop
    cv::Mat frame;
    while (mControlNode.capRead(frame))
    {
        // Display the frame
        cv::imshow(sMainTitle, frame);

        // Handle key presses
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

    // Clean up and close windows
    cv::destroyAllWindows();
}

// Rewind the video by a specified number of frames
void VideoProcessor::rewindVideo(int frames)
{
    // Check if video is loaded
    if (!mControlNode.capIsOpened())
    {
        cout << "No video loaded." << endl;
        return;
    }

    // If frames is negative, rewind to the start
    if (frames < 0)
    {
        mControlNode.capSet(cv::CAP_PROP_POS_FRAMES, 0);
        return;
    }

    // Subtract 1 because POS_FRAMES is the next frame to be read
    int currentFrame = static_cast<int>(mControlNode.capGet(cv::CAP_PROP_POS_FRAMES)) - 1;

    // Set the new frame position, ensuring it doesn't go below 0
    mControlNode.capSet(cv::CAP_PROP_POS_FRAMES, std::max(currentFrame - frames, 0));
}
