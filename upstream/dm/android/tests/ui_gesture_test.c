#include "../native/ui_gesture.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(dm_android_ui_gesture_kind(0, 30, 20, 1, 1) == 1);
    assert(dm_android_ui_gesture_kind(0, -30, 20, 1, 1) == 1);
    assert(dm_android_ui_gesture_kind(30, 0, 20, 1, 1) == 2);
    assert(dm_android_ui_gesture_kind(-30, 0, 20, 1, 1) == 2);
    assert(dm_android_ui_gesture_kind(30, 30, 20, 1, 1) == 0);
    assert(dm_android_ui_gesture_kind(20, 20, 20, 1, 1) == 0);
    assert(dm_android_ui_gesture_kind(0, 30, 20, 0, 1) == 0);
    assert(dm_android_ui_gesture_kind(30, 0, 20, 1, 0) == 0);
    assert(dm_android_ui_gesture_kind(0, 0, 20, 1, 1) == 0);
    puts("Android UI gesture classification: scroll, slider, threshold and permission boundaries pass");
    return 0;
}
