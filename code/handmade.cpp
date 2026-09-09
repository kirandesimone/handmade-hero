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

            *pixel = (green << 16) | blue;
            pixel++;
        }

        row += buffer.bitmap_pitch;
    }
}

inline static int32_t
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

inline static uint32_t
get_tile_map_tile(WorldMap &world_map, TileMap *tile_map, int32_t x, int32_t y)
{
    return tile_map->tiles[y * world_map.tile_map_width + x];
}

inline static TileMap*
get_tile_map(WorldMap &world_map, int32_t x, int32_t y)
{
    TileMap *tile_map = nullptr;

    if ((x >= 0 && x < world_map.tile_map_x_count) &&
        y >= 0 && y < world_map.tile_map_y_count)
    {
        tile_map = &world_map.tile_maps[y * world_map.tile_map_y_count + x];
    }

    return tile_map;
}

static NormalizedWorldPosition
get_normalized_world_position(WorldMap &world_map, WorldPosition &world_pos)
{
    NormalizedWorldPosition norm_world_pos {};

    norm_world_pos.tile_map_x = world_pos.tile_map_x;
    norm_world_pos.tile_map_y = world_pos.tile_map_y;

    float tile_map_relative_x = world_pos.x - world_map.screen_offset_x;
    float tile_map_relative_y = world_pos.y - world_map.screen_offset_y;

    norm_world_pos.tile_x = (int32_t)(tile_map_relative_x / world_map.tile_map_tile_width);
    norm_world_pos.tile_y = (int32_t)(tile_map_relative_y / world_map.tile_map_tile_height);

    // tile relative
    norm_world_pos.x = tile_map_relative_x - norm_world_pos.tile_x * world_map.tile_map_tile_width;
    norm_world_pos.y = tile_map_relative_y - norm_world_pos.tile_y * world_map.tile_map_tile_height;

    // When the player moves off the current tile map. we need to find the next valid tile map
    if (new_tile_x < 0) {
        new_tile_x = world_map.tile_map_width + new_tile_x;
        --norm_world_pos.tile_map_x;
    }

    if (new_tile_x >= world_map.tile_map_width) {
        new_tile_x = world_map.tile_map_width - new_tile_x;
        ++norm_world_pos.tile_map_x;
    }

    if (new_tile_y < 0) {
        new_tile_y = world_map.tile_map_height + new_tile_y;
        --norm_world_pos.tile_map_y;
    }

    if (new_tile_y >= world_map.tile_map_height) {
        new_tile_y = world_map.tile_map_height - new_tile_y;
        ++norm_world_pos.tile_map_y;
    }

    return norm_world_pos;
}

static bool
is_world_map_coordinate_valid(WorldMap &world_map, WorldPosition &world_pos)
{
    bool is_valid = false;
    NormalizedWorldPosition norm_world_pos = get_normalized_world_position(world_map, world_pos);
    TileMap *tile_map = get_tile_map(world_map, norm_world_pos.tile_map_x, norm_world_pos.tile_map_y);

    if (tile_map) {
        if ((new_player_tile_x >= 0 && new_player_tile_x < world_map.tile_map_width) &&
            new_player_tile_y >= 0 && new_player_tile_y < world_map.tile_map_height)
        {
            uint32_t tile_id = get_tile_map_tile(world_map, tile_map, new_player_tile_x, new_player_tile_y);
            is_valid = (tile_id == 0);
        }
    }

    return is_valid;
}

