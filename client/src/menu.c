#include "../lib/all.h"

/* Ressources UI du module menu */
static SDL_Texture* menu_logo = NULL;
static Button* btn_create = NULL;
static Button* btn_join = NULL;
static Button* btn_quit = NULL;
static Button* btn_social = NULL;
static Button* btn_tuto = NULL;
static Button* btn_credits = NULL;

static Input* name_input = NULL;
static Input* code_input = NULL;
static int joining = 0;
static Text* txt_startup_loading = NULL;
static Text* txt_startup_creators_line1_A = NULL;
static Text* txt_startup_creators_line1_B = NULL;
static Text* txt_startup_creators_line2_A = NULL;
static Text* txt_startup_creators_line2_B = NULL;

static const char* NAME_PLACEHOLDERS[] = {"Peter", "Quagmire", "Tom", "Faz Faf"};
static const char* CODE_PLACEHOLDERS[] = {"CODE : #####"};

#define MENU_STARTUP_LOGO_FADE_MS 750U
#define MENU_STARTUP_OPENING_MS 12000U
#define MENU_STARTUP_CREATORS_FADE_MS 1250U
#define MENU_STARTUP_OPENING_LOGO_ANIM_MS 10500U
#define MENU_STARTUP_BG_FADE_MS 750U
#define MENU_STARTUP_TRANSITION_MS 1000U
#define MENU_STARTUP_LOGO_MOVE_MS 750U
#define MENU_BOUNCE_OVERSHOOT 1.08f

#define MENU_LOGO_INTRO_Y 150
#define MENU_LOGO_OPENING_Y 220
#define MENU_LOGO_FINAL_Y 200
#define MENU_LOGO_INTRO_SCALE 1.2f
#define MENU_LOGO_OPENING_SCALE 0.8f
#define MENU_LOGO_FINAL_SCALE 1.00f

#define MENU_CREATOR_LINE1_A_START_Y -140
#define MENU_CREATOR_LINE1_A_END_Y -70
#define MENU_CREATOR_LINE1_B_START_Y -140
#define MENU_CREATOR_LINE1_B_END_Y -70
#define MENU_CREATOR_LINE2_A_START_Y -240
#define MENU_CREATOR_LINE2_A_END_Y -170
#define MENU_CREATOR_LINE2_B_START_Y -240
#define MENU_CREATOR_LINE2_B_END_Y -170

#define MENU_CREATOR_LINE1_A_DELAY_MS 250U // Noam
#define MENU_CREATOR_LINE1_B_DELAY_MS 950U // Romain
#define MENU_CREATOR_LINE2_A_DELAY_MS 2500U // Chloé
#define MENU_CREATOR_LINE2_B_DELAY_MS 3350U // Mathis

typedef enum MenuStartupPhase {
    MENU_STARTUP_PHASE_LOADING = 0,
    MENU_STARTUP_PHASE_OPENING,
    MENU_STARTUP_PHASE_TRANSITION,
    MENU_STARTUP_PHASE_READY
} MenuStartupPhase;

typedef struct MenuStartupState {
    MenuStartupPhase phase;
    float loading_progress;
    int loading_complete;
    int skip_requested;
    int opening_audio_started;
    int transition_initialized;
    Uint32 startup_started_at_ms;
    Uint32 opening_started_at_ms;
    Uint32 transition_started_at_ms;
} MenuStartupState;

typedef struct MenuUiPlacement {
    int valid;
    int target_x;
    int target_y;
    int w;
    int h;
    int from_x;
    int from_y;
} MenuUiPlacement;

