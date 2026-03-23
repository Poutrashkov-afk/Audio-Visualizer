#include "renderer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void set_color(SDL_Renderer *r, Color c, uint8_t a) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, a);
}

static Color get_bar_color(float magnitude, int index, int total, ColorTheme *theme) {
    float position = (total > 1) ? (float)index / (float)(total - 1) : 0.0f;
    Color base;
    if (position < 0.5f)
        base = color_lerp(theme->primary, theme->secondary, position * 2.0f);
    else
        base = color_lerp(theme->secondary, theme->tertiary, (position - 0.5f) * 2.0f);

    float brightness = 0.4f + 0.6f * magnitude;
    return color_brightness(base, brightness);
}

/* ── Particles ─────────────────────────────────────────────── */

static void particle_emit(Renderer *r, float x, float y, Color color,
                          int count, float spread, float speed) {
    for (int i = 0; i < count && r->particle_count < MAX_PARTICLES; i++) {
        Particle *p = &r->particles[r->particle_count++];
        p->x = x;
        p->y = y;
        p->vx = ((float)rand() / RAND_MAX - 0.5f) * spread * 2.0f;
        p->vy = -((float)rand() / RAND_MAX * speed * 1.5f + speed * 0.5f);
        p->lifetime = 0.5f + (float)rand() / RAND_MAX * 1.5f;
        p->max_lifetime = p->lifetime;
        p->size = 1.5f + (float)rand() / RAND_MAX * 2.5f;
        p->color = color;
        p->alive = true;
    }
}

static void particles_update(Renderer *r, float dt) {
    int alive = 0;
    for (int i = 0; i < r->particle_count; i++) {
        Particle *p = &r->particles[i];
        if (!p->alive) continue;

        p->x += p->vx * dt * 60.0f;
        p->y += p->vy * dt * 60.0f;
        p->vy += 0.05f * dt * 60.0f;
        p->lifetime -= dt;

        if (p->lifetime <= 0.0f) {
            p->alive = false;
        } else {
            r->particles[alive++] = *p;
        }
    }
    r->particle_count = alive;
}

static void particles_draw(Renderer *r, SDL_Renderer *sdl) {
    for (int i = 0; i < r->particle_count; i++) {
        Particle *p = &r->particles[i];
        if (!p->alive) continue;

        float alpha_f = p->lifetime / p->max_lifetime;
        uint8_t a = (uint8_t)(alpha_f * 255.0f);
        int size = (int)(p->size * alpha_f);
        if (size < 1) size = 1;

        Color c = color_brightness(p->color, alpha_f);
        SDL_SetRenderDrawColor(sdl, c.r, c.g, c.b, a);

        SDL_Rect rect = {(int)p->x - size / 2, (int)p->y - size / 2, size, size};
        SDL_RenderFillRect(sdl, &rect);
    }
}

/* ── Visualization modes ───────────────────────────────────── */

static void draw_bars(SDL_Renderer *sdl, float *data, int num_bars,
                      ColorTheme *theme, int w, int h, bool mirrored,
                      float bar_width_ratio) {
    float total_width = w * 0.9f;
    float bar_gap = total_width / num_bars;
    int bar_width = (int)(bar_gap * bar_width_ratio);
    if (bar_width < 2) bar_width = 2;
    float start_x = (w - total_width) / 2.0f;

    float max_height = mirrored ? h * 0.42f : h * 0.85f;
    int center_y = mirrored ? h / 2 : h;

    for (int i = 0; i < num_bars; i++) {
        int x = (int)(start_x + i * bar_gap);
        int bar_height = (int)(data[i] * max_height);
        if (bar_height < 2) bar_height = 2;

        Color c = get_bar_color(data[i], i, num_bars, theme);

        if (mirrored) {
            SDL_Rect up = {x, center_y - bar_height, bar_width, bar_height};
            SDL_Rect down = {x, center_y, bar_width, bar_height};
            set_color(sdl, c, 255);
            SDL_RenderFillRect(sdl, &up);
            SDL_RenderFillRect(sdl, &down);
        } else {
            SDL_Rect rect = {x, center_y - bar_height, bar_width, bar_height};
            set_color(sdl, c, 255);
            SDL_RenderFillRect(sdl, &rect);
        }
    }
}

