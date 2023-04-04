# xanim
## About the project
xanim is an animated wallpaper engine for X11 using Xlib, OpenCV and SDL which
uses low system resources (<1% CPU on my six-core machine). It works by
extracting frames from a video and pumping them into the X11 root window
via an SDL_Renderer (inspirated by [glouw/paperview](https://github.com/glouw/paperview)).
Frames are extracted using OpenCV so the supported formats depend on its support, but
the most common formats should work.

I tested xanim on my system (i5-8400) and it used <1% CPU for a Full-HD gif.
Although performance is not a problem, I have experienced issues when trying to
display large videos. **For some reason, SDL_Textures are not consistently put
into VRAM, so large videos might be put in your RAM and exhaust it**.

Another problem might arise when using composite managers like xcompton, picom or xcompmgr,
who draw to an off-screen buffer and then display the full frame with all windows
on the root window. **This causes problems and glitches so I discourage the
use of a composite manager**. If you manage to find the root for this problem,
please create a pull request or send it to me and I will gladly fix it.

## Getting started
This project uses a Makefile, so you need make and a C++ compiler like gcc or clang
to build the project. First clone the repository and then you can build it:
```sh
    git clone https://github.com/Baseng0815/xanim
    cd xanim
    make
    sudo make install
```

Requirements are SDL2, OpenCV and Xlib.

## Usage
```sh
    xanim [OPTION]... [FILE]
```
You can display the video either on a specific monitor, on each monitor,
stretched over all monitors or on a given, manual area. For details,
check out ```xanim --help```.

## Roadmap
* Fix the RAM issue
* Make the program work with composite managers

## Contact
E-Mail: [bastian.engel00@gmail.com](mailto:bastian.engel00@gmail.com)
