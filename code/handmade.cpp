/*
 * Game layer (platform-independent) stuff
 */

#include "handmade.h"
#include <cstdint>


static void
game_render_gradient(BackgroundScreenBuffer &buffer, uint32_t x_offset, uint32_t y_offset)
{
    uint8_t *row = reinterpret_cast<uint8_t *>(buffer.bitmap_mem);
    for (int32_t y = 0; y < buffer.bitmap_height; ++y) {
        uint32_t *pixel = reinterpret_cast<uint32_t *>(row);

        for (int32_t x = 0; x < buffer.bitmap_width; ++x) {
            uint8_t green = static_cast<uint8_t>(x + x_offset);
            uint8_t blue  = static_cast<uint8_t>(y + y_offset);

            *pixel = (green << 16) | blue;
            pixel++;
        }

        row += buffer.bitmap_pitch;
    }
}

static int32_t
round_float(float value)
{
    // change to something better
    return (int32_t)(value + 0.5f);
}

static void
game_draw_rectangle(BackgroundScreenBuffer &buffer,
    float fmin_x, float fmin_y, float fmax_x, float fmax_y,
    float red, float green, float blue)
{
    int32_t min_x = round_float(fmin_x);
    int32_t max_x = round_float(fmax_x);
    int32_t min_y = round_float(fmin_y);
    int32_t max_y = round_float(fmax_y);

    if (min_x < 0) {
        min_x = 0;
    }

    if (max_x >= buffer.bitmap_width) {
        max_x = buffer.bitmap_width;
    }

    if (min_y < 0) {
        min_y = 0;
    }

    if (max_y >= buffer.bitmap_height) {
        max_y = buffer.bitmap_height;
    }

    uint32_t color = (uint32_t)((round_float(red * 255.0f) << 16) |
                                (round_float(green * 255.0f) << 8) |
                                (round_float(blue * 255.0f)));

    uint8_t *pixel_addr = ((uint8_t*)buffer.bitmap_mem +
        (min_x * buffer.bytes_per_pixel) +
        (min_y * buffer.bitmap_pitch));

    for (int32_t y {min_y}; y < max_y; ++y) {
        uint32_t *pixel = (uint32_t*)pixel_addr;
        for (int32_t x {min_x}; x < max_x; ++x) {
            *pixel++ = color;
        }
        pixel_addr += buffer.bitmap_pitch;
    }
}

void
game_fill_sound_output_buffer(ThreadContext &thread, GameSoundOutput &sound_output)
{
    // tone_hz = roughly the hz(cycles per sec) for middle C
    // wave_period = how many frames it takes to complete one whole cycle of the tone
    // running_index_sample = allows us to run the tone infinitely without having a "pop" noise at
    // the end of each completed wave
    sound_output.wave_period = sound_output.samples_per_sec / sound_output.tone_hz;
    float *region1_out = reinterpret_cast<float*>(sound_output.region1);
    float *region2_out = reinterpret_cast<float*>(sound_output.region2);
    float *frames_out = region1_out;
    uint32_t free_frames = (sound_output.region1_size + sound_output.region2_size) / sound_output.frame_size;
    uint32_t region1_size_frame_count = (sound_output.region1_size / sound_output.frame_size);
    uint32_t region_index {};

    // Write our sample data into the buffer
    for (uint32_t frame_count {}; frame_count < free_frames; ++frame_count) {
        // Square Wave
        // float sample_value = (running_sample_index++ % wave_period < wave_period / 2) ? volume : -volume;
        float t = ((2.0f * PI32) * sound_output.running_frame_index) / sound_output.wave_period;
        float frame_value = sinf(t) * sound_output.volume;
        sound_output.running_frame_index++;
        region_index = frame_count;

        if (frame_count >= region1_size_frame_count) {
            frames_out = region2_out;
            region_index = frame_count - region1_size_frame_count;
        }

        for (uint32_t channel {}; channel < sound_output.channel_count; ++channel) {
            frames_out[region_index * sound_output.channel_count + channel] = frame_value;
        }
    }
}

void
game_update_and_render(ThreadContext &thread, GameMemory &memory,
    GameInput *input, BackgroundScreenBuffer &buffer)
{
    GameState *game_state = reinterpret_cast<GameState*>(memory.persistent_storage);
    if (!memory.is_initialized) {
        memory.is_initialized = true;
    }

    GameControllerInput input0 = input->controllers[0];
    // analog is controller joy stick
    if (input0.is_analog) {

    } else {
        float dt_player_x {};
        float dt_player_y {};

        if (input0.Input.Buttons.up.ended_down) {
            dt_player_y = -1.0f;
        }

        if (input0.Input.Buttons.down.ended_down) {
            dt_player_y = 1.0f;
        }

        if (input0.Input.Buttons.right.ended_down) {
            dt_player_x = 1.0f;
        }

        if (input0.Input.Buttons.left.ended_down) {
            dt_player_x = -1.0f;
        }

        dt_player_x *= 100.0f;
        dt_player_y *= 100.0f;

        game_state->player_x += (input->target_seconds_per_frame * dt_player_x);
        game_state->player_y += (input->target_seconds_per_frame * dt_player_y);
    }

    game_draw_rectangle(
        buffer,
        0.0f, 0.0f,
        800.0f, 800.0f,
        0.75f, 0.75f, 0.1f
    );

    constexpr uint32_t tilemap_height = 9;
    constexpr uint32_t tilemap_width = 17;
    constexpr uint32_t tile_height = 50;
    constexpr uint32_t tile_width = 50;
    constexpr uint32_t offset_x = 0;
    constexpr uint32_t offset_y = 0;

    uint32_t tilemap[tilemap_height][tilemap_width] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1},
        {0, 0, 0, 0,  0, 1, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  0},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 1,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 1,  1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1}
    };

    for (uint32_t y {}; y < tilemap_height; ++y) {
        for (uint32_t x {}; x < tilemap_width; ++x) {
            uint32_t tile_id = tilemap[y][x];
            float gray = 0.5f;
            if (tile_id == 1) {
                gray = 1.0f;
            }

            float min_y = (float)(offset_y + (y * tile_height));
            float min_x = (float)(offset_x + (x * tile_width));
            float max_y = (float)(min_y + tile_height);
            float max_x = (float)(min_x + tile_width);

            game_draw_rectangle(
                buffer,
                min_x, min_y,
                max_x, max_y,
                gray, gray, gray
            );
        }
    }

    // player
    float player_width = 0.75f * tile_width;
    float player_height = (float)tile_height;
    float player_left_edge = game_state->player_x - 0.5f * player_width;
    float player_top_edge = game_state->player_y - player_height;

    game_draw_rectangle(
        buffer,
        player_left_edge, player_top_edge,
        player_left_edge + player_width, player_top_edge + player_height,
        0.5f, 0.0f, 0.2f
    );
}