static void draw_circular(SDL_Renderer *sdl, float *data, int num_bars,
                           ColorTheme *theme, int w, int h) {
    int cx = w / 2, cy = h / 2;
    float base_r = (w < h ? w : h) * 0.15f;
    float max_len = (w < h ? w : h) * 0.3f;

    for (int i = 0; i < num_bars; i++) {
        float angle = (2.0f * (float)M_PI * i / num_bars) - (float)M_PI / 2.0f;
        float bar_len = data[i] * max_len + 5.0f;

        int x1 = cx + (int)(cosf(angle) * base_r);
        int y1 = cy + (int)(sinf(angle) * base_r);
        int x2 = cx + (int)(cosf(angle) * (base_r + bar_len));
        int y2 = cy + (int)(sinf(angle) * (base_r + bar_len));

        Color c = get_bar_color(data[i], i, num_bars, theme);
        set_color(sdl, c, 255);
        SDL_RenderDrawLine(sdl, x1, y1, x2, y2);
    }

    /* Center circle outline */
    set_color(sdl, theme->primary, 200);
    int segments = 60;
    for (int i = 0; i < segments; i++) {
        float a1 = 2.0f * (float)M_PI * i / segments;
        float a2 = 2.0f * (float)M_PI * (i + 1) / segments;
        SDL_RenderDrawLine(sdl,
            cx + (int)(cosf(a1) * base_r), cy + (int)(sinf(a1) * base_r),
            cx + (int)(cosf(a2) * base_r), cy + (int)(sinf(a2) * base_r));
    }
}

static void draw_wave(SDL_Renderer *sdl, float *waveform, int waveform_size,
                      ColorTheme *theme, int w, int h) {
    int center_y = h / 2;
    float amplitude = h * 0.35f;

    int step = waveform_size / w;
    if (step < 1) step = 1;

    Color colors[3];
    colors[0] = theme->primary;
    colors[1] = color_lerp(theme->primary, theme->tertiary, 0.33f);
    colors[2] = color_lerp(theme->primary, theme->tertiary, 0.66f);

    for (int layer = 0; layer < 3; layer++) {
        Color c = color_brightness(colors[layer], 1.0f - layer * 0.25f);
        set_color(sdl, c, 255);

        int prev_x = 0;
        int prev_y = center_y;

        for (int x = 0; x < w; x++) {
            int sample_idx = x * step;
            if (sample_idx >= waveform_size) sample_idx = waveform_size - 1;

            float sample = waveform[sample_idx];
            int y = center_y + (int)(sample * amplitude * (1.0f + layer * 0.3f));

            if (x > 0) {
                SDL_RenderDrawLine(sdl, prev_x, prev_y, x, y);
            }
            prev_x = x;
            prev_y = y;
        }
    }
}

