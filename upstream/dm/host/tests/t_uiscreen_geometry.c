#include "../dm_uiscreen_geometry.h"
#include <assert.h>
#include <math.h>

int main(void) {
    assert(dm_ui_safe_forward(.5f, 0, 1920, 1920) == .5f);
    assert(dm_ui_safe_inverse(.5f, 0, 1920, 1920) == .5f);
    assert(dm_ui_safe_forward(0.f, 0, 1020, 1020) == 0.f);
    assert(dm_ui_safe_forward(1.f, 0, 1020, 1020) == 1.f);
    assert(dm_ui_safe_inverse(0.f, 0, 1020, 1020) == 0.f);
    assert(dm_ui_safe_inverse(1.f, 0, 1020, 1020) == 1.f);

    {
        int w, h, x, y, dw, dh;
        dm_ui_handheld_size(1280, 576, &w, &h);
        assert(w == 1020 && h == 459);
        dm_ui_handheld_size(1280, 720, &w, &h);
        assert(w == 1024 && h == 576);

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

    }

    return 0;
}
