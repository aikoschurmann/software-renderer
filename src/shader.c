#include "shader.h"

// Standard Vertex Shader - Now clean and focused on 3D logic
void vs_default(int idx, const Mesh *mesh, Vertex *out, void *uniforms) {
    Uniforms *u = (Uniforms*)uniforms;

    vec4 pos_local = { mesh->p_x[idx], mesh->p_y[idx], mesh->p_z[idx], 1.0f };
    vec4 pos_world = mat4_mul_vec4(u->model, pos_local);
    vec4 pos_clip  = mat4_mul_vec4(u->mvp, pos_local);
    
    out->world_pos = (vec3){pos_world.x, pos_world.y, pos_world.z};
    out->x = pos_clip.x;
    out->y = pos_clip.y;
    out->z = pos_clip.z;
    out->w = pos_clip.w;

    vec4 n_world = mat4_mul_vec4(u->model, (vec4){mesh->n_x[idx], mesh->n_y[idx], mesh->n_z[idx], 0.0f});
    out->nx = n_world.x;
    out->ny = n_world.y;
    out->nz = n_world.z;
}

// -------------------------------------------------------------
// FS: Multi-Point Light Blinn-Phong
// -------------------------------------------------------------

uint32_t fs_multi_light(Triangle *t, float b0, float b1, float b2, void *uniforms) {
    Uniforms *u = (Uniforms*)uniforms;

    float w_true = 1.0f / (b0 * t->v[0].w + b1 * t->v[1].w + b2 * t->v[2].w);

    vec3 world_pos = {
        (b0 * t->v[0].world_pos.x + b1 * t->v[1].world_pos.x + b2 * t->v[2].world_pos.x) * w_true,
        (b0 * t->v[0].world_pos.y + b1 * t->v[1].world_pos.y + b2 * t->v[2].world_pos.y) * w_true,
        (b0 * t->v[0].world_pos.z + b1 * t->v[1].world_pos.z + b2 * t->v[2].world_pos.z) * w_true
    };

    vec3 v0_true = { t->v[0].world_pos.x / t->v[0].w, t->v[0].world_pos.y / t->v[0].w, t->v[0].world_pos.z / t->v[0].w };
    vec3 v1_true = { t->v[1].world_pos.x / t->v[1].w, t->v[1].world_pos.y / t->v[1].w, t->v[1].world_pos.z / t->v[1].w };
    vec3 v2_true = { t->v[2].world_pos.x / t->v[2].w, t->v[2].world_pos.y / t->v[2].w, t->v[2].world_pos.z / t->v[2].w };

    vec3 edge1 = vec3_sub(v1_true, v0_true);
    vec3 edge2 = vec3_sub(v2_true, v0_true);
    vec3 normal = vec3_norm(vec3_cross(edge1, edge2));
    
    vec3 view_dir = vec3_norm(vec3_sub(u->cam_pos, world_pos));

    vec3 diffuse_acc = {0.0f, 0.0f, 0.0f};
    vec3 specular_acc = {0.0f, 0.0f, 0.0f};

    for(int i = 0; i < u->light_count; i++) {
        uint16_t light_idx = u->active_lights[i];
        const PointLight *l = &u->scene_lights[light_idx];

        vec3 L_vec = vec3_sub(l->position, world_pos);
        float dist_sq = vec3_dot(L_vec, L_vec);
        if (dist_sq > 2500.0f) continue;

        float inv_dist = 1.0f / sqrtf(dist_sq);
        float dist = dist_sq * inv_dist; 
        vec3 L = vec3_mul(L_vec, inv_dist); 

        float fade = 1.0f - (dist / 50.0f);

        float att = l->intensity / (1.0f + 0.1f * dist + 0.7f * dist_sq);
        att *= fade; 
        
        float NdotL = vec3_dot(normal, L);
        if(NdotL > 0.0f) {
            float diff_factor = NdotL * att;
            diffuse_acc.x += l->color.x * diff_factor;
            diffuse_acc.y += l->color.y * diff_factor;
            diffuse_acc.z += l->color.z * diff_factor;

            vec3 H = vec3_norm(vec3_add(L, view_dir));
            float NdotH = vec3_dot(normal, H);
            
            if(NdotH > 0.0f) {
                float p = NdotH;
                p *= p; p *= p; p *= p; p *= p; p *= p; p *= p; 
                
                float spec = p * att;
                specular_acc.x += l->color.x * spec;
                specular_acc.y += l->color.y * spec;
                specular_acc.z += l->color.z * spec;
            }
        }
    }

    vec3 final_rgb = {
        u->base_color.x * (0.01f + diffuse_acc.x) + specular_acc.x,
        u->base_color.y * (0.01f + diffuse_acc.y) + specular_acc.y,
        u->base_color.z * (0.01f + diffuse_acc.z) + specular_acc.z
    };

    final_rgb.x = fminf(final_rgb.x, 1.0f);
    final_rgb.y = fminf(final_rgb.y, 1.0f);
    final_rgb.z = fminf(final_rgb.z, 1.0f);

    return vec3_to_color(final_rgb);
}

