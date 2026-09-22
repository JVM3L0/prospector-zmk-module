#include "robot_face.h"

#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <zmk/endpoints.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/wpm.h>

#include "display_colors.h"

/* ---- Tunables ---------------------------------------------------------- */

#define EYE_WIDTH_NORMAL      34
#define EYE_HEIGHT_NORMAL     40
#define EYE_HEIGHT_LOW        26   /* "worried" - narrower eyes            */
#define EYE_HEIGHT_CRITICAL   14   /* half-closed                          */
#define EYE_BLINK_HEIGHT      4

#define BLINK_DURATION_MS     120
#define BLINK_MIN_INTERVAL_MS 2500
#define BLINK_MAX_INTERVAL_MS 6000

#define WPM_ALERT_THRESHOLD   40
#define WPM_WIDEN_MS          150

#define LOW_BATTERY_THRESHOLD      20
#define CRITICAL_BATTERY_THRESHOLD 8

#ifndef PERIPHERAL_COUNT
#define PERIPHERAL_COUNT ZMK_SPLIT_BLE_PERIPHERAL_COUNT
#endif

enum robot_mood {
    ROBOT_MOOD_NORMAL,
    ROBOT_MOOD_LOW_BATTERY,
    ROBOT_MOOD_CRITICAL_BATTERY,
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static struct k_work_delayable blink_work;

static uint8_t peripheral_battery[PERIPHERAL_COUNT] = {0};
static bool peripheral_connected[PERIPHERAL_COUNT] = {false};
static uint8_t central_battery = 100;

static enum zmk_transport active_transport = ZMK_TRANSPORT_USB;
static bool ble_connected = false;

/* ---- Mood ---------------------------------------------------------------
 * Mood is driven by the lowest known battery level across the central
 * device and all connected peripherals.
 */

static uint8_t lowest_battery_level(void) {
    uint8_t lowest = central_battery;
    for (int i = 0; i < PERIPHERAL_COUNT; i++) {
        if (peripheral_connected[i] && peripheral_battery[i] < lowest) {
            lowest = peripheral_battery[i];
        }
    }
    return lowest;
}

static enum robot_mood mood_for_level(uint8_t level) {
    if (level <= CRITICAL_BATTERY_THRESHOLD) {
        return ROBOT_MOOD_CRITICAL_BATTERY;
    } else if (level <= LOW_BATTERY_THRESHOLD) {
        return ROBOT_MOOD_LOW_BATTERY;
    }
    return ROBOT_MOOD_NORMAL;
}

static void apply_mood(struct zmk_widget_robot_face *widget, enum robot_mood mood) {
    lv_color_t color;
    int32_t height;

    switch (mood) {
    case ROBOT_MOOD_CRITICAL_BATTERY:
        color = lv_color_hex(DISPLAY_COLOR_ROBOT_EYES_CRITICAL);
        height = EYE_HEIGHT_CRITICAL;
        break;
    case ROBOT_MOOD_LOW_BATTERY:
        color = lv_color_hex(DISPLAY_COLOR_ROBOT_EYES_LOW);
        height = EYE_HEIGHT_LOW;
        break;
    default:
        color = lv_color_hex(DISPLAY_COLOR_ROBOT_EYES_NORMAL);
        height = EYE_HEIGHT_NORMAL;
        break;
    }

    lv_obj_t *eyes[2] = {widget->eye_left, widget->eye_right};
    for (int i = 0; i < 2; i++) {
        lv_obj_set_style_bg_color(eyes[i], color, LV_PART_MAIN);
        lv_obj_set_height(eyes[i], height);
        lv_obj_set_style_radius(eyes[i], height / 2, LV_PART_MAIN);
    }
}

static void refresh_all_moods(void) {
    enum robot_mood mood = mood_for_level(lowest_battery_level());
    struct zmk_widget_robot_face *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        apply_mood(widget, mood);
    }
}

/* ---- Blinking ------------------------------------------------------------
 * Periodically squashes both eyes down to a thin line and back, at a
 * randomized interval, independent of mood/battery state.
 */

static void blink_height_anim_cb(void *var, int32_t value) {
    lv_obj_set_height((lv_obj_t *)var, value);
}

static void start_blink_anim(lv_obj_t *eye, int32_t open_height) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, eye);
    lv_anim_set_values(&anim, open_height, EYE_BLINK_HEIGHT);
    lv_anim_set_time(&anim, BLINK_DURATION_MS / 2);
    lv_anim_set_playback_time(&anim, BLINK_DURATION_MS / 2);
    lv_anim_set_exec_cb(&anim, blink_height_anim_cb);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_start(&anim);
}

