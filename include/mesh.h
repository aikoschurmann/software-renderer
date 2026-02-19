#ifndef RENDERER_MESH_H
#define RENDERER_MESH_H

#include <stdint.h>
#include <stddef.h>
#include "maths.h"

typedef struct {
    float x, y, z, w;       
    
    vec3 world_pos;

    float nx, ny, nz;       
    float u, v;             
    uint32_t color;         
} Vertex;

typedef struct { Vertex v[3]; uint32_t draw_id; } Triangle;

typedef struct {
    float *p_x, *p_y, *p_z;    
    float *n_x, *n_y, *n_z;    
    float *u, *v;              
    uint32_t *colors;          
    uint32_t *indices;         
    size_t vertex_count;
    size_t index_count;
} Mesh;

Mesh load_mesh(const char *filename);
void free_mesh(Mesh *mesh);

// --- Geometry Helpers ---
BoundingBox mesh_calculate_bounds(const Mesh *mesh);
void mesh_center_origin(Mesh *mesh);

#endif