#include "SDL.h"

typedef struct SDLCanvas {
    SDL_Window *display;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Uint8 *yuvBuffer;
    size_t yPlaneSz;
    size_t uvPlaneSz;
    int yPitch;
    int uvPitch;
} SDLCanvas;

void sdl_open(SDLCanvas *o, int img_width, int img_height);
void sdl_close(SDLCanvas *o);
void sdl_display_frame(SDLCanvas *o);
void sdl_loop(void);
