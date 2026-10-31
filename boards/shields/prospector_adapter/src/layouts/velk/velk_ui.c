#include "velk_ui.h"

#include <stdio.h>

#include "velk_theme.h"

LV_FONT_DECLARE(RF_SemiBold_22);
LV_FONT_DECLARE(RF_Bold_16);
LV_FONT_DECLARE(RF_Bold_18);
LV_FONT_DECLARE(RF_Bold_26);
LV_FONT_DECLARE(RF_Bold_32);
LV_FONT_DECLARE(MSR_Icons_18);
LV_FONT_DECLARE(MSR_Icons_22);
LV_IMAGE_DECLARE(velk_cookie);

/* Material Symbols Rounded (UTF-8). */
#define ICON_BLUETOOTH "\xEE\x86\xA7"     /* U+E1A7 */
#define ICON_USB "\xEE\x87\xA0"           /* U+E1E0 */
#define ICON_BATTERY_ALERT "\xEE\x86\x9C" /* U+E19C */
#define ICON_CAPSLOCK "\xEE\x8C\x98"      /* U+E318 */
#define ICON_LAYERS "\xEE\x94\xBB"        /* U+E53B */

static const char *const battery_icons[] = {
    "\xEE\xAF\x9C", /* battery_0_bar U+EBDC */
    "\xEF\x82\x9C", /* battery_1_bar U+F09C */
    "\xEF\x82\x9D", /* battery_2_bar U+F09D */
    "\xEF\x82\x9E", /* battery_3_bar U+F09E */
    "\xEF\x82\x9F", /* battery_4_bar U+F09F */
    "\xEF\x82\xA0", /* battery_5_bar U+F0A0 */
    "\xEF\x82\xA1", /* battery_6_bar U+F0A1 */
};

/* ---- Geometria (tela 280×240, já girada pelo display_rotate_init) ---------- */

/* 10 + 52 + 6 + 98 + 6 + 30 + 6 + 26 = 234: sobram 6 px embaixo. */
#define PAD_X 14
#define PAD_Y 10
#define GAP 6
#define CONTENT_W (280 - 2 * PAD_X)

#define HERO_H 52
#define GAUGE_SIZE 98
#define GAUGE_STROKE 9
#define CHIP_H 30
#define MODS_H 26

#define ARC_START 135
#define ARC_SWEEP 270
/* Espaço entre preenchido e trilho, como o ExpressiveArc: stroke × 1,6 / raio. */
#define ARC_GAP_DEG 18

/* ---- Cores -------------------------------------------------------------------- */

static struct {
    lv_color_t surface, text, sub, dim, crust, high;
    lv_color_t info, ok, danger, warn, primary, secondary;
    lv_color_t hero_bg, blob;
} col;

/* a → b pela fração k/255 (o Color.lerp do Velksync). */
static lv_color_t lerp(uint32_t a, lv_color_t b, uint8_t k) {
    return lv_color_mix(b, lv_color_hex(a), k);
}

static lv_color_t soften(uint32_t c) { return lerp(c, lv_color_hex(VELK_TEXT), VELK_SOFTEN); }

static void init_colors(void) {
    col.surface = lv_color_hex(VELK_SURFACE);
    col.text = lv_color_hex(VELK_TEXT);
    col.sub = lv_color_hex(VELK_SUB);
    col.crust = lv_color_hex(VELK_CRUST);
    col.high = lv_color_hex(VELK_HIGH);
    col.dim = lerp(VELK_SUB, col.surface, 102); /* 40% para o fundo */

    col.info = soften(VELK_BLUE);
    col.ok = soften(VELK_GREEN);
    col.danger = soften(VELK_RED);
    col.warn = soften(VELK_YELLOW);
    col.primary = soften(VELK_PRIMARY);
    col.secondary = col.primary;

    /* Contêiner tonal: o cartão puxado 14% para a cor da métrica. */
    col.hero_bg = lerp(VELK_CARD, col.info, VELK_HERO_TINT);
    col.blob = lv_color_mix(col.info, col.hero_bg, 56);
}