static void draw_spectrum_line(SDL_Renderer *sdl, float *data, int num_points,
                               ColorTheme *theme, int w, int h) {
    float margin = w * 0.05f;
    float usable = w - 2.0f * margin;
    int center_y = (int)(h * 0.6f);

    /* Top line */
    set_color(sdl, theme->primary, 255);
    int prev_x = (int)margin, prev_y = center_y;

    for (int i = 0; i < num_points; i++) {
        int x = (int)(margin + ((float)i / (num_points - 1)) * usable);
        int y = center_y - (int)(data[i] * h * 0.5f);

        if (i > 0) SDL_RenderDrawLine(sdl, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;

        /* Dots at peaks */
        if (data[i] > 0.4f) {
            Color c = get_bar_color(data[i], i, num_points, theme);
            set_color(sdl, c, 255);
            SDL_Rect dot = {x - 2, y - 2, 5, 5};
            SDL_RenderFillRect(sdl, &dot);
            set_color(sdl, theme->primary, 255);
        }
    }

    /* Bottom line */
    set_color(sdl, theme->secondary, 180);
    prev_x = (int)margin;
    prev_y = center_y;

    for (int i = 0; i < num_points; i++) {
        int x = (int)(margin + ((float)i / (num_points - 1)) * usable);
        int y = center_y + (int)(data[i] * h * 0.15f);
        if (i > 0) SDL_RenderDrawLine(sdl, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
}

static void draw_particle_mode(Renderer *r, SDL_Renderer *sdl, float *data,
                                int num_bands, ColorTheme *theme, int w, int h) {
    (void)sdl;  /* ← ADD THIS LINE */
    float band_width = (float)w / num_bands;

    for (int i = 0; i < num_bands; i++) {
        if (data[i] > 0.15f) {
            float x = (i + 0.5f) * band_width;
            int count = (int)(data[i] * 5);
            if (count < 1) count = 1;
            Color c = get_bar_color(data[i], i, num_bands, theme);
            particle_emit(r, x, (float)(h - 10), c, count,
                          band_width * 0.3f, data[i] * 8.0f);
        }
    }
}

static void draw_waterfall(Renderer *r, SDL_Renderer *sdl, float *data,
                           int num_bands, ColorTheme *theme, int w, int h) {
    /* Ensure texture exists */
    if (!r->waterfall_texture || r->waterfall_w != w || r->waterfall_h != h) {
        if (r->waterfall_texture) SDL_DestroyTexture(r->waterfall_texture);
        r->waterfall_texture = SDL_CreateTexture(sdl, SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET, w, h);
        r->waterfall_w = w;
        r->waterfall_h = h;

        /* Clear */
        SDL_SetRenderTarget(sdl, r->waterfall_texture);
        SDL_SetRenderDrawColor(sdl, 0, 0, 0, 255);
        SDL_RenderClear(sdl);
        SDL_SetRenderTarget(sdl, NULL);
    }

    /* Scroll up: copy texture shifted */
    SDL_Texture *temp = SDL_CreateTexture(sdl, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetRenderTarget(sdl, temp);
    SDL_SetRenderDrawColor(sdl, 0, 0, 0, 255);
    SDL_RenderClear(sdl);

    SDL_Rect src = {0, 1, w, h - 1};
    SDL_Rect dst = {0, 0, w, h - 1};
    SDL_RenderCopy(sdl, r->waterfall_texture, &src, &dst);

    /* Draw new line at bottom */
    float band_w = (float)w / num_bands;
    for (int i = 0; i < num_bands; i++) {
        float mag = data[i];
        Color c = color_lerp(theme->secondary, theme->primary, mag);
        float brightness = mag * 2.0f;
        if (brightness > 1.0f) brightness = 1.0f;
        c = color_brightness(c, brightness);

        SDL_SetRenderDrawColor(sdl, c.r, c.g, c.b, 255);
        SDL_Rect pixel = {(int)(i * band_w), h - 1, (int)(band_w) + 1, 1};
        SDL_RenderFillRect(sdl, &pixel);
    }

    SDL_SetRenderTarget(sdl, NULL);

    /* Swap */
    SDL_DestroyTexture(r->waterfall_texture);
    r->waterfall_texture = temp;

    /* Draw to screen */
    SDL_RenderCopy(sdl, r->waterfall_texture, NULL, NULL);
}

/* ── Main renderer functions ───────────────────────────────── */

void renderer_init(Renderer *r, Settings *settings) {
    memset(r, 0, sizeof(Renderer));
    r->settings = settings;
}

void renderer_destroy(Renderer *r, SDL_Renderer *sdl_renderer) {
    (void)sdl_renderer;
    if (r->trail_texture) SDL_DestroyTexture(r->trail_texture);
    if (r->waterfall_texture) SDL_DestroyTexture(r->waterfall_texture);
    memset(r, 0, sizeof(Renderer));
}

void renderer_render(Renderer *r, SDL_Renderer *sdl,
                     float *band_data, int num_bands,
                     float *waveform, int waveform_size,
                     int mode, ColorTheme *theme) {
    int w, h;
    SDL_GetRendererOutputSize(sdl, &w, &h);

    /* Clear background */
    Color bg = theme->background;

    if (r->settings->fade_trail && mode != 5 && mode != 6) {
        /* Fade effect: draw semi-transparent background */
        int alpha = (int)(r->settings->fade_speed * 3.5f);
        if (alpha < 15) alpha = 15;
        if (alpha > 255) alpha = 255;

        SDL_SetRenderDrawBlendMode(sdl, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sdl, bg.r, bg.g, bg.b, (uint8_t)alpha);
        SDL_Rect full = {0, 0, w, h};
        SDL_RenderFillRect(sdl, &full);
    } else {
        SDL_SetRenderDrawColor(sdl, bg.r, bg.g, bg.b, 255);
        SDL_RenderClear(sdl);
    }

    /* Render current mode */
    switch (mode) {
        case 0: /* Bars */
            draw_bars(sdl, band_data, num_bands, theme, w, h, false,
                      r->settings->bar_width_ratio);
            break;
        case 1: /* Bars Mirrored */
            draw_bars(sdl, band_data, num_bands, theme, w, h, true,
                      r->settings->bar_width_ratio);
            break;
        case 2: /* Circular */
            draw_circular(sdl, band_data, num_bands, theme, w, h);
            break;
        case 3: /* Wave */
            draw_wave(sdl, waveform, waveform_size, theme, w, h);
            break;
        case 4: /* Spectrum Line */
            draw_spectrum_line(sdl, band_data, num_bands, theme, w, h);
            break;
        case 5: /* Particles */
            SDL_SetRenderDrawColor(sdl, bg.r, bg.g, bg.b, 255);
            SDL_RenderClear(sdl);
            draw_particle_mode(r, sdl, band_data, num_bands, theme, w, h);
            break;
        case 6: /* Waterfall */
            draw_waterfall(r, sdl, band_data, num_bands, theme, w, h);
            break;
    }

    /* Beat-reactive particles */
    if (r->settings->particles_enabled && mode != 5) {
        if (r->has_prev) {
            for (int i = 0; i < num_bands; i++) {
                float diff = band_data[i] - r->prev_bands[i];
                if (diff > 0.3f) {
                    float x = ((float)i / num_bands) * w;
                    Color c = get_bar_color(band_data[i], i, num_bands, theme);
                    particle_emit(r, x, h * 0.5f, c, 2, 3.0f, 4.0f);
                }
            }
        }
    }

    /* Update and draw particles */
    float dt = 1.0f / (r->settings->fps_cap > 0 ? r->settings->fps_cap : 60);
    particles_update(r, dt);
    SDL_SetRenderDrawBlendMode(sdl, SDL_BLENDMODE_BLEND);
    particles_draw(r, sdl);

    /* Store previous bands */
    memcpy(r->prev_bands, band_data, sizeof(float) * num_bands);
    r->has_prev = true;
}