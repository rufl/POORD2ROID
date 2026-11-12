#pragma once
#include <stdint.h>


static inline unsigned dm_ui_gcd(unsigned a, unsigned b) {
    while (b) { unsigned r = a % b; a = b; b = r; }
    return a;
}

// Scale by 4/5 while keeping the exact source aspect whenever the rounded ratio
// stays near 80%; for custom dimensions with a small gcd, scale each axis directly.
static inline void dm_ui_handheld_size(int width, int height, int *out_w, int *out_h) {
    if (width <= 0 || height <= 0) { *out_w = *out_h = 0; return; }
    unsigned gcd = dm_ui_gcd((unsigned)width, (unsigned)height);
    unsigned scale = (4 * gcd + 2) / 5;
    if (!scale || 20 * (uint64_t)scale < 15 * (uint64_t)gcd ||
        20 * (uint64_t)scale > 17 * (uint64_t)gcd) {
        *out_w = (int)(((int64_t)width * 4 + 2) / 5);
        *out_h = (int)(((int64_t)height * 4 + 2) / 5);
        return;
    }
    *out_w = (int)((uint64_t)width / gcd * scale);
    *out_h = (int)((uint64_t)height / gcd * scale);
}

// Scale up proportionally (rounding to even dimensions) so neither axis drops below the
// guest UI engine's 1280x720 minimum layout/scissor threshold.
static inline void dm_ui_clamp_min_size(int width, int height, int min_w, int min_h,
                                        int *out_w, int *out_h) {
    if (width <= 0 || height <= 0) { *out_w = *out_h = 0; return; }
    int w = width, h = height;
    if (min_w > 0 && w < min_w) {
        h = (int)(((int64_t)height * min_w + width / 2) / width);
        w = min_w;
    }
    if (min_h > 0 && h < min_h) {
        w = (int)(((int64_t)width * min_h + height / 2) / height);
        h = min_h;
    }
    w = (w + 1) & ~1;
    h = (h + 1) & ~1;
    if (min_w > 0 && w < min_w) w = (min_w + 1) & ~1;
    if (min_h > 0 && h < min_h) h = (min_h + 1) & ~1;
    *out_w = w;
    *out_h = h;
}

// Integer, aspect-preserving viewport fit. Round the dependent edge to the nearest
// pixel; exact aspect ratios (including 20:9 -> 2400x1080) remain exact.
static inline void dm_ui_fit_rect(int source_w, int source_h, int surface_w, int surface_h,
                                  int *x, int *y, int *width, int *height) {
    if (source_w <= 0 || source_h <= 0 || surface_w <= 0 || surface_h <= 0) {
        *x = *y = *width = *height = 0;
        return;
    }
    int64_t width_limited = (int64_t)surface_w * source_h;
    int64_t height_limited = (int64_t)surface_h * source_w;
    if (width_limited <= height_limited) {
        *width = surface_w;
        *height = (int)(((int64_t)surface_w * source_h + source_w / 2) / source_w);
    } else {
        *height = surface_h;
        *width = (int)(((int64_t)surface_h * source_w + source_h / 2) / source_h);
    }
    if (*width > surface_w) *width = surface_w;
    if (*height > surface_h) *height = surface_h;
    *x = (surface_w - *width) / 2;
    *y = (surface_h - *height) / 2;
}
// During wide gameplay, the NVN texture may retain its startup allocation after
// the game's active logical extent changes. Present only that active extent.
static inline int dm_ui_present_source_rect(int source_w, int source_h,
                                            const int crop[4], int wide_gameplay,
                                            int active_w, int active_h,
                                            int *x, int *y, int *width, int *height) {
    if (source_w <= 0 || source_h <= 0) {
        *x = *y = *width = *height = 0;
        return -1;
    }
    // WindowSetCrop describes the active frame inside a potentially larger
    // backing allocation. It is authoritative: logical UI globals can lag the
    // resolution transition by one pump and must never override a valid crop.
    if (crop && (crop[0] || crop[1] || crop[2] || crop[3]) &&
        crop[0] >= 0 && crop[1] >= 0 && crop[2] > 0 && crop[3] > 0 &&
        (int64_t)crop[0] + crop[2] <= source_w &&
        (int64_t)crop[1] + crop[3] <= source_h) {
        *x = crop[0]; *y = crop[1]; *width = crop[2]; *height = crop[3];
        return 1;
    }
    if (wide_gameplay && active_w > 0 && active_h > 0 &&
        active_w <= source_w && active_h <= source_h) {
        *x = *y = 0; *width = active_w; *height = active_h;
        return 3;
    }
    if (crop && (crop[0] || crop[1] || crop[2] || crop[3])) {
        *x = *y = 0; *width = source_w; *height = source_h;
        return 2;
    }
    *x = *y = 0; *width = source_w; *height = source_h;
    return 0;
}



// Surface-normalized pixel location -> normalized location inside the displayed image.
static inline float dm_ui_surface_to_source(float n, int surface_extent, int origin, int extent) {
    return surface_extent > 0 && extent > 0 ? (n * surface_extent - origin) / extent : n;
}


static inline float dm_ui_safe_forward(float n, int origin, int extent, int full) {
    return full > 0 && extent > 0 && extent != full ? (n * full - origin) / extent : n;
}

static inline float dm_ui_safe_inverse(float n, int origin, int extent, int full) {
    return full > 0 && extent > 0 && extent != full ? (n * extent + origin) / full : n;
}
// Perspective X scale must shrink when the render aspect widens, revealing
// more horizontal world without changing its vertical scale.
static inline float dm_ui_world_x_scale(int logical_w, int real_w) {
    return logical_w > 0 && real_w > 0 && real_w > logical_w
        ? (float)logical_w / (float)real_w : 1.0f;
}
