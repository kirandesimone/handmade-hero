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
    float *samples_out = reinterpret_cast<float*>(sound_output.buffer);

    // Write our sample data into the buffer
    for (uint32_t frame {}; frame < sound_output.available_frames; ++frame) {
        // Square Wave
        // float sample_value = (running_sample_index++ % wave_period < wave_period / 2) ? volume : -volume;
        float t = ((2.0f * PI32) * sound_output.running_sample_index) / sound_output.wave_period;
        float sample_value = sinf(t) * sound_output.volume;
        sound_output.running_sample_index++;

        for (uint32_t channel {}; channel < sound_output.channel_count; ++channel) {
            samples_out[frame * sound_output.channel_count + channel] = sample_value;
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
