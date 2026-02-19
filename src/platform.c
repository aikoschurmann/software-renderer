#include "platform.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

struct Platform {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int width, height;
};

Platform* platform_create(const char* title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return NULL;

    Platform* p = calloc(1, sizeof(Platform));
    p->width = width;
    p->height = height;

    p->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                 width, height, 0);
    p->renderer = SDL_CreateRenderer(p->window, -1, 0);
    p->texture = SDL_CreateTexture(p->renderer, SDL_PIXELFORMAT_RGBA8888, 
                                   SDL_TEXTUREACCESS_STREAMING, width, height);

    SDL_SetRelativeMouseMode(SDL_TRUE);
    return p;
}

void platform_destroy(Platform* p) {
    if (p) {
        SDL_DestroyTexture(p->texture);
        SDL_DestroyRenderer(p->renderer);
        SDL_DestroyWindow(p->window);
        free(p);
    }
    SDL_Quit();
}

void platform_poll_events(Platform* p, InputState* input) {
    SDL_Event ev;
    input->mouse_dx = 0;
    input->mouse_dy = 0;

    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) input->quit = true;
        if (ev.type == SDL_MOUSEMOTION) {
            input->mouse_dx += ev.motion.xrel;
            input->mouse_dy += ev.motion.yrel;
        }
    }

    const uint8_t* k = SDL_GetKeyboardState(NULL);
    input->keys[KEY_W]      = k[SDL_SCANCODE_Z];
    input->keys[KEY_A]      = k[SDL_SCANCODE_Q];
    input->keys[KEY_S]      = k[SDL_SCANCODE_S];
    input->keys[KEY_D]      = k[SDL_SCANCODE_D];
    input->keys[KEY_Q]      = k[SDL_SCANCODE_Q];
    input->keys[KEY_E]      = k[SDL_SCANCODE_E];
    input->keys[KEY_SHIFT]  = k[SDL_SCANCODE_LSHIFT];
    input->keys[KEY_SPACE]  = k[SDL_SCANCODE_SPACE];
    input->keys[KEY_ESCAPE] = k[SDL_SCANCODE_ESCAPE];
    input->keys[KEY_L]      = k[SDL_SCANCODE_L];
}

void platform_update_window(Platform* p, const uint32_t* buffer, int width, int height) {
    SDL_UpdateTexture(p->texture, NULL, buffer, width * 4);
    SDL_RenderCopy(p->renderer, p->texture, NULL, NULL);
    SDL_RenderPresent(p->renderer);
}

float platform_get_time(void) {
    return SDL_GetTicks() / 1000.0f;
}

void platform_set_title(Platform* p, const char* title) {
    SDL_SetWindowTitle(p->window, title);
}