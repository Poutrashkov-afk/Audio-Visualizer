#ifndef RENDERER_H
#define RENDERER_H

#include <SDL.h>
#include "settings.h"
#include "fft.h"        /* ← ADD THIS: brings in MAX_BANDS */

typedef struct {
    float x, y;
    float vx, vy;
    float lifetime;
    float max_lifetime;
    float size;
    Color color;
    bool alive;
} Particle;

#define MAX_PARTICLES 500

typedef struct {
    Settings *settings;

    /* Trail / fade surface */
    SDL_Texture *trail_texture;
    int trail_w, trail_h;

    /* Waterfall */
    SDL_Texture *waterfall_texture;
    int waterfall_w, waterfall_h;

    /* Particles */
    Particle particles[MAX_PARTICLES];
    int particle_count;

    /* Previous frame data for beat detection */
    float prev_bands[MAX_BANDS];
    bool has_prev;
} Renderer;

void renderer_init(Renderer *r, Settings *settings);
void renderer_destroy(Renderer *r, SDL_Renderer *sdl_renderer);
void renderer_render(Renderer *r, SDL_Renderer *sdl_renderer,
                     float *band_data, int num_bands,
                     float *waveform, int waveform_size,
                     int mode, ColorTheme *theme);

#endif