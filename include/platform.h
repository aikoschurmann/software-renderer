#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KEY_W, KEY_A, KEY_S, KEY_D,
    KEY_Q, KEY_E, KEY_SPACE, KEY_ESCAPE, KEY_SHIFT,
    KEY_L,
    KEY_COUNT
} KeyCode;

// Input State Container
typedef struct {
    bool keys[KEY_COUNT];
    float mouse_dx;
    float mouse_dy;
    bool quit;
} InputState;

typedef struct Platform Platform;

// Lifecycle
Platform* platform_create(const char* title, int width, int height);
void      platform_destroy(Platform* p);

// Core Loop
void      platform_poll_events(Platform* p, InputState* input);
void      platform_update_window(Platform* p, const uint32_t* buffer, int width, int height);

// Utils
float     platform_get_time(void);
void      platform_set_title(Platform* p, const char* title);

#endif