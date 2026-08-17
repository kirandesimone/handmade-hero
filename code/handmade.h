#ifndef HANDMADE_H
#define HANDMADE_H

#include <cstdint>
#include <cmath>

#define ASSERT(expression) if(!(expression)) {*(int*)0 = 0;}
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

static constexpr float PI32 {3.1415926535f};
static constexpr uint32_t MAX_UINT32 {0xFFFFFFFF};
static constexpr uint64_t KIBIBYTES(uint64_t value) {return value * 1024;};
static constexpr uint64_t MEBIBYTES(uint64_t value) {return KIBIBYTES(value) * 1024;};
static constexpr uint64_t GIBIBYTES(uint64_t value) {return MEBIBYTES(value) * 1024;};
static constexpr uint64_t TEBIBYTES(uint64_t value) {return GIBIBYTES(value) * 1024;};

struct BackgroundScreenBuffer {
    void *bitmap_mem;
    int bitmap_height;
    int bitmap_width;
    int bitmap_pitch;
    int bytes_per_pixel;
};

struct GameSoundOutput {
    void *region1;
    void *region2;
    float volume;
    uint32_t region1_size;
    uint32_t region2_size;
    uint32_t running_frame_index;
    uint32_t tone_hz;
    uint32_t wave_period;
    uint32_t samples_per_sec;
    uint32_t frame_size;
    uint8_t channel_count;
};


struct GameButtonState {
    uint32_t half_transition_state;
    bool ended_down;
};

struct GameControllerInput {
    union input_t {
        GameButtonState buttons_array[4];
        struct buttons_t {
            GameButtonState up;
            GameButtonState right;
            GameButtonState down;
            GameButtonState left;
        } Buttons;
    } Input;
    bool is_analog;
};

struct GameInput {
    GameControllerInput controllers[4];
};

struct GameMemory {
    uint64_t persistent_storage_size;
    void *persistent_storage;
    uint64_t transient_storage_size;
    void *transient_storage;
    bool is_initialized;
};

struct GameState {
    uint32_t x_offset;
    uint32_t y_offset;
};


void game_fill_sound_output_buffer(GameSoundOutput &buffer);
void game_update_and_render(GameMemory &memory, GameSoundOutput &sound_output,
    GameInput *input, BackgroundScreenBuffer &buffer);

void *DEBUGplatform_read_entire_file(const char *filename);
void DEBUGplatform_free_file(void *memory);
bool DEBUGplatform_write_file(void *memory);

uint32_t pow2_round_up(uint32_t value);

#endif