static void blink_work_handler(struct k_work *work) {
    enum robot_mood mood = mood_for_level(lowest_battery_level());
    int32_t open_height = (mood == ROBOT_MOOD_CRITICAL_BATTERY) ? EYE_HEIGHT_CRITICAL
                         : (mood == ROBOT_MOOD_LOW_BATTERY)     ? EYE_HEIGHT_LOW
                                                                 : EYE_HEIGHT_NORMAL;

    struct zmk_widget_robot_face *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        start_blink_anim(widget->eye_left, open_height);
        start_blink_anim(widget->eye_right, open_height);
    }

    uint32_t next_ms = BLINK_MIN_INTERVAL_MS +
                        (sys_rand32_get() % (BLINK_MAX_INTERVAL_MS - BLINK_MIN_INTERVAL_MS));
    k_work_schedule(&blink_work, K_MSEC(next_ms));
}

/* ---- Antenna (USB / BLE indicator) --------------------------------------- */

static void antenna_pulse_anim_cb(void *var, int32_t value) {
    lv_obj_set_style_bg_opa((lv_obj_t *)var, value, LV_PART_MAIN);
}

static void apply_antenna_state(struct zmk_widget_robot_face *widget) {
    lv_anim_del(widget->antenna_ball, antenna_pulse_anim_cb);

    if (active_transport == ZMK_TRANSPORT_USB) {
        lv_obj_set_style_bg_color(widget->antenna_ball,
                                   lv_color_hex(DISPLAY_COLOR_ROBOT_ANTENNA_USB), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(widget->antenna_ball, LV_OPA_COVER, LV_PART_MAIN);
    } else if (ble_connected) {
        lv_obj_set_style_bg_color(widget->antenna_ball,
                                   lv_color_hex(DISPLAY_COLOR_ROBOT_ANTENNA_BLE), LV_PART_MAIN);

        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, widget->antenna_ball);
        lv_anim_set_values(&anim, LV_OPA_40, LV_OPA_COVER);
        lv_anim_set_time(&anim, 900);
        lv_anim_set_playback_time(&anim, 900);
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&anim, antenna_pulse_anim_cb);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
        lv_anim_start(&anim);
    } else {
        lv_obj_set_style_bg_color(widget->antenna_ball,
                                   lv_color_hex(DISPLAY_COLOR_ROBOT_ANTENNA_OFF), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(widget->antenna_ball, LV_OPA_COVER, LV_PART_MAIN);
    }
}

static void refresh_all_antennas(void) {
    struct zmk_widget_robot_face *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        apply_antenna_state(widget);
    }
}

/* ---- WPM reaction: eyes widen briefly when typing fast ------------------- */

static void wpm_width_anim_cb(void *var, int32_t value) {
    lv_obj_set_width((lv_obj_t *)var, value);
}

static void widen_eyes(struct zmk_widget_robot_face *widget, bool widen) {
    int32_t target = widen ? (EYE_WIDTH_NORMAL + 6) : EYE_WIDTH_NORMAL;

    lv_obj_t *eyes[2] = {widget->eye_left, widget->eye_right};
    for (int i = 0; i < 2; i++) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, eyes[i]);
        lv_anim_set_values(&anim, lv_obj_get_width(eyes[i]), target);
        lv_anim_set_time(&anim, WPM_WIDEN_MS);
        lv_anim_set_exec_cb(&anim, wpm_width_anim_cb);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_start(&anim);
    }
}

/* ---- Event listeners ------------------------------------------------------ */

struct battery_update_state {
    uint8_t source; /* 0xFF = central */
    uint8_t level;
};

struct connection_update_state {
    uint8_t source;
    bool connected;
};

struct endpoint_update_state {
    enum zmk_transport transport;
    bool ble_connected;
};

struct wpm_update_state {
    uint8_t wpm;
};

static void robot_battery_update_cb(struct battery_update_state state) {
    if (state.source == 0xFF) {
        central_battery = state.level;
    } else if (state.source < PERIPHERAL_COUNT) {
        peripheral_battery[state.source] = state.level;
    }
    refresh_all_moods();
}

static struct battery_update_state robot_battery_get_state(const zmk_event_t *eh) {
    if (eh == NULL) {
        return (struct battery_update_state){.source = 0xFF, .level = zmk_battery_state_of_charge()};
    }

    const struct zmk_battery_state_changed *central_ev = as_zmk_battery_state_changed(eh);
    if (central_ev) {
        return (struct battery_update_state){.source = 0xFF, .level = central_ev->state_of_charge};
    }

    const struct zmk_peripheral_battery_state_changed *periph_ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (periph_ev) {
        return (struct battery_update_state){.source = periph_ev->source,
                                              .level = periph_ev->state_of_charge};
    }

    return (struct battery_update_state){.source = 0xFF, .level = central_battery};
}

static void robot_connection_update_cb(struct connection_update_state state) {
    if (state.source < PERIPHERAL_COUNT) {
        peripheral_connected[state.source] = state.connected;
    }
    refresh_all_moods();
}

static struct connection_update_state robot_connection_get_state(const zmk_event_t *eh) {
    if (eh == NULL) {
        return (struct connection_update_state){.source = 0, .connected = false};
    }
    const struct zmk_split_central_status_changed *ev = as_zmk_split_central_status_changed(eh);
    if (!ev) {
        return (struct connection_update_state){.source = 0, .connected = false};
    }
    return (struct connection_update_state){.source = ev->slot, .connected = ev->connected};
}

