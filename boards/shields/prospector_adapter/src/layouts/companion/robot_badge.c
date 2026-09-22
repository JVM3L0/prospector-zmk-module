#include "robot_badge.h"

#include <ctype.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/keycode_state_changed.h>
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
#include <zmk/events/caps_word_state_changed.h>
#endif
#include <zmk/keymap.h>
#include <zmk/endpoints.h>
#include <zmk/ble.h>
#include <zmk/hid.h>

#include <fonts.h>
#include <modifier_order.h>
#include "display_colors.h"

LV_FONT_DECLARE(FG_Medium_26);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
static bool caps_word_active = false;
#endif

/* ---- Layer name ----------------------------------------------------------- */

struct layer_update_state {
    uint8_t index;
};

static void robot_layer_update_cb(struct layer_update_state state) {
    struct zmk_widget_robot_badge *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        const char *layer_name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state.index));
        char display_name[24];

        if (layer_name && *layer_name) {
            snprintf(display_name, sizeof(display_name), "%s", layer_name);
        } else {
            snprintf(display_name, sizeof(display_name), "L%d", state.index);
        }

#if IS_ENABLED(CONFIG_PROSPECTOR_LAYER_NAME_UPPERCASE)
        for (int i = 0; display_name[i]; i++) {
            display_name[i] = toupper((unsigned char)display_name[i]);
        }
#endif
        lv_label_set_text(widget->layer_label, display_name);
    }
}

static struct layer_update_state robot_layer_get_state(const zmk_event_t *eh) {
    return (struct layer_update_state){.index = zmk_keymap_highest_layer_active()};
}

/* ---- Output text (USB / BLE n) --------------------------------------------- */

static void update_output_label(struct zmk_widget_robot_badge *widget) {
    struct zmk_endpoint_instance selected = zmk_endpoint_get_selected();
    char text[16];

    if (selected.transport == ZMK_TRANSPORT_USB) {
        snprintf(text, sizeof(text), "USB");
    } else {
        bool connected = zmk_ble_active_profile_is_connected();
        snprintf(text, sizeof(text), "BLE %d%s", selected.ble.profile_index + 1,
                 connected ? "" : "...");
    }
    lv_label_set_text(widget->output_label, text);
}

static void robot_output_update_cb(struct layer_update_state state) {
    ARG_UNUSED(state);
    struct zmk_widget_robot_badge *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        update_output_label(widget);
    }
}

static struct layer_update_state robot_output_get_state(const zmk_event_t *eh) {
    return (struct layer_update_state){0};
}

/* ---- Modifiers + caps word -------------------------------------------------- */

struct mod_update_state {
    bool mods[4];
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
    bool caps_word;
#endif
};

static void robot_mod_update_cb(struct mod_update_state state) {
    struct zmk_widget_robot_badge *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        for (int i = 0; i < 4; i++) {
            enum modifier_type type = modifier_order_get(i);
            lv_color_t color = state.mods[type] ? lv_color_hex(DISPLAY_COLOR_ROBOT_MOD_ACTIVE)
                                                 : lv_color_hex(DISPLAY_COLOR_ROBOT_MOD_INACTIVE);
            lv_obj_set_style_bg_color(widget->mod_dots[i], color, LV_PART_MAIN);
        }

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
        if (state.caps_word) {
            lv_obj_clear_flag(widget->caps_hat, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widget->caps_hat, LV_OBJ_FLAG_HIDDEN);
        }
#endif
    }
}

static struct mod_update_state robot_mod_get_state(const zmk_event_t *eh) {
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
    if (eh != NULL) {
        const struct zmk_caps_word_state_changed *ev = as_zmk_caps_word_state_changed(eh);
        if (ev != NULL) {
            caps_word_active = ev->active;
        }
    }
#endif

    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();
    struct mod_update_state state = {
        .mods = {false, false, false, false},
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
        .caps_word = caps_word_active,
#endif
    };

