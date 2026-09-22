#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_robot_face {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *eye_left;
    lv_obj_t *eye_right;
    lv_obj_t *antenna_stick;
    lv_obj_t *antenna_ball;
};

int zmk_widget_robot_face_init(struct zmk_widget_robot_face *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_robot_face_obj(struct zmk_widget_robot_face *widget);
