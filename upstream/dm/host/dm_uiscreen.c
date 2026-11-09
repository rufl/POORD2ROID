// Front-end pages and loading retain the game's 16:9 output. In gameplay the game's UI globals, touch layout,
// viewport, and scissor stay at the actual wide render dimensions; constraining only the Vulkan viewport to 16:9
// cropped the HUD and large panels because the Switch HUD layout anchors its widgets to the full screen.
// 前端与读取画面仍按游戏原生 16:9 输出。局内游戏 UI 全局尺寸、触摸布局、viewport 与 scissor 均保持真实宽屏出图尺寸；
// 只把 Vulkan viewport 限制到 16:9 会裁切 HUD 和大面板，因为 Switch 手柄 HUD 布局以整屏为锚点。
// Coordinates used by input helpers remain identity-mapped when the full-frame UI rectangle is active.
#include "dm_uiscreen.h"
#include "dm_uiscreen_geometry.h"
#include "hle.h"
#include "dm.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static _Atomic int g_mode = -1;              // -1 = not decided (checks explicit UI setting and configured output aspect)
static _Atomic uint64_t g_surface_size, g_presented_size;
static _Atomic int g_rw, g_rh, g_uw, g_uh;
static uint8_t *main_base(void) { static uint8_t *b; if (!b) for (int i = 0; i < g_nmod; i++) if (!strcmp(g_mod[i].name, "main")) b = (uint8_t *)g_mod[i].base; return b; }
static int mode(void) { int m = atomic_load(&g_mode); if (m >= 0) return m;
    const char *e = getenv("DM_UI_169"), *p = getenv("DM_UI_YPOS"), *r = getenv("DM_GAME_RES"); unsigned w = 0, h = 0;
    int wide = r && sscanf(r, "%ux%u", &w, &h) == 2 && h && (uint64_t)w * 9 > (uint64_t)h * 16;
    m = e ? (strcmp(e, "0") ? p && !strcmp(p, "center") ? DM_UISCREEN_CENTER : p && !strcmp(p, "bottom") ? DM_UISCREEN_BOTTOM : DM_UISCREEN_TOP : DM_UISCREEN_OFF)
          : wide ? DM_UISCREEN_TOP : DM_UISCREEN_OFF;
    int exp = -1; atomic_compare_exchange_strong(&g_mode, &exp, m); return atomic_load(&g_mode); }
