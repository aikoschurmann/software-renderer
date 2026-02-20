#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

#include "platform.h"
#include "renderer.h"
#include "scene.h" 
#include "camera.h"

#define SCREEN_W 1000
#define SCREEN_H 768
#define INSTANCE_COUNT (1024 * 16) 
#define LIGHT_COUNT 256
#define TARGET_FPS_UPDATE 5.0f

typedef struct {
    Platform *platform;
    Renderer *renderer;
    Scene    *scene;
    Uniforms  uniforms;

    float total_time;
    bool  is_running;
} App;

// Add this helper function to modify the final color buffer
void apply_post_processing(uint32_t* buffer, int width, int height, float time) {
    // Post-processing logic goes here
}

bool init_app(App *app) {
    memset(app, 0, sizeof(App));
    
    app->platform = platform_create("SoftRenderer Engine", SCREEN_W, SCREEN_H);
    if (!app->platform) return false;

    app->renderer = renderer_create(SCREEN_W, SCREEN_H, 10, 100, 100);
    renderer_set_cull_mode(app->renderer, CULL_BACK_CCW); 

    app->scene = scene_create(INSTANCE_COUNT);
    app->scene->camera = (Camera){ .position = {0, 30, -50}, .up = {0, 1, 0}, .yaw = 90.0f, .pitch = -25.0f, .fov = 60.0f, .znear = 0.5f, .zfar = 1000.0f };
    app->uniforms.screen_width = (float)SCREEN_W; 
    app->uniforms.screen_height = (float)SCREEN_H;

    // Load Meshes entirely inside the Scene
    Mesh* cube   = scene_load_mesh(app->scene, "/Users/aikoschurmann/code/c/soft-renderer/models/cube.obj");
    if (cube) mesh_center_origin(cube);

    // Add Objects
    int grid_size = (int)sqrt(INSTANCE_COUNT);
    float spacing = 6.0f;
    for (int i = 0; i < INSTANCE_COUNT; i++) {
        int x_idx = i % grid_size; 
        int z_idx = i / grid_size;
        vec3 pos = { (x_idx - grid_size/2.0f) * spacing, 0.0f, (z_idx - grid_size/2.0f) * spacing };
        vec3 color = { 0.3f + (x_idx/(float)grid_size)*0.7f, 0.4f, 0.3f + (z_idx/(float)grid_size)*0.7f };
        
        Mesh *m = cube;
        if (!m) continue;

        Entity* e = scene_add_entity(app->scene, m, pos, (vec3){0,0,0}, 1.25f, color);
        e->fs = fs_multi_light_smooth;
    }

    // Add Lights
    for(int i=0; i<LIGHT_COUNT; i++) {
        scene_add_light(app->scene, (vec3){0,0,0}, 
            (vec3){ (float)rand()/RAND_MAX, (float)rand()/RAND_MAX, (float)rand()/RAND_MAX }, 50.0f);
    }

    app->is_running = true;
    return true;
}

// ---------------------------------------------------------
// LOGIC HELPERS
// ---------------------------------------------------------
// Quick pseudo-random hash (returns a value between 0.0 and 1.0)
static float hash_rnd(int n) {
    float f = sinf((float)n * 137.53f) * 43758.545f;
    return f - floorf(f);
}

// Calculates the smooth staircase logic given a specific timeline and threshold
static float get_smooth_stage(float local_time, float threshold) {
    float base = floorf(local_time);
    float frac = local_time - base; 
    float progress = 0.0f;
    if (frac > threshold) {
        float range = 1.0f - threshold;
        float normalized = (frac - threshold) / range; 
        progress = normalized * normalized * (3.0f - 2.0f * normalized);
    }
    return base + progress;
}

typedef struct {
    // Timing & Propagation
    float irregular_speed;     
    float irregular_amount;    
    float burst_threshold;     
    float propagate_speed;     
    
    // Entity Movement
    float bob_speed;           
    float bob_height;          
    float wave_spread;         
    
    // Rotation Burst
    float rot_timer_speed;     
    float rot_base_speed;      
    float rot_burst_spins;     
    
    // Zoom Burst
    float zoom_timer_speed;    
    float zoom_min;            
    float zoom_max;            
    
    // Color
    float color_base_speed;    
    float color_burst_shift;   
    float color_spread;        
} LogicParams;

