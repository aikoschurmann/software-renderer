#ifndef CORE_MATHS_H
#define CORE_MATHS_H

#include <math.h>
#include <string.h> 
#include <stdint.h>

/* --- Macros --- */
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#define PI 3.14159265359f
#define TO_RAD(deg) ((deg) * (PI / 180.0f))

/* --- Types --- */
typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { float m[4][4]; } mat4;

typedef struct { vec3 min, max, center; } BoundingBox;

/* --- Vector Helpers --- */
static inline vec3 vec3_add(vec3 a, vec3 b) { return (vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
static inline vec3 vec3_sub(vec3 a, vec3 b) { return (vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
static inline vec3 vec3_mul(vec3 v, float s) { return (vec3){v.x*s, v.y*s, v.z*s}; }
static inline vec3 vec3_div(vec3 v, float s) { return (vec3){v.x/s, v.y/s, v.z/s}; } 
static inline float vec3_len(vec3 v) { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }  
static inline float vec3_len_sq(vec3 v) { return v.x*v.x + v.y*v.y + v.z*v.z; }  
static inline float vec3_dot(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline vec3 vec3_cross(vec3 a, vec3 b) {
    return (vec3){ a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}



static inline vec3 vec3_norm(vec3 v) {
    float l = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    return l > 0 ? vec3_mul(v, 1.0f/l) : v;
}
static inline vec3 vec3_mul_vec3(vec3 a, vec3 b) { 
    return (vec3){ a.x * b.x, a.y * b.y, a.z * b.z };
}

/* --- Matrix Helpers --- */
static inline mat4 mat4_identity() {
    mat4 m; memset(&m, 0, sizeof(m));
    m.m[0][0]=1; m.m[1][1]=1; m.m[2][2]=1; m.m[3][3]=1;
    return m;
}

static inline mat4 mat4_translate(float x, float y, float z) {
    mat4 m = mat4_identity();
    m.m[3][0] = x; m.m[3][1] = y; m.m[3][2] = z;
    return m;
}

static inline mat4 mat4_scale(float s) {
    mat4 m = mat4_identity();
    m.m[0][0] = s; m.m[1][1] = s; m.m[2][2] = s;
    return m;
}

// NEW: Non-uniform scaling (allows different scale per axis)
static inline mat4 mat4_scale_aniso(float sx, float sy, float sz) {
    mat4 m = mat4_identity();
    m.m[0][0] = sx; m.m[1][1] = sy; m.m[2][2] = sz;
    return m;
}

static inline mat4 mat4_rotate_y(float angle) {
    mat4 m = mat4_identity();
    float c = cosf(angle), s = sinf(angle);
    m.m[0][0] = c;  m.m[0][2] = s;
    m.m[2][0] = -s; m.m[2][2] = c;
    return m;
}

// NEW: Rotation around X
static inline mat4 mat4_rotate_x(float angle) {
    mat4 m = mat4_identity();
    float c = cosf(angle), s = sinf(angle);
    m.m[1][1] = c;  m.m[1][2] = s;
    m.m[2][1] = -s; m.m[2][2] = c;
    return m;
}

// NEW: Rotation around Z
static inline mat4 mat4_rotate_z(float angle) {
    mat4 m = mat4_identity();
    float c = cosf(angle), s = sinf(angle);
    m.m[0][0] = c;  m.m[0][1] = s;
    m.m[1][0] = -s; m.m[1][1] = c;
    return m;
}

// Matrix Multiplication (A * B)
static inline mat4 mat4_mul(mat4 a, mat4 b) {
    mat4 res = {0};
    for(int c=0; c<4; c++) {
        for(int r=0; r<4; r++) {
            res.m[c][r] = a.m[0][r]*b.m[c][0] + a.m[1][r]*b.m[c][1] + 
                          a.m[2][r]*b.m[c][2] + a.m[3][r]*b.m[c][3];
        }
    }
    return res;
}

// Apply Matrix to Vector
static inline vec4 mat4_mul_vec4(mat4 m, vec4 v) {
    vec4 r;
    r.x = m.m[0][0]*v.x + m.m[1][0]*v.y + m.m[2][0]*v.z + m.m[3][0]*v.w;
    r.y = m.m[0][1]*v.x + m.m[1][1]*v.y + m.m[2][1]*v.z + m.m[3][1]*v.w;
    r.z = m.m[0][2]*v.x + m.m[1][2]*v.y + m.m[2][2]*v.z + m.m[3][2]*v.w;
    r.w = m.m[0][3]*v.x + m.m[1][3]*v.y + m.m[2][3]*v.z + m.m[3][3]*v.w;
    return r;
}

/* --- Camera & Frustum --- */

static inline mat4 mat4_perspective(float fov_rad, float aspect, float znear, float zfar) {
    mat4 m = {0};
    float tan_half_fov = tanf(fov_rad / 2.0f);
    m.m[0][0] = 1.0f / (aspect * tan_half_fov);
    m.m[1][1] = 1.0f / tan_half_fov;
    m.m[2][2] = -(zfar + znear) / (zfar - znear);
    m.m[2][3] = -1.0f;
    m.m[3][2] = -(2.0f * zfar * znear) / (zfar - znear);
    return m;
}

static inline mat4 mat4_lookat(vec3 eye, vec3 center, vec3 up) {
    vec3 f = vec3_norm(vec3_sub(center, eye)); // Forward
    vec3 s = vec3_norm(vec3_cross(f, up));     // Right
    vec3 u = vec3_cross(s, f);                 // Up

    mat4 m = mat4_identity();
    m.m[0][0] = s.x; m.m[1][0] = s.y; m.m[2][0] = s.z;
    m.m[0][1] = u.x; m.m[1][1] = u.y; m.m[2][1] = u.z;
    m.m[0][2] =-f.x; m.m[1][2] =-f.y; m.m[2][2] =-f.z;
    m.m[3][0] = -vec3_dot(s, eye);
    m.m[3][1] = -vec3_dot(u, eye);
    m.m[3][2] =  vec3_dot(f, eye);
    return m;
}

static inline mat4 mat4_viewport(float width, float height) {
    mat4 m = mat4_identity();
    m.m[0][0] = width * 0.5f;
    m.m[1][1] = -height * 0.5f;
    m.m[3][0] = width * 0.5f;
    m.m[3][1] = height * 0.5f;
    return m;
}

/* --- Interpolation --- */
static inline float interp_float(float v0, float v1, float v2, float b0, float b1, float b2) {
    return v0*b0 + v1*b1 + v2*b2;
}

static inline uint32_t vec3_to_color(vec3 v) {
    uint8_t r = (uint8_t)(CLAMP(v.x, 0, 1) * 255.0f);
    uint8_t g = (uint8_t)(CLAMP(v.y, 0, 1) * 255.0f);
    uint8_t b = (uint8_t)(CLAMP(v.z, 0, 1) * 255.0f);
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

static inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static inline uint32_t vec4_to_color(vec4 v) {
    uint8_t r = (uint8_t)(CLAMP(v.x, 0, 1) * 255.0f);
    uint8_t g = (uint8_t)(CLAMP(v.y, 0, 1) * 255.0f);
    uint8_t b = (uint8_t)(CLAMP(v.z, 0, 1) * 255.0f);
    uint8_t a = (uint8_t)(CLAMP(v.w, 0, 1) * 255.0f);
    return (r << 24) | (g << 16) | (b << 8) | a;
}

static inline uint32_t blend_colors(uint32_t src, uint32_t dst) {
    uint8_t sa = src & 0xFF; 
    if (sa == 255) return src;      // Opaque
    if (sa == 0) return dst;        // Transparent
    
    // Extract channels (RGBA8888)
    uint8_t sr = (src >> 24) & 0xFF, sg = (src >> 16) & 0xFF, sb = (src >> 8) & 0xFF;
    uint8_t dr = (dst >> 24) & 0xFF, dg = (dst >> 16) & 0xFF, db = (dst >> 8) & 0xFF;

    float alpha = sa / 255.0f;
    float inv_alpha = 1.0f - alpha;

    uint8_t r = (uint8_t)(sr * alpha + dr * inv_alpha);
    uint8_t g = (uint8_t)(sg * alpha + dg * inv_alpha);
    uint8_t b = (uint8_t)(sb * alpha + db * inv_alpha);

    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

typedef struct {
    vec3 position;
    vec3 rotation; // Euler angles in radians (X, Y, Z)
    vec3 scale;
} Transform;

static inline Transform transform_default() {
    return (Transform){
        .position = {0,0,0}, 
        .rotation = {0,0,0}, 
        .scale    = {1,1,1}
    };
}

static inline mat4 transform_get_matrix(const Transform* t) {
    // Order: Translate * Rotate * Scale
    mat4 s = mat4_scale_aniso(t->scale.x, t->scale.y, t->scale.z);
    
    // Combined Rotation (Y -> X -> Z is a common order, varies by need)
    mat4 rx = mat4_rotate_x(t->rotation.x);
    mat4 ry = mat4_rotate_y(t->rotation.y);
    mat4 rz = mat4_rotate_z(t->rotation.z);
    mat4 rot = mat4_mul(mat4_mul(ry, rx), rz);
    
    mat4 tr = mat4_translate(t->position.x, t->position.y, t->position.z);
    
    // M = T * R * S
    return mat4_mul(mat4_mul(tr, rot), s);
}
#endif