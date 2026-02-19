#include "scene.h"
#include <stdlib.h>
#include <string.h>

Scene* scene_create(size_t initial_capacity) {
    Scene *s = calloc(1, sizeof(Scene));
    s->entity_capacity = initial_capacity > 0 ? initial_capacity : 16;
    s->entities = malloc(s->entity_capacity * sizeof(Entity));
    return s;
}

void scene_destroy(Scene* scene) {
    if (!scene) return;
    
    for (size_t i = 0; i < scene->mesh_count; i++) {
        free_mesh(&scene->meshes[i]);
    }

    free(scene->entities);
    free(scene);
}

Mesh* scene_load_mesh(Scene* scene, const char* filepath) {
    if (scene->mesh_count >= MAX_SCENE_MESHES) return NULL;
    Mesh m = load_mesh(filepath);
    scene->meshes[scene->mesh_count] = m;
    return &scene->meshes[scene->mesh_count++];
}

Entity* scene_add_entity(Scene* scene, Mesh* mesh, vec3 pos, vec3 rot, float scale, vec3 color) {
    if (scene->entity_count >= scene->entity_capacity) {
        scene->entity_capacity *= 2;
        scene->entities = realloc(scene->entities, scene->entity_capacity * sizeof(Entity));
    }
    Entity *e = &scene->entities[scene->entity_count++];
    e->mesh = mesh;
    e->position = pos;
    e->rotation = rot;
    e->scale = scale;
    e->base_color = color;
    e->vs = vs_default;
    e->fs = fs_multi_light; 
    e->visible = true;
    return e;
}

PointLight* scene_add_light(Scene* scene, vec3 pos, vec3 color, float intensity) {
    if (scene->light_count >= MAX_LIGHTS) return NULL;
    PointLight *l = &scene->lights[scene->light_count++];
    l->position = pos;
    l->color = color;
    l->intensity = intensity;
    return l;
}

void scene_render(Scene* scene, Renderer* renderer, Uniforms* base_uniforms) {
    float aspect = base_uniforms->screen_width / base_uniforms->screen_height;
    mat4 view, proj;
    camera_get_matrices(&scene->camera, aspect, &view, &proj);
    mat4 view_proj = mat4_mul(proj, view);

    base_uniforms->view = view;
    base_uniforms->projection = proj;
    base_uniforms->view_proj = view_proj;
    base_uniforms->cam_pos = scene->camera.position;
    base_uniforms->scene_lights = scene->lights; 

    for (size_t i = 0; i < scene->entity_count; i++) {
        Entity *e = &scene->entities[i];

        if (!e->visible) continue;

        mat4 m_scale = mat4_scale(e->scale);
        mat4 m_rot_x = mat4_rotate_x(e->rotation.x);
        mat4 m_rot_y = mat4_rotate_y(e->rotation.y);
        mat4 m_rot_z = mat4_rotate_z(e->rotation.z);
        mat4 m_rot = mat4_mul(m_rot_z, mat4_mul(m_rot_y, m_rot_x));
        mat4 m_trans = mat4_translate(e->position.x, e->position.y, e->position.z);

        mat4 model = mat4_mul(m_trans, mat4_mul(m_rot, m_scale));
        mat4 mvp = mat4_mul(view_proj, model);

        vec4 center_clip = mat4_mul_vec4(mvp, (vec4){0.0f, 0.0f, 0.0f, 1.0f});
        if (center_clip.w < -3.0f) continue; 
        if (center_clip.w > 0.0f) { 
            float w = center_clip.w;
            float margin = 4.0f * e->scale; 
            if (center_clip.x < -w - margin || center_clip.x > w + margin ||
                center_clip.y < -w - margin || center_clip.y > w + margin) {
                continue; 
            }
        }

        Uniforms local_uniforms = *base_uniforms; 
        local_uniforms.light_count = 0; 

        vec4 center_world = mat4_mul_vec4(model, (vec4){0.0f, 0.0f, 0.0f, 1.0f});
        vec3 cw = {center_world.x, center_world.y, center_world.z};
        float max_dist_sq = 58.0f * 58.0f; 

        for (size_t l = 0; l < scene->light_count; l++) {
            vec3 diff = vec3_sub(scene->lights[l].position, cw);
            if (vec3_dot(diff, diff) < max_dist_sq) {
                local_uniforms.active_lights[local_uniforms.light_count++] = (uint16_t)l;
            }
        }

        local_uniforms.model = model;
        local_uniforms.mvp = mvp;
        local_uniforms.base_color = e->base_color;

        renderer_set_uniforms(renderer, &local_uniforms);
        renderer_set_shaders(renderer, e->vs, e->fs);
        renderer_draw_mesh(renderer, e->mesh);
    }
}

extern void apply_post_processing(uint32_t* buffer, int width, int height, float time);

void scene_render_frame(Scene* scene, Renderer* renderer, Platform* platform, Uniforms* uniforms, uint32_t clear_color) {
    renderer_reset(renderer);
    renderer_clear(renderer, clear_color, 1.0f);

    scene_render(scene, renderer, uniforms);

    renderer_bin_triangles(renderer);      
    renderer_rasterize(renderer); 
    
    apply_post_processing(renderer->color_buffer, (int)uniforms->screen_width, (int)uniforms->screen_height, uniforms->dt);
    
    // Swap buffers
    platform_update_window(platform, renderer->color_buffer, (int)uniforms->screen_width, (int)uniforms->screen_height);
}