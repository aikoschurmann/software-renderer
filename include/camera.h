#ifndef CAMERA_H
#define CAMERA_H

#include "maths.h"
#include "platform.h"



typedef struct {
    vec3  position;
    vec3  target;
    vec3  up;
    float yaw;   // Horizontal rotation
    float pitch; // Vertical rotation
    float fov;
    float znear;
    float zfar;
} Camera;

// Generates View and Projection matrices based on camera state
static inline void camera_get_matrices(const Camera *cam, float aspect, mat4 *out_view, mat4 *out_proj) {
    *out_view = mat4_lookat(cam->position, cam->target, cam->up);
    *out_proj = mat4_perspective(TO_RAD(cam->fov), aspect, cam->znear, cam->zfar);
}

void camera_update_freefly(Camera *cam, InputState *input, float dt);

#endif