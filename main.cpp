#include "ObjectHighlighter.h"
#include "VideoProcessor.h"

int main(int argc, char *argv[])
{
    ObjectHighlighter vp;
    vp.loadVideo("video.mp4");
    vp.displayInfo();

    vp.playVideo();
}