void update_game_logic(App *app, float dt) {
    app->total_time += dt;
    float time = app->total_time;

    // --- TWEAK BEHAVIOR HERE ---
    LogicParams p = {
        .irregular_speed   = 0.01f,
        .irregular_amount  = 0.1f,
        .burst_threshold   = 0.8f,  
        .propagate_speed   = 0.05f, 

        .bob_speed         = 2.5f,
        .bob_height        = 3.0f,
        .wave_spread       = 0.4f,

        .rot_timer_speed   = 0.1f,  
        .rot_base_speed    = 0.2f,  
        .rot_burst_spins   = 0.2f,  
        
        .zoom_timer_speed  = 0.15f, 
        .zoom_min          = 0.4f,  
        .zoom_max          = 0.8f,  
        
        .color_base_speed  = 0.2f,
        .color_burst_shift = 0.8f,
        .color_spread      = 0.001f 
    };
    // ---------------------------

    int grid_size = (int)sqrt(INSTANCE_COUNT);
    float field_size = grid_size * 6.0f;
    float center_offset = grid_size / 2.0f;

    // ---------------------------------------------------------
    // ENTITY POSITIONS & COLOR WAVES
    // ---------------------------------------------------------
    for(size_t i = 0; i < app->scene->entity_count; i++) {
        Entity *e = &app->scene->entities[i];
        int x_idx = i % grid_size; 
        int z_idx = i / grid_size;
        
        // Calculate spatial delay based on distance from the center of the grid
        float dx = (float)x_idx - center_offset;
        float dz = (float)z_idx - center_offset;
        float dist_from_center = sqrtf(dx*dx + dz*dz);
        
        // Time shifts slightly backwards the further you are from the center
        float local_time = time - (dist_from_center * p.propagate_speed);
        
        // Rotation timeline determines color pops
        float warped_rot_time = local_time * p.rot_timer_speed + sinf(local_time * p.irregular_speed) * p.irregular_amount;
        float rot_stage = get_smooth_stage(warped_rot_time, p.burst_threshold);

        e->position.y = sinf(local_time * p.bob_speed + dist_from_center * p.wave_spread) * p.bob_height;
        e->rotation.y = local_time * 1.2f + i; 

        // Colors ripple outward
        float t = local_time * p.color_base_speed + dist_from_center * p.color_spread + (rot_stage * p.color_burst_shift);
        e->base_color.x = sinf(t) * 0.5f + 0.5f;
        e->base_color.y = sinf(t + 2.0f) * 0.5f + 0.5f;
        e->base_color.z = sinf(t + 4.0f) * 0.5f + 0.5f;
    }

    // ---------------------------------------------------------
    // LIGHTS (Drifting + Propagating Orbits + Independent Zoom)
    // ---------------------------------------------------------
    int lights_per_row = (int)ceilf(sqrtf(LIGHT_COUNT));
    float light_cell_size = field_size / lights_per_row; 

    for(size_t i = 0; i < app->scene->light_count; i++) {
        PointLight *l = &app->scene->lights[i];
        int row = i / lights_per_row; int col = i % lights_per_row;
        
        float home_x = (col + 0.5f) * light_cell_size - (field_size / 2.0f);
        float home_z = (row + 0.5f) * light_cell_size - (field_size / 2.0f);
        
        // Distance from center determines propagation delay
        float dist_from_center = sqrtf(home_x*home_x + home_z*home_z);
        float local_time = time - (dist_from_center * p.propagate_speed);

        // --- TIMELINES ---
        // 1. Rotation Time
        float warped_rot = local_time * p.rot_timer_speed + sinf(local_time * p.irregular_speed) * p.irregular_amount;
        float rot_stage = get_smooth_stage(warped_rot, p.burst_threshold);
        
        // 2. Zoom Time (Entirely independent)
        float warped_zoom = local_time * p.zoom_timer_speed + cosf(local_time * p.irregular_speed) * p.irregular_amount;
        float zoom_stage = get_smooth_stage(warped_zoom, p.burst_threshold);

        // --- ZOOM LOGIC (Holds state for the entire cycle) ---
        int z_base = (int)zoom_stage;
        float z_frac = zoom_stage - z_base;
        
        // Generate a random zoom target for the current stage, and the next stage
        float current_zoom_target = p.zoom_min + hash_rnd(z_base) * (p.zoom_max - p.zoom_min);
        float next_zoom_target = p.zoom_min + hash_rnd(z_base + 1) * (p.zoom_max - p.zoom_min);
        
        // Smoothly interpolate between the two holding states when a burst happens
        float active_zoom = current_zoom_target + (next_zoom_target - current_zoom_target) * z_frac;

        // --- ROTATION LOGIC ---
        int r_base = (int)rot_stage;
        float r_frac = rot_stage - r_base;
        
        float accumulated_burst_spins = 0.0f;
        for (int b = 0; b < r_base; b++) {
            accumulated_burst_spins += (hash_rnd(b + 100) > 0.5f) ? 1.0f : -1.0f;
        }
        float current_dir = (hash_rnd(r_base + 100) > 0.5f) ? 1.0f : -1.0f;
        float total_burst_spins = accumulated_burst_spins + (r_frac * current_dir);

        float orbit_angle = local_time * p.rot_base_speed + (total_burst_spins * p.rot_burst_spins * 6.28318f);
        float cos_a = cosf(orbit_angle);
        float sin_a = sinf(orbit_angle);

        // --- APPLY POSITIONS ---
        float wander = light_cell_size * 0.4f; 
        float raw_x = home_x + sinf(local_time * 0.8f + i * 13.0f) * wander;
        float raw_y = 15.0f + sinf(local_time * 1.5f + i) * 5.0f;
        float raw_z = home_z + cosf(local_time * 0.6f + i * 17.0f) * wander;

        // Apply independent Zoom
        raw_x *= active_zoom;
        raw_z *= active_zoom;

        // Apply 2D rotation matrix
        l->position.x = raw_x * cos_a - raw_z * sin_a;
        l->position.y = raw_y;
        l->position.z = raw_x * sin_a + raw_z * cos_a;

        // Color and Intensity (Tied to the rotation timeline so it flashes when spinning)
        float t = local_time * p.color_base_speed + (dist_from_center * p.color_spread) + (rot_stage * p.color_burst_shift);
        l->color.x = sinf(t) * 0.5f + 0.5f;
        l->color.y = sinf(t + 2.0f) * 0.5f + 0.5f;
        l->color.z = sinf(t + 4.0f) * 0.5f + 0.5f;
        
        l->intensity = 60.0f + sinf(local_time * 2.0f + i) * 30.0f;
    }
}

