// root window
#include <X11/Xlib.h>

// rendering
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

// video fram extraction
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <stdio.h>

// lightweight options parsing
#include "gopt.h"

const char *VERSION = "xanim version 1.0 (2021-01-12)";
const char *AUTHOR = "Bastian Engel <bastian.engel00@gmail.com>";
const char *PROGRAM_LOCATION;

struct RenderContext {
    Display*        dpy; // X11 display
    Window          rootw; // X11 root window
    SDL_Window*     sdlw; // SDL window
    SDL_Renderer*   sdlr; // SDL renderer
    int sdlwWidth, sdlwHeight; // SDL window dimensions
};

struct Video {
    std::vector<SDL_Texture*> sdlTextures; // SDL Textures
    int framerate; // Framerate in seconds
};

RenderContext setup();
Video loadVideo(const RenderContext&, const std::string&);
void cleanup(RenderContext*);
void printHelp();

int main(int argc, char **argv)
{
    PROGRAM_LOCATION = argv[0];

    struct option options[4];
    // help
    options[0].long_name = "help";
    options[0].short_name = 'h';
    options[0].flags = GOPT_ARGUMENT_FORBIDDEN;
    // version
    options[1].long_name = "version";
    options[1].short_name = 'v';
    options[1].flags = GOPT_ARGUMENT_FORBIDDEN;
    // video file as argument
    options[2].long_name = "file";
    options[2].short_name = 'f';
    options[2].flags = GOPT_ARGUMENT_REQUIRED;
    // video file as last
    options[3].flags = GOPT_LAST;

    argc = gopt(argv, options);
    gopt_errors(argv[0], options);

    if (options[0].count) {
        printHelp();
        exit(EXIT_SUCCESS);
    }

    if (options[1].count) {
        std::cout << VERSION << "\n";
        exit(EXIT_SUCCESS);
    }

    std::string file;
    if (options[2].count) {
        file = file.assign(options[2].argument);
    } else if (argc > 1) {
        file = file.assign(argv[1]);
    } else {
        std::cerr << "no video file specified\n";
        exit(EXIT_FAILURE);
    }

    // real functionality starts here
    RenderContext rc = setup();
    Video video = loadVideo(rc, file);
    int delay = 1000 / video.framerate;

    for (bool running = true; running;) {
        // actual rendering
        for (size_t img = 0; img < video.sdlTextures.size(); img++) {
            SDL_Rect dstArea { 0, 0, 1920, 1080 };
            SDL_RenderCopy(rc.sdlr, video.sdlTextures[img], NULL, &dstArea);
            SDL_RenderPresent(rc.sdlr);

            SDL_Delay(delay);

            // only need to check for quit event as rendering is done permanently
            // (hopefully this doesn't blow the CPU!)
            SDL_Event event;
            SDL_PollEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
        }
    }

    // cleanup
    cleanup(&rc);

    return 0;
}

RenderContext setup()
{
    RenderContext rc;

    // get root window
    rc.dpy = XOpenDisplay(NULL);
    if (rc.dpy == NULL) {
        std::cerr << "failed to open X11 display\n";
        exit(EXIT_FAILURE);
    }
    rc.rootw = DefaultRootWindow(rc.dpy);
    std::cout << "root window grabbed\n";

    // create SDL renderer
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "failed to initialize SDL with SDL_INIT_VIDEO: " << SDL_GetError() << "\n";
        exit(EXIT_FAILURE);
    }

    if ((rc.sdlw = SDL_CreateWindowFrom((void*)rc.rootw)) == NULL) {
        std::cerr << "failed to create SDL window from root window: " << SDL_GetError() << "\n";
        exit(EXIT_FAILURE);
    }

    if ((rc.sdlr = SDL_CreateRenderer(rc.sdlw, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)) == NULL) {
        std::cerr << "failed to create SDL renderer from SDL window: " << SDL_GetError() << "\n";
        exit(EXIT_FAILURE);
    }

    // width and height
    SDL_GetWindowSize(rc.sdlw, &rc.sdlwWidth, &rc.sdlwHeight);
    std::cout << "SDL window and renderer successfully initialized; got dimensions of "
        << rc.sdlwWidth << "x" << rc.sdlwHeight << "\n";

    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        std::cerr << "failed to initialize SDL_image: " << IMG_GetError() << "\n";
        exit(EXIT_FAILURE);
    }

    return rc;
}

