#include <lvgl.h>

#include "robot_face.h"
#include "robot_badge.h"

#include "display_colors.h"

static struct zmk_widget_robot_face robot_face_widget;
static struct zmk_widget_robot_badge robot_badge_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(DISPLAY_COLOR_ROBOT_SCREEN_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    zmk_widget_robot_face_init(&robot_face_widget, screen);
    lv_obj_set_pos(zmk_widget_robot_face_obj(&robot_face_widget), 30, 40);

    zmk_widget_robot_badge_init(&robot_badge_widget, screen);
    lv_obj_set_pos(zmk_widget_robot_badge_obj(&robot_badge_widget), 30, 155);

    return screen;
}