void dm_uiscreen_enable(int m) { atomic_store(&g_mode, m < DM_UISCREEN_OFF || m > DM_UISCREEN_BOTTOM ? DM_UISCREEN_OFF : m); }
// 局内 / 局外（前端各页是按 16:9 整页设计的——点击区、标题、3D 人物是一体——整页居中留边；真全屏只在局内开）。
// 唯一信号：dm_uiscreen_ingame()。定义 = HUDPanel 在且读取画面（LoadScreenPanel）不在，即读取画面两头都归局外：进局时读取画面撤掉后的第一帧起才算局内，
// 出局时读取画面一盖上来就算局外。由界面每帧那一处（dm_cursor.c，游戏主线程）锁存，宿主各处与图形层都只读这一个值。
// 界面矩形：局外居中，局内按前端选的摆法（贴底）。前端各页游戏本身就按 16:9 出图（见 dm_game_res），界面矩形 = 整个画面；居中只在「读取中、已是整屏分辨率」那一段用到。
// In a run / out of a run (the front end pages are designed as whole 16:9 pages - tap areas, titles and the 3D character are one piece - so the whole page is centered with margins; true fullscreen is on only in a run).
// The single signal: dm_uiscreen_ingame(). Definition = HUDPanel present and the loading screen (LoadScreenPanel) absent, i.e. both ends of the loading screen count as out of a run: on entering, in-run starts from the first frame after the loading screen is removed,
// and on leaving, out-of-run starts as soon as the loading screen covers the view. It is latched at the per-frame UI site (dm_cursor.c, game main thread), and everywhere in the host and the graphics layer reads only this one value.
// UI rectangle: centered out of a run, placed as the front end chooses in a run (bottom-aligned). On the front end pages the game itself already renders at 16:9 (see dm_game_res), so UI rectangle = the whole frame; centering is only used in the "loading, already at full-screen resolution" stretch.
static _Atomic int g_ingame;
void (*g_dm_charsel_hook)(void);   // 退出一局回到前端那一刻（diag.c 界面消息 FrontEndNavigation:ToFrontEnd 里调） | The moment of quitting a run back to the front end (called in diag.c's UI message FrontEndNavigation:ToFrontEnd)
void (*g_dm_ingame_hook)(int on);   // 进局 / 出局那一刻（读取画面也算一次出局）；iOS：cloud_ios.m 拿「进局」记一局开始 | The moment of entering / leaving a run (the loading screen also counts as leaving once); iOS: cloud_ios.m uses "entering a run" to mark the start of a run
void dm_uiscreen_set_ingame(int on) { if (atomic_exchange(&g_ingame, on != 0) != (on != 0)) { LOG("界面画面：%s", on ? "进局（真全屏）" : "出局（留边）"); if (g_dm_ingame_hook) g_dm_ingame_hook(on != 0); } }
int dm_uiscreen_ingame(void) { return atomic_load(&g_ingame); }
// 局内开着大面板：整块界面移到正中、上下留同样宽的边，关掉回落贴底；不做过渡（直接跳）。
// 判据就是触摸前端的「面板模式」（摇杆收起、单指当光标的那个状态，front_ios.m 在界面消息回调里同步算出），只开着 NPC 对话菜单不算（菜单跟着 NPC 摆，留在贴底）。
// 消息回调与游戏排界面在同一线程、先后同一帧，所以开 / 关那一帧界面就在新位置；图形层下移、前端游戏视图、世界字投影、点世界换算都每帧现读 yoff，自然跟着走
// A large panel open in a run: the whole UI moves to the center with equal margins above and below, and drops back to bottom-aligned when closed; no transition (a direct jump).
// The criterion is the touch front end's "panel mode" (the state where the stick is hidden and a single finger acts as the cursor, computed synchronously by front_ios.m in the UI message callback); having only the NPC dialog menu open does not count (the menu is placed following the NPC and stays bottom-aligned).
// The message callback and the game's UI layout run on the same thread, one after the other in the same frame, so on the open / close frame the UI is already at the new position; the graphics layer shift, the front end game view, the world text projection and the world tap conversion all read yoff fresh every frame and follow along naturally
static _Atomic int g_panel;
int dm_uiscreen_panel(void) { return atomic_load(&g_panel); }
void dm_uiscreen_set_panel(int on) { if (atomic_exchange(&g_panel, on != 0) != (on != 0) && atomic_load(&g_ingame) && atomic_load(&g_rw)) LOG("界面画面：%s", on ? "开大面板 → 界面移到正中" : "大面板关了 → 界面回落"); }
static int yoff(void) { int rh = atomic_load(&g_rh), uh = atomic_load(&g_uh), m = mode(); if (!atomic_load(&g_rw) || rh <= uh || m == DM_UISCREEN_OFF) return 0;
    if (!atomic_load(&g_ingame) || atomic_load(&g_panel)) m = DM_UISCREEN_CENTER; return m == DM_UISCREEN_CENTER ? (rh - uh) / 2 : m == DM_UISCREEN_BOTTOM ? rh - uh : 0; }

