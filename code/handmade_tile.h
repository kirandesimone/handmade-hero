#ifndef HANDMADE_TILE_H
#define HANDMADE_TILE_H

#include <cstdint>

struct TileChunk {
    uint32_t *tiles;
};

// This is still world space just being queried in a granular way
struct TileChunkPosition {
    uint32_t chunk_x;
    uint32_t chunk_y;

    uint32_t tile_x;
    uint32_t tile_y;
};

struct TileMapPosition {
    // the first 24 bits == tile chunk then lower 8 is tile within tile chunk
    uint32_t tile_chunk_tile_x; // acts like virtual addresses
    uint32_t tile_chunk_tile_y;

    // in meters relative to a tile
    float tile_rel_x;
    float tile_rel_y;
};

// tile maps will be stored sparsely to reduce memory waste
struct TileMap {
    TileChunk *tile_chunks;

    int32_t tile_map_x_count; // how many tile maps across
    int32_t tile_map_y_count;

    int32_t tile_chunk_size; // how many tiles wide and tall a tile chunk is

    uint32_t chunk_mask;
    uint32_t chunk_shift;

    float tile_pixel_length;
    float tile_meter_length;
    float meters_to_pixels;
};

bool is_tile_map_coordinate_valid(TileMap &tile_map, TileMapPosition &tile_map_pos);
int32_t get_tile_chunk_tile(TileMap &tile_map, uint32_t x, uint32_t y);

void normalize_tile_map_position(TileMap &tile_map, TileMapPosition &pos);

#endif // HANDMADE_TILE_H
