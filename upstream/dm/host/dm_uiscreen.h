// Android widescreen gameplay keeps the game's full-screen HUD layout and touch geometry aligned with the wide render target.
// Menus/loading retain their separate 16:9 output. Implementation and rationale: dm_uiscreen.c.
#pragma once
enum { DM_UISCREEN_OFF, DM_UISCREEN_TOP, DM_UISCREEN_CENTER, DM_UISCREEN_BOTTOM };
void dm_uiscreen_enable(int mode);           // Select UI placement mode; Android wide gameplay keeps full render dimensions.
void dm_uiscreen_pump(void);                 // Capture real screen dimensions; Android gameplay UI globals remain full-size.
int dm_screen_real(int *w, int *h);          // Real frame dimensions, independent of the UI layout.
void dm_uiscreen_surface_size(int width, int height);  // Current Vulkan output surface in pixels; supplied by Android window/swapchain.
void dm_uiscreen_presented_size(int width, int height); // Latest presented source image size (after crop); supplied by Vulkan presenter.
void dm_ui_screen_to_game(float screen_x, float screen_y, float *game_x, float *game_y); // Surface-normalized input -> displayed game-image normalized coordinates.
void dm_ui_rect(int *x, int *y, int *w, int *h);   // UI rectangle in real-frame pixels; full frame during gameplay.
float dm_ui_nx(float nx_real);               // Real-frame normalized X -> UI-rectangle normalized X.
float dm_ui_nx_back(float nx_ui);            // UI-rectangle normalized X -> real-frame normalized X.
float dm_ui_ny(float ny_real);               // Real-frame normalized Y -> UI-rectangle normalized Y.
float dm_ui_ny_back(float ny_ui);            // UI-rectangle normalized Y -> real-frame normalized Y.
int dm_ui_proj_dy(void);
int dm_uiscreen_ingame(void);                // 1 = 局内（真全屏）；0 = 局外（前端各页与读取画面：16:9 居中留边）。唯一信号，任意线程读 | 1 = in a run (true fullscreen); 0 = out of a run (front end pages and loading screen: 16:9 centered with margins). The single signal, readable from any thread
void dm_uiscreen_set_ingame(int on);          // 只许界面每帧那一处（dm_cursor.c）调 | May only be called from the per-frame UI site (dm_cursor.c)
void dm_uiscreen_set_panel(int on);           // 局内开着大面板（功能页 / 箱子 / 商人 / 魔盒…，= 触摸前端的面板模式）：界面矩形移到正中；关掉回落前端选的摆法。触摸前端在界面消息回调里调 | A large panel open in a run (function pages / stash / vendor / cube..., = the touch front end's panel mode): the UI rectangle moves to the center; when closed it drops back to the placement the front end chose. Called by the touch front end in the UI message callback
int dm_uiscreen_panel(void);                 // 上面那个状态：局内开着大面板 = 1 | The state above: a large panel open in a run = 1
#include <stdint.h>
uint64_t dm_game_res(int docked);            // 游戏出图分辨率参数位此刻该给的值（{u32 宽, u32 高} 打包）：前端的原生值，真全屏打开且不在局里时改成屏宽的 16:9 | The value the game's render resolution parameter slot should give right now (packed {u32 width, u32 height}): the front end's native value, changed to 16:9 of the screen width when true fullscreen is on and not in a run
void dm_uiscreen_res_reread(void);            // 只许 hle_oe.c 在游戏取走「性能模式变化」通知时调 | May only be called by hle_oe.c when the game takes the "performance mode changed" notification
