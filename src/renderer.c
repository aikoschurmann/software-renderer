#include "renderer.h"
#include "shader.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STARTING_TRI_CAP 8192
#define STARTING_DRAW_CAP 256
#define INITIAL_UNIFORM_POOL_SIZE (1024 * 1024) 

static void* renderer_worker_thread(void* data);
static inline float edge_func(float ax, float ay, float bx, float by, float px, float py);
static void rasterize_triangle_in_tile(Renderer *r, Triangle *t, Tile *tile);

/* --- 1. GEOMETRY & MATH HELPERS --- */
BoundingBox calculate_triangle_bbox(const Triangle *t) {
    BoundingBox b;
    b.min.x = (int)floorf(MIN(t->v[0].x, MIN(t->v[1].x, t->v[2].x)));
    b.max.x = (int)ceilf(MAX(t->v[0].x, MAX(t->v[1].x, t->v[2].x)));
    b.min.y = (int)floorf(MIN(t->v[0].y, MIN(t->v[1].y, t->v[2].y)));
    b.max.y = (int)ceilf(MAX(t->v[0].y, MAX(t->v[1].y, t->v[2].y)));
    return b;
}

static inline float edge_func(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static inline void* get_dc_uniforms(Renderer *r, DrawCall *dc) {
    return (char*)r->uniform_pool + dc->uniform_offset;
}

/* --- 2. INTERNAL SYNCHRONIZATION --- */
static void wait_for_workers(Renderer *r) {
    pthread_mutex_lock(&r->lock);
    while (r->active_workers > 0) {
        pthread_cond_wait(&r->done_working, &r->lock);
    }
    pthread_mutex_unlock(&r->lock);
}

static void signal_workers(Renderer *r, RenderStage stage) {
    pthread_mutex_lock(&r->lock);
    r->stage = stage;
    pthread_cond_broadcast(&r->can_work);
    pthread_mutex_unlock(&r->lock);
}

/* --- 3. BATCH GEOMETRY EXECUTION --- */
static void process_draw_call_vertices(Renderer *r, int dc_idx) {
    DrawCall *dc = &r->draw_calls[dc_idx];
    void* uniforms = get_dc_uniforms(r, dc); 
    const float near_plane = 0.1f;
    
    for (size_t i = 0; i < dc->mesh->vertex_count; i++) {
        Vertex *out = &r->vertex_scratch[dc->vertex_offset + i];
        dc->vertex_shader((int)i, dc->mesh, out, uniforms);

        if (out->w >= near_plane) {
            float inv_w = 1.0f / out->w;
            out->x = (out->x * inv_w + 1.0f) * 0.5f * (float)r->screen_width;
            out->y = (1.0f - out->y * inv_w) * 0.5f * (float)r->screen_height;
            out->z = out->z * inv_w * 0.5f + 0.5f;
            
            out->world_pos.x *= inv_w; out->world_pos.y *= inv_w; out->world_pos.z *= inv_w;
            out->nx *= inv_w; out->ny *= inv_w; out->nz *= inv_w;
            out->w = inv_w; 
        } else {
            out->w = -1.0f; 
        }
    }
}

static void process_draw_call_triangles(Renderer *r, int dc_idx) {
    DrawCall *dc = &r->draw_calls[dc_idx];
    Vertex *v_cache = &r->vertex_scratch[dc->vertex_offset];

    for (size_t i = 0; i < dc->mesh->index_count; i += 3) {
        Vertex *v0 = &v_cache[dc->mesh->indices[i]];
        Vertex *v1 = &v_cache[dc->mesh->indices[i+1]];
        Vertex *v2 = &v_cache[dc->mesh->indices[i+2]];

        if (v0->w < 0 || v1->w < 0 || v2->w < 0) continue;

        float area = edge_func(v0->x, v0->y, v1->x, v1->y, v2->x, v2->y);
        if (dc->cull_mode == CULL_BACK_CCW && area <= 0) continue;
        if (dc->cull_mode == CULL_BACK_CW  && area >= 0) continue;
        if (fabsf(area) < 0.0001f) continue;

        size_t t_idx = atomic_fetch_add(&r->triangle_count, 1);
        Triangle *t = &r->triangles[t_idx];
        t->v[0] = *v0; t->v[1] = *v1; t->v[2] = *v2;
        t->draw_id = (uint32_t)dc_idx;
    }
}

static void renderer_execute_geometry(Renderer *r) {
    if (r->draw_call_count == 0) return;

    // 1. Parallel Vertex Transformation
    atomic_store(&r->next_draw_call, 0);
    signal_workers(r, STAGE_VERTEX);
    while (1) {
        int idx = atomic_fetch_add(&r->next_draw_call, 1);
        if (idx >= (int)r->draw_call_count) break;
        process_draw_call_vertices(r, idx);
    }
    wait_for_workers(r);

    // 2. Pre-allocate triangles safely on Main Thread
    if (r->total_max_triangles > r->triangle_capacity) {
        r->triangle_capacity = r->total_max_triangles * 1.2; // Extra buffer
        r->triangles = realloc(r->triangles, r->triangle_capacity * sizeof(Triangle));
    }

    // 3. Parallel Triangle Assembly
    atomic_store(&r->next_draw_call, 0);
    signal_workers(r, STAGE_ASSEMBLE);
    while (1) {
        int idx = atomic_fetch_add(&r->next_draw_call, 1);
        if (idx >= (int)r->draw_call_count) break;
        process_draw_call_triangles(r, idx);
    }
    wait_for_workers(r);
}

/* --- 4. CORE LIFECYCLE & API --- */
Renderer* renderer_create(size_t w, size_t h, int threads, int tw, int th) {
    Renderer *r = calloc(1, sizeof(Renderer));
    r->screen_width = w; r->screen_height = h;
    r->depth_buffer = malloc(w * h * sizeof(float));
    r->color_buffer = malloc(w * h * sizeof(uint32_t));

    r->triangle_capacity = STARTING_TRI_CAP;                                   
    r->triangles = malloc(r->triangle_capacity * sizeof(Triangle));
    
    r->draw_call_capacity = STARTING_DRAW_CAP;
    r->draw_calls = calloc(r->draw_call_capacity, sizeof(DrawCall));
    r->uniform_pool_cap = INITIAL_UNIFORM_POOL_SIZE;
    r->uniform_pool = malloc(r->uniform_pool_cap);

    r->tile_width = tw; r->tile_height = th;
    r->tile_count_x = (w + tw - 1) / tw; 
    r->tile_count_y = (h + th - 1) / th;
    r->tile_count = r->tile_count_x * r->tile_count_y;
    r->tiles = malloc(r->tile_count * sizeof(Tile));

    for (size_t i = 0; i < r->tile_count; i++){
        Tile *tile = &r->tiles[i];
        int tx = i % r->tile_count_x, ty = i / r->tile_count_x;
        tile->x0 = tx * tw; tile->y0 = ty * th;
        tile->x1 = MIN((tx + 1) * tw, (int)w); tile->y1 = MIN((ty + 1) * th, (int)h);
        tile->triangle_count = 0;
    }

    pthread_mutex_init(&r->lock, NULL);
    pthread_cond_init(&r->can_work, NULL);
    pthread_cond_init(&r->done_working, NULL);
    
    r->thread_count = threads;
    r->stage = STAGE_IDLE;
    r->threads = malloc(sizeof(pthread_t) * (threads - 1));
    r->active_workers = threads - 1; 

    for (int i = 0; i < threads - 1; i++) {
        pthread_create(&r->threads[i], NULL, renderer_worker_thread, r);
    }

    return r;
}

void renderer_destroy(Renderer *r) {
    if (!r) return;
    pthread_mutex_lock(&r->lock);
    r->shutdown = 1;
    pthread_cond_broadcast(&r->can_work);
    pthread_mutex_unlock(&r->lock);

    for (int i = 0; i < r->thread_count - 1; i++) pthread_join(r->threads[i], NULL);

    pthread_mutex_destroy(&r->lock);
    pthread_cond_destroy(&r->can_work);
    pthread_cond_destroy(&r->done_working);
    
    free(r->threads); free(r->color_buffer); free(r->depth_buffer);
    free(r->triangles); free(r->tiles); free(r->tile_tri_indices);
    free(r->vertex_scratch); free(r->bbox_scratch);
    free(r->draw_calls); free(r->uniform_pool);
    free(r);
}

void renderer_reset(Renderer *r) {
    for (size_t i = 0; i < r->tile_count; i++) r->tiles[i].triangle_count = 0;
    atomic_store(&r->triangle_count, 0);
    r->draw_call_count = 0;
    r->uniform_pool_ptr = 0; 
    r->total_vertex_count = 0;
    r->total_max_triangles = 0;
}

void renderer_clear(Renderer *r, uint32_t c, float d) {
    size_t count = r->screen_width * r->screen_height;
    for(size_t i=0; i<count; i++) { r->color_buffer[i] = c; r->depth_buffer[i] = d; }
}


void renderer_set_uniforms(Renderer *r, void *u) { r->uniforms = u; }
void renderer_set_shaders(Renderer *r, VertexShader vs, FragmentShader fs) { r->vertex_shader = vs; r->fragment_shader = fs; }
void renderer_set_cull_mode(Renderer *r, CullMode mode) { r->cull_mode = mode; }

/* --- 5. DRAW CALL RECORDING --- */
void renderer_draw_mesh(Renderer *r, Mesh *mesh) {
    if (!r->vertex_shader || !r->fragment_shader) return;

    if (r->draw_call_count >= r->draw_call_capacity) {
        r->draw_call_capacity *= 2;
        r->draw_calls = realloc(r->draw_calls, r->draw_call_capacity * sizeof(DrawCall));
    }

    DrawCall *dc = &r->draw_calls[r->draw_call_count++];
    dc->mesh = mesh;
    dc->vertex_shader = r->vertex_shader;
    dc->fragment_shader = r->fragment_shader;
    dc->cull_mode = r->cull_mode;
    
    dc->vertex_offset = r->total_vertex_count;
    r->total_vertex_count += mesh->vertex_count;
    r->total_max_triangles += mesh->index_count / 3;

    if (r->total_vertex_count > r->vertex_scratch_cap) {
        r->vertex_scratch_cap = r->total_vertex_count * 1.5;
        r->vertex_scratch = realloc(r->vertex_scratch, r->vertex_scratch_cap * sizeof(Vertex));
    }

    if (r->uniforms) {
        size_t u_size = sizeof(Uniforms);
        if (r->uniform_pool_ptr + u_size > r->uniform_pool_cap) {
            r->uniform_pool_cap *= 2;
            r->uniform_pool = realloc(r->uniform_pool, r->uniform_pool_cap);
        }
        dc->uniform_offset = r->uniform_pool_ptr; 
        memcpy((char*)r->uniform_pool + r->uniform_pool_ptr, r->uniforms, u_size);
        r->uniform_pool_ptr += u_size;
    }
}

/* --- 6. BINNING & RASTERIZATION --- */
void renderer_bin_triangles(Renderer *r) {
    renderer_execute_geometry(r);

    size_t active_triangles = atomic_load(&r->triangle_count);
    if (active_triangles > r->bbox_scratch_cap) {
        r->bbox_scratch_cap = active_triangles * 1.2;
        r->bbox_scratch = realloc(r->bbox_scratch, r->bbox_scratch_cap * sizeof(BoundingBox));
    }

    size_t total_bins = 0;
    for (size_t i = 0; i < active_triangles; i++) {
        r->bbox_scratch[i] = calculate_triangle_bbox(&r->triangles[i]);
        int x0 = CLAMP(r->bbox_scratch[i].min.x / r->tile_width, 0, (int)r->tile_count_x - 1);
        int x1 = CLAMP(r->bbox_scratch[i].max.x / r->tile_width, 0, (int)r->tile_count_x - 1);
        int y0 = CLAMP(r->bbox_scratch[i].min.y / r->tile_height, 0, (int)r->tile_count_y - 1);
        int y1 = CLAMP(r->bbox_scratch[i].max.y / r->tile_height, 0, (int)r->tile_count_y - 1);
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                r->tiles[y * r->tile_count_x + x].triangle_count++;
                total_bins++;
            }
        }
    }

    if (total_bins > r->tile_tri_capacity) {
        r->tile_tri_capacity = total_bins;
        r->tile_tri_indices = realloc(r->tile_tri_indices, total_bins * sizeof(int));
    }

    int current_offset = 0;
    for (size_t i = 0; i < r->tile_count; i++) {
        r->tiles[i].tri_offset = current_offset;
        current_offset += r->tiles[i].triangle_count;
        r->tiles[i].triangle_count = 0;
    }

    for (size_t i = 0; i < active_triangles; i++) {
        int x0 = CLAMP(r->bbox_scratch[i].min.x / r->tile_width, 0, (int)r->tile_count_x - 1);
        int x1 = CLAMP(r->bbox_scratch[i].max.x / r->tile_width, 0, (int)r->tile_count_x - 1);
        int y0 = CLAMP(r->bbox_scratch[i].min.y / r->tile_height, 0, (int)r->tile_count_y - 1);
        int y1 = CLAMP(r->bbox_scratch[i].max.y / r->tile_height, 0, (int)r->tile_count_y - 1);
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                Tile *t = &r->tiles[y * r->tile_count_x + x];
                r->tile_tri_indices[t->tri_offset + t->triangle_count++] = (int)i;
            }
        }
    }
}

