/* =============================================================================
 * op2utility_c.h  —  stable C ABI for OP2Utility (map loading).
 *
 * OP2Utility is a C++ library whose public API is C++ classes (with exceptions
 * and STL types). Those cannot safely cross a DLL boundary between different
 * compilers/runtimes. This wrapper exposes a flat **C** interface — opaque
 * handles, POD out-params, integer error codes, and NO exceptions escaping —
 * which IS stable across toolchains. Build it into a self-contained DLL and any
 * consumer (our MinGW Qt app, or anything else) can use it.
 *
 * Only the Map-loading surface is wrapped for now; extend with more extern "C"
 * functions (VOL, sprites, ...) as needed.
 * ===========================================================================*/
#ifndef OP2UTILITY_C_H
#define OP2UTILITY_C_H

#include <stdint.h>

#if defined(_WIN32)
  #ifdef OP2UTILITYC_BUILD
    #define OP2UTILITYC_API __declspec(dllexport)
  #else
    #define OP2UTILITYC_API __declspec(dllimport)
  #endif
#else
  #define OP2UTILITYC_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle to a loaded map. Allocated/freed only inside the DLL. */
typedef struct Op2Map Op2Map;

typedef enum Op2Result {
    OP2_OK        = 0,
    OP2_ERR_NULL  = 1, /* null handle / argument */
    OP2_ERR_OPEN  = 2, /* file could not be opened */
    OP2_ERR_PARSE = 3, /* file opened but failed to parse */
    OP2_ERR_BOUNDS= 4, /* x/y outside the map */
    OP2_ERR_UNKNOWN = 5
} Op2Result;

/* Library version of this C ABI (bump on breaking changes). */
OP2UTILITYC_API uint32_t op2_capi_version(void);

/* Human-readable message for the most recent failure on the calling thread. */
OP2UTILITYC_API const char* op2_last_error(void);

/* Load a .map (or the map portion of a saved game). Returns NULL on failure;
 * outErr (optional) receives the error code. Free with op2_map_free. */
OP2UTILITYC_API Op2Map* op2_map_read(const char* filename, Op2Result* outErr);
OP2UTILITYC_API Op2Map* op2_map_read_saved_game(const char* filename, Op2Result* outErr);
OP2UTILITYC_API void    op2_map_free(Op2Map* map);

/* Dimensions / metadata (return 0 on a null handle). */
OP2UTILITYC_API uint32_t op2_map_width(const Op2Map* map);       /* tiles */
OP2UTILITYC_API uint32_t op2_map_height(const Op2Map* map);      /* tiles */
OP2UTILITYC_API uint64_t op2_map_tile_count(const Op2Map* map);
OP2UTILITYC_API uint32_t op2_map_version_tag(const Op2Map* map);
OP2UTILITYC_API int32_t  op2_map_is_saved_game(const Op2Map* map); /* 0/1, -1 on null */

/* Per-cell queries. (x, y) are tile coordinates. Each returns Op2Result and
 * writes the value through the out-param. */
OP2UTILITYC_API Op2Result op2_map_cell_type(const Op2Map* map, uint32_t x, uint32_t y, int32_t* outCellType);
OP2UTILITYC_API Op2Result op2_map_tile_mapping_index(const Op2Map* map, uint32_t x, uint32_t y, uint32_t* out);
OP2UTILITYC_API Op2Result op2_map_tileset_index(const Op2Map* map, uint32_t x, uint32_t y, uint32_t* out);
OP2UTILITYC_API Op2Result op2_map_image_index(const Op2Map* map, uint32_t x, uint32_t y, uint32_t* out);
OP2UTILITYC_API Op2Result op2_map_lava_possible(const Op2Map* map, uint32_t x, uint32_t y, int32_t* outBool);

/* Tileset sources: the well bitmaps this map references. A cell's tileset index
 * (op2_map_tileset_index) indexes into this list. */
OP2UTILITYC_API uint32_t op2_map_tileset_source_count(const Op2Map* map);

/* Writes source[index]'s filename (e.g. "well0001", no extension, trailing
 * padding trimmed) into buf as a null-terminated string. */
OP2UTILITYC_API Op2Result op2_map_tileset_source_name(const Op2Map* map, uint32_t index,
                                                      char* buf, int32_t bufLen);

/* Number of tiles in source[index]. */
OP2UTILITYC_API uint32_t op2_map_tileset_source_tile_count(const Op2Map* map, uint32_t index);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OP2UTILITY_C_H */
