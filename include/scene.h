#ifndef SCENE_H
#define SCENE_H

#include "renderer.h"
#include "camera.h"
#include "shader.h"
#include "platform.h" 

#define MAX_SCENE_MESHES 16

typedef struct {
    Mesh *mesh;
    vec3 position;
    vec3 rotation;
    float scale;
    vec3 base_color;
    
    VertexShader vs;
    FragmentShader fs;

    bool visible;
} Entity;

typedef struct {
    Entity *entities;
    size_t entity_count;
    size_t entity_capacity;

    PointLight lights[MAX_LIGHTS];
    size_t light_count;

    Camera camera;

    // Asset Management
    Mesh meshes[MAX_SCENE_MESHES];
    size_t mesh_count;
} Scene;

Scene* scene_create(size_t initial_capacity);
void   scene_destroy(Scene* scene);

Mesh* scene_load_mesh(Scene* scene, const char* filepath);
Entity* scene_add_entity(Scene* scene, Mesh* mesh, vec3 pos, vec3 rot, float scale, vec3 color);
PointLight* scene_add_light(Scene* scene, vec3 pos, vec3 color, float intensity);

void scene_render(Scene* scene, Renderer* renderer, Uniforms* base_uniforms);
void scene_render_frame(Scene* scene, Renderer* renderer, Platform* platform, Uniforms* uniforms, uint32_t clear_color);

#endif