uint32_t fs_multi_light_smooth(Triangle *t, float b0, float b1, float b2, void *uniforms) {
    Uniforms *u = (Uniforms*)uniforms;

    float w_true = 1.0f / (b0 * t->v[0].w + b1 * t->v[1].w + b2 * t->v[2].w);

    vec3 world_pos = {
        (b0 * t->v[0].world_pos.x + b1 * t->v[1].world_pos.x + b2 * t->v[2].world_pos.x) * w_true,
        (b0 * t->v[0].world_pos.y + b1 * t->v[1].world_pos.y + b2 * t->v[2].world_pos.y) * w_true,
        (b0 * t->v[0].world_pos.z + b1 * t->v[1].world_pos.z + b2 * t->v[2].world_pos.z) * w_true
    };

    vec3 normal = {
        (b0 * t->v[0].nx + b1 * t->v[1].nx + b2 * t->v[2].nx) * w_true,
        (b0 * t->v[0].ny + b1 * t->v[1].ny + b2 * t->v[2].ny) * w_true,
        (b0 * t->v[0].nz + b1 * t->v[1].nz + b2 * t->v[2].nz) * w_true
    };
    normal = vec3_norm(normal); 

    vec3 view_dir = vec3_norm(vec3_sub(u->cam_pos, world_pos));
    vec3 total_light = {0.01f, 0.01f, 0.01f}; 

    for(int i = 0; i < u->light_count; i++) {
        uint16_t light_idx = u->active_lights[i];
        const PointLight *l = &u->scene_lights[light_idx];

        vec3 L_vec = vec3_sub(l->position, world_pos);
        
        float dist_sq = vec3_len_sq(L_vec);
        if (dist_sq > 2500.0f) continue; // 50^2 = 2500
        
        float inv_dist = 1 / sqrtf(dist_sq);
        float dist = dist_sq * inv_dist;       // dist = dist^2 * (1/dist)
        vec3 L = vec3_mul(L_vec, inv_dist);    // normalize L_vec without division

        float fade = 1.0f - (dist * 0.02f); // x / 50.0f is same as x * 0.02f

        float att = 1.0f / (1.0f + 0.1f * dist + 0.4f * dist_sq);
        att *= l->intensity * fade;

        float NdotL = vec3_dot(normal, L);
        
        if(NdotL <= 0.0f) continue;

        float spec = 0.0f;
        vec3 H = vec3_norm(vec3_add(L, view_dir));
        float NdotH = vec3_dot(normal, H);
        
        if(NdotH > 0.0f) {
            float p = NdotH;
            p *= p; p *= p; p *= p; p *= p; p *= p; p *= p; 
            spec = p; 
        }

        vec3 light_color = l->color;
        float diff_factor = NdotL * att;
        float spec_factor = spec * att;

        total_light.x += light_color.x * (diff_factor + spec_factor);
        total_light.y += light_color.y * (diff_factor + spec_factor);
        total_light.z += light_color.z * (diff_factor + spec_factor);
    }

    vec3 final_rgb = vec3_mul_vec3(u->base_color, total_light);

    final_rgb.x = final_rgb.x > 1.0f ? 1.0f : final_rgb.x;
    final_rgb.y = final_rgb.y > 1.0f ? 1.0f : final_rgb.y;
    final_rgb.z = final_rgb.z > 1.0f ? 1.0f : final_rgb.z;

    return vec3_to_color(final_rgb);
}

uint32_t fs_normals(Triangle *t, float b0, float b1, float b2, void *uniforms) {
    (void)uniforms;
    float w_true = 1.0f / (b0 * t->v[0].w + b1 * t->v[1].w + b2 * t->v[2].w);
    vec3 normal = {
        (b0 * t->v[0].nx + b1 * t->v[1].nx + b2 * t->v[2].nx) * w_true,
        (b0 * t->v[0].ny + b1 * t->v[1].ny + b2 * t->v[2].ny) * w_true,
        (b0 * t->v[0].nz + b1 * t->v[1].nz + b2 * t->v[2].nz) * w_true
    };
    normal = vec3_norm(normal);
    vec3 color;
    color.x = normal.x * 0.5f + 0.5f; color.y = normal.y * 0.5f + 0.5f; color.z = normal.z * 0.5f + 0.5f;
    return vec3_to_color(color);
}

uint32_t fs_pure_color(Triangle *t, float b0, float b1, float b2, void *uniforms) {
    (void)b0; (void)b1; (void)b2; (void)t;
    Uniforms *u = (Uniforms*)uniforms;
    return vec3_to_color(u->base_color);
}

