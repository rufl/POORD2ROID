#pragma once
#include <stdint.h>

static inline int64_t dm_ui_round_pixel(float value) {
    return (int64_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}
static inline int64_t dm_ui_scale_pixel(int64_t position, int target, int full) {
    if (full <= 0) return position;
    int64_t numerator = position * target;
    return numerator >= 0 ? (numerator + full / 2) / full : (numerator - full / 2) / full;
}

static inline void dm_ui_safe_size(int rw, int rh, int *uw, int *uh) {
    if (rw <= 0 || rh <= 0) { *uw = *uh = 0; return; }
    if ((int64_t)rw * 9 > (int64_t)rh * 16) {
        *uw = (int)((int64_t)rh * 16 / 9) & ~1;
        *uh = rh;
    } else if ((int64_t)rw * 9 < (int64_t)rh * 16) {
        *uw = rw;
        *uh = (int)((int64_t)rw * 9 / 16) & ~1;
    } else {
        *uw = rw;
        *uh = rh;
    }
}
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
// The established NVN UI pass uses four vertices, two active attributes,
// and a 20-byte vertex stride. Its viewport is the full render target, not
// the 16:9 logical UI canvas.
static inline int dm_ui_draw_signature(int vertex_count, int attribute_count, int stride) {
    return vertex_count == 4 && attribute_count == 2 && stride == 20;
}


static inline void dm_ui_safe_viewport(float vx, float vy, float vw, float vh,
                                       int origin_x, int origin_y, int full_w, int full_h,
                                       int target_w, int target_h,
                                       float *x, float *y, float *w, float *h) {
    int64_t left = dm_ui_round_pixel(vx), top = dm_ui_round_pixel(vy);
    int64_t right = dm_ui_round_pixel(vx + vw), bottom = dm_ui_round_pixel(vy + vh);
    int64_t left_px = dm_ui_scale_pixel(origin_x + left, target_w, full_w);
    int64_t top_px = dm_ui_scale_pixel(origin_y + top, target_h, full_h);
    int64_t right_px = dm_ui_scale_pixel(origin_x + right, target_w, full_w);
    int64_t bottom_px = dm_ui_scale_pixel(origin_y + bottom, target_h, full_h);
    *x = (float)left_px;
    *y = (float)top_px;
    *w = (float)(right_px - left_px);
    *h = (float)(bottom_px - top_px);
}

static inline void dm_ui_safe_scissor(int x, int y, int w, int h,
                                      int origin_x, int origin_y, int safe_w, int safe_h,
                                      int full_w, int full_h, int target_w, int target_h,
                                      int *out_x, int *out_y, int *out_w, int *out_h) {
    int64_t right = (int64_t)x + (w > 0 ? w : safe_w);
    int64_t bottom = (int64_t)y + (h > 0 ? h : safe_h);
    int64_t left_px = dm_ui_scale_pixel((int64_t)origin_x + x, target_w, full_w);
    int64_t top_px = dm_ui_scale_pixel((int64_t)origin_y + y, target_h, full_h);
    int64_t right_px = dm_ui_scale_pixel((int64_t)origin_x + right, target_w, full_w);
    int64_t bottom_px = dm_ui_scale_pixel((int64_t)origin_y + bottom, target_h, full_h);
    int64_t safe_left = dm_ui_scale_pixel(origin_x, target_w, full_w);
    int64_t safe_top = dm_ui_scale_pixel(origin_y, target_h, full_h);
    int64_t safe_right = dm_ui_scale_pixel((int64_t)origin_x + safe_w, target_w, full_w);
    int64_t safe_bottom = dm_ui_scale_pixel((int64_t)origin_y + safe_h, target_h, full_h);
    if (left_px < safe_left) left_px = safe_left;
    if (top_px < safe_top) top_px = safe_top;
    if (right_px > safe_right) right_px = safe_right;
    if (bottom_px > safe_bottom) bottom_px = safe_bottom;
    if (left_px < 0) left_px = 0;
    if (top_px < 0) top_px = 0;
    if (right_px > target_w) right_px = target_w;
    if (bottom_px > target_h) bottom_px = target_h;
    if (right_px < left_px) right_px = left_px;
    if (bottom_px < top_px) bottom_px = top_px;
    *out_x = (int)left_px;
    *out_y = (int)top_px;
    *out_w = (int)(right_px - left_px);
    *out_h = (int)(bottom_px - top_px);
}
