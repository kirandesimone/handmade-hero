#ifndef WIN32_HANDMADE_H
#define WIN32_HANDMADE_H

#include "win32_wasapi.h"
#include "handmade.h"

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

struct Win32LoadedGameCode {
    HMODULE dll_handle;

    ptr_game_fill_sound_output_buffer fill_sound_output_buffer;
    ptr_game_update_and_render update_and_render;

    FILETIME last_write_time;

    bool is_stable;
};

struct Win32State {
    void *game_memory_block;
    void *file_record_handle;
    void *file_playback_handle;
    uint64_t game_memory_size;
    // if we want to same more than one inputs we need more slots
    uint16_t input_recording_slot;
    uint16_t input_playback_slot;
    bool is_recording;
    bool is_playback;
};

static Win32LoadedGameCode win32_load_game_code(void);
static void win32_unload_game_code(Win32LoadedGameCode &game);

#endif // WIN32_HANDMADE_H
