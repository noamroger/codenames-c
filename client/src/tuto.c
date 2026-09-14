#include "../lib/all.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#endif

#define TUTO_SLIDES_DIR "assets/img/tuto"
#define TUTO_SLIDES_GLOB "assets/img/tuto/*.png"
#define TUTO_MAX_PATH 512

typedef struct TutorialImageFile {
    int number;
    char path[TUTO_MAX_PATH];
} TutorialImageFile;

typedef struct TutorialState {
    Window* window;
    Button* btn_prev;
    Button* btn_next;
    SDL_Texture** slides;
    int slide_count;
    int current_index;
    int active;
    int initialized;
    int seen;
} TutorialState;

static TutorialState g_tuto = {0};

static int tutorial_parse_numeric_png(const char* file_name, int* out_number) {
    if (!file_name || !out_number) return EXIT_FAILURE;

    size_t len = strlen(file_name);
    if (len < 5) return EXIT_FAILURE;
    if (strcmp(file_name + (len - 4), ".png") != 0) return EXIT_FAILURE;

    size_t number_len = len - 4;
    if (number_len == 0 || number_len >= 32) return EXIT_FAILURE;

    char number_buf[32] = {0};
    for (size_t i = 0; i < number_len; i++) {
        unsigned char ch = (unsigned char)file_name[i];
        if (!isdigit(ch)) return EXIT_FAILURE;
        number_buf[i] = (char)ch;
    }

    *out_number = atoi(number_buf);
    return EXIT_SUCCESS;
}

static int tutorial_cmp_file(const void* a, const void* b) {
    const TutorialImageFile* fa = (const TutorialImageFile*)a;
    const TutorialImageFile* fb = (const TutorialImageFile*)b;
    if (fa->number < fb->number) return -1;
    if (fa->number > fb->number) return 1;
    return strcmp(fa->path, fb->path);
}

static int tutorial_append_file(TutorialImageFile** files, int* count, int number, const char* file_name) {
    if (!files || !count || !file_name) return EXIT_FAILURE;

    TutorialImageFile* grown = (TutorialImageFile*)realloc(*files, sizeof(TutorialImageFile) * (size_t)(*count + 1));
    if (!grown) return EXIT_FAILURE;

    *files = grown;
    TutorialImageFile* slot = &((*files)[*count]);
    slot->number = number;
    format_to(slot->path, sizeof(slot->path), "%s/%s", TUTO_SLIDES_DIR, file_name);
    (*count)++;

    return EXIT_SUCCESS;
}

static int tutorial_collect_files(TutorialImageFile** out_files, int* out_count) {
    if (!out_files || !out_count) return EXIT_FAILURE;
    *out_files = NULL;
    *out_count = 0;

#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(TUTO_SLIDES_GLOB, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) {
        return EXIT_SUCCESS;
    }

    do {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        int number = 0;
        if (tutorial_parse_numeric_png(find_data.cFileName, &number) != EXIT_SUCCESS) continue;

        if (tutorial_append_file(out_files, out_count, number, find_data.cFileName) != EXIT_SUCCESS) {
            FindClose(h_find);
            return EXIT_FAILURE;
        }
    } while (FindNextFileA(h_find, &find_data));

    FindClose(h_find);
#else
    DIR* dir = opendir(TUTO_SLIDES_DIR);
    if (!dir) return EXIT_SUCCESS;

    struct dirent* ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        int number = 0;
        if (tutorial_parse_numeric_png(ent->d_name, &number) != EXIT_SUCCESS) continue;

        if (tutorial_append_file(out_files, out_count, number, ent->d_name) != EXIT_SUCCESS) {
            closedir(dir);
            return EXIT_FAILURE;
        }
    }

    closedir(dir);
#endif

    if (*out_count > 1) {
        qsort(*out_files, (size_t)*out_count, sizeof(TutorialImageFile), tutorial_cmp_file);
    }

    return EXIT_SUCCESS;
}

static void tutorial_release_slides(void) {
    if (g_tuto.slides) {
        for (int i = 0; i < g_tuto.slide_count; i++) {
            if (g_tuto.slides[i]) free_image(g_tuto.slides[i]);
        }
        free(g_tuto.slides);
        g_tuto.slides = NULL;
    }

    g_tuto.slide_count = 0;
    g_tuto.current_index = 0;
}

