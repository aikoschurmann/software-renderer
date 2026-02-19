#include "mesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

// Generic dynamic array growth macro
#define ARRAY_PUSH(type, ptr, count, cap, item) \
    do { \
        if ((count) >= (cap)) { \
            (cap) = (cap) == 0 ? 128 : (cap) * 2; \
            void *temp = realloc((ptr), sizeof(type) * (cap)); \
            if (!temp) { fprintf(stderr, "Out of memory\n"); exit(1); } \
            (ptr) = (type*)temp; \
        } \
        (ptr)[(count)++] = (item); \
    } while (0)

#define HASH_TABLE_SIZE 65536 

// --- DEDUPLICATION STRUCTURES ---
typedef struct {
    size_t p_idx; // Position Index
    size_t t_idx; // UV Index
    size_t n_idx; // Normal Index
} VertexKey;

typedef struct HashNode {
    VertexKey key;
    uint32_t  final_index;
    struct HashNode *next;
} HashNode;


static void v3_normalize_ptr(float *x, float *y, float *z) {
    float len = sqrtf((*x)*(*x) + (*y)*(*y) + (*z)*(*z));
    if (len > 1e-6f) {
        float inv = 1.0f / len;
        *x *= inv; *y *= inv; *z *= inv;
    }
}

static uint32_t hash_key(VertexKey k) {
    uint32_t h = 2166136261u;
    h = (h ^ k.p_idx) * 16777619u;
    h = (h ^ k.t_idx) * 16777619u;
    h = (h ^ k.n_idx) * 16777619u;
    return h % HASH_TABLE_SIZE;
}

// --- NORMAL CALCULATION ---
static void calculate_normals(Mesh *mesh) {
    // 1. Reset existing normals (just in case)
    memset(mesh->n_x, 0, mesh->vertex_count * sizeof(float));
    memset(mesh->n_y, 0, mesh->vertex_count * sizeof(float));
    memset(mesh->n_z, 0, mesh->vertex_count * sizeof(float));

    // 2. Accumulate Face Normals
    for (size_t i = 0; i < mesh->index_count; i += 3) {
        uint32_t i0 = mesh->indices[i];
        uint32_t i1 = mesh->indices[i+1];
        uint32_t i2 = mesh->indices[i+2];

        vec3 v0 = { mesh->p_x[i0], mesh->p_y[i0], mesh->p_z[i0] };
        vec3 v1 = { mesh->p_x[i1], mesh->p_y[i1], mesh->p_z[i1] };
        vec3 v2 = { mesh->p_x[i2], mesh->p_y[i2], mesh->p_z[i2] };

        vec3 e1 = vec3_sub(v1, v0);
        vec3 e2 = vec3_sub(v2, v0);
        vec3 norm = vec3_cross(e1, e2);

        // Add to all vertices (weighted by area automatically via cross product magnitude)
        mesh->n_x[i0] += norm.x; mesh->n_y[i0] += norm.y; mesh->n_z[i0] += norm.z;
        mesh->n_x[i1] += norm.x; mesh->n_y[i1] += norm.y; mesh->n_z[i1] += norm.z;
        mesh->n_x[i2] += norm.x; mesh->n_y[i2] += norm.y; mesh->n_z[i2] += norm.z;
    }

    // 3. Normalize Result
    for (size_t i = 0; i < mesh->vertex_count; i++) {
        v3_normalize_ptr(&mesh->n_x[i], &mesh->n_y[i], &mesh->n_z[i]);
    }
}

