#include <X11/Xlib.h>
#include <SDL2/SDL.h>
#include <unistd.h>
#include <stdio.h>

struct render_context {
    Display*        dpy; // X11 display
    Window          rootw; // X11 root window
    SDL_Window*     sdlw; // SDL window
    SDL_Renderer*   sdlr; // SDL renderer
};

struct textures {
    SDL_Texture *sdlt; // SDL textures
    size_t count; // texture count
};


struct render_context setup();
struct textures load_textures();
void cleanup(struct render_context *rc);

int main()
{
    struct render_context rc = setup();

    int prev_time = SDL_GetTicks();

    for (int i = 0;; i++) {
        int cur_time = SDL_GetTicks();
        int dt = cur_time - prev_time;
        prev_time = cur_time;

        SDL_Rect rect = {
            .h = 1000, .w = 1000
        };

        // actual rendering
        SDL_SetRenderDrawColor(rc.sdlr, 255, 0, 0, 255);

        SDL_RenderFillRect(rc.sdlr, &rect);
        SDL_RenderPresent(rc.sdlr);

        // event polling
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

struct render_context setup()
{
    struct render_context rc;
    // get root window
    rc.dpy = XOpenDisplay(NULL);
    if (rc.dpy == NULL) {
        fprintf(stderr, "failed to open the x display");
        exit(1);
    }

    rc.rootw = RootWindow(rc.dpy, DefaultScreen(rc.dpy));

    // create SDL renderer
    SDL_Init(SDL_INIT_VIDEO);
    rc.sdlw = SDL_CreateWindowFrom((void*)rc.rootw);
    rc.sdlr = SDL_CreateRenderer(rc.sdlw, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    return rc;
}

struct textures load_textures()
{
    struct textures tex;
}

void cleanup(struct render_context *rc)
{
    XCloseDisplay(rc->dpy);
    SDL_Quit();
    SDL_DestroyWindow(rc->sdlw);
    SDL_DestroyRenderer(rc->sdlr);
}
