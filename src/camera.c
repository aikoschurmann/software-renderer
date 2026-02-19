#include "camera.h"
#include <math.h>

void camera_update_freefly(Camera *cam, InputState *input, float dt) {
    // 1. Mouse Look
    cam->yaw += input->mouse_dx * 0.15f;
    cam->pitch -= input->mouse_dy * 0.15f;
    if (cam->pitch > 89.0f) cam->pitch = 89.0f;
    if (cam->pitch < -89.0f) cam->pitch = -89.0f;

    // 2. Calculate Direction Vectors
    vec3 front = vec3_norm((vec3){ 
        cosf(TO_RAD(cam->yaw)) * cosf(TO_RAD(cam->pitch)), 
        sinf(TO_RAD(cam->pitch)), 
        sinf(TO_RAD(cam->yaw)) * cosf(TO_RAD(cam->pitch))
    });
    vec3 right = vec3_norm(vec3_cross(front, cam->up));
    vec3 world_up = {0.0f, 1.0f, 0.0f}; 
    
    // 3. Movement
    float speed = 30.0f * dt; 
    if(input->keys[KEY_W]) cam->position = vec3_add(cam->position, vec3_mul(front, speed));
    if(input->keys[KEY_S]) cam->position = vec3_sub(cam->position, vec3_mul(front, speed));
    if(input->keys[KEY_A]) cam->position = vec3_sub(cam->position, vec3_mul(right, speed));
    if(input->keys[KEY_D]) cam->position = vec3_add(cam->position, vec3_mul(right, speed));
    if(input->keys[KEY_SPACE]) cam->position = vec3_add(cam->position, vec3_mul(world_up, speed));
    if(input->keys[KEY_SHIFT]) cam->position = vec3_sub(cam->position, vec3_mul(world_up, speed));
    
    cam->target = vec3_add(cam->position, front);
}