static void rasterize_triangle_in_tile(Renderer *r, Triangle *t, Tile *tile) {
    DrawCall *dc = &r->draw_calls[t->draw_id];
    void* uniforms = get_dc_uniforms(r, dc);
    
    BoundingBox bbox = calculate_triangle_bbox(t);
    int min_x = MAX(bbox.min.x, tile->x0), max_x = MIN(bbox.max.x, tile->x1 - 1);
    int min_y = MAX(bbox.min.y, tile->y0), max_y = MIN(bbox.max.y, tile->y1 - 1);

    float area = edge_func(t->v[0].x, t->v[0].y, t->v[1].x, t->v[1].y, t->v[2].x, t->v[2].y);
    if (area <= 0) return; 
    float inv_area = 1.0f / area;

    float dw0_dx = (t->v[2].y - t->v[1].y) * inv_area;
    float dw1_dx = (t->v[0].y - t->v[2].y) * inv_area;
    float dw2_dx = (t->v[1].y - t->v[0].y) * inv_area;

    float dw0_dy = (t->v[1].x - t->v[2].x) * inv_area;
    float dw1_dy = (t->v[2].x - t->v[0].x) * inv_area;
    float dw2_dy = (t->v[0].x - t->v[1].x) * inv_area;

    float w0_row = edge_func(t->v[1].x, t->v[1].y, t->v[2].x, t->v[2].y, min_x + 0.5f, min_y + 0.5f) * inv_area;
    float w1_row = edge_func(t->v[2].x, t->v[2].y, t->v[0].x, t->v[0].y, min_x + 0.5f, min_y + 0.5f) * inv_area;
    float w2_row = edge_func(t->v[0].x, t->v[0].y, t->v[1].x, t->v[1].y, min_x + 0.5f, min_y + 0.5f) * inv_area;

    for (int y = min_y; y <= max_y; y++) {
        float l0 = w0_row, l1 = w1_row, l2 = w2_row;
        int row_base = y * r->screen_width;

        for (int x = min_x; x <= max_x; x++) {
            if (l0 >= 0 && l1 >= 0 && l2 >= 0) {
                float z = l0 * t->v[0].z + l1 * t->v[1].z + l2 * t->v[2].z;
                int idx = row_base + x;
                
                if (z < r->depth_buffer[idx]) {
                    r->depth_buffer[idx] = z;
                    r->color_buffer[idx] = dc->fragment_shader(t, l0, l1, l2, uniforms);
                }
            }
            l0 += dw0_dx; l1 += dw1_dx; l2 += dw2_dx;
        }
        w0_row += dw0_dy; w1_row += dw1_dy; w2_row += dw2_dy;
    }
}