Mesh load_mesh(const char *filename) {
    Mesh mesh = {0};
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Failed to open OBJ file: %s\n", filename);
        return mesh;
    }

    // --- RAW DATA POOLS ---
    vec3 *pos_pool = NULL;  size_t p_count = 0, p_cap = 0;
    vec3 *norm_pool = NULL; size_t n_count = 0, n_cap = 0;
    vec2 *uv_pool = NULL;   size_t u_count = 0, u_cap = 0;

    // --- FINAL MESH BUFFERS (SoA) ---
    float *f_px = NULL, *f_py = NULL, *f_pz = NULL;
    float *f_nx = NULL, *f_ny = NULL, *f_nz = NULL;
    float *f_u = NULL, *f_v = NULL;
    uint32_t *f_colors = NULL;
    uint32_t *f_indices = NULL;
    
    size_t v_count = 0, v_cap = 0;
    size_t i_count = 0, i_cap = 0;

    HashNode **buckets = calloc(HASH_TABLE_SIZE, sizeof(HashNode*));
    if (!buckets) { fprintf(stderr, "Out of memory\n"); exit(1); }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "v ", 2) == 0) {
            vec3 p; sscanf(line, "v %f %f %f", &p.x, &p.y, &p.z);
            ARRAY_PUSH(vec3, pos_pool, p_count, p_cap, p);
        } else if (strncmp(line, "vt ", 3) == 0) {
            vec2 uv; sscanf(line, "vt %f %f", &uv.x, &uv.y);
            ARRAY_PUSH(vec2, uv_pool, u_count, u_cap, uv);
        } else if (strncmp(line, "vn ", 3) == 0) {
            vec3 n; sscanf(line, "vn %f %f %f", &n.x, &n.y, &n.z);
            ARRAY_PUSH(vec3, norm_pool, n_count, n_cap, n);
        } else if (strncmp(line, "f ", 2) == 0) {
            int v[3], t[3], n[3];
            int has_uvs = 0, has_norms = 0, matches = 0;

            if ((matches = sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d", 
                &v[0], &t[0], &n[0], &v[1], &t[1], &n[1], &v[2], &t[2], &n[2])) == 9) {
                has_uvs = 1; has_norms = 1;
            } else if ((matches = sscanf(line, "f %d//%d %d//%d %d//%d", 
                &v[0], &n[0], &v[1], &n[1], &v[2], &n[2])) == 6) {
                has_norms = 1;
            } else if ((matches = sscanf(line, "f %d/%d %d/%d %d/%d", 
                &v[0], &t[0], &v[1], &t[1], &v[2], &t[2])) == 6) {
                has_uvs = 1;
            } else {
                sscanf(line, "f %d %d %d", &v[0], &v[1], &v[2]);
            }

            for (int k = 0; k < 3; k++) {
                VertexKey key;
                key.p_idx = v[k] - 1;
                key.t_idx = has_uvs   ? t[k] - 1 : -1;
                key.n_idx = has_norms ? n[k] - 1 : -1;

                uint32_t h = hash_key(key);
                uint32_t final_idx = (uint32_t)-1;
                HashNode *node = buckets[h];
                
                while (node) {
                    if (node->key.p_idx == key.p_idx && 
                        node->key.t_idx == key.t_idx && 
                        node->key.n_idx == key.n_idx) {
                        final_idx = node->final_index;
                        break;
                    }
                    node = node->next;
                }

                if (final_idx == (uint32_t)-1) {
                    final_idx = v_count;
                    if (v_count >= v_cap) {
                        v_cap = v_cap == 0 ? 128 : v_cap * 2;
                        f_px = realloc(f_px, v_cap * sizeof(float));
                        f_py = realloc(f_py, v_cap * sizeof(float));
                        f_pz = realloc(f_pz, v_cap * sizeof(float));
                        f_nx = realloc(f_nx, v_cap * sizeof(float));
                        f_ny = realloc(f_ny, v_cap * sizeof(float));
                        f_nz = realloc(f_nz, v_cap * sizeof(float));
                        f_u  = realloc(f_u,  v_cap * sizeof(float));
                        f_v  = realloc(f_v,  v_cap * sizeof(float));
                        f_colors = realloc(f_colors, v_cap * sizeof(uint32_t));
                    }

                    if (key.p_idx >= 0 && key.p_idx < p_count) {
                        f_px[v_count] = pos_pool[key.p_idx].x;
                        f_py[v_count] = pos_pool[key.p_idx].y;
                        f_pz[v_count] = pos_pool[key.p_idx].z;
                    }

                    if (key.n_idx >= 0 && key.n_idx < n_count) {
                        f_nx[v_count] = norm_pool[key.n_idx].x;
                        f_ny[v_count] = norm_pool[key.n_idx].y;
                        f_nz[v_count] = norm_pool[key.n_idx].z;
                    } else {
                        f_nx[v_count] = 0; f_ny[v_count] = 0; f_nz[v_count] = 0;
                    }

                    if (key.t_idx >= 0 && key.t_idx < u_count) {
                        f_u[v_count] = uv_pool[key.t_idx].x;
                        f_v[v_count] = uv_pool[key.t_idx].y;
                    } else {
                        f_u[v_count] = 0; f_v[v_count] = 0;
                    }
                    
                    f_colors[v_count] = 0xFFFFFFFF;
                    v_count++;

                    HashNode *new_node = malloc(sizeof(HashNode));
                    new_node->key = key;
                    new_node->final_index = final_idx;
                    new_node->next = buckets[h];
                    buckets[h] = new_node;
                }
                ARRAY_PUSH(uint32_t, f_indices, i_count, i_cap, final_idx);
            }
        }
    }

    // --- ASSIGN TO MESH ---
    mesh.vertex_count = v_count;
    mesh.index_count  = i_count;
    mesh.p_x = f_px; mesh.p_y = f_py; mesh.p_z = f_pz;
    mesh.n_x = f_nx; mesh.n_y = f_ny; mesh.n_z = f_nz;
    mesh.u   = f_u;  mesh.v   = f_v;
    mesh.colors  = f_colors;
    mesh.indices = f_indices;

    // --- AUTO-CALCULATE NORMALS IF MISSING ---
    if (n_count == 0 && mesh.index_count > 0) {
        calculate_normals(&mesh);
    }

    // --- CLEANUP ---
    free(pos_pool); free(uv_pool); free(norm_pool);
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashNode *node = buckets[i];
        while (node) {
            HashNode *next = node->next;
            free(node);
            node = next;
        }
    }
    free(buckets);
    fclose(file);

    printf("Mesh Loaded: %zu vertices, %zu indices\n", mesh.vertex_count, mesh.index_count);
    return mesh;
}

