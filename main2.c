#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#include "platform.h"
#include "renderer.h"
#include "scene.h" 

#define SCREEN_W 1000
#define SCREEN_H 768
#define LIGHT_COUNT 3
#define TARGET_FPS_UPDATE 0.5f

typedef struct {
    Platform *platform;
    Renderer *renderer;
    Scene    *scene;
    Uniforms  uniforms;

    Entity   *dragon_entity;
    Entity   *light_cubes[LIGHT_COUNT];

    float total_time;
    bool  is_running;
} App;

void apply_post_processing(uint32_t* buffer, int width, int height, float time) {

}


bool init_app(App *app) {
    memset(app, 0, sizeof(App));
    app->platform = platform_create("SoftRenderer - Kinetic Lights Demo", SCREEN_W, SCREEN_H);
    app->renderer = renderer_create(SCREEN_W, SCREEN_H, 10, 100, 100);
    renderer_set_cull_mode(app->renderer, CULL_BACK_CCW); 

    app->scene = scene_create(10); 
    app->scene->camera = (Camera){ .position = {0, 30, -50}, .up = {0, 1, 0}, .yaw = 90.0f, .pitch = -25.0f, .fov = 60.0f, .znear = 0.5f, .zfar = 1000.0f };
    app->uniforms.screen_width = (float)SCREEN_W; 
    app->uniforms.screen_height = (float)SCREEN_H;

    Mesh* dragon = scene_load_mesh(app->scene, "/Users/aikoschurmann/code/c/soft-renderer/models/xyzrgb_dragon.obj");
    if (dragon) mesh_center_origin(dragon);
    Mesh* cube = scene_load_mesh(app->scene, "/Users/aikoschurmann/code/c/soft-renderer/models/cube.obj");
    if (cube) mesh_center_origin(cube);

    app->dragon_entity = scene_add_entity(app->scene, dragon, (vec3){0,0,0}, (vec3){0,0,0}, 0.1f, (vec3){0.8f, 0.8f, 0.8f});
    app->dragon_entity->fs = fs_multi_light_smooth; 

    for(int i = 0; i < LIGHT_COUNT; i++) {
        scene_add_light(app->scene, (vec3){0,0,0}, (vec3){1,1,1}, 50.0f);
        app->light_cubes[i] = scene_add_entity(app->scene, cube, (vec3){0,0,0}, (vec3){0,0,0}, 0.3f, (vec3){1,1,1});
        app->light_cubes[i]->fs = fs_pure_color;
    }

    app->is_running = true;
    return app->platform != NULL;
}

// GAME LOGIC: Animate objects
void update_game_logic(App *app, float dt) {
    app->total_time += dt;
    float t = app->total_time;

    // Tumble the Dragon
    app->dragon_entity->rotation.y = t * 0.6f;
    app->dragon_entity->rotation.x = sinf(t * 0.4f) * 0.3f; 

    // Animate Lights
    app->scene->lights[0].position = (vec3){ cosf(t * 1.2f) * 14.0f, sinf(t * 2.4f) * 8.0f + 5.0f, sinf(t * 1.2f) * 14.0f };
    app->scene->lights[0].color = (vec3){1.0f, 0.1f, 0.1f};

    app->scene->lights[1].position = (vec3){ cosf(t * 0.5f) * 20.0f, 4.0f + cosf(t * 0.8f) * 4.0f, sinf(t * 0.5f) * 12.0f };
    app->scene->lights[1].color = (vec3){0.1f, 1.0f, 0.1f};

    app->scene->lights[2].position = (vec3){ sinf(t * 1.8f) * (10.0f + sinf(t) * 5.0f), cosf(t * 1.5f) * 12.0f + 5.0f, cosf(t * 1.8f) * (10.0f + sinf(t) * 5.0f) };
    app->scene->lights[2].color = (vec3){0.2f, 0.4f, 1.0f};

    for(int i = 0; i < LIGHT_COUNT; i++) {
        app->scene->lights[i].intensity = 35.0f + sinf(t * 2.0f + i) * 10.0f;
        app->light_cubes[i]->position = app->scene->lights[i].position;
        app->light_cubes[i]->base_color = app->scene->lights[i].color;
    }
}

int main(void) {
    srand((unsigned int)time(NULL));
    App app;
    if (!init_app(&app)) return 1;

    float last_time = platform_get_time();
    float fps_timer = 0.0f; int frame_count = 0;
    InputState input = {0};

    while(app.is_running) {
        float current_time = platform_get_time();
        float dt = current_time - last_time;
        last_time = current_time;

        platform_poll_events(app.platform, &input);
        if (input.quit || input.keys[KEY_ESCAPE]) app.is_running = false;

        // 1. Process Input (Camera is now entirely hidden away!)
        camera_update_freefly(&app.scene->camera, &input, dt);

        // 2. Process Game Logic
        update_game_logic(&app, dt);

        // 3. Render Pipeline (1 line manages the whole frame!)
        scene_render_frame(app.scene, app.renderer, app.platform, &app.uniforms, 0x0A0A0AFF);

        // UI / Framerate updating
        frame_count++; fps_timer += dt;
        if (fps_timer >= TARGET_FPS_UPDATE) {
            char title[128];
            sprintf(title, "KINETIC RGB LIGHTS | FPS: %.1f | Tris: %zu", frame_count / fps_timer, app.renderer->triangle_count);
            platform_set_title(app.platform, title);
            frame_count = 0; fps_timer = 0.0f;
        }
    }
    
    scene_destroy(app.scene);
    renderer_destroy(app.renderer);
    platform_destroy(app.platform);
    return 0;
}