static MenuStartupState menu_startup = {
    MENU_STARTUP_PHASE_LOADING,
    0.0f,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

static MenuUiPlacement ui_btn_create = {0};
static MenuUiPlacement ui_btn_join = {0};
static MenuUiPlacement ui_btn_quit = {0};
static MenuUiPlacement ui_btn_social = {0};
static MenuUiPlacement ui_btn_tuto = {0};
static MenuUiPlacement ui_btn_credits = {0};
static MenuUiPlacement ui_input_name = {0};
static MenuUiPlacement ui_input_code = {0};

static float menu_clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static int menu_lerp_int(int from, int to, float t) {
    return from + (int)lroundf((double)(to - from) * (double)t);
}

// Fonction d'easing pour un effet de rebond à la fin de l'animation d'ouverture du logo
static float menu_ease_out_back(float t) {
    const float c1 = MENU_BOUNCE_OVERSHOOT;
    const float c3 = c1 + 1.0f;
    float x = t - 1.0f;
    return 1.0f + c3 * x * x * x + c1 * x * x;
}

static float menu_smoothstep(float t) {
    float x = menu_clamp01(t);
    return x * x * (3.0f - (2.0f * x));
}

static float menu_opening_creator_pose(Uint32 elapsed_ms, Uint32 delay_ms) {
    if (elapsed_ms <= delay_ms) return 0.0f;

    Uint32 delayed_elapsed_ms = elapsed_ms - delay_ms;
    float pose_t = menu_clamp01((float)delayed_elapsed_ms / (float)MENU_STARTUP_OPENING_LOGO_ANIM_MS);
    return menu_smoothstep(pose_t);
}

static Uint8 menu_opening_credits_alpha(Uint32 elapsed_ms, Uint32 delay_ms) {
    if (elapsed_ms <= delay_ms) return 0;

    if (MENU_STARTUP_CREATORS_FADE_MS == 0) return 255;

    Uint32 delayed_elapsed_ms = elapsed_ms - delay_ms;

    if (delayed_elapsed_ms < MENU_STARTUP_CREATORS_FADE_MS) {
        float fade_in_t = (float)delayed_elapsed_ms / (float)MENU_STARTUP_CREATORS_FADE_MS;
        return (Uint8)(255.0f * menu_clamp01(fade_in_t));
    }

    if (elapsed_ms >= MENU_STARTUP_OPENING_MS) {
        return 0;
    }

    if (elapsed_ms > (MENU_STARTUP_OPENING_MS - MENU_STARTUP_CREATORS_FADE_MS)) {
        Uint32 remaining_ms = MENU_STARTUP_OPENING_MS - elapsed_ms;
        float fade_out_t = (float)remaining_ms / (float)MENU_STARTUP_CREATORS_FADE_MS;
        return (Uint8)(255.0f * menu_clamp01(fade_out_t));
    }

    return 255;
}

static void menu_draw_black_overlay(AppContext* context, Uint8 alpha) {
    if (!context || !context->renderer || alpha == 0) return;

    SDL_SetRenderDrawBlendMode(context->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(context->renderer, 0, 0, 0, alpha);
    SDL_Rect full_screen = {0, 0, WIN_WIDTH, WIN_HEIGHT};
    SDL_RenderFillRect(context->renderer, &full_screen);
    SDL_SetRenderDrawBlendMode(context->renderer, SDL_BLENDMODE_NONE);
}

static void menu_render_startup_logo(AppContext* context, int y, float scale, Uint8 alpha) {
    if (!context || !menu_logo) return;
    display_image(context->renderer, menu_logo, 0, y, scale, 0, SDL_FLIP_NONE, 1, alpha);
}

static void menu_render_loading_bar(AppContext* context) {
    if (!context || !context->renderer) return;

    const int bar_width = 560;
    const int bar_height = 30;
    const int bar_x = (WIN_WIDTH - bar_width) / 2;
    const int bar_y = (WIN_HEIGHT / 2) + 255;

    SDL_Rect bg_rect = {bar_x, bar_y, bar_width, bar_height};
    SDL_SetRenderDrawColor(context->renderer, 30, 30, 30, 255);
    SDL_RenderFillRect(context->renderer, &bg_rect);

    int inner_padding = 3;
    SDL_Rect fill_rect = {
        bar_x + inner_padding,
        bar_y + inner_padding,
        (int)((float)(bar_width - inner_padding * 2) * menu_clamp01(menu_startup.loading_progress)),
        bar_height - inner_padding * 2
    };

    SDL_SetRenderDrawColor(context->renderer, 224, 198, 149, 255);
    SDL_RenderFillRect(context->renderer, &fill_rect);

    SDL_SetRenderDrawColor(context->renderer, 230, 230, 230, 255);
    SDL_RenderDrawRect(context->renderer, &bg_rect);

    if (txt_startup_loading) {
        int loading_percent = (int)lroundf((double)(menu_clamp01(menu_startup.loading_progress) * 100.0f));
        char loading_label[64];
        format_to(loading_label, sizeof(loading_label), "chargement %d%%", loading_percent);
        update_text(context, txt_startup_loading, loading_label);

        int label_center_y_screen = bar_y - 22;
        int label_rel_y = (WIN_HEIGHT / 2) - label_center_y_screen;
        update_text_position(txt_startup_loading, 0, label_rel_y);
        display_text(context, txt_startup_loading);
    }
}

static void menu_capture_button_placement(Button* button, MenuUiPlacement* placement) {
    if (!button || !button->cfg || !placement) return;

    placement->valid = 1;
    placement->target_x = button->cfg->rect.x;
    placement->target_y = button->cfg->rect.y;
    placement->w = button->cfg->rect.w;
    placement->h = button->cfg->rect.h;
}

static void menu_capture_input_placement(Input* input, MenuUiPlacement* placement) {
    if (!input || !input->cfg || !placement) return;

    placement->valid = 1;
    placement->target_x = input->cfg->rect.x;
    placement->target_y = input->cfg->rect.y;
    placement->w = input->cfg->rect.w;
    placement->h = input->cfg->rect.h;
}

static void menu_set_button_position(Button* button, int x, int y) {
    if (!button) return;
    button_edit_cfg(button, BTN_CFG_X, x);
    button_edit_cfg(button, BTN_CFG_Y, y);
}

static void menu_set_input_position(Input* input, int x, int y) {
    if (!input) return;
    edit_in_cfg(input->id, IN_CFG_X, x);
    edit_in_cfg(input->id, IN_CFG_Y, y);
}

static void menu_capture_ui_targets() {
    menu_capture_button_placement(btn_create, &ui_btn_create);
    menu_capture_button_placement(btn_join, &ui_btn_join);
    menu_capture_button_placement(btn_quit, &ui_btn_quit);
    menu_capture_button_placement(btn_social, &ui_btn_social);
    menu_capture_button_placement(btn_tuto, &ui_btn_tuto);
    menu_capture_button_placement(btn_credits, &ui_btn_credits);
    menu_capture_input_placement(name_input, &ui_input_name);
    menu_capture_input_placement(code_input, &ui_input_code);
}

static void menu_prepare_bounce_starts() {
    if (ui_btn_create.valid) { // Du coin inférieur gauche
        ui_btn_create.from_x = -120;
        ui_btn_create.from_y = WIN_HEIGHT + 120;
        menu_set_button_position(btn_create, ui_btn_create.from_x, ui_btn_create.from_y);
    }
    if (ui_btn_join.valid) { // Du coin inférieur droit
        ui_btn_join.from_x = WIN_WIDTH + 120;
        ui_btn_join.from_y = WIN_HEIGHT + 120;
        menu_set_button_position(btn_join, ui_btn_join.from_x, ui_btn_join.from_y);
    }
    if (ui_btn_quit.valid) { // Du dessous
        ui_btn_quit.from_x = ui_btn_quit.target_x;
        ui_btn_quit.from_y = WIN_HEIGHT + 120;
        menu_set_button_position(btn_quit, ui_btn_quit.from_x, ui_btn_quit.from_y);
    }
    if (ui_btn_social.valid) { // De la droite + un peu plus haut
        ui_btn_social.from_x = WIN_WIDTH + 120;
        ui_btn_social.from_y = ui_btn_social.target_y - 120;
        menu_set_button_position(btn_social, ui_btn_social.from_x, ui_btn_social.from_y);
    }
    if (ui_btn_tuto.valid) { // De la droite
        ui_btn_tuto.from_x = WIN_WIDTH + 120;
        ui_btn_tuto.from_y = ui_btn_tuto.target_y;
        menu_set_button_position(btn_tuto, ui_btn_tuto.from_x, ui_btn_tuto.from_y);
    }
    if (ui_btn_credits.valid) { // Du coin inférieur droit
        ui_btn_credits.from_x = WIN_WIDTH + 120;
        ui_btn_credits.from_y = WIN_HEIGHT - 120;
        menu_set_button_position(btn_credits, ui_btn_credits.from_x, ui_btn_credits.from_y);
    }
    if (ui_input_name.valid) { // Du coin supérieur droit
        ui_input_name.from_x = WIN_WIDTH + 160;
        ui_input_name.from_y = -ui_input_name.target_y;
        menu_set_input_position(name_input, ui_input_name.from_x, ui_input_name.from_y);
    }
    if (ui_input_code.valid) { // Du coin inférieur droit
        ui_input_code.from_x = WIN_WIDTH + 120;
        ui_input_code.from_y = WIN_HEIGHT + 120;
        menu_set_input_position(code_input, ui_input_code.from_x, ui_input_code.from_y);
    }
}

static void menu_apply_bounce(float progress) {
    float eased = menu_ease_out_back(menu_clamp01(progress));

    if (ui_btn_create.valid) {
        menu_set_button_position(
            btn_create,
            menu_lerp_int(ui_btn_create.from_x, ui_btn_create.target_x, eased),
            menu_lerp_int(ui_btn_create.from_y, ui_btn_create.target_y, eased)
        );
    }
    if (ui_btn_join.valid) {
        menu_set_button_position(
            btn_join,
            menu_lerp_int(ui_btn_join.from_x, ui_btn_join.target_x, eased),
            menu_lerp_int(ui_btn_join.from_y, ui_btn_join.target_y, eased)
        );
    }
    if (ui_btn_quit.valid) {
        menu_set_button_position(
            btn_quit,
            menu_lerp_int(ui_btn_quit.from_x, ui_btn_quit.target_x, eased),
            menu_lerp_int(ui_btn_quit.from_y, ui_btn_quit.target_y, eased)
        );
    }
    if (ui_btn_social.valid) {
        menu_set_button_position(
            btn_social,
            menu_lerp_int(ui_btn_social.from_x, ui_btn_social.target_x, eased),
            menu_lerp_int(ui_btn_social.from_y, ui_btn_social.target_y, eased)
        );
    }
    if (ui_btn_tuto.valid) {
        menu_set_button_position(
            btn_tuto,
            menu_lerp_int(ui_btn_tuto.from_x, ui_btn_tuto.target_x, eased),
            menu_lerp_int(ui_btn_tuto.from_y, ui_btn_tuto.target_y, eased)
        );
    }
    if (ui_btn_credits.valid) {
        menu_set_button_position(
            btn_credits,
            menu_lerp_int(ui_btn_credits.from_x, ui_btn_credits.target_x, eased),
            menu_lerp_int(ui_btn_credits.from_y, ui_btn_credits.target_y, eased)
        );
    }
    if (ui_input_name.valid) {
        menu_set_input_position(
            name_input,
            menu_lerp_int(ui_input_name.from_x, ui_input_name.target_x, eased),
            menu_lerp_int(ui_input_name.from_y, ui_input_name.target_y, eased)
        );
    }
    if (ui_input_code.valid) {
        menu_set_input_position(
            code_input,
            menu_lerp_int(ui_input_code.from_x, ui_input_code.target_x, eased),
            menu_lerp_int(ui_input_code.from_y, ui_input_code.target_y, eased)
        );
    }
}

static void menu_finalize_ui_positions() {
    if (ui_btn_create.valid) menu_set_button_position(btn_create, ui_btn_create.target_x, ui_btn_create.target_y);
    if (ui_btn_join.valid) menu_set_button_position(btn_join, ui_btn_join.target_x, ui_btn_join.target_y);
    if (ui_btn_quit.valid) menu_set_button_position(btn_quit, ui_btn_quit.target_x, ui_btn_quit.target_y);
    if (ui_btn_social.valid) menu_set_button_position(btn_social, ui_btn_social.target_x, ui_btn_social.target_y);
    if (ui_btn_tuto.valid) menu_set_button_position(btn_tuto, ui_btn_tuto.target_x, ui_btn_tuto.target_y);
    if (ui_btn_credits.valid) menu_set_button_position(btn_credits, ui_btn_credits.target_x, ui_btn_credits.target_y);
    if (ui_input_name.valid) menu_set_input_position(name_input, ui_input_name.target_x, ui_input_name.target_y);
    if (ui_input_code.valid) menu_set_input_position(code_input, ui_input_code.target_x, ui_input_code.target_y);
}

static void menu_render_main_widgets(AppContext* context) {
    if (!context) return;

    if (name_input) input_render(context->renderer, name_input);

    if (btn_create) button_render(context->renderer, btn_create);
    if (btn_quit) button_render(context->renderer, btn_quit);
    if (btn_social) button_render(context->renderer, btn_social);
    if (btn_tuto) button_render(context->renderer, btn_tuto);
    if (btn_credits) button_render(context->renderer, btn_credits);

    if (joining) {
        if (code_input) input_render(context->renderer, code_input);
    } else if (btn_join) {
        button_render(context->renderer, btn_join);
    }
}

static void menu_render_opening_credits(AppContext* context) {
    if (!context) return;

    Uint32 now = SDL_GetTicks();
    Uint32 opening_elapsed_ms = 0;
    if (menu_startup.opening_started_at_ms > 0 && now > menu_startup.opening_started_at_ms) {
        opening_elapsed_ms = now - menu_startup.opening_started_at_ms;
    }

    float logo_anim_t = 1.0f;
    if (menu_startup.opening_started_at_ms > 0) {
        logo_anim_t = menu_clamp01(
            (float)(now - menu_startup.opening_started_at_ms) / (float)MENU_STARTUP_OPENING_LOGO_ANIM_MS
        );
    }
    float logo_pose_t = menu_smoothstep(logo_anim_t);
    float logo_scale = MENU_LOGO_INTRO_SCALE + (MENU_LOGO_OPENING_SCALE - MENU_LOGO_INTRO_SCALE) * logo_pose_t;
    int logo_y = menu_lerp_int(MENU_LOGO_INTRO_Y, MENU_LOGO_OPENING_Y, logo_pose_t);

    // Un seul mouvement vertical par ligne: pas de décalage Y entre A et B.
    float creator_line1_pose_t = menu_opening_creator_pose(opening_elapsed_ms, MENU_CREATOR_LINE1_A_DELAY_MS);
    float creator_line2_pose_t = menu_opening_creator_pose(opening_elapsed_ms, MENU_CREATOR_LINE2_A_DELAY_MS);

    int creator_line1_y = menu_lerp_int(MENU_CREATOR_LINE1_A_START_Y, MENU_CREATOR_LINE1_A_END_Y, creator_line1_pose_t);
    int creator_line2_y = menu_lerp_int(MENU_CREATOR_LINE2_A_START_Y, MENU_CREATOR_LINE2_A_END_Y, creator_line2_pose_t);

    menu_draw_black_overlay(context, 255);
    menu_render_startup_logo(context, logo_y, logo_scale, 255);

    if (txt_startup_creators_line1_A) {
        txt_startup_creators_line1_A->cfg.opacity = menu_opening_credits_alpha(opening_elapsed_ms, MENU_CREATOR_LINE1_A_DELAY_MS);
        update_text_position(txt_startup_creators_line1_A, txt_startup_creators_line1_A->cfg.x, creator_line1_y);
        display_text(context, txt_startup_creators_line1_A);
    }
    if (txt_startup_creators_line1_B) {
        txt_startup_creators_line1_B->cfg.opacity = menu_opening_credits_alpha(opening_elapsed_ms, MENU_CREATOR_LINE1_B_DELAY_MS);
        update_text_position(txt_startup_creators_line1_B, txt_startup_creators_line1_B->cfg.x, creator_line1_y);
        display_text(context, txt_startup_creators_line1_B);
    }
    if (txt_startup_creators_line2_A) {
        txt_startup_creators_line2_A->cfg.opacity = menu_opening_credits_alpha(opening_elapsed_ms, MENU_CREATOR_LINE2_A_DELAY_MS);
        update_text_position(txt_startup_creators_line2_A, txt_startup_creators_line2_A->cfg.x, creator_line2_y);
        display_text(context, txt_startup_creators_line2_A);
    }
    if (txt_startup_creators_line2_B) {
        txt_startup_creators_line2_B->cfg.opacity = menu_opening_credits_alpha(opening_elapsed_ms, MENU_CREATOR_LINE2_B_DELAY_MS);
        update_text_position(txt_startup_creators_line2_B, txt_startup_creators_line2_B->cfg.x, creator_line2_y);
        display_text(context, txt_startup_creators_line2_B);
    }
}

static void menu_enter_startup_transition(Uint32 now) {
    if (audio_is_playing(SOUND_OPENING_CODENAMES)) {
        (void)audio_stop_with_fade(SOUND_OPENING_CODENAMES, 250, AUDIO_FADE_OUT_BY_VOLUME, NULL);
    }

    menu_startup.phase = MENU_STARTUP_PHASE_TRANSITION;
    menu_startup.transition_started_at_ms = now;
    menu_startup.transition_initialized = 0;
    menu_startup.skip_requested = 0;
}

static void menu_update_startup_state(AppContext* context) {
    if (!context) return;

    Uint32 now = SDL_GetTicks();

    if (menu_startup.phase == MENU_STARTUP_PHASE_LOADING) {
        float logo_progress = menu_clamp01((float)(now - menu_startup.startup_started_at_ms) / (float)MENU_STARTUP_LOGO_FADE_MS);
        if (menu_startup.loading_complete && logo_progress >= 1.0f) {
            if (menu_startup.skip_requested) {
                menu_enter_startup_transition(now);
            } else {
                menu_startup.phase = MENU_STARTUP_PHASE_OPENING;
                menu_startup.opening_started_at_ms = now;
                menu_startup.opening_audio_started = 0;
            }
        }
    }

    if (menu_startup.phase == MENU_STARTUP_PHASE_OPENING) {
        if (!menu_startup.opening_audio_started) {
            (void)audio_play_with_fade(SOUND_OPENING_CODENAMES, 0, 150, AUDIO_FADE_IN_BY_VOLUME, NULL);
            menu_startup.opening_audio_started = 1;
        }

        if (menu_startup.skip_requested || (now - menu_startup.opening_started_at_ms) >= MENU_STARTUP_OPENING_MS) {
            menu_enter_startup_transition(now);
        }
    }

    if (menu_startup.phase == MENU_STARTUP_PHASE_TRANSITION && !menu_startup.transition_initialized) {
        menu_capture_ui_targets();
        menu_prepare_bounce_starts();
        menu_startup.transition_initialized = 1;
    }
}

void menu_set_startup_loading_progress(float progress) {
    menu_startup.loading_progress = menu_clamp01(progress);
}

void menu_mark_startup_loading_complete() {
    menu_startup.loading_progress = 1.0f;
    menu_startup.loading_complete = 1;
}

void menu_request_startup_skip() {
    if (!menu_startup.loading_complete) return;
    if (menu_startup.phase == MENU_STARTUP_PHASE_TRANSITION || menu_startup.phase == MENU_STARTUP_PHASE_READY) return;

    menu_startup.skip_requested = 1;
}

int menu_should_render_background() {
    return menu_startup.phase != MENU_STARTUP_PHASE_LOADING;
}

int menu_is_startup_animation_complete() {
    return menu_startup.phase == MENU_STARTUP_PHASE_READY;
}

static void name_on_submit(AppContext* context, const char* text) {
	if (!context) return;

    printf("Name input submitted: %s\n", text ? text : "");
    if (context->player_name) {
        free(context->player_name);
        context->player_name = NULL;
    }
    if (text) {
        context->player_name = strdup(text);
        if (!context->player_name) {
            context->player_name = malloc(1);
            if (context->player_name) context->player_name[0] = '\0';
        }
        write_property("PLAYER_NAME", text);
    } else {
        context->player_name = NULL;
        write_property("PLAYER_NAME", "");
    }
}

static void code_on_submit(AppContext* context, const char* text) {
	if (!context) return;

    printf("Code input submitted: %s\n", text ? text : "");
    if (text) {
        char trame[20];
        format_to(trame, sizeof(trame), "%d %s %s", MSG_JOINLOBBY, text, context->player_name ? context->player_name : "NONE");
        send_tcp(context->sock, trame);
        context->player_role = ROLE_NONE;
        context->player_team = TEAM_NONE;
    }
}

static ButtonReturn menu_button_click(AppContext* context, Button* button) {
	if (!context || !button) return BTN_NONE;

    if (button == btn_join) {
        joining = 1;
    } else if (button == btn_social) {
        // social_open();
    } else if (button == btn_tuto) {
        tuto_open();
    } else if (button == btn_credits) {
        credits_open();
    } else if (button == btn_create) {
        char trame[20];
        format_to(trame, sizeof(trame), "%d %s", MSG_CREATELOBBY, context->player_name ? context->player_name : "NONE");
        send_tcp(context->sock, trame);
        context->player_role = ROLE_NONE;
        context->player_team = TEAM_NONE;
    } else if (button == btn_quit) {
        return BTN_MENU_QUIT;
    }
    return BTN_NONE;
}

// Création de bouttons optimisée pour le menu
static Button* menu_create_button(AppContext* context, int id, int x, int y, int h, const char* text, const char* hover_text) {
    if (!context) return NULL;

    ButtonConfig* cfg = button_config_init();
    if (!cfg) return NULL;

    cfg->x = x;
    cfg->y = y;
    cfg->h = h;
    cfg->font_path = FONT_LARABIE;
    cfg->color = COL_WHITE;
    cfg->hover_text = hover_text;
    cfg->text = text;
    cfg->callback = menu_button_click;

    Button* created = button_create(context->renderer, id, cfg);
    free(cfg);
    return created;
}

static int menu_init_buttons(AppContext* context) {
    int loading_fails = 0;

    btn_create = menu_create_button(context, BTN_CREATE_LOBBY, -300, -180, 100, "Créer", "Créer un lobby");
    if (!btn_create) loading_fails++;

    btn_join = menu_create_button(context, BTN_JOIN_LOBBY, 300, -180, 100, "Rejoindre", "Rejoindre un lobby existant");
    if (!btn_join) loading_fails++;

    btn_quit = menu_create_button(context, BTN_MENU_QUIT, 0, -400, 100, "Quitter", "Quitter le jeu");
    if (!btn_quit) loading_fails++;

    btn_social = menu_create_button(context, BTN_MENU_SOCIAL, 775, -275, 55, "Social", "Menu social (non implémenté)");
    if (!btn_social) loading_fails++;

    btn_tuto = menu_create_button(context, BTN_MENU_TUTO, 775, -400, 55, "Tutoriel", "Voir le tutoriel");
    if (!btn_tuto) loading_fails++;

    btn_credits = menu_create_button(context, BTN_MENU_CREDITS, 775, -475, 55, "Crédits", "Voir les crédits");
    if (!btn_credits) loading_fails++;

    return loading_fails;
}

static int menu_init_name_input(AppContext* context) {
    if (!context) return EXIT_FAILURE;

    InputConfig* cfg_in_name = input_config_init();
    if (!cfg_in_name) return EXIT_FAILURE;

    cfg_in_name->x = 775;
    cfg_in_name->y = 450;
    cfg_in_name->w = 250;
    cfg_in_name->h = 60;
    cfg_in_name->font_path = FONT_LARABIE;
    cfg_in_name->font_size = 28;
    cfg_in_name->placeholders = NAME_PLACEHOLDERS;
    cfg_in_name->placeholder_count = 4;
    cfg_in_name->submitted_label = "Pseudo : ";
    cfg_in_name->maxlen = 16;
    cfg_in_name->save_player_data = 1;
    cfg_in_name->on_submit = name_on_submit;
    cfg_in_name->allowed_pattern = "^[a-zA-Z0-9_zéèêëàâäåæçîïìùûüÿœ~]*$";
    cfg_in_name->submit_pattern = "^[a-zA-Z0-9_zéèêëàâäåæçîïìùûüÿœ~]{3,16}$";
    cfg_in_name->bg_path = "assets/img/inputs/empty.png";
    cfg_in_name->bg_padding = 14;

    name_input = input_create(context->renderer, INPUT_NAME, cfg_in_name);
    free(cfg_in_name);

    return name_input ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int menu_init_code_input(AppContext* context) {
    if (!context) return EXIT_FAILURE;

    InputConfig* cfg_in_code = input_config_init();
    if (!cfg_in_code) return EXIT_FAILURE;

    cfg_in_code->x = 300;
    cfg_in_code->y = -180;
    cfg_in_code->w = 385;
    cfg_in_code->h = 100;
    cfg_in_code->font_path = FONT_LARABIE;
    cfg_in_code->font_size = 28;
    cfg_in_code->placeholders = CODE_PLACEHOLDERS;
    cfg_in_code->placeholder_count = 1;
    cfg_in_code->submitted_label = "Rejoindre : ";
    cfg_in_code->maxlen = 5;
    cfg_in_code->centered = 1;
    cfg_in_code->on_submit = code_on_submit;
    cfg_in_code->allowed_pattern = "^[0-9]$";
    cfg_in_code->submit_pattern = "^[0-9]{5}$";
    cfg_in_code->submit_sound = "assets/audio/sfx/input/submit.ogg";
    cfg_in_code->bg_path = "assets/img/inputs/empty.png";
    cfg_in_code->bg_padding = 24;

    code_input = input_create(context->renderer, INPUT_JOIN_CODE, cfg_in_code);
    free(cfg_in_code);

    return code_input ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void menu_apply_random_player_name_if_needed(AppContext* context) {
    if (!context) return;
    if (context->player_name && context->player_name[0] != '\0') return;

    char saved[INPUT_DEFAULT_MAX + 1] = {0};
    int has_saved = (read_property(saved, sizeof(saved), "PLAYER_NAME") == EXIT_SUCCESS && saved[0] != '\0');
    if (has_saved) return;

    FILE* uf = fopen("assets/misc/usernames.txt", "r");
    if (!uf) return;

    int line_count = 0;
    char line_buf[128];
    while (fgets(line_buf, sizeof(line_buf), uf)) line_count++;

    if (line_count > 0) {
        srand((unsigned int)SDL_GetTicks());
        int chosen = rand() % line_count;

        rewind(uf);
        int cur = 0;
        while (fgets(line_buf, sizeof(line_buf), uf)) {
            if (cur == chosen) {
                size_t len = strlen(line_buf);
                if (len > 0 && line_buf[len - 1] == '\n') line_buf[len - 1] = '\0';

                name_on_submit(context, line_buf);
                if (name_input) {
                    input_set_text(name_input, line_buf);
                    input_submit(context, name_input);
                }
                break;
            }
            cur++;
        }
    }

    fclose(uf);
}

ButtonReturn menu_handle_event(AppContext* context, SDL_Event* e) {
    if (!context || !e) return BTN_NONE;

    if (!menu_is_startup_animation_complete()) {
        return BTN_NONE;
    }

    if (tuto_is_active()) {
        tuto_handle_event(context, e);
        return BTN_NONE;
    }

    if (credits_is_active()) {
        credits_handle_event(context, e);
        return BTN_NONE;
    }

    if (name_input) input_handle_event(context, name_input, e);
    if (code_input && joining) input_handle_event(context, code_input, e);

    ButtonReturn ret = BTN_NONE;
    ButtonReturn r = BTN_NONE;

    if (btn_create) {
        r = button_handle_event(context, btn_create, e);
        if (r != BTN_NONE) ret = r;
    }
    if (btn_join && !joining) {
        r = button_handle_event(context, btn_join, e);
        if (r != BTN_NONE) ret = r;
    }
    if (btn_quit) {
        r = button_handle_event(context, btn_quit, e);
        if (r != BTN_NONE) ret = r;
    }
    if (btn_social) {
        r = button_handle_event(context, btn_social, e);
        if (r != BTN_NONE) ret = r;
    }
    if (btn_tuto) {
        r = button_handle_event(context, btn_tuto, e);
        if (r != BTN_NONE) ret = r;
    }
    if (btn_credits) {
        r = button_handle_event(context, btn_credits, e);
        if (r != BTN_NONE) ret = r;
    }

    return ret;
}

int menu_init(AppContext* context) {
    if (!context) return EXIT_FAILURE;

    int loading_fails = 0;

    menu_logo = load_image(context->renderer, "assets/img/others/logo_titre.png");
    if (!menu_logo) {
        printf("Failed to load menu logo image\n");
        loading_fails++;
    }

    txt_startup_loading = init_text(
        context,
        "chargement 0%",
        create_text_config(FONT_LARABIE, 34, COL_WHITE, 0, -210, 0, 255)
    );
    if (!txt_startup_loading) {
        loading_fails++;
    }

    const char* creator_line1_A_text = "NoamSQL (Roger Noam)    ";
    const char* creator_line1_B_text = "-    ~WolfGang_PRoxa~ (Piau Romain)";
    const char* creator_line2_A_text = "Quinton Chloé    ";
    const char* creator_line2_B_text = "-    KaptainePirate (Maudet Mathis)";

    int x_offset_1_A = 0;
    int x_offset_1_B = 0;
    int x_offset_2_A = 0;
    int x_offset_2_B = 0;

    TTF_Font* creators_font = load_font(FONT_GROOVELLO, 56);
    if (creators_font) {
        int line1_A_w = 0, line1_B_w = 0;
        int line2_A_w = 0, line2_B_w = 0;
        int unused_h = 0;

        if (
            TTF_SizeUTF8(creators_font, creator_line1_A_text, &line1_A_w, &unused_h) == 0 &&
            TTF_SizeUTF8(creators_font, creator_line1_B_text, &line1_B_w, &unused_h) == 0 &&
            TTF_SizeUTF8(creators_font, creator_line2_A_text, &line2_A_w, &unused_h) == 0 &&
            TTF_SizeUTF8(creators_font, creator_line2_B_text, &line2_B_w, &unused_h) == 0
        ) {
            int line1_total_w = line1_A_w + line1_B_w;
            int line2_total_w = line2_A_w + line2_B_w;

            x_offset_1_A = -(line1_total_w / 2) + (line1_A_w / 2);
            x_offset_1_B = -(line1_total_w / 2) + line1_A_w + (line1_B_w / 2);
            x_offset_2_A = -(line2_total_w / 2) + (line2_A_w / 2);
            x_offset_2_B = -(line2_total_w / 2) + line2_A_w + (line2_B_w / 2);
        }
    }

    txt_startup_creators_line1_A = init_text(
        context,
        creator_line1_A_text,
        create_text_config(FONT_GROOVELLO, 56, COL_WHITE, x_offset_1_A, MENU_CREATOR_LINE1_A_START_Y, 0, 255)
    );
    if (!txt_startup_creators_line1_A) {
        loading_fails++;
    }

    txt_startup_creators_line1_B = init_text(
        context,
        creator_line1_B_text,
        create_text_config(FONT_GROOVELLO, 56, COL_WHITE, x_offset_1_B, MENU_CREATOR_LINE1_B_START_Y, 0, 255)
    );
    if (!txt_startup_creators_line1_B) {
        loading_fails++;
    }

    txt_startup_creators_line2_A = init_text(
        context,
        creator_line2_A_text,
        create_text_config(FONT_GROOVELLO, 56, COL_WHITE, x_offset_2_A, MENU_CREATOR_LINE2_A_START_Y, 0, 255)
    );
    if (!txt_startup_creators_line2_A) {
        loading_fails++;
    }

    txt_startup_creators_line2_B = init_text(
        context,
        creator_line2_B_text,
        create_text_config(FONT_GROOVELLO, 56, COL_WHITE, x_offset_2_B, MENU_CREATOR_LINE2_B_START_Y, 0, 255)
    );
    if (!txt_startup_creators_line2_B) {
        loading_fails++;
    }

    loading_fails += menu_init_buttons(context);

    if (menu_init_name_input(context) != EXIT_SUCCESS) {
        loading_fails++;
    }

    if (name_input) {
        const char* loaded_name = input_get_text(name_input);
        if (loaded_name && loaded_name[0] != '\0') {
            input_submit(context, name_input);
        }
    }

    menu_apply_random_player_name_if_needed(context);

    if (name_input) {
        printf("Setting submit sound for name input\n");
        edit_in_cfg(INPUT_NAME, IN_CFG_SUBMIT_SOUND, (intptr_t)"assets/audio/sfx/input/submit.ogg");
    }

    if (menu_init_code_input(context) != EXIT_SUCCESS) {
        loading_fails++;
    }

    if (tuto_init(context) != EXIT_SUCCESS) {
        printf("Failed to initialize tutorial\n");
        loading_fails++;
    }

    if (credits_init(context) != EXIT_SUCCESS) {
        printf("Failed to initialize credits\n");
        loading_fails++;
    }

    menu_startup.phase = MENU_STARTUP_PHASE_LOADING;
    menu_startup.loading_progress = 0.0f;
    menu_startup.loading_complete = 0;
    menu_startup.skip_requested = 0;
    menu_startup.opening_audio_started = 0;
    menu_startup.transition_initialized = 0;
    menu_startup.startup_started_at_ms = SDL_GetTicks();
    menu_startup.opening_started_at_ms = 0;
    menu_startup.transition_started_at_ms = 0;

    memset(&ui_btn_create, 0, sizeof(ui_btn_create));
    memset(&ui_btn_join, 0, sizeof(ui_btn_join));
    memset(&ui_btn_quit, 0, sizeof(ui_btn_quit));
    memset(&ui_btn_social, 0, sizeof(ui_btn_social));
    memset(&ui_btn_tuto, 0, sizeof(ui_btn_tuto));
    memset(&ui_btn_credits, 0, sizeof(ui_btn_credits));
    memset(&ui_input_name, 0, sizeof(ui_input_name));
    memset(&ui_input_code, 0, sizeof(ui_input_code));

    return loading_fails;
}

void menu_display(AppContext* context) {
    if (!context || !context->lobby) return;

    menu_update_startup_state(context);

    if (context->lobby->id != -1) {
        context->app_state = APP_STATE_LOBBY;
        joining = 0;
    }

    if (
        menu_startup.phase == MENU_STARTUP_PHASE_TRANSITION &&
        !audio_is_playing(SOUND_OPENING_CODENAMES) &&
        !audio_is_playing(MUSIC_MENU_LOBBY)
    ) {
        audio_play_with_fade(MUSIC_MENU_LOBBY, -1, 1500, AUDIO_FADE_IN_BY_VOLUME, NULL);
    }

    if (menu_startup.phase == MENU_STARTUP_PHASE_LOADING) {
        Uint32 now = SDL_GetTicks();
        float logo_progress = menu_clamp01((float)(now - menu_startup.startup_started_at_ms) / (float)MENU_STARTUP_LOGO_FADE_MS);
        Uint8 logo_alpha = (Uint8)(255.0f * logo_progress);

        menu_draw_black_overlay(context, 255);

        menu_render_startup_logo(context, MENU_LOGO_INTRO_Y, MENU_LOGO_INTRO_SCALE, logo_alpha);

        if (logo_progress >= 1.0f) {
            menu_render_loading_bar(context);
        }

        return;
    }

    if (menu_startup.phase == MENU_STARTUP_PHASE_OPENING) {
        menu_render_opening_credits(context);
        return;
    }

    if (menu_startup.phase == MENU_STARTUP_PHASE_TRANSITION) {
        Uint32 now = SDL_GetTicks();
        float transition_progress = menu_clamp01((float)(now - menu_startup.transition_started_at_ms) / (float)MENU_STARTUP_TRANSITION_MS);
        float logo_progress = menu_clamp01((float)(now - menu_startup.transition_started_at_ms) / (float)MENU_STARTUP_LOGO_MOVE_MS);
        float bg_fade_progress = menu_clamp01((float)(now - menu_startup.transition_started_at_ms) / (float)MENU_STARTUP_BG_FADE_MS);

        float smooth_logo = menu_ease_out_back(logo_progress);
        float logo_scale = MENU_LOGO_OPENING_SCALE + (MENU_LOGO_FINAL_SCALE - MENU_LOGO_OPENING_SCALE) * smooth_logo;
        int logo_y = menu_lerp_int(MENU_LOGO_OPENING_Y, MENU_LOGO_FINAL_Y, smooth_logo);

        Uint8 overlay_alpha = (Uint8)(255.0f * (1.0f - bg_fade_progress));
        menu_draw_black_overlay(context, overlay_alpha);

        menu_apply_bounce(transition_progress);
        menu_render_startup_logo(context, logo_y, logo_scale, 255);
        menu_render_main_widgets(context);

        if (transition_progress >= 1.0f) {
            menu_finalize_ui_positions();
            menu_startup.phase = MENU_STARTUP_PHASE_READY;
        }
    } else {
        menu_render_startup_logo(context, MENU_LOGO_FINAL_Y, MENU_LOGO_FINAL_SCALE, 255);
        menu_render_main_widgets(context);
    }

    if (credits_is_active()) {
        credits_display(context);
        return;
    }

    if (tuto_is_active()) {
        tuto_display(context);
    }
}

int menu_free() {
    if (menu_logo) {
        free_image(menu_logo);
        menu_logo = NULL;
    }

    if (txt_startup_loading) {
        destroy_text(txt_startup_loading);
        txt_startup_loading = NULL;
    }

    if (txt_startup_creators_line1_A) {
        destroy_text(txt_startup_creators_line1_A);
        txt_startup_creators_line1_A = NULL;
    }

    if (txt_startup_creators_line1_B) {
        destroy_text(txt_startup_creators_line1_B);
        txt_startup_creators_line1_B = NULL;
    }

    if (txt_startup_creators_line2_A) {
        destroy_text(txt_startup_creators_line2_A);
        txt_startup_creators_line2_A = NULL;
    }

    if (txt_startup_creators_line2_B) {
        destroy_text(txt_startup_creators_line2_B);
        txt_startup_creators_line2_B = NULL;
    }

    if (btn_create) {
        button_destroy(btn_create);
        btn_create = NULL;
    }
    if (btn_join) {
        button_destroy(btn_join);
        btn_join = NULL;
    }
    if (btn_quit) {
        button_destroy(btn_quit);
        btn_quit = NULL;
    }
    if (btn_social) {
        button_destroy(btn_social);
        btn_social = NULL;
    }
    if (btn_tuto) {
        button_destroy(btn_tuto);
        btn_tuto = NULL;
    }
    if (btn_credits) {
        button_destroy(btn_credits);
        btn_credits = NULL;
    }

    if (name_input) {
        input_destroy(name_input);
        name_input = NULL;
    }
    if (code_input) {
        input_destroy(code_input);
        code_input = NULL;
    }

    credits_free();
    tuto_free();

    joining = 0;

    menu_startup.phase = MENU_STARTUP_PHASE_LOADING;
    menu_startup.loading_progress = 0.0f;
    menu_startup.loading_complete = 0;
    menu_startup.skip_requested = 0;
    menu_startup.opening_audio_started = 0;
    menu_startup.transition_initialized = 0;
    menu_startup.startup_started_at_ms = 0;
    menu_startup.opening_started_at_ms = 0;
    menu_startup.transition_started_at_ms = 0;

    memset(&ui_btn_create, 0, sizeof(ui_btn_create));
    memset(&ui_btn_join, 0, sizeof(ui_btn_join));
    memset(&ui_btn_quit, 0, sizeof(ui_btn_quit));
    memset(&ui_btn_social, 0, sizeof(ui_btn_social));
    memset(&ui_btn_tuto, 0, sizeof(ui_btn_tuto));
    memset(&ui_btn_credits, 0, sizeof(ui_btn_credits));
    memset(&ui_input_name, 0, sizeof(ui_input_name));
    memset(&ui_input_code, 0, sizeof(ui_input_code));

    return EXIT_SUCCESS;
}