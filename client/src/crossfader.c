/* Crossfader widget implementation */
#include "../lib/all.h"

static Crossfader* crossfaders[MAX_CROSSFADERS];
static int crossfader_count = 0;

static const char* player_key_for_crossfader_id(int id) {
    if (id == CROSSFADER_MUSIC_VOLUME) return "MUSIC_VOLUME";
    if (id == CROSSFADER_SFX_VOLUME) return "SOUND_EFFECTS_VOLUME";
    if (id == CROSSFADER_LUMINOSITY) return "LUMINOSITY";
    return NULL;
}

static void crossfader_load_saved_value_if_enabled(Crossfader* cf) {
    if (!cf || !cf->cfg || !cf->cfg->save_player_data) return;

    const char* key = player_key_for_crossfader_id(cf->id);
    if (!key) return;

    char buf[256] = {0};
    if (read_property(buf, sizeof(buf), key) == EXIT_SUCCESS) {
        cf->cfg->value = atoi(buf);
    }
}

static void crossfader_save_value_if_enabled(const Crossfader* cf) {
    if (!cf || !cf->cfg || !cf->cfg->save_player_data) return;

    const char* key = player_key_for_crossfader_id(cf->id);
    if (!key) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", cf->cfg->value);
    write_property(key, buf);
}

/** Initialise une CrossfaderConfig avec des valeurs par défaut. */
CrossfaderConfig* crossfader_config_init(void) {
    CrossfaderConfig* cfg = (CrossfaderConfig*)calloc(1, sizeof(CrossfaderConfig));
    if (!cfg) return NULL;

    cfg->x = 0;
    cfg->y = 0;
    cfg->w = 100;
    cfg->h = 20;
    cfg->min = 0;
    cfg->max = 100;
    cfg->value = 50;
    cfg->hidden = 0;
    cfg->save_player_data = 0;
    cfg->color_0_pct = (SDL_Color){120, 120, 120, 200};
    cfg->color_100_pct = (SDL_Color){200, 200, 200, 220};
    cfg->knob_color = (SDL_Color){200, 200, 200, 255};
    cfg->on_change = NULL;

    cfg->rect = (SDL_Rect){0, 0, 0, 0};
    cfg->dragging = 0;
    cfg->save_pending = 0;
    cfg->hover = 0;
    cfg->track_texture = NULL;
    cfg->knob_texture = NULL;

    return cfg;
}

void crossfaders_init(void) {
    for (int i = 0; i < MAX_CROSSFADERS; ++i) crossfaders[i] = NULL;
    crossfader_count = 0;
}

Crossfader* crossfader_create(SDL_Renderer* renderer, int id, const CrossfaderConfig* cfg_in) {
    (void)renderer;
    if (crossfader_count >= MAX_CROSSFADERS) return NULL;

    Crossfader* cf = (Crossfader*)malloc(sizeof(Crossfader));
    if (!cf) return NULL;
    cf->id = id;

    /* Crossfader owns its config. We copy cfg_in (or defaults) into a freshly allocated struct. */
    cf->cfg = crossfader_config_init();
    if (!cf->cfg) {
        free(cf);
        return NULL;
    }
    if (cfg_in) {
        /* Shallow copy first, then fix runtime fields below. */
        *cf->cfg = *cfg_in;
    }

    if (cf->cfg->min >= cf->cfg->max) {
        free(cf->cfg);
        free(cf);
        return NULL;
    }

    /* runtime-managed fields: always start from a clean state */
    cf->cfg->track_texture = NULL;
    cf->cfg->knob_texture = NULL;
    cf->cfg->dragging = 0;
    cf->cfg->save_pending = 0;
    cf->cfg->hover = 0;

    /* optional persisted value */
    crossfader_load_saved_value_if_enabled(cf);

    /* clamp value */
    if (cf->cfg->value < cf->cfg->min) cf->cfg->value = cf->cfg->min;
    if (cf->cfg->value > cf->cfg->max) cf->cfg->value = cf->cfg->max;

    /* keep rect in sync */
    cf->cfg->rect = (SDL_Rect){cf->cfg->x, cf->cfg->y, cf->cfg->w, cf->cfg->h};

    crossfaders[crossfader_count++] = cf;
    return cf;
}

