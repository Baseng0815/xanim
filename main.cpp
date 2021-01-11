#include <X11/Xlib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <iostream>
#include <vector>

#define MAX_FRAMES 1024

struct RenderContext {
    Display*        dpy; // X11 display
    Window          rootw; // X11 root window
    SDL_Window*     sdlw; // SDL window
    SDL_Renderer*   sdlr; // SDL renderer
    int sdlw_width, sdlw_height; // SDL window dimensions
};


RenderContext setup();
std::vector<SDL_Texture*> loadTextures(const RenderContext&);
void cleanup(RenderContext*);

int main()
{
    RenderContext rc = setup();
    std::vector<SDL_Texture*> textures = loadTextures(rc);

    for (;;) {
        // actual rendering
        for (size_t img = 0; img < textures.size(); img++) {
            SDL_Rect dstArea { 0, 0, 1920, 1080 };
            SDL_RenderCopy(rc.sdlr, textures[img], NULL, &dstArea);
            SDL_RenderPresent(rc.sdlr);

            SDL_Delay(10);
        }

        // only need to check for quit event as rendering is done permanently
        // (hopefully this doesn't blow the CPU!)
        SDL_Event event;
        SDL_PollEvent(&event);
        if (event.type == SDL_QUIT) {
            break;
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
        exit(1);
    }
    rc.rootw = DefaultRootWindow(rc.dpy);
    std::cout << "root window grabbed\n";

    // create SDL renderer
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "failed to initialize SDL with SDL_INIT_VIDEO: " << SDL_GetError();
        exit(1);
    }

    if ((rc.sdlw = SDL_CreateWindowFrom((void*)rc.rootw)) == NULL) {
        std::cerr << "failed to create SDL window from root window: " << SDL_GetError();
        exit(1);
    }

    if ((rc.sdlr = SDL_CreateRenderer(rc.sdlw, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)) == NULL) {
        std::cerr << "failed to create SDL renderer from SDL window: " << SDL_GetError();
        exit(1);
    }

    // width and height
    SDL_GetWindowSize(rc.sdlw, &rc.sdlw_width, &rc.sdlw_height);
    std::cout << "SDL window and renderer successfully initialized; got dimensions of "
        << rc.sdlw_width << "x" << rc.sdlw_height << "\n";

    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        std::cerr << "failed to initialize SDL_image: " << IMG_GetError();
        exit(1);
    }

    return rc;
}

std::vector<SDL_Texture*> loadTextures(const RenderContext &rc)
{
    std::vector<SDL_Texture*> textures;

    // open video
    cv::VideoCapture vc("small.mp4");
    if (!vc.isOpened()) {
        std::cerr << "failed to open video\n";
        exit(1);
    }

    // get some properties
    cv::Mat firstFrame;
    vc >> firstFrame;
    int channels = firstFrame.channels();

    vc.set(cv::CAP_PROP_POS_FRAMES, 0);

    unsigned int width = vc.get(cv::CAP_PROP_FRAME_WIDTH);
    unsigned int height = vc.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << "image dimensions " << width << "x" << height << ", channels " << channels << "\n";
    if (width != rc.sdlw_width || height != rc.sdlw_height) {
        std::cout << "image dimensions and window dimensions differ; frames will be rendered accordingly\n";
    }

    uint8_t *pixelData = new uint8_t[width * height * 3];
    std::cout << width * height * 3 << "\n";
    size_t frameCount = vc.get(cv::CAP_PROP_FRAME_COUNT);

    // get textures
    for (size_t frameIndex = 0; frameIndex < frameCount; frameIndex++) {
        // get opencv frame
        int pct = (frameIndex + 1) / (float)frameCount * 100;
        std::cout << "decoding frame " << frameIndex << "... (" << pct << "%)\n";
        cv::Mat frame;
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

        textures.push_back(SDL_CreateTextureFromSurface(rc.sdlr, surface));
        SDL_SaveBMP(surface, "out.bmp");
        SDL_FreeSurface(surface);
    }

    delete[] pixelData;

    if (textures.size() <= 0) {
        std::cerr << "no textures were loaded\n";
        exit(1);
    }

    std::cout << textures.size() << " textures were created\n";

    return textures;
}

void cleanup(RenderContext *rc)
{
    XCloseDisplay(rc->dpy);

    SDL_Quit();
    SDL_DestroyWindow(rc->sdlw);
    SDL_DestroyRenderer(rc->sdlr);
}
