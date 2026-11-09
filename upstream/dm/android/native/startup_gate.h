#pragma once

static inline int dm_android_startup_ready(int has_window, int activity_resumed) {
    return has_window && activity_resumed;
}
