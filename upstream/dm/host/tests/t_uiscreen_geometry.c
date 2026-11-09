#include "../dm_uiscreen_geometry.h"
#include <assert.h>
#include <math.h>

static void expect_size(int rw, int rh, int want_w, int want_h) {
    int w, h;
    dm_ui_safe_size(rw, rh, &w, &h);
    assert(w == want_w && h == want_h);
}

int main(void) {
    expect_size(2316, 1080, 1920, 1080);
    expect_size(2340, 1080, 1920, 1080);
    expect_size(1920, 1080, 1920, 1080);
    expect_size(1440, 1080, 1440, 810);
    expect_size(0, 1080, 0, 0);
    expect_size(1920, 0, 0, 0);

    assert(dm_ui_safe_forward(.5f, 198, 1920, 2316) == .5f);
    assert(fabsf(dm_ui_safe_forward(198.f / 2316.f, 198, 1920, 2316)) < 1e-6f);
    assert(fabsf(dm_ui_safe_forward(2118.f / 2316.f, 198, 1920, 2316) - 1.f) < 1e-6f);
    assert(dm_ui_safe_forward(0.f, 0, 1920, 1920) == 0.f);
    for (int i = 0; i <= 100; i++) {
        float ui = i / 100.f;
        float real = dm_ui_safe_inverse(ui, 198, 1920, 2316);
        assert(fabsf(dm_ui_safe_forward(real, 198, 1920, 2316) - ui) < 1e-6f);
    }
    assert(dm_ui_safe_forward(0.f, 198, 1920, 2316) < 0.f);
    assert(dm_ui_safe_forward(1.f, 198, 1920, 2316) > 1.f);
    assert(dm_ui_draw_signature(4, 2, 20));
    assert(!dm_ui_draw_signature(3, 2, 20));
    assert(!dm_ui_draw_signature(4, 1, 20));
    assert(!dm_ui_draw_signature(4, 2, 16));
    {
        float vx, vy, vw, vh;
        dm_ui_safe_viewport(0, 0, 816, 459, 102, 0, 1020, 459, 1020, 459,
                            &vx, &vy, &vw, &vh);
        assert(vx == 102 && vy == 0 && vw == 816 && vh == 459);
        int sx, sy, sw, sh;
        dm_ui_safe_scissor(0, 0, 1020, 459, 102, 0, 816, 459,
                           1020, 459, 1020, 459, &sx, &sy, &sw, &sh);
        assert(sx == 102 && sy == 0 && sw == 816 && sh == 459);
    }

    {
        float x, y, w, h;
        dm_ui_safe_viewport(0, 0, 1024, 576, 128, 0, 1280, 576, 1280, 576,
                            &x, &y, &w, &h);
        assert(fabsf(dm_ui_world_x_scale(816, 1020) - 0.8f) < 1e-6f);
        assert(fabsf(w / h - 16.f / 9.f) < 1e-6f);
        dm_ui_safe_viewport(0, 0, 1024, 576, 128, 0, 1280, 576, 1920, 864,
                            &x, &y, &w, &h);
        assert(x == 192 && y == 0 && w == 1536 && h == 864);
    }
    {
        float x, y, w, h;
        dm_ui_safe_viewport(20, 10, 400, 200, 102, 0, 1020, 459, 2400, 1080,
                            &x, &y, &w, &h);
        assert(x == 287 && y == 24 && w == 941 && h == 470);
    }
    {
        int x, y, w, h;
        dm_ui_safe_scissor(0, 0, 1024, 576, 128, 0, 1024, 576,
                           1280, 576, 1280, 576, &x, &y, &w, &h);
        assert(x == 128 && y == 0 && w == 1024 && h == 576);
        dm_ui_safe_scissor(-12, 0, 80, 576, 128, 0, 1024, 576,
                           1280, 576, 1280, 576, &x, &y, &w, &h);
        assert(x == 128 && y == 0 && w == 68 && h == 576);
        dm_ui_safe_scissor(1000, 0, 50, 576, 128, 0, 1024, 576,
                           1280, 576, 1280, 576, &x, &y, &w, &h);
        assert(x == 1128 && y == 0 && w == 24 && h == 576);
        dm_ui_safe_scissor(0, 0, 1024, 576, 128, 0, 1024, 576,
                           1280, 576, 1920, 864, &x, &y, &w, &h);
        assert(x == 192 && y == 0 && w == 1536 && h == 864);
    }

    {
        int w, h, x, y, dw, dh;
        dm_ui_handheld_size(1280, 576, &w, &h);
        assert(w == 1020 && h == 459);
        dm_ui_handheld_size(1280, 720, &w, &h);
        assert(w == 1024 && h == 576);
        expect_size(1020, 459, 816, 459);

        dm_ui_fit_rect(1020, 459, 2400, 1080, &x, &y, &dw, &dh);
        assert(x == 0 && y == 0 && dw == 2400 && dh == 1080);
        dm_ui_fit_rect(1920, 1080, 2400, 1080, &x, &y, &dw, &dh);
        assert(x == 240 && y == 0 && dw == 1920 && dh == 1080);
        assert(fabsf(dm_ui_surface_to_source(0.f, 2400, x, dw) + .125f) < 1e-6f);
        assert(fabsf(dm_ui_surface_to_source(240.f / 2400.f, 2400, x, dw)) < 1e-6f);
        assert(fabsf(dm_ui_surface_to_source(1200.f / 2400.f, 2400, x, dw) - .5f) < 1e-6f);
        assert(fabsf(dm_ui_surface_to_source(2160.f / 2400.f, 2400, x, dw) - 1.f) < 1e-6f);
        assert(fabsf(dm_ui_surface_to_source(1.f, 2400, x, dw) - 1.125f) < 1e-6f);
        assert(fabsf(dm_ui_surface_to_source(.1f, 2400, x, dw)) < 1e-6f);
        assert(fabsf(dm_ui_surface_to_source(.5f, 2400, x, dw) - .5f) < 1e-6f);
        assert(fabsf(dm_ui_surface_to_source(.9f, 2400, x, dw) - 1.f) < 1e-6f);

        dm_ui_fit_rect(1024, 460, 2400, 1080, &x, &y, &dw, &dh);
        assert(x == 0 && y == 1 && dw == 2400 && dh == 1078);
        assert(fabsf(dm_ui_surface_to_source(0.f, 1080, y, dh) + 1.f / 1078.f) < 1e-6f);
        assert(fabsf(dm_ui_surface_to_source(1.f, 1080, y, dh) - 1079.f / 1078.f) < 1e-6f);

        float vx, vy, vw, vh;
        dm_ui_safe_viewport(0, 0, 816, 459, 102, 0, 1020, 459, 2400, 1080,
                            &vx, &vy, &vw, &vh);
        assert(fabsf(vx - 240.f) < .01f && fabsf(vy) < .01f &&
               fabsf(vw - 1920.f) < .01f && fabsf(vh - 1080.f) < .01f);
        int sx, sy, sw, sh;
        dm_ui_safe_scissor(0, 0, 816, 459, 102, 0, 816, 459,
                           1020, 459, 2400, 1080, &sx, &sy, &sw, &sh);
        assert(sx == 240 && sy == 0 && sw == 1920 && sh == 1080);
    }

    return 0;
}
