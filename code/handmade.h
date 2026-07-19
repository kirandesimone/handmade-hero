#ifndef HANDMADE_H
#define HANDMADE_H

#include <cstdint>
#include <cmath>

static constexpr float PI32 {3.1415926535};

struct BackgroundScreenBuffer {
    void *bitmap_mem;
    int bitmap_height;
    int bitmap_width;
    int bitmap_pitch;
    int bytes_per_pixel;
};

struct GameSoundOutput {
    unsigned char *buffer;
    uint32_t available_frames;
    uint32_t running_sample_index;
    uint32_t tone_hz;
    uint32_t wave_period;
    uint32_t samples_per_sec;
    float volume;
    uint8_t channel_count;
};

static void game_update_and_render(BackgroundScreenBuffer &buffer, uint32_t x_offset, uint32_t y_offset);
static void game_fill_sound_output_buffer(GameSoundOutput &buffer);

#endif