Crossfader* crossfader_get_by_id(int id) {
    for (int i = 0; i < crossfader_count; ++i) {
        if (crossfaders[i] && crossfaders[i]->id == id) return crossfaders[i];
    }
    return NULL;
}

int crossfader_get_value(int id) {
    Crossfader* cf = crossfader_get_by_id(id);
    if (!cf) return 0;
    return cf->cfg->value;
}

int crossfader_set_value(int id, int value) {
    Crossfader* cf = crossfader_get_by_id(id);
    if (!cf) return EXIT_FAILURE;
    if (value < cf->cfg->min) value = cf->cfg->min;
    if (value > cf->cfg->max) value = cf->cfg->max;
    if (cf->cfg->value != value) {
        cf->cfg->value = value;
        if (cf->cfg->on_change) cf->cfg->on_change(NULL, cf->cfg->value);
        if (cf->cfg->save_player_data) {
            if (cf->cfg->dragging) {
                /* defer until release */
                cf->cfg->save_pending = 1;
            } else {
                crossfader_save_value_if_enabled(cf);
            }
        }
    }
    return EXIT_SUCCESS;
}

void crossfader_set_on_change(Crossfader* cf, void (*cb)(AppContext*, int)) {
    if (!cf) return;
    cf->cfg->on_change = cb;
}

int edit_crfd_cfg(int id, CrfdCfgKey key, intptr_t value) {
    Crossfader* cf = crossfader_get_by_id(id);
    if (!cf || !cf->cfg) return EXIT_FAILURE;

    switch (key) {
        case CRFD_CFG_X:
            cf->cfg->x = (int)value;
            cf->cfg->rect.x = (int)value;
            return EXIT_SUCCESS;
        case CRFD_CFG_Y:
            cf->cfg->y = (int)value;
            cf->cfg->rect.y = (int)value;
            return EXIT_SUCCESS;
        case CRFD_CFG_W:
            cf->cfg->w = (int)value;
            cf->cfg->rect.w = (int)value;
            return EXIT_SUCCESS;
        case CRFD_CFG_H:
            cf->cfg->h = (int)value;
            cf->cfg->rect.h = (int)value;
            return EXIT_SUCCESS;
        case CRFD_CFG_MIN: {
            int new_min = (int)value;
            if (new_min >= cf->cfg->max) return EXIT_FAILURE;
            cf->cfg->min = new_min;
            if (cf->cfg->value < cf->cfg->min) return crossfader_set_value(id, cf->cfg->min);
            return EXIT_SUCCESS;
        }
        case CRFD_CFG_MAX: {
            int new_max = (int)value;
            if (new_max <= cf->cfg->min) return EXIT_FAILURE;
            cf->cfg->max = new_max;
            if (cf->cfg->value > cf->cfg->max) return crossfader_set_value(id, cf->cfg->max);
            return EXIT_SUCCESS;
        }
        case CRFD_CFG_VALUE:
            return crossfader_set_value(id, (int)value);
        case CRFD_CFG_HIDDEN:
            cf->cfg->hidden = ((int)value != 0);
            return EXIT_SUCCESS;
        case CRFD_CFG_SAVE_PLAYER_DATA:
            cf->cfg->save_player_data = ((int)value != 0);
            return EXIT_SUCCESS;
        case CRFD_CFG_COLOR_0_PCT:
            if (!value) return EXIT_FAILURE;
            cf->cfg->color_0_pct = *(const SDL_Color*)value;
            return EXIT_SUCCESS;
        case CRFD_CFG_COLOR_100_PCT:
            if (!value) return EXIT_FAILURE;
            cf->cfg->color_100_pct = *(const SDL_Color*)value;
            return EXIT_SUCCESS;
        case CRFD_CFG_KNOB_COLOR:
            if (!value) return EXIT_FAILURE;
            cf->cfg->knob_color = *(const SDL_Color*)value;
            return EXIT_SUCCESS;
        case CRFD_CFG_ON_CHANGE:
            if (!value) {
                cf->cfg->on_change = NULL;
                return EXIT_SUCCESS;
            }
            cf->cfg->on_change = *(void (**)(AppContext*, int))value;
            return EXIT_SUCCESS;
        case CRFD_CFG_RECT:
            if (!value) return EXIT_FAILURE;
            cf->cfg->rect = *(const SDL_Rect*)value;
            cf->cfg->x = cf->cfg->rect.x;
            cf->cfg->y = cf->cfg->rect.y;
            cf->cfg->w = cf->cfg->rect.w;
            cf->cfg->h = cf->cfg->rect.h;
            return EXIT_SUCCESS;
        case CRFD_CFG_DRAGGING:
            cf->cfg->dragging = ((int)value != 0);
            return EXIT_SUCCESS;
        case CRFD_CFG_SAVE_PENDING:
            cf->cfg->save_pending = ((int)value != 0);
            return EXIT_SUCCESS;
        case CRFD_CFG_HOVER:
            cf->cfg->hover = ((int)value != 0);
            return EXIT_SUCCESS;
        case CRFD_CFG_TRACK_TEXTURE:
            cf->cfg->track_texture = (SDL_Texture*)value;
            return EXIT_SUCCESS;
        case CRFD_CFG_KNOB_TEXTURE:
            cf->cfg->knob_texture = (SDL_Texture*)value;
            return EXIT_SUCCESS;
        default:
            return EXIT_FAILURE;
    }
}