    state.mods[MOD_TYPE_GUI] = (mods & (MOD_LGUI | MOD_RGUI)) != 0;
    state.mods[MOD_TYPE_ALT] = (mods & (MOD_LALT | MOD_RALT)) != 0;
    state.mods[MOD_TYPE_CTRL] = (mods & (MOD_LCTL | MOD_RCTL)) != 0;
    state.mods[MOD_TYPE_SHIFT] = (mods & (MOD_LSFT | MOD_RSFT)) != 0;

    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_robot_badge_layer, struct layer_update_state,
                            robot_layer_update_cb, robot_layer_get_state)
ZMK_SUBSCRIPTION(widget_robot_badge_layer, zmk_layer_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_robot_badge_output, struct layer_update_state,
                            robot_output_update_cb, robot_output_get_state)
ZMK_SUBSCRIPTION(widget_robot_badge_output, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_robot_badge_output, zmk_ble_active_profile_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_robot_badge_mods, struct mod_update_state,
                            robot_mod_update_cb, robot_mod_get_state)
ZMK_SUBSCRIPTION(widget_robot_badge_mods, zmk_keycode_state_changed);
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
ZMK_SUBSCRIPTION(widget_robot_badge_mods, zmk_caps_word_state_changed);
#endif

/* ---- Construction ------------------------------------------------------------ */

int zmk_widget_robot_badge_init(struct zmk_widget_robot_badge *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 180, 90);
    lv_obj_set_style_bg_color(widget->obj, lv_color_hex(DISPLAY_COLOR_ROBOT_BODY_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(widget->obj, lv_color_hex(DISPLAY_COLOR_ROBOT_HEAD_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(widget->obj, 24, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    /* Layer "badge" text, centered */
    widget->layer_label = lv_label_create(widget->obj);
    lv_label_set_text(widget->layer_label, "");
    lv_obj_set_style_text_font(widget->layer_label, &FG_Medium_26, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->layer_label, lv_color_hex(DISPLAY_COLOR_ROBOT_TEXT), LV_PART_MAIN);
    lv_obj_align(widget->layer_label, LV_ALIGN_TOP_MID, 0, 12);

    /* Output text, small, below the layer badge */
    widget->output_label = lv_label_create(widget->obj);
    lv_label_set_text(widget->output_label, "");
    lv_obj_set_style_text_font(widget->output_label, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->output_label, lv_color_hex(DISPLAY_COLOR_ROBOT_TEXT_DIM), LV_PART_MAIN);
    lv_obj_align(widget->output_label, LV_ALIGN_TOP_MID, 0, 46);

    /* Modifier dots along the bottom edge */
    int dot_size = 10;
    int dot_gap = 18;
    int total_width = 4 * dot_size + 3 * dot_gap;
    int start_x = (180 - total_width) / 2;

    for (int i = 0; i < 4; i++) {
        widget->mod_dots[i] = lv_obj_create(widget->obj);
        lv_obj_set_size(widget->mod_dots[i], dot_size, dot_size);
        lv_obj_set_style_radius(widget->mod_dots[i], dot_size / 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(widget->mod_dots[i], lv_color_hex(DISPLAY_COLOR_ROBOT_MOD_INACTIVE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(widget->mod_dots[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(widget->mod_dots[i], 0, LV_PART_MAIN);
        lv_obj_set_pos(widget->mod_dots[i], start_x + i * (dot_size + dot_gap), 68);
    }

    /* Caps word "hat", hidden unless active */
    widget->caps_hat = lv_obj_create(widget->obj);
    lv_obj_set_size(widget->caps_hat, 60, 10);
    lv_obj_set_style_bg_color(widget->caps_hat, lv_color_hex(DISPLAY_COLOR_ROBOT_CAPS_WORD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->caps_hat, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->caps_hat, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(widget->caps_hat, 4, LV_PART_MAIN);
    lv_obj_align(widget->caps_hat, LV_ALIGN_TOP_MID, 0, -8);
    lv_obj_add_flag(widget->caps_hat, LV_OBJ_FLAG_HIDDEN);

    sys_slist_append(&widgets, &widget->node);

    update_output_label(widget);

    widget_robot_badge_layer_init();
    widget_robot_badge_output_init();
    widget_robot_badge_mods_init();

    return 0;
}

lv_obj_t *zmk_widget_robot_badge_obj(struct zmk_widget_robot_badge *widget) {
    return widget->obj;
}