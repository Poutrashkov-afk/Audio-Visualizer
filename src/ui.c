#include "ui.h"
#include <stdio.h>
#include <string.h>

/* ── Colors ──────────────────────────────────────────────── */
#define UI_BG_R 18
#define UI_BG_G 18
#define UI_BG_B 28

static void draw_text(SDL_Renderer *r, TTF_Font *font, const char *text,
                      int x, int y, SDL_Color color) {
    if (!text || !text[0] || !font) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void draw_text_right(SDL_Renderer *r, TTF_Font *font, const char *text,
                            int right_x, int y, SDL_Color color) {
    if (!text || !text[0] || !font) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = {right_x - surf->w, y, surf->w, surf->h};
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void draw_filled_rect(SDL_Renderer *r, int x, int y, int w, int h,
                              uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

/* ── Slider ──────────────────────────────────────────────── */

typedef struct {
    int x, y, w;
    const char *label;
    float min_val, max_val, value;
    bool is_int;
} SliderDef;

static int draw_slider(SDL_Renderer *r, TTF_Font *font, SliderDef *s, int idx,
                        int active, SDL_Point mouse, bool mouse_down) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color dim = {180, 180, 190, 255};

    draw_text(r, font, s->label, s->x, s->y, dim);

    char val_str[32];
    if (s->is_int)
        snprintf(val_str, sizeof(val_str), "%d", (int)s->value);
    else
        snprintf(val_str, sizeof(val_str), "%.2f", s->value);
    draw_text_right(r, font, val_str, s->x + s->w, s->y, white);

    int track_y = s->y + 20;
    int track_h = 10;
    draw_filled_rect(r, s->x, track_y, s->w, track_h, 50, 50, 72, 255);

    float ratio = (s->value - s->min_val) / (s->max_val - s->min_val);
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;
    int fill_w = (int)(s->w * ratio);
    draw_filled_rect(r, s->x, track_y, fill_w, track_h, 0, 160, 255, 255);

    int hx = s->x + fill_w;
    int hy = track_y + track_h / 2;
    int hr = (active == idx) ? 8 : 6;

    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_Rect handle = {hx - hr, hy - hr, hr * 2, hr * 2};
    SDL_RenderFillRect(r, &handle);

    SDL_Rect hit = {s->x, track_y - 8, s->w, track_h + 16};
    bool hovered = SDL_PointInRect(&mouse, &hit);

    if (mouse_down && (active == idx || (active < 0 && hovered))) {
        float new_ratio = (float)(mouse.x - s->x) / s->w;
        if (new_ratio < 0) new_ratio = 0;
        if (new_ratio > 1) new_ratio = 1;
        s->value = s->min_val + new_ratio * (s->max_val - s->min_val);
        if (s->is_int) s->value = (float)(int)(s->value + 0.5f);
        return idx;
    }

    return active;
}

/* ── UI Panel ────────────────────────────────────────────── */

void ui_init(UIPanel *ui, Settings *settings, AudioEngine *audio) {
    memset(ui, 0, sizeof(UIPanel));
    ui->settings = settings;
    ui->audio = audio;
    ui->panel_width = 370;
    ui->active_slider = -1;
    ui->active_dropdown = -1;
    ui->dropdown_scroll = 0;
    ui->device_click_consumed = false;
    ui->device_category = 1; /* Default to loopback (System Audio) */

    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return;
    }

    const char *font_paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        NULL
    };

    for (int i = 0; font_paths[i]; i++) {
        ui->font = TTF_OpenFont(font_paths[i], 14);
        if (ui->font) {
            ui->font_small = TTF_OpenFont(font_paths[i], 12);
            ui->font_title = TTF_OpenFont(font_paths[i], 18);
            printf("Loaded font: %s\n", font_paths[i]);
            break;
        }
    }

    if (!ui->font) {
        fprintf(stderr, "Warning: Could not load any font\n");
    }
}