static int point_in_rect(int x, int y, SDL_Rect* r) {
    return x >= r->x && x <= (r->x + r->w) && y >= r->y && y <= (r->y + r->h);
}

/* Compute pixel X of knob center based on value */
static int knob_x_for_value(Crossfader* cf, int knob_w) {
    if (!cf) return 0;
    int range = cf->cfg->max - cf->cfg->min;
    if (range <= 0) return cf->cfg->rect.x + knob_w/2;
    double t = (double)(cf->cfg->value - cf->cfg->min) / (double)range;
    int span = cf->cfg->rect.w - knob_w;
    if (span < 0) span = 0;
    return cf->cfg->rect.x + (int)(t * span) + knob_w/2;
}

static double value_percent_for_crossfader(const Crossfader* cf) {
    int range = cf->cfg->max - cf->cfg->min;
    if (range <= 0) return 0.0;
    double t = (double)(cf->cfg->value - cf->cfg->min) / (double)range;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return t;
}

static SDL_Color lerp_color(SDL_Color c0, SDL_Color c1, double t) {
    SDL_Color out;
    out.r = (Uint8)round((1.0 - t) * c0.r + t * c1.r);
    out.g = (Uint8)round((1.0 - t) * c0.g + t * c1.g);
    out.b = (Uint8)round((1.0 - t) * c0.b + t * c1.b);
    out.a = (Uint8)round((1.0 - t) * c0.a + t * c1.a);
    return out;
}

static SDL_Color brighten_color(SDL_Color c, int delta) {
    int r = (int)c.r + delta;
    int g = (int)c.g + delta;
    int b = (int)c.b + delta;
    SDL_Color out = {
        (Uint8)((r > 255) ? 255 : r),
        (Uint8)((g > 255) ? 255 : g),
        (Uint8)((b > 255) ? 255 : b),
        c.a
    };
    return out;
}

