#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_robot_badge {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *layer_label;
    lv_obj_t *output_label;
    lv_obj_t *caps_hat;
    lv_obj_t *mod_dots[4];
};

int zmk_widget_robot_badge_init(struct zmk_widget_robot_badge *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_robot_badge_obj(struct zmk_widget_robot_badge *widget);
