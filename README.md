# Euclase Web Engine :3
## Info
This is my web engine made in c++. It does not use any chromium libraries or other similar projects, opting to manually parse html and css.

It has a basic GUI, and can run simple websites.
CSS support is limited, but growing.


## Building
This uses CMake. to build, I would recommend using a CMake GUI in an IDE, but it builds normally. Make sure you initialize the submodules.

This project uses Curl, Freetype, my custom image loader, libwebp, nanosvg, and later the v8 javascript engine.

You will need to build and provide the v8 engine separetely, which I wish you luck with.