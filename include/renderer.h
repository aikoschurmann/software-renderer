#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include "mesh.h"

typedef void (*VertexShader)(int index, const Mesh *mesh, Vertex *out_vertex, void *uniforms);
typedef uint32_t (*FragmentShader)(Triangle *t, float b0, float b1, float b2, void *uniforms);

typedef enum { CULL_NONE, CULL_BACK_CCW, CULL_BACK_CW } CullMode;
typedef enum { STAGE_IDLE, STAGE_VERTEX, STAGE_ASSEMBLE, STAGE_RASTER } RenderStage;

typedef struct { int x0, x1, y0, y1; } TileRange;
typedef struct { int x0, y0, x1, y1; int tri_offset, triangle_count; } Tile;

typedef struct {
    Mesh           *mesh;
    size_t          uniform_offset; 
    VertexShader    vertex_shader;
    FragmentShader  fragment_shader;
    CullMode        cull_mode;
    size_t          vertex_offset; 
} DrawCall;

typedef struct {
    uint32_t    *color_buffer;
    float       *depth_buffer;
    size_t      screen_width, screen_height;

    Triangle    *triangles;
    atomic_size_t triangle_count;
    size_t      triangle_capacity;
    
    Tile        *tiles;
    int         *tile_tri_indices; 
    size_t      tile_count, tile_count_x, tile_count_y;
    int         tile_width, tile_height;
    size_t      tile_tri_capacity;

    Vertex      *vertex_scratch;
    BoundingBox *bbox_scratch;
    size_t       vertex_scratch_cap, bbox_scratch_cap;
    size_t       total_vertex_count;
    size_t       total_max_triangles;

    DrawCall    *draw_calls;
    size_t       draw_call_count, draw_call_capacity;
    void        *uniform_pool;
    size_t       uniform_pool_ptr, uniform_pool_cap;

    RenderStage     stage;
    atomic_int      next_tile;
    atomic_int      next_draw_call;
    
    int             thread_count;
    pthread_t      *threads;
    pthread_mutex_t lock;
    pthread_cond_t  can_work, done_working;   
    int             active_workers, shutdown;

    void           *uniforms;      
    VertexShader    vertex_shader; 
    FragmentShader  fragment_shader; 
    CullMode        cull_mode; 
} Renderer;

Renderer* renderer_create(size_t w, size_t h, int threads, int tw, int th);
void      renderer_destroy(Renderer *r);
void      renderer_clear(Renderer *r, uint32_t c, float depth);
void      renderer_set_uniforms(Renderer *r, void *uniforms);
void      renderer_set_shaders(Renderer *r, VertexShader vs, FragmentShader fs);
void      renderer_set_cull_mode(Renderer *r, CullMode mode);
void      renderer_draw_mesh(Renderer *r, Mesh *mesh);
void      renderer_reset(Renderer *r);
void      renderer_bin_triangles(Renderer *r);
void      renderer_rasterize(Renderer *r);

#endif