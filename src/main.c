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
#define INSTANCE_COUNT (1024 * 8) 
#define LIGHT_COUNT 256
#define TARGET_FPS_UPDATE 0.5f

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

    int grid_size = (int)sqrt(INSTANCE_COUNT);
    for(size_t i = 0; i < app->scene->entity_count; i++) {
        Entity *e = &app->scene->entities[i];
        int x_idx = i % grid_size; int z_idx = i / grid_size;
        e->position.y = sinf(app->total_time * 2.5f + (x_idx + z_idx) * 0.4f) * 3.0f;
        e->rotation.y = app->total_time * 1.2f + i;
    }

    float field_size = grid_size * 6.0f;
    int lights_per_row = (int)ceilf(sqrtf(LIGHT_COUNT));
    float light_cell_size = field_size / lights_per_row; 

    for(size_t i=0; i < app->scene->light_count; i++) {
        PointLight *l = &app->scene->lights[i];
        int row = i / lights_per_row; int col = i % lights_per_row;
        float home_x = (col + 0.5f) * light_cell_size - (field_size / 2.0f);
        float home_z = (row + 0.5f) * light_cell_size - (field_size / 2.0f);
        float wander = light_cell_size * 0.4f; 

        l->intensity = 60.0f + sinf(app->total_time * 2.0f + i) * 30.0f;
        
        l->position = (vec3){ home_x + sinf(app->total_time * 0.8f + i * 13.0f) * wander, 15.0f + sinf(app->total_time * 1.5f + i) * 5.0f, home_z + cosf(app->total_time * 0.6f + i * 17.0f) * wander };
    }

    for(size_t i = 0; i < app->scene->entity_count; i++) {
        Entity *e = &app->scene->entities[i];
        float t = app->total_time * 0.5f + i * 0.001f;

        // Procedural "Rainbow" or "Neon" shifting
        e->base_color.x = sinf(t) * 0.5f + 0.5f;
        e->base_color.y = sinf(t + 2.0f) * 0.5f + 0.5f;
        e->base_color.z = sinf(t + 4.0f) * 0.5f + 0.5f;
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