void free_mesh(Mesh *mesh) {
    free(mesh->p_x); free(mesh->p_y); free(mesh->p_z);
    free(mesh->n_x); free(mesh->n_y); free(mesh->n_z);
    free(mesh->u);   free(mesh->v);
    free(mesh->colors); free(mesh->indices);
}

BoundingBox mesh_calculate_bounds(const Mesh *mesh) {
    BoundingBox bb;
    bb.min = (vec3){FLT_MAX, FLT_MAX, FLT_MAX};
    bb.max = (vec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (size_t i = 0; i < mesh->vertex_count; i++) {
        if (mesh->p_x[i] < bb.min.x) bb.min.x = mesh->p_x[i];
        if (mesh->p_x[i] > bb.max.x) bb.max.x = mesh->p_x[i];
        
        if (mesh->p_y[i] < bb.min.y) bb.min.y = mesh->p_y[i];
        if (mesh->p_y[i] > bb.max.y) bb.max.y = mesh->p_y[i];

        if (mesh->p_z[i] < bb.min.z) bb.min.z = mesh->p_z[i];
        if (mesh->p_z[i] > bb.max.z) bb.max.z = mesh->p_z[i];
    }

    bb.center.x = (bb.min.x + bb.max.x) * 0.5f;
    bb.center.y = (bb.min.y + bb.max.y) * 0.5f;
    bb.center.z = (bb.min.z + bb.max.z) * 0.5f;
    return bb;
}

void mesh_center_origin(Mesh *mesh) {
    BoundingBox bb = mesh_calculate_bounds(mesh);

    for (size_t i = 0; i < mesh->vertex_count; i++) {
        mesh->p_x[i] -= bb.center.x;
        mesh->p_y[i] -= bb.center.y;
        mesh->p_z[i] -= bb.center.z;
    }
}
