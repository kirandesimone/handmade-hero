#include "handmade_tile.h"
#include "handmade_math.h"


inline void
normalize_coordinate(TileMap &tile_map, uint32_t &tile_chunk_tile, float &tile_rel_pos)
{
    // assuming that our world is toroidal topology which allows us to not care if tile wraps
    // rounding makes tile_rel_pos relative to the center of a tile
    int32_t tile_offset = round_float(tile_rel_pos / tile_map.tile_meter_length);
    tile_chunk_tile += tile_offset;
    tile_rel_pos -= tile_offset * tile_map.tile_meter_length;
}

// takes a world position and maps it to a tile in a tile chunk??
void
normalize_tile_map_position(TileMap &tile_map, TileMapPosition &pos)
{
    normalize_coordinate(tile_map, pos.tile_chunk_tile_x, pos.tile_rel_x);
    normalize_coordinate(tile_map, pos.tile_chunk_tile_y, pos.tile_rel_y);
}


// this unpacks the tile chunk and tile from tile_map_tile_x/y
inline static TileChunkPosition
unpack_tile_chunk_position(TileMap &tile_map, uint32_t tile_chunk_tile_x, uint32_t tile_chunk_tile_y)
{
    TileChunkPosition tile_chunk {};
    tile_chunk.chunk_x = tile_chunk_tile_x >> tile_map.chunk_shift;
    tile_chunk.chunk_y = tile_chunk_tile_y >> tile_map.chunk_shift;
    tile_chunk.tile_x = tile_chunk_tile_x & tile_map.chunk_mask;
    tile_chunk.tile_y = tile_chunk_tile_y & tile_map.chunk_mask;

    return tile_chunk;
}


inline static TileChunk*
get_tile_chunk(TileMap &tile_map, int32_t x, int32_t y)
{
    TileChunk *tile_chunk = nullptr;

    if ((x >= 0 && x < tile_map.tile_map_x_count) &&
        y >= 0 && y < tile_map.tile_map_y_count)
    {
        tile_chunk = &tile_map.tile_chunks[y * tile_map.tile_map_y_count + x];
    }

    return tile_chunk;
}


inline static int32_t
access_tile_chunk_tile(TileMap &tile_map, TileChunk *tile_chunk, int32_t x, int32_t y)
{
    int32_t tile_value {-1};

    if (tile_chunk) {
        tile_value = tile_chunk->tiles[y * tile_map.tile_chunk_size + x];
    }

    return tile_value;
}


int32_t
get_tile_chunk_tile(TileMap &tile_map, uint32_t x, uint32_t y)
{
    TileChunkPosition tile_chunk_pos = unpack_tile_chunk_position(tile_map, x, y);
    TileChunk *tile_chunk = get_tile_chunk(tile_map, tile_chunk_pos.chunk_x, tile_chunk_pos.chunk_y);

    int32_t tile_id = access_tile_chunk_tile(tile_map, tile_chunk,
        tile_chunk_pos.tile_x, tile_chunk_pos.tile_y);

    return tile_id;
}


bool
is_tile_map_coordinate_valid(TileMap &tile_map, TileMapPosition &tile_map_pos)
{
    bool is_valid = false;
    int32_t tile_id = get_tile_chunk_tile(tile_map,
        tile_map_pos.tile_chunk_tile_x, tile_map_pos.tile_chunk_tile_y);

    is_valid = (tile_id == 0);

    return is_valid;
}