/* Update value from pixel x coordinate */
static void update_value_from_mouse(Crossfader* cf, AppContext* ctx, int mouse_x) {
    int knob_w = cf->cfg->rect.h; /* use height as knob size */
    int left = cf->cfg->rect.x + knob_w/2;
    int right = cf->cfg->rect.x + cf->cfg->rect.w - knob_w/2;
    if (mouse_x < left) mouse_x = left;
    if (mouse_x > right) mouse_x = right;
    double t = (double)(mouse_x - left) / (double)(right - left);
    int newv = cf->cfg->min + (int)round(t * (cf->cfg->max - cf->cfg->min));
    if (newv < cf->cfg->min) newv = cf->cfg->min;
    if (newv > cf->cfg->max) newv = cf->cfg->max;
    if (newv != cf->cfg->value) {
        cf->cfg->value = newv;
        if (cf->cfg->on_change) cf->cfg->on_change(ctx, cf->cfg->value);
    /* defer write until release to avoid frequent file writes */
    if (cf->cfg->save_player_data) cf->cfg->save_pending = 1;
    }
}

void crossfaders_handle_event(AppContext* ctx, SDL_Event* event) {
    if (!event) return;
    if (event->type == SDL_MOUSEMOTION) {
        int mx = event->motion.x;
        int my = event->motion.y;
        for (int i = 0; i < crossfader_count; ++i) {
            Crossfader* cf = crossfaders[i];
            if (!cf || cf->cfg->hidden) continue;
            cf->cfg->hover = point_in_rect(mx, my, &cf->cfg->rect);
            if (cf->cfg->dragging) update_value_from_mouse(cf, ctx, mx);
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        for (int i = 0; i < crossfader_count; ++i) {
            Crossfader* cf = crossfaders[i];
            if (!cf || cf->cfg->hidden) continue;
            if (point_in_rect(mx, my, &cf->cfg->rect)) {
                cf->cfg->dragging = 1;
                update_value_from_mouse(cf, ctx, mx);
            }
        }
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        for (int i = 0; i < crossfader_count; ++i) {
            Crossfader* cf = crossfaders[i];
            if (!cf || !cf->cfg) continue;
            cf->cfg->dragging = 0;
            if (cf->cfg->save_player_data && cf->cfg->save_pending) {
                crossfader_save_value_if_enabled(cf);
                cf->cfg->save_pending = 0;
            }
        }
    }
}

/* --- Shape helpers -------------------------------------------------------- */

static void render_filled_circle(SDL_Renderer* renderer, int cx, int cy, int r) {
    if (r <= 0) return;
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrt((double)(r * r - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void render_circle_outline(SDL_Renderer* renderer, int cx, int cy, int r) {
    if (r <= 0) return;
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrt((double)(r * r - dy * dy));
        SDL_RenderDrawPoint(renderer, cx - dx, cy + dy);
        SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
    }
}

static void render_filled_half_circle(SDL_Renderer* renderer, int cx, int cy, int r, int is_left) {
    if (r <= 0) return;
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrt((double)(r * r - dy * dy));
        if (is_left) {
            SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx, cy + dy);
        } else {
            SDL_RenderDrawLine(renderer, cx, cy + dy, cx + dx, cy + dy);
        }
    }
}

static void render_half_circle_outline(SDL_Renderer* renderer, int cx, int cy, int r, int is_left) {
    if (r <= 0) return;
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrt((double)(r * r - dy * dy));
        if (is_left) {
            SDL_RenderDrawPoint(renderer, cx - dx, cy + dy);
        } else {
            SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
        }
    }
}

/* Full pill (rounded both ends) */
static void render_filled_capsule(SDL_Renderer* renderer, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    int r = h / 2;
    int cx_left  = x + r;
    int cx_right = x + w - r;
    int cy = y + r;

    if (w > 2 * r) {
        SDL_Rect center = { x + r, y, w - 2 * r, h };
        SDL_RenderFillRect(renderer, &center);
    }

    render_filled_half_circle(renderer, cx_left, cy, r, 1);
    render_filled_half_circle(renderer, cx_right, cy, r, 0);
}