static int tutorial_load_slides(SDL_Renderer* renderer) {
    if (!renderer) return EXIT_FAILURE;

    tutorial_release_slides();

    TutorialImageFile* files = NULL;
    int file_count = 0;
    if (tutorial_collect_files(&files, &file_count) != EXIT_SUCCESS) {
        free(files);
        return EXIT_FAILURE;
    }

    if (file_count <= 0) {
        free(files);
        return EXIT_SUCCESS;
    }

    SDL_Texture** slides = (SDL_Texture**)calloc((size_t)file_count, sizeof(SDL_Texture*));
    if (!slides) {
        free(files);
        return EXIT_FAILURE;
    }

    int loaded_count = 0;
    for (int i = 0; i < file_count; i++) {
        SDL_Texture* tex = load_image(renderer, files[i].path);
        if (!tex) {
            printf("Tutorial: failed to load slide '%s'\n", files[i].path);
            continue;
        }
        slides[loaded_count++] = tex;
    }

    free(files);

    if (loaded_count <= 0) {
        free(slides);
        return EXIT_SUCCESS;
    }

    SDL_Texture** resized = (SDL_Texture**)realloc(slides, sizeof(SDL_Texture*) * (size_t)loaded_count);
    g_tuto.slides = resized ? resized : slides;
    g_tuto.slide_count = loaded_count;
    g_tuto.current_index = 0;

    printf("Tutorial: %d slide(s) loaded\n", g_tuto.slide_count);
    return EXIT_SUCCESS;
}

static void tutorial_reset_button_states(void) {
    if (g_tuto.btn_prev && g_tuto.btn_prev->cfg) {
        g_tuto.btn_prev->cfg->is_hovered = 0;
        g_tuto.btn_prev->cfg->is_pressed = 0;
    }
    if (g_tuto.btn_next && g_tuto.btn_next->cfg) {
        g_tuto.btn_next->cfg->is_hovered = 0;
        g_tuto.btn_next->cfg->is_pressed = 0;
    }
}

static int tutorial_is_prev_enabled(void) {
    return g_tuto.slide_count > 1 && g_tuto.current_index > 0;
}

static int tutorial_should_center_next_button(void) {
    return !tutorial_is_prev_enabled();
}

static void tutorial_clamp_index(void) {
    if (g_tuto.slide_count <= 0) {
        g_tuto.current_index = 0;
        return;
    }

    if (g_tuto.current_index < 0) g_tuto.current_index = 0;
    if (g_tuto.current_index >= g_tuto.slide_count) g_tuto.current_index = g_tuto.slide_count - 1;
}

static void tutorial_update_title(void) {
    if (!g_tuto.window) return;

    if (g_tuto.slide_count > 0) {
        char title[96];
        format_to(title, sizeof(title), "Tutoriel (%d/%d)", g_tuto.current_index + 1, g_tuto.slide_count);
        window_edit_cfg(g_tuto.window, WIN_CFG_TITLE, (intptr_t)title);
    } else {
        window_edit_cfg(g_tuto.window, WIN_CFG_TITLE, (intptr_t)"Tutoriel");
    }
}

static void tutorial_update_next_button_label(void) {
    if (!g_tuto.btn_next) return;

    int is_last = (g_tuto.slide_count <= 0 || g_tuto.current_index >= g_tuto.slide_count - 1);
    const char* label = is_last ? "Terminer le tutoriel" : "Suivant";
    button_edit_cfg(g_tuto.btn_next, BTN_CFG_TEXT, (intptr_t)label);
}

static void tutorial_sync_ui(void) {
    tutorial_clamp_index();
    tutorial_update_title();
    tutorial_update_next_button_label();
}

static void tutorial_finish(void) {
    g_tuto.active = 0;
    g_tuto.seen = 1;
    tutorial_reset_button_states();

    if (write_property("SEEN_TUTO", "1") != EXIT_SUCCESS) {
        printf("Tutorial: failed to write SEEN_TUTO=1\n");
    }
}

static ButtonReturn tutorial_button_click(AppContext* context, Button* button) {
    (void)context;
    if (!button) return BTN_NONE;

    if (button == g_tuto.btn_prev) {
        if (tutorial_is_prev_enabled()) {
            g_tuto.current_index--;
            tutorial_sync_ui();
        }
        return BTN_NONE;
    }

    if (button == g_tuto.btn_next) {
        if (g_tuto.slide_count > 0 && g_tuto.current_index < g_tuto.slide_count - 1) {
            g_tuto.current_index++;
            tutorial_sync_ui();
        } else {
            tutorial_finish();
        }
        return BTN_NONE;
    }

    return BTN_NONE;
}