void
game_update_and_render(ThreadContext &thread, GameMemory &memory,
    GameInput *input, BackgroundScreenBuffer &buffer)
{
    GameState *game_state = reinterpret_cast<GameState*>(memory.persistent_storage);
    if (!memory.is_initialized) {
        game_state->player_x = 220.0f;
        game_state->player_y = 150.0f;
        game_state->player_tile_map_x = 0;
        game_state->player_tile_map_y = 0;
        memory.is_initialized = true;
    }

    // Don't know if these are contsexpr since the function isn't
    constexpr int32_t tile_map_x_count = 2;
    constexpr int32_t tile_map_y_count = 2;
    constexpr int32_t tile_map_height = 9;
    constexpr int32_t tile_map_width = 17;
    constexpr float tile_map_origin_x = 10.0f;
    constexpr float tile_map_origin_y = 10.0f;
    constexpr float tile_width = 56.0f;
    constexpr float tile_height = 56.0f;

    uint32_t tiles00[tile_map_height][tile_map_width] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1},
        {1, 0, 0, 0,  0, 1, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  0},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 1,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 1,  1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1}
    };

    uint32_t tiles01[tile_map_height][tile_map_width] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1},
        {0, 0, 0, 0,  0, 1, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 1,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 1,  1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1}
    };

    uint32_t tiles10[tile_map_height][tile_map_width] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1},
        {1, 0, 0, 0,  0, 1, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  0},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 1,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 1,  1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1}
    };

    uint32_t tiles11[tile_map_height][tile_map_width] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1},
        {0, 0, 0, 0,  0, 1, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 1,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 1,  1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1}
    };

    WorldMap world_map {
        .tile_map_x_count = tile_map_x_count,
        .tile_map_y_count = tile_map_y_count,
        .screen_origin_x = tile_map_origin_x,
        .screen_origin_y = tile_map_origin_y,
        .tile_map_width = tile_map_width,
        .tile_map_height = tile_map_height,
        .tile_map_tile_width = tile_width,
        .tile_map_tile_height = tile_height,
    };

    TileMap tile_maps[tile_map_x_count][tile_map_y_count];

    tile_maps[0][0].tiles = *tiles00; // access it as a 1-D array instead as 2-D (same as accessing the back buffer)
    tile_maps[0][1].tiles = *tiles01;
    tile_maps[1][0].tiles = *tiles10;
    tile_maps[1][1].tiles = *tiles11;

    world_map.tile_maps = *tile_maps;

    float player_width = 0.75f * world_map.tile_map_tile_width;
    float player_height = world_map.tile_map_tile_height;

    GameControllerInput input0 = input->controllers[0];
    // analog is controller joy stick
    if (input0.is_analog) {
    } else {
        // pixels per second
        float player_velocity_x {};
        float player_velocity_y {};

        if (input0.Input.Buttons.up.ended_down) {
            player_y = -1.0f;
        }

        if (input0.Input.Buttons.down.ended_down) {
            player_y = 1.0f;
        }

        if (input0.Input.Buttons.right.ended_down) {
            player_x = 1.0f;
        }

        if (input0.Input.Buttons.left.ended_down) {
            player_x = -1.0f;
        }

        float speed = 64.0f;
        player_x *= speed;
        player_y *= speed;

        float new_player_x = game_state->player_x + (player_pixels_x * input->target_seconds_per_frame);
        float new_player_y = game_state->player_y + (player_pixels_y * input->target_seconds_per_frame);

        if (is_world_map_coordinate_valid(world_map, game_state->player_tile_map_x, game_state->player_tile_map_y, new_player_x, new_player_y) &&
            is_world_map_coordinate_valid(world_map, game_state->player_tile_map_x, game_state->player_tile_map_y, (new_player_x - 0.5f * player_width), new_player_y) &&
            is_world_map_coordinate_valid(world_map, game_state->player_tile_map_x, game_state->player_tile_map_y, (new_player_x + 0.5f * player_width), new_player_y))
        {
            game_state->player_x = new_player_x;
            game_state->player_y = new_player_y;
        }
    }

    TileMap *tile_map = get_tile_map(world_map, game_state->player_tile_map_x, game_state->player_tile_map_y);

    game_draw_rectangle(
        buffer,
        0.0f, 0.0f,
        960.0f, 540.0f,
        0.0f, 1.0f, 1.0f
    );

    // TILEMAP RENDERING
    for (int32_t y {}; y < tile_map_height; ++y) {
        for (int32_t x {}; x < tile_map_width; ++x) {
            uint32_t tile_id = get_tile_map_tile(world_map, tile_map, x, y);
            float gray = 0.5f;
            if (tile_id == 1) {
                gray = 1.0f;
            }

            float min_y = (float)(tile_map_origin_y + (y * tile_height));
            float min_x = (float)(tile_map_origin_x + (x * tile_width));
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

    float player_left_edge = game_state->player_x - 0.5f * player_width;
    float player_top_edge = game_state->player_y - player_height;

    // player
    game_draw_rectangle(
        buffer,
        player_left_edge, player_top_edge,
        player_left_edge + player_width, player_top_edge + player_height,
        0.5f, 0.0f, 0.2f
    );

    game_draw_rectangle(
        buffer,
        game_state->player_x - 5.0f, game_state->player_y - 5.0f,
        game_state->player_x + 5.0f, game_state->player_y + 5.0f,
        0.0f, 0.7f, 0.7f
    );
}