/* Left-capped pill: rounded on the left, straight on the right at x+w */
static void render_filled_left_capsule(SDL_Renderer* renderer, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    int r = h / 2;
    int cx_left = x + r;
    int cy = y + r;

    if (w > r) {
        SDL_Rect center = { cx_left, y, w - r, h };
        SDL_RenderFillRect(renderer, &center);
    }

    render_filled_half_circle(renderer, cx_left, cy, r, 1);
}

/* Capsule outline */
static void render_capsule_outline(SDL_Renderer* renderer, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    int r = h / 2;
    int cx_left  = x + r;
    int cx_right = x + w - r;
    int cy = y + r;

    SDL_RenderDrawLine(renderer, cx_left, y,         cx_right, y);
    SDL_RenderDrawLine(renderer, cx_left, y + h - 1, cx_right, y + h - 1);

    render_half_circle_outline(renderer, cx_left, cy, r, 1);
    render_half_circle_outline(renderer, cx_right, cy, r, 0);
}

/* -------------------------------------------------------------------------- */

void crossfaders_render(SDL_Renderer* renderer) {
    if (!renderer) return;
    for (int i = 0; i < crossfader_count; ++i) {
        Crossfader* cf = crossfaders[i];
        if (!cf || cf->cfg->hidden) continue;

        int knob_w = cf->cfg->rect.h; /* knob diameter == track height */
        int r      = knob_w / 2;
        int kx     = knob_x_for_value(cf, knob_w);
        int cy     = cf->cfg->rect.y + r;
        double t   = value_percent_for_crossfader(cf);

        SDL_Color fill_color = lerp_color(cf->cfg->color_0_pct, cf->cfg->color_100_pct, t);
        SDL_Color base_track_color = (SDL_Color){
            (Uint8)(fill_color.r / 2),
            (Uint8)(fill_color.g / 2),
            (Uint8)(fill_color.b / 2),
            (Uint8)((fill_color.a > 60) ? fill_color.a - 40 : fill_color.a)
        };

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        /* Draw track as full capsule */
        SDL_SetRenderDrawColor(renderer,
            base_track_color.r, base_track_color.g,
            base_track_color.b, base_track_color.a);
        render_filled_capsule(renderer,
            cf->cfg->rect.x, cf->cfg->rect.y,
            cf->cfg->rect.w, cf->cfg->rect.h);

        /* Draw progress filled portion (left-capped pill up to knob center) */
        int filled_w = kx - cf->cfg->rect.x;
        if (filled_w > 0) {
            SDL_SetRenderDrawColor(renderer,
                fill_color.r, fill_color.g,
                fill_color.b, fill_color.a);
            render_filled_left_capsule(renderer,
                cf->cfg->rect.x, cf->cfg->rect.y,
                filled_w, cf->cfg->rect.h);
        }

        /* Draw round knob */
        SDL_Color knob_color = cf->cfg->knob_color;
        if (cf->cfg->hover || cf->cfg->dragging)
            knob_color = brighten_color(knob_color, 30);
        SDL_SetRenderDrawColor(renderer,
            knob_color.r, knob_color.g,
            knob_color.b, knob_color.a);
        render_filled_circle(renderer, kx, cy, r);

        /* Outlines */
        SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
        render_capsule_outline(renderer,
            cf->cfg->rect.x, cf->cfg->rect.y,
            cf->cfg->rect.w, cf->cfg->rect.h);
        render_circle_outline(renderer, kx, cy, r);
    }
}

void crossfaders_free(void) {
    for (int i = 0; i < crossfader_count; ++i) {
        if (crossfaders[i]) {
            if (crossfaders[i]->cfg) {
                if (crossfaders[i]->cfg->track_texture) free_image(crossfaders[i]->cfg->track_texture);
                if (crossfaders[i]->cfg->knob_texture) free_image(crossfaders[i]->cfg->knob_texture);
                free(crossfaders[i]->cfg);
            }
            free(crossfaders[i]);
            crossfaders[i] = NULL;
        }
    }
    crossfader_count = 0;
}