static int tutorial_get_content_rect(SDL_Rect* out_rect) {
    if (!out_rect || !g_tuto.window || !g_tuto.window->cfg) return EXIT_FAILURE;

    const WindowConfig* cfg = g_tuto.window->cfg;

    const int side_padding = 24;
    const int top_padding = cfg->titlebar_h + 20;
    const int bottom_padding = 120;

    SDL_Rect r = {
        .x = cfg->rect.x + side_padding,
        .y = cfg->rect.y + top_padding,
        .w = cfg->rect.w - (2 * side_padding),
        .h = cfg->rect.h - top_padding - bottom_padding
    };

    if (r.w <= 0 || r.h <= 0) return EXIT_FAILURE;

    *out_rect = r;
    return EXIT_SUCCESS;
}

static SDL_Texture* tutorial_current_slide(void) {
    if (g_tuto.slide_count <= 0 || !g_tuto.slides) return NULL;
    if (g_tuto.current_index < 0 || g_tuto.current_index >= g_tuto.slide_count) return NULL;
    return g_tuto.slides[g_tuto.current_index];
}

static void tutorial_render_slide(SDL_Renderer* renderer) {
    if (!renderer || !g_tuto.window || !g_tuto.window->cfg) return;

    SDL_Rect content = {0};
    if (tutorial_get_content_rect(&content) != EXIT_SUCCESS) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 15, 15, 15, 220);
    SDL_RenderFillRect(renderer, &content);
    SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
    SDL_RenderDrawRect(renderer, &content);

    SDL_Texture* slide = tutorial_current_slide();
    if (!slide) return;

    int tex_w = 0;
    int tex_h = 0;
    if (SDL_QueryTexture(slide, NULL, NULL, &tex_w, &tex_h) != 0) return;
    if (tex_w <= 0 || tex_h <= 0) return;

    double sx = (double)content.w / (double)tex_w;
    double sy = (double)content.h / (double)tex_h;
    double scale = (sx < sy) ? sx : sy;
    if (scale <= 0.0) return;

    SDL_Rect dst = {0};
    dst.w = (int)lround((double)tex_w * scale);
    dst.h = (int)lround((double)tex_h * scale);
    dst.x = content.x + (content.w - dst.w) / 2;
    dst.y = content.y + (content.h - dst.h) / 2;

    SDL_RenderSetClipRect(renderer, &content);
    SDL_RenderCopy(renderer, slide, NULL, &dst);
    SDL_RenderSetClipRect(renderer, NULL);
}

static int tutorial_read_seen_flag(void) {
    char buf[32] = {0};
    if (read_property(buf, sizeof(buf), "SEEN_TUTO") != EXIT_SUCCESS) {
        if (write_property("SEEN_TUTO", "0") != EXIT_SUCCESS) {
            printf("Tutorial: failed to initialize SEEN_TUTO=0\n");
        }
        return 0;
    }

    if (buf[0] == '\0') return 0;
    return atoi(buf) != 0;
}

static void tutorial_reposition_buttons(void) {
    if (!g_tuto.window || !g_tuto.window->cfg) return;

    const int nav_y = -((g_tuto.window->cfg->h / 2) - 68);
    const int horizontal_margin = 215;
    int show_prev = tutorial_is_prev_enabled();
    int next_rel_x = tutorial_should_center_next_button() ? 0 : horizontal_margin;

    if (g_tuto.btn_prev && show_prev) {
        window_place_button(g_tuto.window, g_tuto.btn_prev, -horizontal_margin, nav_y);
    } else if (g_tuto.btn_prev && g_tuto.btn_prev->cfg) {
        g_tuto.btn_prev->cfg->is_hovered = 0;
        g_tuto.btn_prev->cfg->is_pressed = 0;
    }

    if (g_tuto.btn_next) {
        window_place_button(g_tuto.window, g_tuto.btn_next, next_rel_x, nav_y);
    }
}

static void tutorial_open_internal(int reset_to_first) {
    if (!g_tuto.initialized || !g_tuto.window) return;

    g_tuto.active = 1;
    if (reset_to_first) g_tuto.current_index = 0;
    tutorial_sync_ui();
    tutorial_reset_button_states();
}

