/*
 * Liga os eventos do ZMK aos setters do velk_ui.c. Os listeners rodam na
 * thread do display, então podem mexer no LVGL direto.
 */
#include <stdio.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/ble.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
#include <zmk/hid_indicators.h>
#include <zmk/events/hid_indicators_changed.h>
#endif

#include "velk_ui.h"

/* ---- Baterias e conexão das metades -------------------------------------------- */

static uint8_t battery_level[VELK_HALVES];
static bool half_connected[VELK_HALVES];

struct half_state {
    uint8_t slot;
};

static void half_update_cb(struct half_state state) {
    if (state.slot < VELK_HALVES) {
        velk_ui_set_battery(state.slot, battery_level[state.slot], half_connected[state.slot]);
    }
}

static struct half_state half_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *bat =
        as_zmk_peripheral_battery_state_changed(eh);
    if (bat && bat->source < VELK_HALVES) {
        battery_level[bat->source] = bat->state_of_charge;
        /* Recebeu bateria: a metade está conectada. */
        half_connected[bat->source] = true;
        return (struct half_state){.slot = bat->source};
    }

    const struct zmk_split_central_status_changed *conn = as_zmk_split_central_status_changed(eh);
    if (conn && conn->slot < VELK_HALVES) {
        half_connected[conn->slot] = conn->connected;
        return (struct half_state){.slot = conn->slot};
    }

    return (struct half_state){.slot = VELK_HALVES};
}

ZMK_DISPLAY_WIDGET_LISTENER(velk_half, struct half_state, half_update_cb, half_get_state)
ZMK_SUBSCRIPTION(velk_half, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(velk_half, zmk_split_central_status_changed);

/* ---- Saída: USB ou perfil BLE -------------------------------------------------- */

struct output_state {
    bool usb;
    uint8_t profile;
    bool connected;
};

static void output_update_cb(struct output_state state) {
    velk_ui_set_output(state.usb, state.profile, state.connected);
}

static struct output_state output_get_state(const zmk_event_t *eh) {
    struct zmk_endpoint_instance selected = zmk_endpoint_get_selected();
    return (struct output_state){
        .usb = selected.transport == ZMK_TRANSPORT_USB,
        .profile = zmk_ble_active_profile_index(),
        .connected = zmk_ble_active_profile_is_connected(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(velk_output, struct output_state, output_update_cb, output_get_state)
ZMK_SUBSCRIPTION(velk_output, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(velk_output, zmk_ble_active_profile_changed);

/* ---- Camada ------------------------------------------------------------------------ */

struct layer_state {
    uint8_t index;
};

static void layer_update_cb(struct layer_state state) {
    const char *name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state.index));
    char fallback[8];
    if (!name || !*name) {
        snprintf(fallback, sizeof(fallback), "L%d", state.index);
        name = fallback;
    }
    velk_ui_set_layer(name, state.index == 0);
}

static struct layer_state layer_get_state(const zmk_event_t *eh) {
    return (struct layer_state){.index = zmk_keymap_highest_layer_active()};
}

ZMK_DISPLAY_WIDGET_LISTENER(velk_layer, struct layer_state, layer_update_cb, layer_get_state)
ZMK_SUBSCRIPTION(velk_layer, zmk_layer_state_changed);

/* ---- Modificadores ---------------------------------------------------------------- */

struct mods_state {
    zmk_mod_flags_t mods;
};

static void mods_update_cb(struct mods_state state) {
    velk_ui_set_mods(state.mods & (MOD_LCTL | MOD_RCTL), state.mods & (MOD_LALT | MOD_RALT),
                     state.mods & (MOD_LGUI | MOD_RGUI), state.mods & (MOD_LSFT | MOD_RSFT));
}

static struct mods_state mods_get_state(const zmk_event_t *eh) {
    return (struct mods_state){.mods = zmk_hid_get_explicit_mods()};
}

ZMK_DISPLAY_WIDGET_LISTENER(velk_mods, struct mods_state, mods_update_cb, mods_get_state)
ZMK_SUBSCRIPTION(velk_mods, zmk_keycode_state_changed);

/* ---- Caps Lock do computador (LED do HID) ------------------------------------- */

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
/* Bit do Caps Lock no relatório de LEDs do HID (Num = 0, Caps = 1, Scroll = 2). */
#define CAPS_LOCK_BIT BIT(1)

struct caps_state {
    bool on;
};

static void caps_update_cb(struct caps_state state) { velk_ui_set_caps(state.on); }

static struct caps_state caps_get_state(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    zmk_hid_indicators_t indicators =
        ev ? ev->indicators : zmk_hid_indicators_get_current_profile();
    return (struct caps_state){.on = indicators & CAPS_LOCK_BIT};
}

ZMK_DISPLAY_WIDGET_LISTENER(velk_caps, struct caps_state, caps_update_cb, caps_get_state)
ZMK_SUBSCRIPTION(velk_caps, zmk_hid_indicators_changed);
#endif

/* ---- Início ------------------------------------------------------------------------ */

void velk_zmk_init(void) {
    velk_half_init();
    velk_output_init();
    velk_layer_init();
    velk_mods_init();
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    velk_caps_init();
#endif
}