void ui_destroy(UIPanel *ui) {
    if (ui->font) TTF_CloseFont(ui->font);
    if (ui->font_small) TTF_CloseFont(ui->font_small);
    if (ui->font_title) TTF_CloseFont(ui->font_title);
    TTF_Quit();
}

void ui_toggle(UIPanel *ui) {
    ui->visible = !ui->visible;
    ui->active_slider = -1;
    ui->active_dropdown = -1;
    ui->dropdown_scroll = 0;
    ui->device_click_consumed = false;
}

bool ui_handle_event(UIPanel *ui, SDL_Event *event) {
    if (!ui->visible) return false;

    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        ui->active_slider = -1;
        ui->device_click_consumed = false;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        SDL_Point mouse = {event->button.x, event->button.y};

        if (mouse.x < ui->panel_width) {
            return true;
        }

        ui->active_dropdown = -1;
    }

    if (event->type == SDL_MOUSEWHEEL) {
        SDL_Point mouse;
        SDL_GetMouseState(&mouse.x, &mouse.y);
        if (mouse.x < ui->panel_width) {
            if (ui->active_dropdown == 0) {
                ui->dropdown_scroll -= event->wheel.y;
                /* Total items needs to be computed dynamically in the wheel handler, but we will clamp it in the draw loop anyway */
                if (ui->dropdown_scroll < 0) ui->dropdown_scroll = 0;
            } else {
                ui->scroll_y += event->wheel.y * 25;
                if (ui->scroll_y > 0) ui->scroll_y = 0;
                int min_scroll = -(ui->content_height - 600);
                if (min_scroll > 0) min_scroll = 0;
                if (ui->scroll_y < min_scroll) ui->scroll_y = min_scroll;
            }
            return true;
        }
    }

    return false;
}