// 跟着世界走的界面字（地上物品名、NPC 名字牌、血条、飘字）：游戏用镜头的「世界点 → 屏幕」main+0x1237c60(镜头, 点, out) 算出真实画面像素（y = [镜头+0x144] × (0.5 − 0.5·ndc_y)），
// 直接当界面坐标用（调用方只有 0x90d110 四处与 0xa71bb0）。界面整趟下移 y0 之后这些字就低了 y0（名字牌落到脚下）。
// 镜头对象同时是出图用的，投影矩阵（+0x80 起 4 列）每帧由 0x162f9ac 无条件重算，往对象里写值留不住也不该写。所以在函数入口（观测门 OBS_UI_PROJECT，只在 touch 规则域）
// 把 x0 换成镜头的一份副本：宽屏时水平投影系数乘「逻辑宽 / 实际宽」（20:9 时 0.8），显示更多横向场景，不动垂直缩放；y += (2·y0 / 高) × w 将画面下移 y0。游戏的镜头一个字节不动。y0 = 0 时不换。
// UI text that follows the world (ground item names, NPC nameplates, health bars, floating text): the game uses the camera's "world point → screen" main+0x1237c60(camera, point, out) to compute real frame pixels (y = [camera+0x144] × (0.5 − 0.5·ndc_y)),
// and uses them directly as UI coordinates (the only callers are four sites in 0x90d110 and 0xa71bb0). Once the whole UI pass is shifted down by y0, this text ends up y0 too low (nameplates land at the feet).
// The camera object is also used for rendering, and the projection matrix (4 columns from +0x80) is unconditionally recomputed every frame by 0x162f9ac, so writing it directly would not stick. At the observation gate (OBS_UI_PROJECT, touch-rule domain only),
// substitute a copy: multiply horizontal projection by logical_width / real_width (0.8 at 20:9) to reveal more scene without changing vertical scale, and add y += (2*y0 / height) * w to shift the image down by exactly y0. The game camera remains untouched; when y0 is zero, no copy is needed.
static _Atomic int g_proj_on;
static void ui_project_obs(const uint64_t *regs) {
    const uint8_t *c = (const uint8_t *)(uintptr_t)regs[0]; if (!c || c[0x160]) return;
    int rw = atomic_load(&g_rw), uw = atomic_load(&g_uw);
    int horizontal = atomic_load(&g_proj_on) && atomic_load(&g_ingame) && uw > 0 && rw > uw;
    int dy = atomic_load(&g_proj_on) && atomic_load(&g_ingame) ? yoff() : 0; if (!horizontal && !dy) return;
    float H = *(const float *)(c + 0x144); if (!(H > 0)) return;
    static _Thread_local _Alignas(16) uint8_t cam[0x170]; memcpy(cam, c, sizeof cam); float *m = (float *)(cam + 0x80);
    float sx = horizontal ? dm_ui_world_x_scale(uw, rw) : 1.f, sy = 2.0f * dy / H;
    ((uint64_t *)(uintptr_t)regs)[0] = (uint64_t)(uintptr_t)cam;
}
int dm_ui_proj_dy(void) { return atomic_load(&g_proj_on) && atomic_load(&g_ingame) ? yoff() : 0; }