uint32_t fs_wireframe(Triangle *t, float b0, float b1, float b2, void *uniforms) {
    (void)t; (void)uniforms;
    float threshold = 0.02f; 
    float min_dist = b0;
    if (b1 < min_dist) min_dist = b1;
    if (b2 < min_dist) min_dist = b2;
    vec3 final_color = (min_dist < threshold) ? (vec3){0.0f, 1.0f, 0.0f} : (vec3){0.1f, 0.0f, 0.2f};
    return vec3_to_color(final_color);
}

uint32_t fs_plasma_glow(Triangle *t, float b0, float b1, float b2, void *uniforms) {
    Uniforms *u = (Uniforms*)uniforms;
    float w_true = 1.0f / (b0 * t->v[0].w + b1 * t->v[1].w + b2 * t->v[2].w);
    vec3 world_pos = {
        (b0 * t->v[0].world_pos.x + b1 * t->v[1].world_pos.x + b2 * t->v[2].world_pos.x) * w_true,
        (b0 * t->v[0].world_pos.y + b1 * t->v[1].world_pos.y + b2 * t->v[2].world_pos.y) * w_true,
        (b0 * t->v[0].world_pos.z + b1 * t->v[1].world_pos.z + b2 * t->v[2].world_pos.z) * w_true
    };

    float threshold = 0.08f;
    float min_b = MIN(b0, MIN(b1, b2));
    vec3 color;
    if (min_b < threshold) {
        float wave = sinf(world_pos.y * 0.2f + u->dt * 5.0f) * 0.5f + 0.5f;
        color.x = 0.1f + wave * 0.9f; color.y = 0.8f - wave * 0.4f; color.z = 1.0f;
    } else {
        float dist = vec3_len(world_pos) * 0.01f;
        color = vec3_mul((vec3){0.1f, 0.05f, 0.2f}, 1.0f / (1.0f + dist));
    }
    return vec3_to_color(color);
}

uint32_t fs_cyber_neon(Triangle *t, float b0, float b1, float b2, void *uniforms) {
    Uniforms *u = (Uniforms*)uniforms;

    float w_true = 1.0f / (b0 * t->v[0].w + b1 * t->v[1].w + b2 * t->v[2].w);
    vec3 world_pos = {
        (b0 * t->v[0].world_pos.x + b1 * t->v[1].world_pos.x + b2 * t->v[2].world_pos.x) * w_true,
        (b0 * t->v[0].world_pos.y + b1 * t->v[1].world_pos.y + b2 * t->v[2].world_pos.y) * w_true,
        (b0 * t->v[0].world_pos.z + b1 * t->v[1].world_pos.z + b2 * t->v[2].world_pos.z) * w_true
    };

    vec3 normal = vec3_norm((vec3){
        (b0 * t->v[0].nx + b1 * t->v[1].nx + b2 * t->v[2].nx) * w_true,
        (b0 * t->v[0].ny + b1 * t->v[1].ny + b2 * t->v[2].ny) * w_true,
        (b0 * t->v[0].nz + b1 * t->v[1].nz + b2 * t->v[2].nz) * w_true
    });

    vec3 view_dir = vec3_norm(vec3_sub(u->cam_pos, world_pos));
    vec3 total_light = {0.05f, 0.05f, 0.08f}; 

    for(int i = 0; i < u->light_count; i++) {
        // FIX: Lookup light using new array indexing
        uint16_t light_idx = u->active_lights[i];
        const PointLight *l = &u->scene_lights[light_idx];
        
        vec3 L_vec = vec3_sub(l->position, world_pos);
        float dist = vec3_len(L_vec);
        vec3 L = vec3_div(L_vec, dist);

        float att = 1.0f / (1.0f + 0.1f * dist + 0.02f * dist * dist);
        att *= l->intensity;

        float NdotL = MAX(vec3_dot(normal, L), 0.0f);
        vec3 H = vec3_norm(vec3_add(L, view_dir));
        
        float ndoth = MAX(vec3_dot(normal, H), 0.0f);
        ndoth *= ndoth; ndoth *= ndoth; ndoth *= ndoth; ndoth *= ndoth; ndoth *= ndoth; // 32
        float spec = ndoth;

        vec3 diff_comp = vec3_mul(l->color, NdotL);
        vec3 spec_comp = vec3_mul(l->color, spec);
        total_light = vec3_add(total_light, vec3_mul(vec3_add(diff_comp, spec_comp), att));
    }

    vec3 base = vec3_mul_vec3(u->base_color, total_light);
    float edge_threshold = 0.05f;
    float min_b = MIN(b0, MIN(b1, b2));
    if (min_b < edge_threshold) {
        float pulse = sinf(u->dt * 4.0f + world_pos.y) * 0.5f + 0.5f;
        vec3 neon_color = {0.0f, 0.8f, 1.0f}; 
        base = vec3_add(base, vec3_mul(neon_color, pulse));
    }
    return vec3_to_color(base);
}