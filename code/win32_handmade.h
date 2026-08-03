#ifndef WIN32_HANDMADE_H
#define WIN32_HANDMADE_H

#include <cstdint>
#include <Windows.h>
#include <winerror.h>
#include <stdio.h>


struct Win32Buffer {
    BITMAPINFO bitmap_info;
    void *bitmap_mem;
    int bitmap_height;
    int bitmap_width;
    int bitmap_pitch;
    int bytes_per_pixel;
};

struct Win32WinDimensions {
    int width;
    int height;
};

#endif // WIN32_HANDMADE_H