// 游戏的出图分辨率（主机 / 掌机档各一个参数位，见 thread.c 与 hle_oe.c）：真全屏打开时按「玩家单位在不在」给值——不在（前端各页）= 屏宽的 16:9，
// 配置出图档位是局内分辨率。超宽档启动及菜单/读取页使用同宽的 16:9 高度，确保初始目标容纳两种模式。
// 建立玩家单位后通过现有性能模式通知切到配置的超宽分辨率；销毁后切回。游戏按各自比例排界面，呈现层不拉伸。
// The output profile is the in-world render size. For a configured aspect wider than 16:9, startup and
// menu/loading output uses the same width at 16:9 height, so the initial target can contain both modes.
// Player-unit creation switches to the configured wide size through the existing performance-mode notification;
// destroying it switches back. The game lays out UI for each aspect, and the presenter preserves it without stretch.
uint64_t front_game_res(int docked); void dm_oe_res_changed(void);
// 游戏处理 31 时拿「屏幕宽高」全局量和新分辨率比（main+0xe10e10），相同就不切。出局时这对量里还是我们写的 16:9 高，正好等于要切去的值，游戏会以为已经切好了。
// 所以在游戏取走 31 的那一刻（hle_oe.c，同一个线程、紧挨着它的比较之前）把真实高写回：同一帧里写回、比较、切换、游戏自己写成新值，中间没有一帧按 4:3 排界面。
// When handling 31 the game compares the "screen width / height" globals with the new resolution (main+0xe10e10) and does not switch if they are equal. On leaving a run this pair still holds the 16:9 height we wrote, which happens to equal the value to switch to, so the game would think the switch is already done.
// So at the moment the game takes 31 (hle_oe.c, same thread, right before its comparison) write the real height back: within the same frame come the write-back, comparison, switch, and the game writing the new value itself, with not a single frame laying out the UI at 4:3 in between.
void dm_uiscreen_res_reread(void) {
    uint8_t *b = main_base(); int rw = atomic_load(&g_rw); if (!b || !rw) return; int32_t *g = (int32_t *)(b + 0x211da38);
    if (g[0] == atomic_load(&g_uw) && g[1] == atomic_load(&g_uh)) { g[0] = rw; g[1] = atomic_load(&g_rh); }
    atomic_store(&g_rw, 0);
}
// In-game, preserve the actual wide dimensions for both UI layout and rendering. The game-provided HUD
// layout already uses full-screen anchors; dm_ui_rect is therefore the full render target and input mapping is identity.
// 局内 UI 排版与绘制均保留真实宽屏尺寸。游戏 HUD 布局以整屏为锚点，因此 dm_ui_rect 覆盖整个出图目标，输入映射为恒等变换。
static _Atomic int g_world, g_res_live;
uint64_t dm_game_res(int docked) {
    uint64_t r = front_game_res(docked); uint32_t w = (uint32_t)r, h = (uint32_t)(r >> 32);
    if (mode() != DM_UISCREEN_OFF && !atomic_load(&g_world)) {
        if ((uint64_t)w * 9 > (uint64_t)h * 16) h = (uint32_t)((uint64_t)w * 9 / 16) & ~1u;
        else if (atomic_load(&g_res_live) && (uint64_t)w * 9 < (uint64_t)h * 16) h = (uint32_t)((uint64_t)w * 9 / 16) & ~1u;
    }
    return (uint64_t)h << 32 | w;
}