int main(void) {
    srand((unsigned int)time(NULL));
    App app;
    if (!init_app(&app)) return 1;

    float last_time = platform_get_time();
    float fps_timer = 0.0f; 
    int frame_count = 0;
    InputState input = {0};

    while(app.is_running) {
        float current_time = platform_get_time();
        float dt = current_time - last_time;
        last_time = current_time;

        platform_poll_events(app.platform, &input);
        if (input.quit || input.keys[KEY_ESCAPE]) app.is_running = false;

        // 1. Process Input Controller
        camera_update_freefly(&app.scene->camera, &input, dt);

        // 2. Process Game Entities
        update_game_logic(&app, dt);

        // 3. Render
        app.uniforms.dt = app.total_time;

        scene_render_frame(app.scene, app.renderer, app.platform, &app.uniforms, 0x000000FF);

        // FPS tracking
        frame_count++; fps_timer += dt;
        if (fps_timer >= TARGET_FPS_UPDATE) {
            char title[128];
            sprintf(title, "ENGINE REFACTOR STRESS TEST | FPS: %.1f | Objects: %zu", frame_count / fps_timer, app.scene->entity_count);
            platform_set_title(app.platform, title);
            frame_count = 0; fps_timer = 0.0f;
        }
    }
    
    scene_destroy(app.scene);
    renderer_destroy(app.renderer);
    platform_destroy(app.platform);
    
    return 0;
}