#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <lvgl.h>

/*
 * Tela no estilo do painel de desempenho do Velksync. Só LVGL: o firmware
 * (velk_zmk.c) e o simulador alimentam os mesmos setters.
 */

#define VELK_HALVES 2

lv_obj_t *velk_ui_create(lv_obj_t *parent);

void velk_ui_set_output(bool usb, uint8_t profile_index, bool connected);
void velk_ui_set_battery(uint8_t half, uint8_t level, bool connected);
void velk_ui_set_caps(bool on);
void velk_ui_set_layer(const char *name, bool is_base);
void velk_ui_set_mods(bool ctrl, bool alt, bool gui, bool shift);