void dm_uiscreen_pump(void) {
    uint8_t *b = main_base(); if (!b) return;
    { int world = ((uint64_t (*)(uint32_t))(b + 0x45f9d0))(*(uint32_t *)(b + 0x2d20e4c)) != 0;
      int first = !atomic_exchange(&g_res_live, 1);
      if ((atomic_exchange(&g_world, world) != world || (first && !world)) && mode() != DM_UISCREEN_OFF) { LOG("界面画面：%s → 游戏分辨率切到 %u x %u", world ? "玩家单位建立" : first ? "窗口已建好、在前端" : "玩家单位销毁", (unsigned)dm_game_res(1), (unsigned)(dm_game_res(1) >> 32)); dm_oe_res_changed(); } }
    static int reg; if (!reg) { reg = 1; if (gate_obs_present(OBS_UI_PROJECT)) { gate_obs_set(OBS_UI_PROJECT, ui_project_obs); atomic_store(&g_proj_on, 1); } }
    int32_t *g = (int32_t *)(b + 0x211da38); int m = mode(), rw = atomic_load(&g_rw);
    if (!atomic_load(&g_world) && m != DM_UISCREEN_OFF) return;
    if (m == DM_UISCREEN_OFF) {
        if (rw) { if (g[0] == atomic_load(&g_uw) && g[1] == atomic_load(&g_uh)) { g[0] = rw; g[1] = atomic_load(&g_rh); } atomic_store(&g_rw, 0); LOG("界面画面：关掉，屏幕宽高写回 %d x %d", g[0], g[1]); }
        return;
    }
    int w = g[0], h = g[1]; if (w <= 0 || h <= 0) return;
    if (rw && w == atomic_load(&g_uw) && h == atomic_load(&g_uh)) return;
    // Keep gameplay UI layout at the same dimensions as the world render target. The Switch HUD
    // layout uses full-screen anchors; shrinking only this logical canvas cuts off panels and HUD.
#ifdef __ANDROID__
    // Android gameplay UI must keep the full wide dimensions; the game HUD layout uses full-screen anchors.
    int uw = w, uh = h;
#else
    int uw = w, uh = h; if ((long)w * 9 < (long)h * 16) uh = (int)((long)w * 9 / 16) & ~1;
#endif
    atomic_store(&g_rw, w); atomic_store(&g_rh, h); atomic_store(&g_uw, uw); atomic_store(&g_uh, uh);
    g[0] = uw; g[1] = uh;
    LOG("界面画面：游戏屏幕 %d x %d → UI 逻辑画布 %d x %d，安全矩形 x=%d y=%d", w, h, uw, uh, (w - uw) / 2, yoff());
}
int dm_screen_real(int *w, int *h) {
    int rw = atomic_load(&g_rw); if (rw) { *w = rw; *h = atomic_load(&g_rh); return 1; }
    uint8_t *b = main_base(); if (!b) return 0; *w = *(int32_t *)(b + 0x211da38); *h = *(int32_t *)(b + 0x211da3c); return *w > 0 && *h > 0;
}
void dm_uiscreen_surface_size(int width, int height) {
    uint64_t size = width > 0 && height > 0 ? (uint64_t)(uint32_t)width << 32 | (uint32_t)height : 0;
    atomic_store(&g_surface_size, size);
}
void dm_uiscreen_presented_size(int width, int height) {
    uint64_t size = width > 0 && height > 0 ? (uint64_t)(uint32_t)width << 32 | (uint32_t)height : 0;
    atomic_store(&g_presented_size, size);
}
void dm_ui_screen_to_game(float screen_x, float screen_y, float *game_x, float *game_y) {
    uint64_t surface = atomic_load(&g_surface_size), image = atomic_load(&g_presented_size);
    int surface_w = (int)(surface >> 32), surface_h = (int)(uint32_t)surface;
    int source_w = (int)(image >> 32), source_h = (int)(uint32_t)image;
    if (!image) dm_screen_real(&source_w, &source_h);
    if (!source_w || !source_h || !surface_w || !surface_h) {
        *game_x = screen_x; *game_y = screen_y; return;
    }
    int x, y, width, height;
    dm_ui_fit_rect(source_w, source_h, surface_w, surface_h, &x, &y, &width, &height);
    *game_x = dm_ui_surface_to_source(screen_x, surface_w, x, width);
    *game_y = dm_ui_surface_to_source(screen_y, surface_h, y, height);
}
void dm_ui_rect(int *x, int *y, int *w, int *h) {
    int rw = 0, rh = 0; dm_screen_real(&rw, &rh); int uw = atomic_load(&g_rw) ? atomic_load(&g_uw) : rw;
    *x = (rw - uw) / 2; *y = yoff(); *w = uw; *h = atomic_load(&g_rw) ? atomic_load(&g_uh) : rh;
}
float dm_ui_nx(float nx) { int x,y,w,h,rw=0,rh=0; dm_ui_rect(&x,&y,&w,&h); if (!dm_screen_real(&rw,&rh)) return nx; return dm_ui_safe_forward(nx,x,w,rw); }
float dm_ui_nx_back(float nx) { int x,y,w,h,rw=0,rh=0; dm_ui_rect(&x,&y,&w,&h); if (!dm_screen_real(&rw,&rh)) return nx; return dm_ui_safe_inverse(nx,x,w,rw); }
float dm_ui_ny(float ny) { int x,y,w,h,rw=0,rh=0; dm_ui_rect(&x,&y,&w,&h); if (!dm_screen_real(&rw,&rh)) return ny; return dm_ui_safe_forward(ny,y,h,rh); }
float dm_ui_ny_back(float ny) { int x,y,w,h,rw=0,rh=0; dm_ui_rect(&x,&y,&w,&h); if (!dm_screen_real(&rw,&rh)) return ny; return dm_ui_safe_inverse(ny,y,h,rh); }
