#ifndef SHADER_H
#define SHADER_H

#include "renderer.h"

#define MAX_LIGHTS 1024

typedef struct {
    vec3 position;
    vec3 color;
    float intensity;
} PointLight;

typedef struct {
    // Coordinate Spaces
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 view_proj;
    mat4 mvp;
    
    // System Info
    float screen_width;
    float screen_height;

    const PointLight *scene_lights;        
    uint16_t active_lights[MAX_LIGHTS];    
    int light_count;
    
    vec3 base_color;
    vec3 cam_pos; 
    float dt;
} Uniforms;

void vs_default(int idx, const Mesh *mesh, Vertex *out, void *uniforms);

// Shaders
uint32_t fs_multi_light(Triangle *t, float b0, float b1, float b2, void *uniforms);
uint32_t fs_multi_light_smooth(Triangle *t, float b0, float b1, float b2, void *uniforms);
uint32_t fs_pure_color(Triangle *t, float b0, float b1, float b2, void *uniforms);
uint32_t fs_wireframe(Triangle *t, float b0, float b1, float b2, void *uniforms);
uint32_t fs_normals(Triangle *t, float b0, float b1, float b2, void *uniforms);
uint32_t fs_plasma_glow(Triangle *t, float b0, float b1, float b2, void *uniforms);
uint32_t fs_cyber_neon(Triangle *t, float b0, float b1, float b2, void *uniforms);
#endif