void process_tile(Renderer *r, int tile_index) {
    Tile *tile = &r->tiles[tile_index];
    for (int i = 0; i < tile->triangle_count; i++) {
        int tri_idx = r->tile_tri_indices[tile->tri_offset + i];
        rasterize_triangle_in_tile(r, &r->triangles[tri_idx], tile);
    }
}

void renderer_rasterize(Renderer* r) {
    atomic_store(&r->next_tile, 0);
    signal_workers(r, STAGE_RASTER);

    while (1) {
        int idx = atomic_fetch_add(&r->next_tile, 1);
        if (idx >= (int)r->tile_count) break;
        process_tile(r, idx); 
    }
    wait_for_workers(r);
}

/* --- 7. WORKER THREAD IMPLEMENTATION --- */
static void* renderer_worker_thread(void* data) {
    Renderer* r = (Renderer*)data;
    while (1) {
        pthread_mutex_lock(&r->lock);
        while (r->stage == STAGE_IDLE && !r->shutdown) {
            r->active_workers--;
            if (r->active_workers == 0) pthread_cond_signal(&r->done_working);
            pthread_cond_wait(&r->can_work, &r->lock);
            r->active_workers++;
        }
        if (r->shutdown) { pthread_mutex_unlock(&r->lock); break; }
        RenderStage current_stage = r->stage;
        pthread_mutex_unlock(&r->lock);

        if (current_stage == STAGE_VERTEX) {
            while (1) {
                int idx = atomic_fetch_add(&r->next_draw_call, 1);
                if (idx >= (int)r->draw_call_count) break;
                process_draw_call_vertices(r, idx);
            }
        } else if (current_stage == STAGE_ASSEMBLE) {
            while (1) {
                int idx = atomic_fetch_add(&r->next_draw_call, 1);
                if (idx >= (int)r->draw_call_count) break;
                process_draw_call_triangles(r, idx);
            }
        } else if (current_stage == STAGE_RASTER) {
            while (1) {
                int idx = atomic_fetch_add(&r->next_tile, 1);
                if (idx >= (int)r->tile_count) break;
                process_tile(r, idx);
            }
        }

        pthread_mutex_lock(&r->lock);
        int vertex_done = (current_stage == STAGE_VERTEX && atomic_load(&r->next_draw_call) >= (int)r->draw_call_count);
        int assemble_done = (current_stage == STAGE_ASSEMBLE && atomic_load(&r->next_draw_call) >= (int)r->draw_call_count);
        int raster_done = (current_stage == STAGE_RASTER && atomic_load(&r->next_tile) >= (int)r->tile_count);
        
        // FIX: Only switch to idle if no one else has already
        if (vertex_done || assemble_done || raster_done) {
            if (r->stage != STAGE_IDLE) {
                r->stage = STAGE_IDLE;
            }
        }
        pthread_mutex_unlock(&r->lock);
    }
    return NULL;
}