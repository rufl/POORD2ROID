#ifndef DM_ANDROID_UI_GESTURE_H
#define DM_ANDROID_UI_GESTURE_H

// Return 1 for vertical list scrolling, 2 for horizontal slider dragging.
// Item and tab touches never call this with permission enabled.
static inline int dm_android_ui_gesture_kind(float dx, float dy, float tolerance,
                                             int allow_scroll, int allow_slider) {
    float ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (ay > tolerance && ay > ax && allow_scroll) return 1;
    if (ax > tolerance && ax > ay && allow_slider) return 2;
    return 0;
}

#endif
