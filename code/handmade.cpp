/*
 * Game layer (platform-independent) stuff
 */

#include "handmade.h"

static void
game_render_gradient(BackgroundScreenBuffer &buffer, uint32_t x_offset, uint32_t y_offset)
{
    uint8_t *row = reinterpret_cast<uint8_t *>(buffer.bitmap_mem);
    for (int32_t y = 0; y < buffer.bitmap_height; ++y) {
        uint32_t *pixel = reinterpret_cast<uint32_t *>(row);

        for (int32_t x = 0; x < buffer.bitmap_width; ++x) {
            uint8_t green = static_cast<uint8_t>(x + x_offset);
            uint8_t blue  = static_cast<uint8_t>(y + y_offset);

            *pixel = (green << 8) | blue;
            pixel++;
        }

        row += buffer.bitmap_pitch;
    }
}

void
game_fill_sound_output_buffer(GameSoundOutput &sound_output)
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
    // Write our sample data into the buffer
    for (uint32_t frame_count {}; frame_count < free_frames; ++frame_count) {
        // Square Wave
        // float sample_value = (running_sample_index++ % wave_period < wave_period / 2) ? volume : -volume;
        float t = ((2.0f * PI32) * sound_output.running_frame_index) / sound_output.wave_period;
        float frame_value = sinf(t) * sound_output.volume;
        sound_output.running_frame_index++;
        if (frame_count > (sound_output.region1_size / sound_output.frame_size)) {
            frames_out = region2_out;
        }

        for (uint32_t channel {}; channel < sound_output.channel_count; ++channel) {
            frames_out[frame_count * sound_output.channel_count + channel] = frame_value;
        }
    }
}

void
game_update_and_render(GameMemory &memory, GameSoundOutput &sound_output,
    GameInput *input, BackgroundScreenBuffer &buffer)
{
    GameState *state = reinterpret_cast<GameState*>(memory.persistent_storage);
    if (!memory.is_initialized) {
        state->x_offset = 0;
        state->y_offset = 0;
        memory.is_initialized = true;
    }

    GameControllerInput input0 = input->controllers[0];
    // analog is controller joy stick
    if (input0.is_analog) {

    } else {

    }

    if (input0.Input.Buttons.up.ended_down) {
        state->y_offset--;
    }

    void *file_memory = DEBUGplatform_read_entire_file("test.txt");
    game_fill_sound_output_buffer(sound_output);
    game_render_gradient(buffer, state->x_offset, state->y_offset);
}