void ui_draw(UIPanel *ui, SDL_Renderer *renderer, int screen_w, int screen_h) {
    if (!ui->visible || !ui->font) return;

    (void)screen_w;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int pw = ui->panel_width;

    draw_filled_rect(renderer, 0, 0, pw, screen_h, UI_BG_R, UI_BG_G, UI_BG_B, 245);

    SDL_SetRenderDrawColor(renderer, 80, 130, 255, 255);
    SDL_RenderDrawLine(renderer, pw - 1, 0, pw - 1, screen_h);

    draw_filled_rect(renderer, 0, 0, pw, 52, 12, 12, 24, 255);
    SDL_SetRenderDrawColor(renderer, 80, 130, 255, 255);
    SDL_RenderDrawLine(renderer, 0, 52, pw, 52);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color dim = {160, 165, 180, 255};
    SDL_Color label_color = {120, 180, 255, 255};

    draw_text(renderer, ui->font_title, "Settings", 20, 15, white);
    draw_text(renderer, ui->font_small, "[Tab] close", pw - 90, 19, dim);

    SDL_Point mouse;
    uint32_t buttons = SDL_GetMouseState(&mouse.x, &mouse.y);
    bool mouse_down = (buttons & SDL_BUTTON_LMASK) != 0;

    bool fresh_click = false;
    if (mouse_down && !ui->was_mouse_down) {
        fresh_click = true;
    }
    ui->was_mouse_down = mouse_down;

    int x = 20;
    int y = 70 + ui->scroll_y;
    int w = pw - 40;
    int content_start_y = y;

    /* ═══ AUDIO SOURCE ═══ */
    draw_text(renderer, ui->font_small, "AUDIO SOURCE", x, y, label_color);
    y += 20;

    /* CATEGORY TABS [Inputs] [Loopbacks] */
    int cat_w = (w - 10) / 2;
    SDL_Rect in_tab = {x, y, cat_w, 28};
    SDL_Rect out_tab = {x + cat_w + 10, y, cat_w, 28};

    bool in_hover = SDL_PointInRect(&mouse, &in_tab);
    bool out_hover = SDL_PointInRect(&mouse, &out_tab);

    bool is_in = (ui->device_category == 0);

    /* Inputs Tab */
    draw_filled_rect(renderer, in_tab.x, in_tab.y, in_tab.w, in_tab.h,
                     is_in ? 0 : (in_hover ? 50 : 30),
                     is_in ? 140 : (in_hover ? 50 : 30),
                     is_in ? 100 : (in_hover ? 70 : 48), 255);
    SDL_SetRenderDrawColor(renderer, is_in ? 0 : 60, is_in ? 220 : 65, is_in ? 150 : 90, 255);
    SDL_RenderDrawRect(renderer, &in_tab);
    draw_text(renderer, ui->font_small, is_in ? " Microphones" : " Microphones", in_tab.x + 10, in_tab.y + 7, white);

    /* Loopbacks Tab */
    draw_filled_rect(renderer, out_tab.x, out_tab.y, out_tab.w, out_tab.h,
                     !is_in ? 140 : (out_hover ? 50 : 30),
                     !is_in ? 100 : (out_hover ? 50 : 30),
                     !is_in ? 0 : (out_hover ? 70 : 48), 255);
    SDL_SetRenderDrawColor(renderer, !is_in ? 220 : 60, !is_in ? 160 : 65, !is_in ? 0 : 90, 255);
    SDL_RenderDrawRect(renderer, &out_tab);
    draw_text(renderer, ui->font_small, !is_in ? " System Audio" : " System Audio", out_tab.x + 10, out_tab.y + 7, white);

    if (fresh_click) {
        if (in_hover && !is_in) { ui->device_category = 0; ui->active_dropdown = -1; }
        if (out_hover && is_in) { ui->device_category = 1; ui->active_dropdown = -1; }
    }
    y += 36;

    draw_text(renderer, ui->font_small, is_in ? "Microphone Device:" : "System Device:", x, y, dim);
    y += 18;

    /* Pre-compute filtered devices based on current category tab */
    int filtered_indices[MAX_DEVICES];
    int filtered_count = 0;

    for (int i = 0; i < ui->audio->device_count; i++) {
        bool is_loop = ui->audio->devices[i].is_loopback;
        if ((is_in && !is_loop) || (!is_in && is_loop)) {
            filtered_indices[filtered_count++] = i;
        }
    }

    /* Show current device */
    const char *current_name = "System Default";
    for (int i = 0; i < ui->audio->device_count; i++) {
        if (ui->audio->devices[i].index == ui->settings->device_index) {
            current_name = ui->audio->devices[i].name;
            break;
        }
    }

    /* Selector box */
    SDL_Rect dev_box = {x, y, w, 28};
    bool dev_hover = SDL_PointInRect(&mouse, &dev_box);
    draw_filled_rect(renderer, x, y, w, 28,
                     dev_hover ? 45 : 30, dev_hover ? 45 : 30,
                     dev_hover ? 70 : 48, 255);
    SDL_SetRenderDrawColor(renderer,
                           ui->active_dropdown == 0 ? 0 : 60,
                           ui->active_dropdown == 0 ? 140 : 65,
                           ui->active_dropdown == 0 ? 255 : 90, 255);
    SDL_RenderDrawRect(renderer, &dev_box);

    char dev_display[48];
    strncpy(dev_display, current_name, 44);
    dev_display[44] = '\0';
    if (strlen(current_name) > 44) strcat(dev_display, "...");
    draw_text(renderer, ui->font_small, dev_display, x + 8, y + 6, white);

    const char *arrow = ui->active_dropdown == 0 ? "^" : "v";
    draw_text_right(renderer, ui->font_small, arrow, x + w - 4, y + 6, dim);

    if (fresh_click && dev_hover) {
        if (ui->active_dropdown == 0) {
            ui->active_dropdown = -1;
        } else {
            ui->active_dropdown = 0;
            ui->dropdown_scroll = 0;
            ui->device_click_consumed = true;
        }
    }

    int dev_box_bottom = y + 32;
    y = dev_box_bottom;

    if (ui->active_dropdown == 0) {
        int dd_y = dev_box_bottom;
        int item_h = 28;
        int total_items = filtered_count + 1;
        int max_visible = 10;
        if (max_visible > total_items) max_visible = total_items;

        int max_scroll = total_items - max_visible;
        if (max_scroll < 0) max_scroll = 0;
        if (ui->dropdown_scroll < 0) ui->dropdown_scroll = 0;
        if (ui->dropdown_scroll > max_scroll) ui->dropdown_scroll = max_scroll;

        int dd_height = max_visible * item_h;

        draw_filled_rect(renderer, x, dd_y, w, dd_height, 18, 18, 32, 255);

        for (int vi = 0; vi < max_visible; vi++) {
            int actual_idx = vi + ui->dropdown_scroll;
            if (actual_idx >= total_items) break;

            int iy = dd_y + vi * item_h;
            SDL_Rect opt = {x, iy, w, item_h};
            bool opt_hover = SDL_PointInRect(&mouse, &opt);

            bool is_sel;
            const char *item_name;
            int device_idx;

            if (actual_idx == 0) {
                is_sel = (ui->settings->device_index == -1);
                item_name = "System Default";
                device_idx = -1;
            } else {
                int list_idx = filtered_indices[actual_idx - 1];
                AudioDeviceInfo *dev = &ui->audio->devices[list_idx];
                is_sel = (dev->index == ui->settings->device_index);
                item_name = dev->name;
                device_idx = dev->index;
            }

            if (is_sel) {
                draw_filled_rect(renderer, x + 1, iy, w - 2, item_h, 0, 70, 140, 255);
            } else if (opt_hover) {
                draw_filled_rect(renderer, x + 1, iy, w - 2, item_h, 40, 45, 70, 255);
            }

            if (vi > 0) {
                SDL_SetRenderDrawColor(renderer, 45, 45, 65, 255);
                SDL_RenderDrawLine(renderer, x + 4, iy, x + w - 4, iy);
            }

            char name_buf[38];
            strncpy(name_buf, item_name, 34);
            name_buf[34] = '\0';
            if (strlen(item_name) > 34) strcat(name_buf, "...");

            SDL_Color tc = (is_sel || opt_hover) ? white : dim;
            char prefix_name[42];
            snprintf(prefix_name, sizeof(prefix_name), "%s%s",
                     is_sel ? "> " : "  ", name_buf);
            draw_text(renderer, ui->font_small, prefix_name, x + 4, iy + 6, tc);

            if (fresh_click && opt_hover && !ui->device_click_consumed && !is_sel) {
                ui->settings->device_index = device_idx;
                ui->active_dropdown = -1;
                if (ui->on_device_change) {
                    ui->on_device_change(device_idx, ui->callback_userdata);
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 140, 255, 255);
        SDL_Rect dd_border = {x, dd_y, w, dd_height};
        SDL_RenderDrawRect(renderer, &dd_border);

        if (ui->dropdown_scroll > 0) {
            draw_filled_rect(renderer, x + w - 55, dd_y + 2, 52, 14, 18, 18, 32, 255);
            SDL_Color scroll_col = {100, 150, 255, 255};
            draw_text_right(renderer, ui->font_small, "^ more", x + w - 2, dd_y + 1, scroll_col);
        }
        if (ui->dropdown_scroll < max_scroll) {
            int bottom = dd_y + dd_height;
            draw_filled_rect(renderer, x + w - 55, bottom - 16, 52, 14, 18, 18, 32, 255);
            SDL_Color scroll_col = {100, 150, 255, 255};
            draw_text_right(renderer, ui->font_small, "v more", x + w - 2, bottom - 15, scroll_col);
        }

        ui->content_height = 700;
        return;
    }

    y += 10;

    /* ═══ VISUALIZATION ═══ */
    SDL_SetRenderDrawColor(renderer, 60, 65, 90, 255);
    SDL_RenderDrawLine(renderer, x, y + 6, x + w, y + 6);
    draw_text(renderer, ui->font_small, "VISUALIZATION", x, y - 4, label_color);
    y += 20;

    draw_text(renderer, ui->font_small, "Mode:", x, y, dim);
    y += 18;

    SDL_Rect mode_box = {x, y, w, 28};
    draw_filled_rect(renderer, x, y, w, 28, 30, 30, 48, 255);
    SDL_SetRenderDrawColor(renderer, 60, 65, 90, 255);
    SDL_RenderDrawRect(renderer, &mode_box);

    const char *mode_name = VIS_MODE_NAMES[ui->settings->visualization_mode];
    draw_text(renderer, ui->font_small, mode_name, x + 30, y + 6, white);

    SDL_Rect left_btn = {x + 2, y + 2, 24, 24};
    bool left_hover = SDL_PointInRect(&mouse, &left_btn);
    draw_filled_rect(renderer, left_btn.x, left_btn.y, 24, 24,
                     left_hover ? 80 : 50, left_hover ? 80 : 50,
                     left_hover ? 110 : 70, 255);
    draw_text(renderer, ui->font_small, "<", x + 9, y + 6, white);

    if (fresh_click && left_hover) {
        ui->settings->visualization_mode--;
        if (ui->settings->visualization_mode < 0)
            ui->settings->visualization_mode = MAX_VIS_MODES - 1;
    }

    SDL_Rect right_btn = {x + w - 26, y + 2, 24, 24};
    bool right_hover = SDL_PointInRect(&mouse, &right_btn);
    draw_filled_rect(renderer, right_btn.x, right_btn.y, 24, 24,
                     right_hover ? 80 : 50, right_hover ? 80 : 50,
                     right_hover ? 110 : 70, 255);
    draw_text(renderer, ui->font_small, ">", x + w - 19, y + 6, white);

    if (fresh_click && right_hover) {
        ui->settings->visualization_mode++;
        if (ui->settings->visualization_mode >= MAX_VIS_MODES)
            ui->settings->visualization_mode = 0;
    }

    y += 36;

    draw_text(renderer, ui->font_small, "Theme:", x, y, dim);
    y += 18;

    SDL_Rect theme_box = {x, y, w, 28};
    draw_filled_rect(renderer, x, y, w, 28, 30, 30, 48, 255);
    SDL_SetRenderDrawColor(renderer, 60, 65, 90, 255);
    SDL_RenderDrawRect(renderer, &theme_box);

    draw_text(renderer, ui->font_small, THEMES[ui->settings->theme_index].name,
              x + 30, y + 6, white);

    SDL_Rect tl = {x + 2, y + 2, 24, 24};
    bool tl_hover = SDL_PointInRect(&mouse, &tl);
    draw_filled_rect(renderer, tl.x, tl.y, 24, 24,
                     tl_hover ? 80 : 50, tl_hover ? 80 : 50,
                     tl_hover ? 110 : 70, 255);
    draw_text(renderer, ui->font_small, "<", x + 9, y + 6, white);

    if (fresh_click && tl_hover) {
        ui->settings->theme_index--;
        if (ui->settings->theme_index < 0) ui->settings->theme_index = NUM_THEMES - 1;
    }

    SDL_Rect tr = {x + w - 26, y + 2, 24, 24};
    bool tr_hover = SDL_PointInRect(&mouse, &tr);
    draw_filled_rect(renderer, tr.x, tr.y, 24, 24,
                     tr_hover ? 80 : 50, tr_hover ? 80 : 50,
                     tr_hover ? 110 : 70, 255);
    draw_text(renderer, ui->font_small, ">", x + w - 19, y + 6, white);

    if (fresh_click && tr_hover) {
        ui->settings->theme_index++;
        if (ui->settings->theme_index >= NUM_THEMES) ui->settings->theme_index = 0;
    }

    y += 44;

    SDL_SetRenderDrawColor(renderer, 60, 65, 90, 255);
    SDL_RenderDrawLine(renderer, x, y + 6, x + w, y + 6);
    draw_text(renderer, ui->font_small, "AUDIO PROCESSING", x, y - 4, label_color);
    y += 22;

    SliderDef sliders[] = {
        {x, y,       w, "Bar Count",   8,    256,   (float)ui->settings->bar_count,    true},
        {x, y + 44,  w, "Smoothing",   0,    0.95f, ui->settings->smoothing,           false},
        {x, y + 88,  w, "Sensitivity", 0.1f, 5.0f,  ui->settings->sensitivity,         false},
        {x, y + 132, w, "Min Freq",    20,   2000,  (float)ui->settings->min_frequency, true},
        {x, y + 176, w, "Max Freq",    2000, 20000, (float)ui->settings->max_frequency, true},
        {x, y + 220, w, "Fade Speed",  1,    100,   (float)ui->settings->fade_speed,    true},
        {x, y + 264, w, "Bar Width",   0.2f, 1.0f,  ui->settings->bar_width_ratio,      false},
    };

    int num_sliders = 7;
    for (int i = 0; i < num_sliders; i++) {
        ui->active_slider = draw_slider(renderer, ui->font_small, &sliders[i],
                                         i, ui->active_slider, mouse, mouse_down);
    }

    ui->settings->bar_count = (int)sliders[0].value;
    ui->settings->smoothing = sliders[1].value;
    ui->settings->sensitivity = sliders[2].value;
    ui->settings->min_frequency = (int)sliders[3].value;
    ui->settings->max_frequency = (int)sliders[4].value;
    ui->settings->fade_speed = (int)sliders[5].value;
    ui->settings->bar_width_ratio = sliders[6].value;

    y += 310;

    SDL_SetRenderDrawColor(renderer, 60, 65, 90, 255);
    SDL_RenderDrawLine(renderer, x, y + 6, x + w, y + 6);
    draw_text(renderer, ui->font_small, "EFFECTS", x, y - 4, label_color);
    y += 22;

    int btn_w = (w - 10) / 2;
    int btn_h = 30;

    struct { const char *label; bool *value; } toggles[] = {
        {"Glow",       &ui->settings->glow},
        {"Fade Trail", &ui->settings->fade_trail},
        {"Particles",  &ui->settings->particles_enabled},
        {"Show FPS",   &ui->settings->show_fps},
    };

    for (int i = 0; i < 4; i++) {
        int bx = x + (i % 2) * (btn_w + 10);
        int by = y + (i / 2) * (btn_h + 6);

        SDL_Rect btn = {bx, by, btn_w, btn_h};
        bool hover = SDL_PointInRect(&mouse, &btn);

        bool on = *toggles[i].value;
        if (on) {
            draw_filled_rect(renderer, bx, by, btn_w, btn_h, 0, 140, 100, 255);
            SDL_SetRenderDrawColor(renderer, 0, 220, 150, 255);
        } else {
            draw_filled_rect(renderer, bx, by, btn_w, btn_h,
                             hover ? 55 : 50, hover ? 55 : 50, hover ? 80 : 70, 255);
            SDL_SetRenderDrawColor(renderer, 60, 65, 90, 255);
        }
        SDL_RenderDrawRect(renderer, &btn);

        char label[48];
        snprintf(label, sizeof(label), "%s %s", on ? "+" : "o", toggles[i].label);
        SDL_Color tc = on ? white : dim;
        draw_text(renderer, ui->font_small, label, bx + 8, by + 7, tc);

        if (fresh_click && hover) {
            *toggles[i].value = !on;
        }
    }

    y += btn_h * 2 + 20;

    SDL_SetRenderDrawColor(renderer, 60, 65, 90, 255);
    SDL_RenderDrawLine(renderer, x, y + 6, x + w, y + 6);
    draw_text(renderer, ui->font_small, "PRESETS", x, y - 4, label_color);
    y += 22;

    SDL_Rect save_btn = {x, y, btn_w, 32};
    bool save_hover = SDL_PointInRect(&mouse, &save_btn);
    draw_filled_rect(renderer, x, y, btn_w, 32,
                     save_hover ? 0 : 0, save_hover ? 180 : 140,
                     save_hover ? 100 : 80, 255);
    SDL_SetRenderDrawColor(renderer, 0, 220, 120, 255);
    SDL_RenderDrawRect(renderer, &save_btn);
    draw_text(renderer, ui->font_small, "Save", x + btn_w / 2 - 15, y + 8, white);
    if (fresh_click && save_hover) {
        settings_save(ui->settings, SETTINGS_FILE);
        printf("Settings saved!\n");
    }

    SDL_Rect load_btn = {x + btn_w + 10, y, btn_w, 32};
    bool load_hover = SDL_PointInRect(&mouse, &load_btn);
    draw_filled_rect(renderer, load_btn.x, y, btn_w, 32,
                     load_hover ? 180 : 140, load_hover ? 130 : 100,
                     0, 255);
    SDL_SetRenderDrawColor(renderer, 220, 160, 0, 255);
    SDL_RenderDrawRect(renderer, &load_btn);
    draw_text(renderer, ui->font_small, "Load", load_btn.x + btn_w / 2 - 15, y + 8, white);
    if (fresh_click && load_hover) {
        settings_load(ui->settings, SETTINGS_FILE);
        printf("Settings loaded!\n");
    }

    y += 50;

    ui->content_height = y - content_start_y - ui->scroll_y;

    int visible_height = screen_h - 60;
    int min_scroll = -(ui->content_height - visible_height);
    if (min_scroll > 0) min_scroll = 0;
    if (ui->scroll_y < min_scroll) ui->scroll_y = min_scroll;
    if (ui->scroll_y > 0) ui->scroll_y = 0;
}
/* ── HUD ─────────────────────────────────────────────────── */

void ui_draw_hud(SDL_Renderer *renderer, TTF_Font *font, TTF_Font *font_small,
                 Settings *settings, float fps, float rms_level, bool audio_running,
                 int screen_w, int screen_h) {
    if (!font) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (settings->show_fps) {
        char fps_str[32];
        snprintf(fps_str, sizeof(fps_str), "FPS: %.0f", fps);
        SDL_Color fps_color;
        if (fps > 50) fps_color = (SDL_Color){100, 255, 100, 255};
        else if (fps > 30) fps_color = (SDL_Color){255, 255, 0, 255};
        else fps_color = (SDL_Color){255, 80, 80, 255};
        draw_text_right(renderer, font_small, fps_str, screen_w - 10, 10, fps_color);
    }

    ColorTheme *theme = settings_get_theme(settings);
    const char *mode_name = VIS_MODE_NAMES[settings->visualization_mode];
    SDL_Color mode_color = {theme->primary.r, theme->primary.g, theme->primary.b, 255};

    draw_filled_rect(renderer, screen_w - 160, screen_h - 35, 150, 25, 0, 0, 0, 160);
    draw_text_right(renderer, font, mode_name, screen_w - 15, screen_h - 32, mode_color);

    if (audio_running) {
        int bar_w = 100;
        int bar_x = screen_w - bar_w - 15;
        int bar_y = screen_h - 48;

        draw_filled_rect(renderer, bar_x, bar_y, bar_w, 4, 40, 40, 60, 255);
        int fill_w = (int)(bar_w * (rms_level * 10.0f));
        if (fill_w > bar_w) fill_w = bar_w;
        if (fill_w > 0) {
            uint8_t r, g, b;
            if (rms_level < 0.08f) { r = 0; g = 200; b = 100; }
            else if (rms_level < 0.15f) { r = 255; g = 200; b = 0; }
            else { r = 255; g = 50; b = 50; }
            draw_filled_rect(renderer, bar_x, bar_y, fill_w, 4, r, g, b, 255);
        }
    }
}