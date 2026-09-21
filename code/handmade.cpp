/*
 * Game layer (platform-independent) stuff
 */

#include "handmade.h"


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


static void
game_render_gradient(GameBackBuffer &buffer, uint32_t x_offset, uint32_t y_offset)
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


static void
game_draw_rectangle(GameBackBuffer &buffer,
    float left, float right, float top, float bottom,
    float red, float green, float blue)
{
    int32_t left_x = floor_float(left);
    int32_t right_x = floor_float(right);
    int32_t top_y = floor_float(top);
    int32_t bottom_y = floor_float(bottom);

    if (left_x < 0) {
        left_x = 0;
    }

    if (right_x >= buffer.bitmap_width) {
        right_x = buffer.bitmap_width;
    }

    if (top_y < 0) {
        top_y = 0;
    }

    if (bottom_y >= buffer.bitmap_height) {
        bottom_y = buffer.bitmap_height;
    }

    uint32_t color = (uint32_t)((floor_float(red * 255.0f) << 16) |
                                (floor_float(green * 255.0f) << 8) |
                                (floor_float(blue * 255.0f)));

    uint8_t *pixel_addr = ((uint8_t*)buffer.bitmap_mem +
        (left_x * buffer.bytes_per_pixel) +
        (top_y * buffer.bitmap_pitch));

    for (int32_t y {top_y}; y < bottom_y; ++y) {
        uint32_t *pixel = (uint32_t*)pixel_addr;
        for (int32_t x {left_x}; x < right_x; ++x) {
            *pixel++ = color;
        }
        pixel_addr += buffer.bitmap_pitch;
    }
}

// this unpacks the tile map and tile from tile_map_tile_x/y
// maybe change to unpack_world_position??
inline static TileChunkPosition
unpack_tile_chunk_position(WorldMap &world_map, uint32_t tile_map_tile_x, uint32_t tile_map_tile_y)
{
    TileChunkPosition tile_chunk {};
    tile_chunk.chunk_x = tile_map_tile_x >> world_map.chunk_shift;
    tile_chunk.chunk_y = tile_map_tile_y >> world_map.chunk_shift;
    tile_chunk.tile_x = tile_map_tile_x & world_map.chunk_mask;
    tile_chunk.tile_y = tile_map_tile_y & world_map.chunk_mask;

    return tile_chunk;
}


inline static TileChunk*
get_tile_chunk(WorldMap &world_map, int32_t x, int32_t y)
{
    TileChunk *tile_chunk = nullptr;

    if ((x >= 0 && x < world_map.tile_map_x_count) &&
        y >= 0 && y < world_map.tile_map_y_count)
    {
        tile_chunk = &world_map.tile_chunks[y * world_map.tile_map_y_count + x];
    }

    return tile_chunk;
}


inline static int32_t
get_tile_chunk_tile(WorldMap &world_map, TileChunk *tile_chunk, int32_t x, int32_t y)
{
    int32_t tile_value {-1};

    if (tile_chunk) {
        tile_value = tile_chunk->tiles[y * world_map.tile_chunk_size + x];
    }

    return tile_value;
}


static int32_t
get_tile_chunk_tile(WorldMap &world_map, uint32_t x, uint32_t y)
{
    TileChunkPosition tile_chunk_pos = unpack_tile_chunk_position(world_map, x, y);
    TileChunk *tile_chunk = get_tile_chunk(world_map, tile_chunk_pos.chunk_x, tile_chunk_pos.chunk_y);

    int32_t tile_id = get_tile_chunk_tile(world_map, tile_chunk,
        tile_chunk_pos.tile_x, tile_chunk_pos.tile_y);

    return tile_id;
}


inline void
normalize_coordinate(WorldMap &world_map, uint32_t &tile, float &tile_rel_pos)
{
    // assuming that our world is toroidal topology which allows us to not care if tile wraps
    int32_t tile_offset = floor_float(tile_rel_pos / world_map.tile_meter_length);
    tile += tile_offset;
    tile_rel_pos -= tile_offset * world_map.tile_meter_length;
}


