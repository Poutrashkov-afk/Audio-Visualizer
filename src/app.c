#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_device_change(int device_index, void *userdata) {
    App *app = (App *)userdata;
    audio_restart_device(&app->audio, device_index);
    printf("Switched to device %d\n", device_index);
}

bool app_init(App *app) {
    memset(app, 0, sizeof(App));

    /* Load or create settings */
    if (!settings_load(&app->settings, SETTINGS_FILE)) {
        settings_init_defaults(&app->settings);
    }

    /* Init SDL */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    /* Create window */
    uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (app->settings.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    app->window = SDL_CreateWindow(
        "Audio Visualizer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        app->settings.window_width, app->settings.window_height,
        flags
    );

    if (!app->window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return false;
    }

    app->sdl_renderer = SDL_CreateRenderer(
        app->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!app->sdl_renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawBlendMode(app->sdl_renderer, SDL_BLENDMODE_BLEND);

    /* Init audio */
    if (!audio_init(&app->audio)) {
        fprintf(stderr, "Audio init failed\n");
        return false;
    }

    /* Init FFT */
    fft_init(&app->fft, app->settings.chunk_size, app->settings.sample_rate);

    /* Buffers */
    app->audio_chunk = (float *)calloc(app->settings.chunk_size, sizeof(float));
    app->waveform = (float *)calloc(app->settings.chunk_size, sizeof(float));

    /* Init renderer */
    renderer_init(&app->renderer, &app->settings);

    /* Init UI */
    ui_init(&app->ui, &app->settings, &app->audio);
    app->ui.on_device_change = on_device_change;
    app->ui.callback_userdata = app;

    /* Timing */
    app->perf_freq = SDL_GetPerformanceFrequency();
    app->last_time = SDL_GetPerformanceCounter();

    app->show_help = true;
    app->help_timer = 5.0f;
    app->running = true;

    return true;
}

void app_destroy(App *app) {
    settings_save(&app->settings, SETTINGS_FILE);

    ui_destroy(&app->ui);
    renderer_destroy(&app->renderer, app->sdl_renderer);
    fft_destroy(&app->fft);
    audio_destroy(&app->audio);

    free(app->audio_chunk);
    free(app->waveform);

    if (app->sdl_renderer) SDL_DestroyRenderer(app->sdl_renderer);
    if (app->window) SDL_DestroyWindow(app->window);
    SDL_Quit();
}

static void toggle_fullscreen(App *app) {
    app->settings.fullscreen = !app->settings.fullscreen;
    SDL_SetWindowFullscreen(app->window,
        app->settings.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

static void handle_events(App *app) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            app->running = false;
            return;
        }

        /* UI handles events first */
        if (ui_handle_event(&app->ui, &event)) continue;

        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    if (app->ui.visible)
                        ui_toggle(&app->ui);
                    else if (app->settings.fullscreen)
                        toggle_fullscreen(app);
                    else
                        app->running = false;
                    break;

                case SDLK_F11:
                case SDLK_f:
                    if (!app->ui.visible) toggle_fullscreen(app);
                    break;

                case SDLK_TAB:
                    ui_toggle(&app->ui);
                    break;

                case SDLK_SPACE:
                case SDLK_RIGHT:
                    app->settings.visualization_mode =
                        (app->settings.visualization_mode + 1) % MAX_VIS_MODES;
                    break;

                case SDLK_LEFT:
                    app->settings.visualization_mode =
                        (app->settings.visualization_mode - 1 + MAX_VIS_MODES) % MAX_VIS_MODES;
                    break;

                case SDLK_UP:
                    app->settings.sensitivity += 0.1f;
                    if (app->settings.sensitivity > 5.0f) app->settings.sensitivity = 5.0f;
                    break;

                case SDLK_DOWN:
                    app->settings.sensitivity -= 0.1f;
                    if (app->settings.sensitivity < 0.1f) app->settings.sensitivity = 0.1f;
                    break;

                case SDLK_g:
                    app->settings.glow = !app->settings.glow;
                    break;

                case SDLK_p:
                    app->settings.particles_enabled = !app->settings.particles_enabled;
                    break;

                case SDLK_h:
                    app->show_help = !app->show_help;
                    app->help_timer = app->show_help ? 999.0f : 0.0f;
                    break;

                case SDLK_s:
                    if (event.key.keysym.mod & KMOD_CTRL) {
                        settings_save(&app->settings, SETTINGS_FILE);
                        printf("Settings saved\n");
                    }
                    break;

                default:
                    break;
            }
        }

        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                app->settings.window_width = event.window.data1;
                app->settings.window_height = event.window.data2;
            }
        }
    }
}

void app_run(App *app) {
    printf("\n========================================\n");
    printf("  Audio Visualizer (C/SDL2)\n");
    printf("========================================\n\n");

    /* Start audio */
    audio_start(&app->audio, app->settings.device_index,
                app->settings.sample_rate, app->settings.chunk_size);

    while (app->running) {
        /* Timing */
        uint64_t now = SDL_GetPerformanceCounter();
        app->frame_time = (float)(now - app->last_time) / (float)app->perf_freq;
        app->last_time = now;

        /* FPS smoothing */
        if (app->frame_time > 0.0f)
            app->fps = app->fps * 0.9f + (1.0f / app->frame_time) * 0.1f;

        /* Help timer */
        if (app->help_timer > 0.0f && app->help_timer < 999.0f)
            app->help_timer -= app->frame_time;

        /* Events */
        handle_events(app);

        /* Get audio data */
        audio_get_chunk(&app->audio, app->audio_chunk, app->settings.chunk_size);
        memcpy(app->waveform, app->audio_chunk, sizeof(float) * app->settings.chunk_size);

        /* Process FFT */
        fft_process(&app->fft, app->audio_chunk,
                    app->settings.bar_count,
                    app->settings.smoothing,
                    app->settings.sensitivity,
                    app->settings.min_frequency,
                    app->settings.max_frequency);

        /* Compute audio levels */
        memcpy(app->audio.current_chunk, app->audio_chunk,
               sizeof(float) * app->settings.chunk_size);
        audio_compute_levels(&app->audio);

        /* Render */
        ColorTheme *theme = settings_get_theme(&app->settings);

        renderer_render(&app->renderer, app->sdl_renderer,
                        app->fft.smoothed_bands, app->fft.band_count,
                        app->waveform, app->settings.chunk_size,
                        app->settings.visualization_mode, theme);

        /* HUD */
        int sw, sh;
        SDL_GetRendererOutputSize(app->sdl_renderer, &sw, &sh);

        ui_draw_hud(app->sdl_renderer, app->ui.font, app->ui.font_small,
                    &app->settings, app->fps, app->audio.rms_level,
                    app->audio.running, sw, sh);

        /* Settings panel */
        ui_draw(&app->ui, app->sdl_renderer, sw, sh);

        /* Present */
        SDL_RenderPresent(app->sdl_renderer);

        /* Frame cap */
        if (app->settings.fps_cap > 0) {
            float target = 1.0f / app->settings.fps_cap;
            float elapsed = (float)(SDL_GetPerformanceCounter() - now) / (float)app->perf_freq;
            if (elapsed < target) {
                SDL_Delay((uint32_t)((target - elapsed) * 1000.0f));
            }
        }
    }
}