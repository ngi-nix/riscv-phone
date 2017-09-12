#include <stdio.h>

#include "display.h"

void sdl_open(SDLCanvas *o, int img_width, int img_height) {
    // Initialize the SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init() Failed: %s\n", SDL_GetError());
        exit(1);
    }

    o->display = SDL_CreateWindow("SDL Tutorial",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              img_width, img_height, 0);
    if (o->display == NULL) {
        fprintf(stderr, "SDL_SetVideoMode() Failed: %s\n", SDL_GetError());
        exit(1);
    }

    o->renderer = SDL_CreateRenderer(o->display, -1, 0);
    if (o->renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer() Failed: %s\n", SDL_GetError());
        exit(1);
    }
    
    o->texture = SDL_CreateTexture(o->renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, img_width, img_height);
    if (o->texture == NULL) {
        fprintf(stderr, "SDL_CreateTextureFromSurface() Failed: %s\n", SDL_GetError());
        exit(1);
    }

    o->yPlaneSz = img_width * img_height;
    o->uvPlaneSz = img_width * img_height / 4;
    o->yuvBuffer = (Uint8*)malloc(o->yPlaneSz + 2 * o->uvPlaneSz);
    o->yPitch = img_width;
    o->uvPitch = img_width / 2;
}

void sdl_close(SDLCanvas *o) {
    free(o->yuvBuffer);
    SDL_DestroyTexture(o->texture);
    SDL_DestroyRenderer(o->renderer);
    SDL_DestroyWindow(o->display);
    SDL_Quit();
}

void sdl_display_frame(SDLCanvas *o) {
    SDL_UpdateYUVTexture(o->texture, NULL, o->yuvBuffer, o->yPitch, o->yuvBuffer + o->yPlaneSz, o->uvPitch,  o->yuvBuffer + o->yPlaneSz + o->uvPlaneSz, o->uvPitch);
    SDL_RenderClear(o->renderer);
    SDL_RenderCopy(o->renderer, o->texture, NULL, NULL);
    SDL_RenderPresent(o->renderer);
}

void sdl_loop(void) {
    SDL_Event event;
    
    while(1) {
        // Check for messages
        if (SDL_PollEvent(&event)) {
            // Check for the quit message
            if (event.type == SDL_QUIT) {
                // Quit the program
                break;
            }
        }
    }
}
