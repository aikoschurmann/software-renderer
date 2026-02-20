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
#define LIGHT_COUNT 256 * 2
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
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>

void apply_post_processing(uint32_t* buffer, int width, int height, float time) {
    
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
void update_game_logic(App *app, float dt) {
    app->total_time += dt;
    float t = app->total_time;

    int grid_size = (int)sqrt(INSTANCE_COUNT);
    float spacing = 6.0f;

    // 1. Update Entities: Ripple Waves and Topographical Colors
    for(size_t i = 0; i < app->scene->entity_count; i++) {
        Entity *e = &app->scene->entities[i];
        int x_idx = i % grid_size; 
        int z_idx = i / grid_size;
        
        // Reconstruct local coordinates relative to the center
        float cx = (x_idx - grid_size / 2.0f) * spacing;
        float cz = (z_idx - grid_size / 2.0f) * spacing;
        
        // Calculate distance from center for radial ripples
        float dist = sqrtf(cx*cx + cz*cz);

        // Complex wave: Radial ripple + diagonal sweep
        float wave1 = sinf(t * 3.0f - dist * 0.3f);
        float wave2 = sinf(t * 1.5f + (cx + cz) * 0.1f);
        
        e->position.y = (wave1 + wave2) * 3.0f; // Height varies from -6 to +6
        e->rotation.y = t * 1.2f + dist * 0.1f;

        // Topographical colors based on height
        float norm_h = (e->position.y + 6.0f) / 12.0f; // Normalize height to 0.0 - 1.0
        
        // Shift colors from Deep Purple (low) -> Cyan (mid) -> Hot Pink (high)
        e->base_color.x = norm_h;                                   // Red increases with height
        e->base_color.y = 0.2f + sinf(t * 2.0f + cx * 0.1f) * 0.3f; // Pulsing green
        e->base_color.z = 1.0f - norm_h * 0.5f;                     // Blue decreases with height
    }

    // 2. Update Lights: Swirling Vortex and Morphing Colors
    float field_size = grid_size * spacing;
    float radius = field_size * 0.45f; // Lights sweep across 90% of the grid

    for(size_t i = 0; i < app->scene->light_count; i++) {
        PointLight *l = &app->scene->lights[i];
        
        // Distribute lights mathematically using a phase offset
        float phase = (float)i / app->scene->light_count * PI * 2.0f;

        // Lissajous curve paths (figure-8s and sweeping orbits)
        float speed = 0.8f;
        float nx = sinf(t * speed + phase * 3.0f);
        float nz = sinf(t * speed * 1.3f + phase * 2.0f);
        float ny = sinf(t * speed * 2.7f + phase);

        l->position.x = nx * radius;
        l->position.z = nz * radius;
        // Lights dip close to the cubes and soar high up
        l->position.y = 12.0f + ny * 10.0f; 

        // Dynamic pulsing intensity based on height
        l->intensity = 60.0f + (ny * 40.0f);

        // Morphing neon colors over time
        // Offset the RGB sine waves so they cycle through vibrant spectrums
        l->color.x = 0.5f + 0.5f * sinf(t * 1.5f + phase + 0.0f);
        l->color.y = 0.5f + 0.5f * sinf(t * 2.1f + phase + 2.0f);
        l->color.z = 0.5f + 0.5f * sinf(t * 1.8f + phase + 4.0f);
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

        scene_render_frame(app.scene, app.renderer, app.platform, &app.uniforms, 0x050508FF);

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