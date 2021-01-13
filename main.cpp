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

    std::vector<SDL_Rect> monitors;
};

struct Video {
    std::vector<SDL_Texture*> sdlTextures; // SDL Textures
    int framerate; // Framerate in seconds
};

enum class DrawType {
    MONITOR, // video is played given monitor
    AREA, // video is played on given area
    STRETCH, // video is played over all monitors
    EACH // video is played on each monitor
};

struct Options {
    DrawType drawType = DrawType::MONITOR;
    int monitorIndex = 0;
    SDL_Rect targetArea;
    std::string videoFile;
};

Options parseOptions(int argc, char **argv);
RenderContext setup();
Video loadVideo(const RenderContext&, const std::string&);
void cleanup(RenderContext*);
void printHelp();

int main(int argc, char **argv)
{
    Options options = parseOptions(argc, argv);
    RenderContext rc = setup();
    Video video = loadVideo(rc, options.videoFile);

    if (options.drawType == DrawType::MONITOR && !(options.monitorIndex >= 0 && options.monitorIndex < rc.monitors.size())) {
        std::cerr << "monitor index not in range. max allowed: " << rc.monitors.size() - 1 << "\n";
        std::exit(EXIT_FAILURE);
    }

    int delay = 1000 / video.framerate;

    for (bool running = true; running;) {
        // actual rendering
        for (size_t img = 0; img < video.sdlTextures.size(); img++) {
            SDL_RenderClear(rc.sdlr);
            switch (options.drawType) {
                case DrawType::MONITOR:
                    SDL_RenderCopy(rc.sdlr, video.sdlTextures[img], NULL, &rc.monitors[options.monitorIndex]);
                    break;

                case DrawType::AREA:
                    SDL_RenderCopy(rc.sdlr, video.sdlTextures[img], NULL, &options.targetArea);
                    break;

                case DrawType::STRETCH:
                    SDL_RenderCopy(rc.sdlr, video.sdlTextures[img], NULL, NULL);
                    break;

                case DrawType::EACH:
                    for (const SDL_Rect &rect : rc.monitors) {
                        SDL_RenderCopy(rc.sdlr, video.sdlTextures[img], NULL, &rect);
                    }

            }
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

    return EXIT_SUCCESS;
}

Options parseOptions(int argc, char **argv)
{
    Options ops;

    PROGRAM_LOCATION = argv[0];

    option options[8];
    // help
    options[0].long_name = "help";
    options[0].short_name = 'h';
    options[0].flags = GOPT_ARGUMENT_FORBIDDEN;
    // version
    options[1].long_name = "version";
    options[1].short_name = 'v';
    options[1].flags = GOPT_ARGUMENT_FORBIDDEN;

    // monitor
    options[2].long_name = "monitor";
    options[2].short_name = 'm';
    options[2].flags = GOPT_ARGUMENT_REQUIRED;
    // area
    options[3].long_name = "area";
    options[3].short_name = 'a';
    options[3].flags = GOPT_ARGUMENT_REQUIRED;
    // stretch
    options[4].long_name = "stretch";
    options[4].short_name = 's';
    options[4].flags = GOPT_ARGUMENT_FORBIDDEN;
    // each
    options[5].long_name = "each";
    options[5].short_name = 'e';
    options[5].flags = GOPT_ARGUMENT_FORBIDDEN;

    // video file as argument
    options[6].long_name = "file";
    options[6].short_name = 'f';
    options[6].flags = GOPT_ARGUMENT_REQUIRED;

    // gopt needs a GOPT_LAST option
    options[7].flags = GOPT_LAST;

    argc = gopt(argv, options);
    gopt_errors(argv[0], options);

    // help
    if (options[0].count) {
        printHelp();
        std::exit(EXIT_SUCCESS);
    }

    // version
    if (options[1].count) {
        std::cout << VERSION << "\n";
        std::exit(EXIT_SUCCESS);
    }

    // monitor
    if (options[2].count) {
        ops.monitorIndex = atoi(options[2].argument);
        std::cout << "drawing on monitor of index " << ops.monitorIndex << "\n";
    // area
    } else if (options[3].count) {
        ops.drawType = DrawType::AREA;
        sscanf(options[2].argument, "%ix%i+%i+%i", &ops.targetArea.w, &ops.targetArea.h, &ops.targetArea.x, &ops.targetArea.y);
        printf("widthxheight+x+y %ix%i+%i+%i\n",ops.targetArea.w, ops.targetArea.h, ops.targetArea.x, ops.targetArea.y);
        std::cout << "drawing on area\n";
    // stretch
    } else if (options[4].count) {
        ops.drawType = DrawType::STRETCH;
        std::cout << "drawing stretched over all monitors\n";
    // each
    } else if (options[5].count) {
        ops.drawType = DrawType::EACH;
        std::cout << "drawing on each monitor\n";
    }

    // video file
    if (options[6].count) {
        ops.videoFile = options[6].argument;
    } else if (argc > 1) {
        ops.videoFile = argv[1];
    } else {
        std::cerr << "no video file specified\n";
        std::exit(EXIT_FAILURE);
    }

    return ops;
}

RenderContext setup()
{
    RenderContext rc;

    // get root window
    rc.dpy = XOpenDisplay(NULL);
    if (rc.dpy == NULL) {
        std::cerr << "failed to open X11 display\n";
        std::exit(EXIT_FAILURE);
    }
    rc.rootw = DefaultRootWindow(rc.dpy);
    std::cout << "root window grabbed\n";

    // create SDL renderer
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "failed to initialize SDL with SDL_INIT_VIDEO: " << SDL_GetError() << "\n";
        std::exit(EXIT_FAILURE);
    }

    if ((rc.sdlw = SDL_CreateWindowFrom((void*)rc.rootw)) == NULL) {
        std::cerr << "failed to create SDL window from root window: " << SDL_GetError() << "\n";
        std::exit(EXIT_FAILURE);
    }

    if ((rc.sdlr = SDL_CreateRenderer(rc.sdlw, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)) == NULL) {
        std::cerr << "failed to create SDL renderer from SDL window: " << SDL_GetError() << "\n";
        std::exit(EXIT_FAILURE);
    }

    // width and height of X11 root
    SDL_GetWindowSize(rc.sdlw, &rc.sdlwWidth, &rc.sdlwHeight);
    std::cout << "SDL window and renderer successfully initialized; got dimensions of "
        << rc.sdlwWidth << "x" << rc.sdlwHeight << "\n";

    // width and height of each individual monitor
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        SDL_Rect rect;
        SDL_GetDisplayBounds(i, &rect);
        rc.monitors.emplace_back(rect);
        printf("monitor %i dimensions: %ix%i+%i+%i\n", i, rect.w, rect.h, rect.x, rect.y);
    }

    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        std::cerr << "failed to initialize SDL_image: " << IMG_GetError() << "\n";
        std::exit(EXIT_FAILURE);
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
        std::exit(EXIT_FAILURE);
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
        std::exit(EXIT_FAILURE);
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
        -h, --help          view this help message\n\
        -m, --monitor       draw on a specific monitor\n\
        -a, --area          specify area (wxh+x+y)\n\
        -s, --stretch       stretch over all monitors\n\
        -e, --each          draw on each monitor\n\
        -f, --help          view this help message\n",
           VERSION, PROGRAM_LOCATION, AUTHOR);
}
