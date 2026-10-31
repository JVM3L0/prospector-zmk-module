#include <lvgl.h>

#include "velk_ui.h"

void velk_zmk_init(void);

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    velk_ui_create(screen);
    velk_zmk_init();

    return screen;
}