static void
normalize_world_position(WorldMap &world_map, WorldPosition &pos)
{
    normalize_coordinate(world_map, pos.tile_x, pos.tile_rel_x);
    normalize_coordinate(world_map, pos.tile_y, pos.tile_rel_y);
}


static bool
is_world_map_coordinate_valid(WorldMap &world_map, WorldPosition &world_pos)
{
    bool is_valid = false;
    int32_t tile_id = get_tile_chunk_tile(world_map, world_pos.tile_x, world_pos.tile_y);
    is_valid = (tile_id == 0);

    return is_valid;
}


void
game_update_and_render(ThreadContext &thread, GameMemory &memory,
    GameInput *input, GameBackBuffer &buffer)
{
    GameState *game_state = reinterpret_cast<GameState*>(memory.persistent_storage);
    if (!memory.is_initialized) {
        game_state->player_pos.tile_rel_x = 0.8f;
        game_state->player_pos.tile_rel_y = 0.8f;
        game_state->player_pos.tile_x = 3;
        game_state->player_pos.tile_y = 2;

        memory.is_initialized = true;
    }

    // Don't know if these are contsexpr since the function isn't
    constexpr int32_t tile_map_x_count = 1;
    constexpr int32_t tile_map_y_count = 1;
    constexpr int32_t tile_chunk_size = 256;
    constexpr float tile_pixel_length = 60.0f;
    constexpr float tile_meter_length = 1.4f;
    constexpr float screen_offset_x = 0.0f;
    float screen_offset_y = (float)buffer.bitmap_height;

    uint32_t tiles00[tile_chunk_size][tile_chunk_size] = {
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  1, 1, 0, 1, 0,  0, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1, 1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1},
        {1, 0, 0, 0,  0, 1, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 1, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  1, 1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 1,  1, 1, 0, 1, 0,  0, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 1,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 1,  1, 1, 0, 1, 1,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 1,  1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1},
        {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  1, 1, 0, 1, 0,  0, 1, 0, 0,  0, 1, 0, 0,  1, 1, 0, 0,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1, 1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1},
        {1, 0, 0, 0,  0, 1, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 1, 0, 1,  1, 1, 1, 0,  0, 0, 0, 0,  1},
        {1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  1, 1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  1},
        {1, 0, 1, 0,  0, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 1,  1, 1, 0, 1, 0,  0, 1, 1, 1,  0, 0, 0, 0,  0, 0, 0, 1,  1},
        {1, 0, 1, 1,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 1,  1, 1, 0, 1, 1,  0, 1, 0, 0,  0, 1, 0, 0,  0, 0, 1, 1,  1},
        {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1},
    };

    WorldMap world_map {
        .tile_map_x_count = tile_map_x_count,
        .tile_map_y_count = tile_map_y_count,
        .screen_offset_x = screen_offset_x,
        .screen_offset_y = screen_offset_y,
        .tile_chunk_size = tile_chunk_size,
        .chunk_mask = 0xFF, // 256 x 256 chunk sizes
        .chunk_shift = 8,
        .tile_pixel_length = tile_pixel_length,
        .tile_meter_length = tile_meter_length,
        .meters_to_pixels = tile_pixel_length / tile_meter_length,
    };

    TileChunk tile_chunks[tile_map_y_count][tile_map_x_count];

    tile_chunks[0][0].tiles = *tiles00; // access it as a 1-D array instead as 2-D (same as accessing the back buffer)
    world_map.tile_chunks = *tile_chunks;

    float player_width = 0.75f * world_map.tile_meter_length;
    float player_height = world_map.tile_meter_length - 0.4f;

    GameControllerInput input0 = input->controllers[0];
    // analog is controller joy stick
    if (input0.is_analog) {
    } else {
        float speed = 4.0f;
        float player_x {};
        float player_y {};

        if (input0.Input.Buttons.up.ended_down) {
            player_y = 1.0f;
        }

        if (input0.Input.Buttons.down.ended_down) {
            player_y = -1.0f;
        }

        if (input0.Input.Buttons.right.ended_down) {
            player_x = 1.0f;
        }

        if (input0.Input.Buttons.left.ended_down) {
            player_x = -1.0f;
        }

        // turns into a velocity
        player_x *= speed;
        player_y *= speed;

        // player x and y by delta time
        float new_player_x = game_state->player_pos.tile_rel_x + (player_x * input->target_seconds_per_frame);
        float new_player_y = game_state->player_pos.tile_rel_y + (player_y * input->target_seconds_per_frame);

        WorldPosition player_pos = game_state->player_pos;
        player_pos.tile_rel_x = new_player_x;
        player_pos.tile_rel_y = new_player_y;
        normalize_world_position(world_map, player_pos);

        WorldPosition player_pos_left_edge = player_pos;
        player_pos_left_edge.tile_rel_x -= 0.5f * player_width;
        normalize_world_position(world_map, player_pos_left_edge);

        WorldPosition player_pos_right_edge = player_pos;
        player_pos_right_edge.tile_rel_x += 0.5f * player_width;
        normalize_world_position(world_map, player_pos_right_edge);

        if (is_world_map_coordinate_valid(world_map, player_pos) &&
            is_world_map_coordinate_valid(world_map, player_pos_left_edge) &&
            is_world_map_coordinate_valid(world_map, player_pos_right_edge))
        {
            game_state->player_pos = player_pos;
        }
    }

    game_draw_rectangle(
        buffer,
        0.0f, 0.0f,
        960.0f, 540.0f,
        0.0f, 1.0f, 1.0f
    );

    // TILEMAP RENDERING
    // NOTE: Scrolling happens when we only render tiles that are
    // relative to the player position.
    float center_x = 0.5f * buffer.bitmap_width;
    float center_y = 0.5f * buffer.bitmap_height;

    for (int32_t rel_y {-10}; rel_y < 10; ++rel_y) {
        for (int32_t rel_x {-20}; rel_x < 20; ++rel_x) {
            // underflow wrapping here
            uint32_t y = game_state->player_pos.tile_y + rel_y;
            uint32_t x = game_state->player_pos.tile_x + rel_x;
            int32_t tile_id = get_tile_chunk_tile(world_map, x, y);
            float gray = 0.5f;

            if (tile_id == 1) {
                gray = 1.0f;
            }

            if (game_state->player_pos.tile_x == x && game_state->player_pos.tile_y == y) {
                gray = 0.0f;
            }

            // For smooth scrolling we You moved the within-tile offsets out of the player drawing and into the tile drawing, with opposite signs:
            // Walking right: tile_rel_x increases. Your tile left calculation subtracts that offset, so the map slides left.
            // Walking up: tile_rel_y increases. Your tile top calculation adds that offset, so the map slides down.
            float left = center_x
                - (world_map.meters_to_pixels * game_state->player_pos.tile_rel_x)
                + (rel_x * world_map.tile_pixel_length);
            float top = center_y
                + (world_map.meters_to_pixels * game_state->player_pos.tile_rel_y)
                - (rel_y * world_map.tile_pixel_length);
            float right = left + world_map.tile_pixel_length;
            float bottom = top + world_map.tile_pixel_length;

            game_draw_rectangle(
                buffer,
                left, right,
                top, bottom,
                gray, gray, gray
            );

            if (rel_x == 0) {
                game_draw_rectangle(
                    buffer,
                    left, left + 5.0f,
                    top, top + 5.0f,
                    0.0f, 0.7f, 0.7f
                );
            }
        }
    }

    // lock the player drawing to the anchor point (center) for the smooth scrolling
    float player_left_edge = center_x - world_map.meters_to_pixels
        * (0.5f * player_width);

    float player_bottom_edge = center_y + world_map.tile_pixel_length;

    // playera
    game_draw_rectangle(
        buffer,
        player_left_edge,
        player_left_edge + (player_width * world_map.meters_to_pixels),
        player_bottom_edge - (player_height * world_map.meters_to_pixels),
        player_bottom_edge,
        0.5f, 0.0f, 0.2f
    );
    /*
    game_draw_rectangle(
        buffer,
        game_state->player_pos.tile_rel_x - 5.0f,
        game_state->player_pos.tile_rel_y - 5.0f,
        game_state->player_pos.tile_rel_x + 5.0f,
        game_state->player_pos.tile_rel_y + 5.0f,
    );
    */
}