static void robot_endpoint_update_cb(struct endpoint_update_state state) {
    active_transport = state.transport;
    ble_connected = state.ble_connected;
    refresh_all_antennas();
}

static struct endpoint_update_state robot_endpoint_get_state(const zmk_event_t *eh) {
    struct zmk_endpoint_instance selected = zmk_endpoint_get_selected();
    return (struct endpoint_update_state){
        .transport = selected.transport,
        .ble_connected = zmk_ble_active_profile_is_connected(),
    };
}

static void robot_wpm_update_cb(struct wpm_update_state state) {
    struct zmk_widget_robot_face *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widen_eyes(widget, state.wpm >= WPM_ALERT_THRESHOLD);
    }
}

static struct wpm_update_state robot_wpm_get_state(const zmk_event_t *eh) {
    return (struct wpm_update_state){.wpm = zmk_wpm_get_state()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_robot_face_battery, struct battery_update_state,
                            robot_battery_update_cb, robot_battery_get_state)
ZMK_SUBSCRIPTION(widget_robot_face_battery, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(widget_robot_face_battery, zmk_peripheral_battery_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_robot_face_connection, struct connection_update_state,
                            robot_connection_update_cb, robot_connection_get_state)
ZMK_SUBSCRIPTION(widget_robot_face_connection, zmk_split_central_status_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_robot_face_endpoint, struct endpoint_update_state,
                            robot_endpoint_update_cb, robot_endpoint_get_state)
ZMK_SUBSCRIPTION(widget_robot_face_endpoint, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_robot_face_endpoint, zmk_ble_active_profile_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_robot_face_wpm, struct wpm_update_state,
                            robot_wpm_update_cb, robot_wpm_get_state)
ZMK_SUBSCRIPTION(widget_robot_face_wpm, zmk_wpm_state_changed);

/* ---- Construction --------------------------------------------------------- */

static lv_obj_t *create_eye(lv_obj_t *parent, int x) {
    lv_obj_t *eye = lv_obj_create(parent);
    lv_obj_set_size(eye, EYE_WIDTH_NORMAL, EYE_HEIGHT_NORMAL);
    lv_obj_set_pos(eye, x, 0);
    lv_obj_set_style_radius(eye, EYE_HEIGHT_NORMAL / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(eye, lv_color_hex(DISPLAY_COLOR_ROBOT_EYES_NORMAL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(eye, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(eye, 0, LV_PART_MAIN);
    return eye;
}

int zmk_widget_robot_face_init(struct zmk_widget_robot_face *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 180, 110);
    lv_obj_set_style_bg_color(widget->obj, lv_color_hex(DISPLAY_COLOR_ROBOT_HEAD_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(widget->obj, lv_color_hex(DISPLAY_COLOR_ROBOT_HEAD_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(widget->obj, 36, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    widget->eye_left = create_eye(widget->obj, 40);
    lv_obj_align(widget->eye_left, LV_ALIGN_LEFT_MID, 20, 4);

    widget->eye_right = create_eye(widget->obj, 106);
    lv_obj_align(widget->eye_right, LV_ALIGN_RIGHT_MID, -20, 4);

    /* Antenna, anchored above the head */
    widget->antenna_stick = lv_obj_create(widget->obj);
    lv_obj_set_size(widget->antenna_stick, 4, 18);
    lv_obj_set_style_bg_color(widget->antenna_stick, lv_color_hex(DISPLAY_COLOR_ROBOT_HEAD_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->antenna_stick, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->antenna_stick, 0, LV_PART_MAIN);
    lv_obj_align(widget->antenna_stick, LV_ALIGN_TOP_MID, 0, -18);

    widget->antenna_ball = lv_obj_create(widget->obj);
    lv_obj_set_size(widget->antenna_ball, 16, 16);
    lv_obj_set_style_radius(widget->antenna_ball, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->antenna_ball, 0, LV_PART_MAIN);
    lv_obj_align_to(widget->antenna_ball, widget->antenna_stick, LV_ALIGN_OUT_TOP_MID, 0, 0);

    if (sys_slist_is_empty(&widgets)) {
        central_battery = zmk_battery_state_of_charge();
        struct zmk_endpoint_instance selected = zmk_endpoint_get_selected();
        active_transport = selected.transport;
        ble_connected = zmk_ble_active_profile_is_connected();

        k_work_init_delayable(&blink_work, blink_work_handler);
        k_work_schedule(&blink_work, K_MSEC(BLINK_MIN_INTERVAL_MS));
    }

    sys_slist_append(&widgets, &widget->node);

    apply_mood(widget, mood_for_level(lowest_battery_level()));
    apply_antenna_state(widget);

    widget_robot_face_battery_init();
    widget_robot_face_connection_init();
    widget_robot_face_endpoint_init();
    widget_robot_face_wpm_init();

    return 0;
}

lv_obj_t *zmk_widget_robot_face_obj(struct zmk_widget_robot_face *widget) {
    return widget->obj;
}