/* ---- Objetos ---------------------------------------------------------------- */

struct gauge {
    lv_obj_t *track, *fill, *pct, *unit, *icon, *name;
};

static struct {
    lv_obj_t *out_icon, *out_ring, *out_label, *out_value;
    struct gauge halves[VELK_HALVES];
    lv_obj_t *caps_chip, *caps_icon, *caps_text;
    lv_obj_t *layer_chip, *layer_icon, *layer_text;
    lv_obj_t *mods[4], *mod_labels[4];
} ui;

static lv_obj_t *box(lv_obj_t *parent) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, lv_color_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, "");
    return label;
}

static lv_obj_t *bare_arc(lv_obj_t *parent) {
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, GAUGE_SIZE, GAUGE_SIZE);
    lv_obj_set_style_arc_width(arc, GAUGE_STROKE, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    return arc;
}

static void create_hero(lv_obj_t *root) {
    lv_obj_t *hero = box(root);
    lv_obj_set_size(hero, CONTENT_W, HERO_H);
    lv_obj_set_pos(hero, PAD_X, PAD_Y);
    lv_obj_set_style_bg_color(hero, col.hero_bg, 0);
    lv_obj_set_style_bg_opa(hero, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hero, HERO_H / 2, 0);

    ui.out_ring = box(hero);
    lv_obj_set_size(ui.out_ring, 40, 40);
    lv_obj_align(ui.out_ring, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_radius(ui.out_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ui.out_ring, 3, 0);
    lv_obj_set_style_border_color(ui.out_ring, col.info, 0);

    ui.out_icon = text(ui.out_ring, &MSR_Icons_22, col.info);
    lv_obj_center(ui.out_icon);

    /* O ícone já diz Bluetooth ou USB: a linha única mostra o estado. */
    ui.out_label = text(hero, &RF_SemiBold_22, col.info);
    lv_obj_align(ui.out_label, LV_ALIGN_LEFT_MID, 58, 0);

    lv_obj_t *blob = lv_image_create(hero);
    lv_image_set_src(blob, &velk_cookie);
    lv_obj_set_style_image_recolor(blob, col.blob, 0);
    lv_obj_set_style_image_recolor_opa(blob, LV_OPA_COVER, 0);
    lv_obj_align(blob, LV_ALIGN_RIGHT_MID, -4, 0);

    /* Filho da forma: o alinhamento ao centro se refaz quando o texto muda. */
    ui.out_value = text(blob, &RF_Bold_26, col.info);
    lv_obj_center(ui.out_value);
}

static void create_gauge(lv_obj_t *root, struct gauge *g, int x, int y, const char *name) {
    lv_obj_t *cont = box(root);
    lv_obj_set_size(cont, GAUGE_SIZE, GAUGE_SIZE);
    lv_obj_set_pos(cont, x, y);

    g->track = bare_arc(cont);
    g->fill = bare_arc(cont);

    /* Número grande e "%" menor: "100%" inteiro em 32 px vazava do arco. */
    lv_obj_t *value = box(cont);
    lv_obj_set_size(value, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(value, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(value, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(value, 1, 0);
    lv_obj_align(value, LV_ALIGN_CENTER, 0, -4);

    g->pct = text(value, &RF_Bold_32, col.ok);
    g->unit = text(value, &RF_Bold_18, col.ok);
    lv_obj_set_style_pad_bottom(g->unit, 4, 0);

    /* Ícone + letra na abertura de baixo do arco. */
    lv_obj_t *row = box(cont);
    lv_obj_set_size(row, LV_SIZE_CONTENT, 18);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 2, 0);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Os ícones de bateria do Material são em pé: gira para deitar. */
    g->icon = text(row, &MSR_Icons_18, col.ok);
    lv_obj_set_size(g->icon, 18, 18);
    lv_obj_set_style_transform_pivot_x(g->icon, 9, 0);
    lv_obj_set_style_transform_pivot_y(g->icon, 9, 0);
    lv_obj_set_style_transform_rotation(g->icon, 900, 0);

    g->name = text(row, &RF_Bold_16, col.text);
    lv_label_set_text(g->name, name);
}

static lv_obj_t *create_chip(lv_obj_t *parent, lv_obj_t **icon, lv_obj_t **label, const char *glyph) {
    lv_obj_t *chip = box(parent);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, CHIP_H);
    lv_obj_set_style_radius(chip, CHIP_H / 2, 0);
    lv_obj_set_style_border_width(chip, 2, 0);
    lv_obj_set_style_pad_left(chip, 8, 0);
    lv_obj_set_style_pad_right(chip, 12, 0);
    lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(chip, 4, 0);

    *icon = text(chip, &MSR_Icons_22, col.text);
    lv_label_set_text(*icon, glyph);
    *label = text(chip, &RF_Bold_18, col.text);
    return chip;
}

/* Sempre visível: contorno colorido quando inativo, preenchido quando ativo. */
static void style_chip(lv_obj_t *chip, lv_obj_t *icon, lv_obj_t *label, bool on, lv_color_t color) {
    lv_obj_set_style_border_color(chip, color, 0);
    lv_obj_set_style_bg_color(chip, color, 0);
    lv_obj_set_style_bg_opa(chip, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_color_t fg = on ? col.crust : color;
    lv_obj_set_style_text_color(icon, fg, 0);
    lv_obj_set_style_text_color(label, fg, 0);
}

static void create_chips(lv_obj_t *root, int y) {
    lv_obj_t *row = box(root);
    lv_obj_set_size(row, CONTENT_W, CHIP_H);
    lv_obj_set_pos(row, PAD_X, y);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);

    ui.caps_chip = create_chip(row, &ui.caps_icon, &ui.caps_text, ICON_CAPSLOCK);
    lv_label_set_text(ui.caps_text, "CAPS");
    ui.layer_chip = create_chip(row, &ui.layer_icon, &ui.layer_text, ICON_LAYERS);
}

static void create_mods(lv_obj_t *root, int y) {
    static const char *const names[] = {"CTRL", "ALT", "GUI", "SHIFT"};

    lv_obj_t *row = box(root);
    lv_obj_set_size(row, CONTENT_W, MODS_H);
    lv_obj_set_pos(row, PAD_X, y);
    lv_obj_set_style_pad_hor(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    /* Mesmo preenchimento ativo ou não: a linha não pula ao apertar. */
    for (int i = 0; i < 4; i++) {
        ui.mods[i] = box(row);
        lv_obj_set_size(ui.mods[i], LV_SIZE_CONTENT, MODS_H);
        lv_obj_set_style_pad_hor(ui.mods[i], 8, 0);
        lv_obj_set_style_radius(ui.mods[i], MODS_H / 2, 0);
        lv_obj_set_style_bg_color(ui.mods[i], col.secondary, 0);
        lv_obj_set_flex_flow(ui.mods[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(ui.mods[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        ui.mod_labels[i] = text(ui.mods[i], &RF_Bold_18, col.dim);
        lv_label_set_text(ui.mod_labels[i], names[i]);
    }
}

lv_obj_t *velk_ui_create(lv_obj_t *parent) {
    init_colors();

    lv_obj_t *root = box(parent);
    lv_obj_set_size(root, 280, 240);
    lv_obj_set_style_bg_color(root, col.surface, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    create_hero(root);

    int gauges_y = PAD_Y + HERO_H + GAP;
    int col_w = (CONTENT_W - 8) / 2;
    int inset = (col_w - GAUGE_SIZE) / 2;
    create_gauge(root, &ui.halves[0], PAD_X + inset, gauges_y, "L");
    create_gauge(root, &ui.halves[1], PAD_X + col_w + 8 + inset, gauges_y, "R");

    int chips_y = gauges_y + GAUGE_SIZE + GAP;
    create_chips(root, chips_y);
    create_mods(root, chips_y + CHIP_H + GAP);

    velk_ui_set_output(false, 0, false);
    for (int i = 0; i < VELK_HALVES; i++) {
        velk_ui_set_battery(i, 0, false);
    }
    velk_ui_set_caps(false);
    velk_ui_set_layer("BASE", true);
    velk_ui_set_mods(false, false, false, false);

    return root;
}

/* ---- Setters ------------------------------------------------------------------ */

void velk_ui_set_output(bool usb, uint8_t profile_index, bool connected) {
    lv_label_set_text(ui.out_icon, usb ? ICON_USB : ICON_BLUETOOTH);
    if (usb) {
        lv_label_set_text(ui.out_label, "USB");
        /* Duas letras em 26 px não cabem na forma de 46 px. */
        lv_obj_set_style_text_font(ui.out_value, &RF_Bold_18, 0);
        lv_label_set_text(ui.out_value, "PC");
    } else {
        lv_label_set_text(ui.out_label, connected ? "Conectado" : "Sem conexao");
        lv_obj_set_style_text_font(ui.out_value, &RF_Bold_26, 0);
        lv_label_set_text_fmt(ui.out_value, "%d", profile_index + 1);
    }
}

void velk_ui_set_battery(uint8_t half, uint8_t level, bool connected) {
    if (half >= VELK_HALVES) {
        return;
    }
    struct gauge *g = &ui.halves[half];
    if (level > 100) {
        level = 100;
    }

    bool low = connected && level < VELK_LOW_BATTERY;
    lv_color_t color = !connected ? col.dim : low ? col.danger : col.ok;
    int filled = connected ? (ARC_SWEEP * level) / 100 : 0;

    if (filled > 0) {
        lv_arc_set_bg_angles(g->fill, ARC_START, (ARC_START + filled) % 360);
        lv_obj_set_style_arc_color(g->fill, color, LV_PART_MAIN);
        lv_obj_remove_flag(g->fill, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g->fill, LV_OBJ_FLAG_HIDDEN);
    }

    int track_start = ARC_START + (filled > 0 ? filled + ARC_GAP_DEG : 0);
    int track_end = ARC_START + ARC_SWEEP;
    if (track_start < track_end) {
        lv_arc_set_bg_angles(g->track, track_start % 360, track_end % 360);
        lv_obj_set_style_arc_color(g->track, lv_color_mix(color, col.high, 56), LV_PART_MAIN);
        lv_obj_remove_flag(g->track, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g->track, LV_OBJ_FLAG_HIDDEN);
    }

    if (connected) {
        lv_label_set_text_fmt(g->pct, "%d", level);
        lv_label_set_text(g->unit, "%");
        lv_label_set_text(g->icon, low ? ICON_BATTERY_ALERT
                                       : battery_icons[(level * 6 + 50) / 100]);
    } else {
        lv_label_set_text(g->pct, "--");
        lv_label_set_text(g->unit, "");
        lv_label_set_text(g->icon, battery_icons[0]);
    }
    lv_obj_set_style_text_color(g->pct, color, 0);
    lv_obj_set_style_text_color(g->unit, color, 0);
    lv_obj_set_style_text_color(g->icon, color, 0);
}

void velk_ui_set_caps(bool on) {
    style_chip(ui.caps_chip, ui.caps_icon, ui.caps_text, on, col.warn);
}

void velk_ui_set_layer(const char *name, bool is_base) {
    lv_label_set_text(ui.layer_text, name);
    style_chip(ui.layer_chip, ui.layer_icon, ui.layer_text, !is_base, col.primary);
}

void velk_ui_set_mods(bool ctrl, bool alt, bool gui, bool shift) {
    bool on[4] = {ctrl, alt, gui, shift};
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_bg_opa(ui.mods[i], on[i] ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(ui.mod_labels[i], on[i] ? col.crust : col.dim, 0);
    }
}
