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
        dm_ui_handheld_size(1600, 900, &w, &h);
        assert(w == 1280 && h == 720);
        dm_ui_handheld_size(1600, 720, &w, &h);
        assert(w == 1280 && h == 576);

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
    {
        int x, y, w, h;
        const int stale_crop[4] = {0, 0, 1280, 576};
        const int invalid_crop[4] = {0, 0, 1280, 721};
        // The fixed 1280x720 backing allocation contains only a 1280x576 active
        // gameplay frame; filling 20:9 must use that frame, not allocation padding.
        // The valid engine crop wins even if logical dimensions still report
        // the stale 1280x720 allocation during the gameplay transition.
        assert(dm_ui_present_source_rect(1280, 720, stale_crop, 1, 1280, 720,
                                         &x, &y, &w, &h) == 1);
        assert(x == 0 && y == 0 && w == 1280 && h == 576);
        dm_ui_fit_rect(w, h, 2400, 1080, &x, &y, &w, &h);
        assert(x == 0 && y == 0 && w == 2400 && h == 1080);

        // If the active dimensions exceed the backing image, preserve the valid
        // engine crop or fall back to the complete backing image.
        assert(dm_ui_present_source_rect(1280, 720, stale_crop, 1, 1600, 900,
                                         &x, &y, &w, &h) == 1);
        assert(dm_ui_present_source_rect(1280, 720, stale_crop, 0, 1280, 576,
                                         &x, &y, &w, &h) == 1);
        assert(x == 0 && y == 0 && w == 1280 && h == 576);
        assert(dm_ui_present_source_rect(1280, 720, invalid_crop, 1, 1280, 576,
                                         &x, &y, &w, &h) == 3);
        assert(x == 0 && y == 0 && w == 1280 && h == 576);
        assert(dm_ui_present_source_rect(1280, 720, invalid_crop, 0, 0, 0,
                                         &x, &y, &w, &h) == 2);
        assert(x == 0 && y == 0 && w == 1280 && h == 720);
        assert(dm_ui_present_source_rect(1280, 576, stale_crop, 1, 1280, 576,
                                         &x, &y, &w, &h) == 1);
        dm_ui_fit_rect(w, h, 2400, 1080, &x, &y, &w, &h);
        assert(x == 0 && y == 0 && w == 2400 && h == 1080);
    }
    {
        int x, y, w, h;
        const int docked_crop[4] = {0, 0, 1280, 576};
        const int handheld_crop[4] = {0, 0, 1020, 459};
        // Gameplay fills 20:9 in both profiles: normalized touch coordinates
        // are identity at left/center/right and top/center/bottom.
        assert(dm_ui_present_source_rect(1280, 720, docked_crop, 1, 1280, 720, &x, &y, &w, &h) == 1);
        dm_ui_fit_rect(w, h, 2400, 1080, &x, &y, &w, &h);
        assert(x == 0 && y == 0 && w == 2400 && h == 1080);
        assert(dm_ui_surface_to_source(.5f, 2400, x, w) == .5f);
        assert(dm_ui_surface_to_source(.5f, 1080, y, h) == .5f);
        assert(dm_ui_present_source_rect(1020, 572, handheld_crop, 1, 1020, 572, &x, &y, &w, &h) == 1);
        dm_ui_fit_rect(w, h, 2400, 1080, &x, &y, &w, &h);
        assert(x == 0 && y == 0 && w == 2400 && h == 1080);
        assert(dm_ui_surface_to_source(0.f, 2400, x, w) == 0.f);
        assert(dm_ui_surface_to_source(1.f, 2400, x, w) == 1.f);

        // Handheld front-end backing is 1020x572 after even-height rounding.
        // Its 16:9-ish image is centered, with 237-pixel side bars; touches in
        // those bars map outside the game instead of activating edge widgets.
        dm_ui_fit_rect(1020, 572, 2400, 1080, &x, &y, &w, &h);
        assert(x == 237 && y == 0 && w == 1926 && h == 1080);
        assert(fabsf(dm_ui_surface_to_source(237.f / 2400.f, 2400, x, w)) < 1e-6f);
        assert(fabsf(dm_ui_surface_to_source(2163.f / 2400.f, 2400, x, w) - 1.f) < 1e-6f);
        assert(dm_ui_surface_to_source(0.f, 2400, x, w) < 0.f);
        assert(dm_ui_surface_to_source(1.f, 2400, x, w) > 1.f);
        assert(fabsf(dm_ui_surface_to_source(.5f, 2400, x, w) - .5f) < 1e-6f);
    }

    return 0;
}