Video loadVideo(const RenderContext &rc, const std::string &file)
{
    Video video;

    // open video
    cv::VideoCapture vc(file);
    if (!vc.isOpened()) {
        std::cerr << "failed to open video file " << file << "; make sure it exists and is valid\n";
        exit(EXIT_FAILURE);
    }

    std::cout << "loading video file " << file << "...\n";

    // get some properties
    cv::Mat firstFrame;
    vc >> firstFrame;
    int channels = firstFrame.channels();

    vc.set(cv::CAP_PROP_POS_FRAMES, 0);

    unsigned int width = vc.get(cv::CAP_PROP_FRAME_WIDTH);
    unsigned int height = vc.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << "image dimensions " << width << "x" << height << ", channels " << channels << "\n";
    if (width != rc.sdlwWidth || height != rc.sdlwHeight) {
        std::cout << "image dimensions and window dimensions differ; frames will be rendered accordingly\n";
    }
    video.framerate = vc.get(cv::CAP_PROP_FPS);

    uint8_t *pixelData = new uint8_t[width * height * 3];
    size_t frameCount = vc.get(cv::CAP_PROP_FRAME_COUNT);

    // get textures
    cv::Mat frame;
    for (size_t frameIndex = 0; frameIndex < frameCount; frameIndex++) {
        // get opencv frame
        int pct = (frameIndex + 1) / (float)frameCount * 100;
        std::cout << "parsing frame " << frameIndex << "... (" << pct << "%)\n";
        vc >> frame;

        // opencv mat format may differ, but we need a common pixel format to shove into
        // SDL (we will use 8 bits per channel with 3 channels)
        frame.convertTo(frame, CV_8U);

        for (size_t x = 0; x < width; x++) {
            for (size_t y = 0; y < height; y++) {
                // opencv uses BGR, but we want RGB
                uint8_t b = frame.at<uint8_t>(y, x * channels + 0);
                uint8_t g = frame.at<uint8_t>(y, x * channels + 1);
                uint8_t r = frame.at<uint8_t>(y, x * channels + 2);
                pixelData[3 * (y * width + x) + 0] = r;
                pixelData[3 * (y * width + x) + 1] = g;
                pixelData[3 * (y * width + x) + 2] = b;
            }
        }

        // then, convert to SDL_Surface
        SDL_Surface *surface = SDL_CreateRGBSurfaceFrom((void*)pixelData, width,
                                                        height, 24, width * 3, 0x0000ff, 0x00ff00, 0xff0000, 0);

        if (surface) {
            // for some reason textures are stored in RAM instead of VRAM so large videos
            // may cause problems, TODO fix or add warning
            SDL_Texture *texture = SDL_CreateTextureFromSurface(rc.sdlr, surface);
            if (texture) {
                video.sdlTextures.push_back(texture);
            } else {
                std::cerr << "Texture of frame " << frameIndex << " could not be created\n";
            }
            SDL_FreeSurface(surface);
        } else {
            std::cerr << "Surface of frame " << frameIndex << " could not be created\n";
        }

        SDL_Rect dstArea { 0, 0, 1920, 1080 };
        SDL_RenderCopy(rc.sdlr, video.sdlTextures.back(), NULL, &dstArea);
        SDL_RenderPresent(rc.sdlr);
    }

    delete[] pixelData;

    if (video.sdlTextures.size() <= 0) {
        std::cerr << "no textures were loaded\n";
        exit(EXIT_FAILURE);
    }

    std::cout << video.sdlTextures.size() << " textures were created\n";

    return video;
}

void cleanup(RenderContext *rc)
{
    XCloseDisplay(rc->dpy);

    SDL_Quit();
    SDL_DestroyWindow(rc->sdlw);
    SDL_DestroyRenderer(rc->sdlr);
}

void printHelp()
{
    printf("\
           %s\n\
           Usage: %s [OPTION]... [FILE]\n\
           ABOUT\n\
           This program was written by %s\n\
           and is licensed under the GPLv2.0\n\
           You can find the source here: https://github.com/Baseng0815/xanim\n\
           OPTIONS\n\
           -v, --version       show version information\n\
           -h, --help          view this help message\n",
           VERSION, PROGRAM_LOCATION, AUTHOR);
}