int tuto_init(AppContext* context) {
    if (!context || !context->renderer) return EXIT_FAILURE;
    if (g_tuto.initialized) return EXIT_SUCCESS;

    if (tutorial_load_slides(context->renderer) != EXIT_SUCCESS) {
        printf("Tutorial: failed to load slides metadata\n");
    }

    WindowConfig* cfg_window = window_config_init();
    if (!cfg_window) {
        tuto_free();
        return EXIT_FAILURE;
    }

    cfg_window->x = 0;
    cfg_window->y = 0;
    cfg_window->w = 1450;
    cfg_window->h = 900;
    cfg_window->movable = 0;
    cfg_window->bg_color = (SDL_Color){24, 24, 24, 240};
    cfg_window->border_color = (SDL_Color){220, 220, 220, 255};
    cfg_window->titlebar_color = (SDL_Color){35, 80, 120, 255};
    cfg_window->titlebar_h = 52;
    cfg_window->border_thickness = 3;
    cfg_window->title = "Tutoriel";

    g_tuto.window = window_create(WINDOW_TUTO, cfg_window);
    free(cfg_window);

    if (!g_tuto.window) {
        tuto_free();
        return EXIT_FAILURE;
    }

    ButtonConfig* cfg_prev = button_config_init();
    if (!cfg_prev) {
        tuto_free();
        return EXIT_FAILURE;
    }
    cfg_prev->w = 320;
    cfg_prev->h = 72;
    cfg_prev->font_path = FONT_LARABIE;
    cfg_prev->color = COL_WHITE;
    cfg_prev->text = "Précédent";
    cfg_prev->callback = tutorial_button_click;
    g_tuto.btn_prev = button_create(context->renderer, BTN_TUTO_PREV, cfg_prev);
    free(cfg_prev);

    if (!g_tuto.btn_prev) {
        tuto_free();
        return EXIT_FAILURE;
    }

    ButtonConfig* cfg_next = button_config_init();
    if (!cfg_next) {
        tuto_free();
        return EXIT_FAILURE;
    }
    cfg_next->w = 360;
    cfg_next->h = 72;
    cfg_next->font_path = FONT_LARABIE;
    cfg_next->color = COL_WHITE;
    cfg_next->text = "Suivant";
    cfg_next->callback = tutorial_button_click;
    g_tuto.btn_next = button_create(context->renderer, BTN_TUTO_NEXT, cfg_next);
    free(cfg_next);

    if (!g_tuto.btn_next) {
        tuto_free();
        return EXIT_FAILURE;
    }

    tutorial_reposition_buttons();

    g_tuto.seen = tutorial_read_seen_flag();
    g_tuto.current_index = 0;
    g_tuto.active = (g_tuto.seen == 0);
    g_tuto.initialized = 1;

    tutorial_sync_ui();
    tutorial_reset_button_states();

    if (g_tuto.active) {
        printf("Tutorial: auto-opened (SEEN_TUTO=0)\n");
    }

    return EXIT_SUCCESS;
}

void tuto_open(void) {
    tutorial_open_internal(1);
}

int tuto_is_active(void) {
    return g_tuto.active != 0;
}

void tuto_handle_event(AppContext* context, SDL_Event* e) {
    if (!g_tuto.active || !e) return;

    if (g_tuto.window) window_handle_event(context, g_tuto.window, e);
    if (g_tuto.btn_prev && tutorial_is_prev_enabled()) button_handle_event(context, g_tuto.btn_prev, e);
    if (g_tuto.btn_next) button_handle_event(context, g_tuto.btn_next, e);
}

void tuto_display(AppContext* context) {
    if (!context || !context->renderer || !g_tuto.active) return;

    if (g_tuto.window) {
        tutorial_reposition_buttons();
        window_render(context->renderer, g_tuto.window);
    }

    tutorial_render_slide(context->renderer);

    if (g_tuto.btn_prev && tutorial_is_prev_enabled()) {
        button_render(context->renderer, g_tuto.btn_prev);
    }
    if (g_tuto.btn_next) button_render(context->renderer, g_tuto.btn_next);
}

int tuto_free(void) {
    if (g_tuto.btn_prev) {
        button_destroy(g_tuto.btn_prev);
        g_tuto.btn_prev = NULL;
    }

    if (g_tuto.btn_next) {
        button_destroy(g_tuto.btn_next);
        g_tuto.btn_next = NULL;
    }

    if (g_tuto.window) {
        window_destroy(g_tuto.window);
        g_tuto.window = NULL;
    }

    tutorial_release_slides();

    g_tuto.active = 0;
    g_tuto.initialized = 0;
    g_tuto.seen = 0;

    return EXIT_SUCCESS;
}