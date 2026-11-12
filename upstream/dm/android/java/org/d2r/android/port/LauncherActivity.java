// 启动界面：设置、数据包导入和存档操作；保留全屏背景，控制项放进适配横屏的侧边卡片。
// Launcher: settings, package import and save management over the existing full-screen artwork, with a
// landscape-adaptive control panel.
package org.d2r.android.port;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.provider.OpenableColumns;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public final class LauncherActivity extends Activity {
    // launch-settings.conf 由 native 严格解析（键白名单、失败即中止启动、且不接受任意路径），
    // 所以只有它认识的键才写进去；界面自己的状态另存一份，互不污染。
    // launch-settings.conf is parsed strictly by native (key whitelist, abort startup on failure, no arbitrary
    // paths allowed), so only keys it knows may go in. The launcher's own state lives in a separate file.
    private static final String CONFIG_NAME = "launch-settings.conf";
    private static final String STATE_NAME = "launcher-state.conf";

    // 导入的包解到这里，必须是 boot() 认的那个固定位置：它靠 manifest.json 是否存在决定用哪套数据。
    // The imported package unpacks here, the fixed location boot() looks for: it decides which data to use by
    // whether manifest.json is present.
    private static final String PACKAGE_DIRECTORY = "d2r-package";
    private static final String MANIFEST_NAME = PackageArchive.MANIFEST;
    private static final int PICK_PACKAGE = 1;
    private static final int EXPORT_SAVE = 2;
    private static final int IMPORT_SAVE = 3;

    private static final int GOLD = 0xffe8c078;
    private static final int PARCHMENT = 0xffe8e0d0;
    private static final int MUTED = 0xffa2988c;
    private static final int WARNING = 0xffd08a78;

    private Switch overlaySwitch;
    private TextView profileItem, resolutionItem, aspectItem, driverItem, importItem, statusView, startItem;
    private boolean handheld;
    private boolean aspect169;
    private int resolutionIndex;
    private String customResolution = "";   // 预设之外的手写分辨率（-1 档时用）| a hand-written resolution outside the presets
    private int driverIndex;
    private boolean launchStarting;
    private String packageSummary = "";
    private boolean importing;
    private volatile boolean cancelImport;
    // The output presets keep at least 720p vertical UI height (guest UI layout clamps to 1280x720 minimum,
    // so sub-720p targets clip the bottom HUD and options footer). Default 20:9 is 1600x720.
    private static final String[] RESOLUTION_PRESETS = {
        "", "1600x720", "1920x864", "2000x900", "1280x720", "1600x900", "1920x1080", "2560x1440"
    };
    // Keep System Vulkan, MaliSX2 and PanVK for Mali/kbase, plus Turnip for Qualcomm KGSL.
    private static final String[] DRIVER_STATES = {"system", "malisx2", "panvk", "turnip"};
    private static final String TURNIP_ASSET = "turnip/vulkan.purple.so";
    private static final String PANVK_ASSET = "panvk/libvulkan_panfrost.so";
    private static final String MALISX2_ASSET = "malisx2/libvulkan_malisx2.so";

    private String text(String key) {
        int resource = getResources().getIdentifier(key, "string", getPackageName());
        return resource == 0 ? key : getString(resource);
    }

    // 启动界面只拥有这几个键。文件里其余的键是别处写的（例如诊断用的 input_receipts），重写时必须原样留下 ——
    // 否则「从启动界面启动游戏」会把它们悄悄抹掉。实测就丢过一次 input_receipts=1：配置里写着、native 读到的
    // 却是「未设」，HID 收样一直没生成，查了半天才发现是这个覆盖。
    // The launcher owns only these keys. Any other key in the file was written elsewhere (a diagnostic such as
    // input_receipts, for instance) and has to survive a rewrite -- otherwise starting the game from the launcher
    // silently drops it. Measured once with input_receipts=1: the file said so, native read "unset", the HID
    // receipts were never produced, and the overwrite was the last thing found.
    private static final String[] OWNED_KEYS = {"debug_overlay", "operation_mode", "resolution", "present_hz"};

    // 游戏还活着时启动界面立刻让位。
    // 从桌面点图标回来时，安卓会在**游戏上面**再开一个启动界面（启动界面是本应用的入口活动），玩家看到的是启动界面、
    // 像是"游戏没了"；再点一次「启动游戏」就会开第三个活动，把中间那个游戏活动销毁 —— 而销毁游戏活动在本项目里等于
    // 结束整个进程（一个进程只跑一个游戏），新开的那个也一起死，安卓随后重新起进程从头加载，表现成"应用终止、这一局没了"。
    // 所以只要游戏活动还在（同进程的静态标记），这里直接 finish 自己，把下面的游戏露出来，走我们已经真机验证过的
    // 「回前台重建交换链」那条路。想改设置（分辨率 / 换数据包）请先把应用从最近任务里划掉 —— 那才是真正退出。
    // While the game is alive the launcher steps aside. Tapping the icon from the home screen makes Android push a
    // *second* launcher instance on top of the game (the launcher is this app's entry activity), so the player sees
    // the launcher and the game looks gone; tapping "start game" then creates a third activity and destroys the game
    // one in the middle -- and in this project destroying the game activity ends the whole process (one guest per
    // process), taking the new activity down with it, after which Android starts a fresh process and reloads from
    // scratch, i.e. "the app terminated and the session is gone". So whenever the game activity is still alive (a
    // static flag in this process) the launcher finishes itself at once, revealing the game underneath and going
    // through the background-return path already verified on device. To change settings (resolution, another
    // package) swipe the app away from recents first -- that is a real exit.
    private boolean stepAsideForRunningGame() {
        if (!D2RActivity.hostRunning) return false;
        finish();
        return true;
    }

    private File configFile() {
        File directory = new File(getFilesDir(), "temp");
        if (!directory.isDirectory() && !directory.mkdirs()) throw new IllegalStateException("cannot create " + directory);
        return new File(directory, CONFIG_NAME);
    }

    private File stateFile() { return new File(getFilesDir(), STATE_NAME); }

    // 大资源解在外部私有目录：那里放得下 9 GB 级的数据，内部数据目录通常放不下。
    // 但**模块必须解在内部目录**：外部存储是 noexec 挂载（实测 /storage/emulated 带 noexec），
    // 从那里 dlopen 会失败；内部目录没有 noexec，图形驱动就是从内部目录加载的。
    // Big assets go to the external private directory, which has room for multi-gigabyte data. The modules must
    // go to the internal one: external storage is mounted noexec (measured on /storage/emulated), so dlopen from
    // there fails, while the internal directory has no such flag — the graphics driver already loads from it.
    private File packageDirectory() {
        File external = getExternalFilesDir(null);
        return external == null ? new File(getFilesDir(), PACKAGE_DIRECTORY) : new File(external, PACKAGE_DIRECTORY);
    }

    private File packageGameDirectory() { return new File(getFilesDir(), PACKAGE_DIRECTORY + "/game"); }

    private File packageManifest() { return new File(packageDirectory(), MANIFEST_NAME); }

    // 导入是否完成，以 manifest.json 为准（它在解包最后才写）。没导入就不该能启动游戏。
    // "Imported" means manifest.json exists, and it is written last. Without it the game must not start.
    private boolean packageReady() { return packageManifest().isFile(); }

    // 只做最简单的 key=value 读取；权威解析在 native 的 launch_settings.c。
    // Minimal key=value read; the authoritative parser is launch_settings.c on the native side.
    private void write(File file, String content) {
        try (OutputStreamWriter writer = new OutputStreamWriter(
                new FileOutputStream(file, false), StandardCharsets.UTF_8)) {
            writer.write(content);
        } catch (IOException error) {
            setStatus(error.getMessage(), WARNING);
        }
    }

    private void setStatus(String message, int colour) {
        String value = message == null ? "" : message;
        statusView.setTextColor(colour);
        statusView.setText(value);
        statusView.setVisibility(value.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private GradientDrawable shape(int fill, int stroke, int radius) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(fill);
        drawable.setCornerRadius(dp(radius));
        drawable.setStroke(dp(1), stroke);
        return drawable;
    }

    private TextView item(String value, float size, int colour, boolean bold, boolean clickable) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextColor(colour);
        view.setTextSize(size);
        view.setGravity(Gravity.START | Gravity.CENTER_VERTICAL);
        if (bold) view.setTypeface(Typeface.DEFAULT_BOLD);
        view.setPadding(0, dp(2), 0, dp(2));
        view.setClickable(clickable);
        return view;
    }

    private TextView buttonItem(String value, int textColour, int fill, int stroke,
                                int height, float size, boolean bold) {
        TextView view = item(value, size, textColour, bold, true);
        view.setMaxLines(2);
        view.setEllipsize(TextUtils.TruncateAt.END);
        view.setPadding(dp(12), dp(6), dp(12), dp(6));
        view.setMinHeight(dp(height));
        view.setBackground(new RippleDrawable(ColorStateList.valueOf(0x44ffffff),
                                               shape(fill, stroke, 10), null));
        view.setFocusable(true);
        return view;
    }

    private void addGap(LinearLayout layout, int height) {
        layout.addView(new View(this), new LinearLayout.LayoutParams(1, dp(height)));
    }

    private void applyImmersiveMode() {
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                             WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    private void addChoice(LinearLayout row, TextView choice) {
        choice.setMaxLines(2);
        choice.setEllipsize(TextUtils.TruncateAt.END);
        choice.setTextSize(12);
        choice.setPadding(dp(10), dp(5), dp(10), dp(5));
        choice.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33ffffff),
                                                 shape(0x55413a32, 0x667c6d59, 9), null));
        choice.setFocusable(true);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, dp(48), 1f);
        row.addView(choice, params);
    }

    private String profileLabel() {
        return text("dmAndroidLaunchProfile") + "：" +
            text(handheld ? "dmAndroidLaunchHandheld" : "dmAndroidLaunchConsole");
    }

    private String resolutionLabel() {
        if (resolutionIndex < 0) {   // 预设里没有的手写分辨率：原样留着，不被启动界面冲掉
            String[] parts = customResolution.split("x");
            return text("dmAndroidLaunchResolution") + "：" + (parts.length == 2 ? parts[0] + "×" + parts[1] : customResolution);
        }
        String preset = RESOLUTION_PRESETS[resolutionIndex];
        if (preset.isEmpty()) return text("dmAndroidLaunchResolution") + "：" + text("dmAndroidLaunchFollowProfile");
        String[] parts = preset.split("x");
        return text("dmAndroidLaunchResolution") + "：" + parts[0] + "×" + parts[1];
    }

    private String aspectLabel() {
        return text("dmAndroidLaunchAspect") + "：" +
            text(aspect169 ? "dmAndroidLaunchAspect169" : "dmAndroidLaunchAspectWide");
    }

    private boolean turnipBundled() {
        try (InputStream stream = getAssets().open(TURNIP_ASSET)) {
            return stream != null;
        } catch (IOException error) {
            return false;
        }
    }
    private boolean turnipAvailable() {
        return android.os.Build.VERSION.SDK_INT >= 30
            && turnipBundled() && new File("/dev/kgsl-3d0").exists();
    }
    private boolean panvkBundled() {
        try (InputStream stream = getAssets().open(PANVK_ASSET)) {
            return stream != null;
        } catch (IOException error) {
            return false;
        }
    }
    private boolean panvkAvailable() {
        return android.os.Build.VERSION.SDK_INT >= 31
            && panvkBundled() && new File("/dev/mali0").exists();
    }
    private boolean malisx2Bundled() {
        try (InputStream stream = getAssets().open(MALISX2_ASSET)) {
            return stream != null;
        } catch (IOException error) {
            return false;
        }
    }
    private boolean malisx2Available() {
        return android.os.Build.VERSION.SDK_INT >= 29
            && malisx2Bundled() && new File("/dev/mali0").exists();
    }
    private boolean driverAvailable(String driver) {
        if (driver.equals("system")) return true;
        if (driver.equals("malisx2")) return malisx2Available();
        if (driver.equals("panvk")) return panvkAvailable();
        if (driver.equals("turnip")) return turnipAvailable();
        return false;
    }
    private int nextDriverIndex() {
        for (int offset = 1; offset <= DRIVER_STATES.length; offset++) {
            int candidate = (driverIndex + offset) % DRIVER_STATES.length;
            if (driverAvailable(DRIVER_STATES[candidate])) return candidate;
        }
        return 0;
    }
    private String driverLabel() {
        String state = DRIVER_STATES[driverIndex];
        String key = state.equals("malisx2") ? "dmAndroidLaunchDriverMalisx2"
                : state.equals("panvk") ? "dmAndroidLaunchDriverPanvk"
                : state.equals("turnip") ? "dmAndroidLaunchDriverTurnip" : "dmAndroidLaunchDriverSystem";
        return text("dmAndroidLaunchDriver") + "：" + text(key);
    }

    // 数据包里那张子组档位表（一行裸数字，不是 key=value）。Turnip 的预编译着色器按该值生成；
    // PanVK 使用真实物理子组并由 native 对照同一份 profile。
    // The package's subgroup profile is a bare number. Turnip shaders are built for that explicit size; PanVK uses
    // its reported physical size and the native host-profile check verifies that it matches.
    private String readProfileSize() {
        File profile = new File(packageDirectory(), "shaders/host-subgroup-size.txt");
        try (InputStream stream = new FileInputStream(profile)) {
            byte[] text = new byte[16];
            int count = stream.read(text);
            if (count <= 0) return null;
            String value = new String(text, 0, count, StandardCharsets.UTF_8).trim();
            return value.isEmpty() ? null : value;
        } catch (IOException error) {
            return null;
        }
    }
    private String readSmallText(InputStream input) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        byte[] buffer = new byte[128];
        int count;
        while ((count = input.read(buffer)) != -1) {
            if (bytes.size() + count > 128) return null;
            bytes.write(buffer, 0, count);
        }
        return new String(bytes.toByteArray(), StandardCharsets.UTF_8).trim();
    }
    private String fileSha256(File file) throws IOException {
        final MessageDigest digest;
        try {
            digest = MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException error) {
            throw new IOException("SHA-256 unavailable", error);
        }
        try (InputStream input = new FileInputStream(file)) {
            byte[] buffer = new byte[262144];
            int count;
            while ((count = input.read(buffer)) != -1) digest.update(buffer, 0, count);
        }
        byte[] hash = digest.digest();
        char[] digits = "0123456789abcdef".toCharArray();
        char[] text = new char[hash.length * 2];
        for (int i = 0; i < hash.length; i++) {
            int value = hash[i] & 0xff;
            text[i * 2] = digits[value >>> 4];
            text[i * 2 + 1] = digits[value & 15];
        }
        return new String(text);
    }
    private String copyDriverAsset(String asset, File target) {
        File directory = target.getParentFile();
        if (!directory.isDirectory() && !directory.mkdirs()) return "cannot create driver directory";
        String checksumAsset = asset.substring(0, asset.lastIndexOf('/') + 1) + "driver.sha256";
        String expectedHash;
        try (InputStream checksum = getAssets().open(checksumAsset)) {
            expectedHash = readSmallText(checksum);
        } catch (IOException error) {
            return "bundled driver checksum is missing";
        }
        if (expectedHash == null || !expectedHash.matches("[0-9a-f]{64}")) return "bundled driver checksum is invalid";
        File marker = new File(directory, target.getName() + ".sha256");
        File temporary = new File(directory, target.getName() + ".pending");
        File markerTemporary = new File(directory, marker.getName() + ".pending");
        if (temporary.exists() && !temporary.delete()) return "cannot remove incomplete driver copy";
        int expectedSize;
        try (InputStream source = getAssets().open(asset)) {
            expectedSize = source.available();
            if (expectedSize <= 0) return "bundled driver asset is empty";
            String installedHash = null;
            if (marker.isFile()) {
                try (InputStream previous = new FileInputStream(marker)) {
                    installedHash = readSmallText(previous);
                } catch (IOException ignored) { }
            }
            if (target.isFile() && target.length() == expectedSize && expectedHash.equals(installedHash)) {
                if (expectedHash.equals(fileSha256(target))) return null;
                Log.e("D2RAndroid", "[driver] installed library hash mismatch; copying bundled asset again: "
                        + target.getName());
            }
            try (FileOutputStream output = new FileOutputStream(temporary, false)) {
                byte[] buffer = new byte[262144];
                int count;
                while ((count = source.read(buffer)) > 0) output.write(buffer, 0, count);
                output.getFD().sync();
            }
            if (temporary.length() != expectedSize || !expectedHash.equals(fileSha256(temporary))) {
                temporary.delete();
                return "bundled driver copy failed size or SHA-256 verification";
            }
            if (target.exists() && !target.delete()) return "cannot replace old driver library";
            if (!temporary.renameTo(target)) return "cannot activate copied driver library";
            try (FileOutputStream output = new FileOutputStream(markerTemporary, false)) {
                output.write((expectedHash + "\n").getBytes(StandardCharsets.UTF_8));
                output.getFD().sync();
            }
            if (marker.exists() && !marker.delete()) return "cannot replace driver checksum marker";
            if (!markerTemporary.renameTo(marker)) return "cannot activate driver checksum marker";
            return null;
        } catch (IOException error) {
            temporary.delete();
            markerTemporary.delete();
            return error.getMessage();
        }
    }
    private String writeDriverConfiguration(File configuration, String content) {
        File pending = new File(configuration.getParentFile(), configuration.getName() + ".pending");
        try (FileOutputStream output = new FileOutputStream(pending, false)) {
            output.write(content.getBytes(StandardCharsets.UTF_8));
            output.getFD().sync();
        } catch (IOException error) {
            pending.delete();
            return error.getMessage();
        }
        if (configuration.exists() && !configuration.delete()) {
            pending.delete();
            return "cannot replace driver configuration";
        }
        if (!pending.renameTo(configuration)) {
            pending.delete();
            return "cannot activate driver configuration";
        }
        return null;
    }
    // 系统 Vulkan 始终可选；Turnip 只匹配 KGSL，PanVK 只匹配 Mali kbase。
    // The system driver is always available; Turnip is KGSL-only and PanVK is Mali-kbase-only.
    private String prepareDriver() {
        if (driverIndex < 0 || driverIndex >= DRIVER_STATES.length
                || !driverAvailable(DRIVER_STATES[driverIndex])) driverIndex = 0;
        String selected = DRIVER_STATES[driverIndex];
        File temporary = new File(getFilesDir(), "temp");
        File configuration = new File(temporary, "gpu-driver.conf");
        if (selected.equals("system")) {
            if (configuration.exists() && !configuration.delete()) {
                return "cannot remove stale custom driver configuration";
            }
            return null;
        }
        String profile = readProfileSize();
        if (profile == null) return "host-subgroup-size.txt";
        boolean panvk = selected.equals("panvk");
        boolean malisx2 = selected.equals("malisx2");
        String libraryName = panvk ? "libvulkan_panfrost.so"
                : malisx2 ? "libvulkan_malisx2.so" : "vulkan.purple.so";
        String asset = panvk ? PANVK_ASSET : malisx2 ? MALISX2_ASSET : TURNIP_ASSET;
        File directory = new File(temporary, "driver-" + selected);
        File library = new File(directory, libraryName);
        String copyProblem = copyDriverAsset(asset, library);
        if (copyProblem != null) return copyProblem;
        StringBuilder content = new StringBuilder("# written by the launcher\n");
        content.append("backend=").append(selected).append('\n');
        content.append("directory=").append(directory.getAbsolutePath()).append('\n');
        content.append("library=").append(libraryName).append('\n');
        content.append("require_custom=1\n");
        if (!panvk && !malisx2) content.append("subgroup_size=").append(profile).append('\n');
        return writeDriverConfiguration(configuration, content.toString());
    }

    private String importLabel() {
        return packageSummary.isEmpty()
                ? text("dmAndroidLaunchImport") + "：" + text("dmAndroidLaunchNotSet")
                : String.format(text("dmAndroidLaunchImported"), packageSummary);
    }

    // 启动界面自己的状态文件：只放界面显示需要的东西（导入摘要、所选驱动），native 不读它。
    // The launcher's own state file: display-only data (import summary, chosen driver), never read by native.
    private void writeState() {
        StringBuilder content = new StringBuilder("# 仅供界面显示 | display only\n");
        if (!packageSummary.isEmpty()) content.append("package_summary=").append(packageSummary).append('\n');
        content.append("driver=").append(DRIVER_STATES[driverIndex]).append('\n');
        write(stateFile(), content.toString());
    }

    // 没有数据包就把"启动游戏"置灰：点了会失败的事不该看起来能点。
    // Without a package the start entry greys out: something that would fail should not look available.
    private void updateStartState() {
        boolean ready = packageReady() && !importing && !launchStarting;
        startItem.setEnabled(ready);
        startItem.setTextColor(ready ? 0xff24190f : MUTED);
        startItem.setAlpha(ready ? 1f : 0.55f);
    }

    // 把当前内存里的设置反映到界面上。点一下之后只调这个，**绝不重新读盘** ——
    // 读盘会把刚改的内存值覆盖回去，结果就是点了没反应（分辨率与画面档位两行都曾如此）。
    // Reflect the in-memory settings onto the labels. Click handlers call only this and never re-read from disk:
    // reloading overwrites the value just changed, so tapping did nothing (true for both the resolution and the
    // profile row).
    private void applyLabels() {
        if (profileItem != null) profileItem.setText(profileLabel());
        if (resolutionItem != null) resolutionItem.setText(resolutionLabel());
        if (aspectItem != null) aspectItem.setText(aspectLabel());
        if (driverItem != null) driverItem.setText(driverLabel());
        if (importItem != null) importItem.setText(importLabel());
        if (startItem != null) updateStartState();
    }

    // 从盘重读全部设置到内存，再刷新界面。只在进入/回到界面时调。
    // Read every setting from disk into memory, then refresh the labels. Called only when the screen is (re)entered.
    private void refreshFromDisk() {
        handheld = !"console".equals(LaunchSettings.read(configFile(), "operation_mode"));
        // 调试浮层默认关闭：只有配置里明确写了 1 才是开着（用户要求全局默认不显示）。
        // The debug overlay defaults to off: only an explicit 1 in the config means on (the user asked for it to be
        // off globally by default).
        if (overlaySwitch != null) overlaySwitch.setChecked("1".equals(LaunchSettings.read(configFile(), "debug_overlay")));
        String preset = LaunchSettings.read(configFile(), "resolution");
        // Upgrade missing and legacy empty resolution settings to the Xiaomi-wide default.
        // An empty value left by older launchers otherwise bypasses DM_GAME_RES and the game falls back to 800x600.
        resolutionIndex = preset == null || preset.isEmpty() ? 1 : 0;
        if ("1280x576".equals(preset)) preset = "1600x720";
        if (preset != null && !preset.isEmpty()) {
            int found = -1;
            for (int i = 0; i < RESOLUTION_PRESETS.length; i++) if (RESOLUTION_PRESETS[i].equals(preset)) found = i;
            // 预设里没有的值（玩家自己写进配置的）原样留着：启动界面以前会把这种值悄悄抹掉，
            // 手写的 720p 每次启动都失效就是这个原因。
            // A value that is not one of the presets (the player wrote it into the configuration) is kept as it is:
            // the launcher used to silently drop it, which is why a hand-written 720p stopped working on every launch.
            if (found >= 0) resolutionIndex = found;
            else if (!preset.isEmpty()) { resolutionIndex = -1; customResolution = preset; }
        }
        String selectedResolution = resolutionIndex < 0 ? customResolution : RESOLUTION_PRESETS[resolutionIndex];
        aspect169 = LaunchSettings.is169Resolution(selectedResolution);
        // Driver choices saved by older APKs remain selected only when that backend exists on this GPU.
        // A stale Turnip selection on Mali resets to stock; an available PanVK selection is retained.
        String driver = LaunchSettings.read(stateFile(), "driver");
        driverIndex = 0;
        if (driver != null) for (int i = 0; i < DRIVER_STATES.length; i++) if (DRIVER_STATES[i].equals(driver)) driverIndex = i;
        if (!driverAvailable(DRIVER_STATES[driverIndex])) driverIndex = 0;
        Log.i("D2RAndroid", "[driver] launcher restored=" + (driver == null ? "default" : driver)
                + " selected=" + DRIVER_STATES[driverIndex]
                + " available={system:true,malisx2:" + malisx2Available()
                + ",panvk:" + panvkAvailable() + ",turnip:" + turnipAvailable() + "}");
        applyLabels();
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        if (stepAsideForRunningGame()) return;
        applyImmersiveMode();

        float density = getResources().getDisplayMetrics().density;
        float widthDp = getResources().getDisplayMetrics().widthPixels / density;
        int panelWidthDp = Math.min(480, Math.max(340, Math.round(widthDp * 0.48f)));
        panelWidthDp = Math.min(panelWidthDp, Math.max(300, Math.round(widthDp - 44)));

        FrameLayout root = new FrameLayout(this);
        root.addView(new LauncherBackgroundView(this),
            new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                         ViewGroup.LayoutParams.MATCH_PARENT));

        FrameLayout panel = new FrameLayout(this);
        panel.setBackground(shape(0xe91a1918, 0x77e8c078, 18));
        panel.setClipToOutline(true);
        FrameLayout.LayoutParams panelParams = new FrameLayout.LayoutParams(
            dp(panelWidthDp), ViewGroup.LayoutParams.MATCH_PARENT,
            Gravity.RIGHT | Gravity.CENTER_VERTICAL);
        panelParams.setMargins(0, dp(14), dp(18), dp(14));
        root.addView(panel, panelParams);


        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(dp(21), dp(13), dp(18), dp(12));

        TextView title = item(text("dmAndroidLaunchTitle"), 23, GOLD, true, false);
        title.setMaxLines(1);
        title.setEllipsize(TextUtils.TruncateAt.END);
        content.addView(title);

        TextView subtitle = item(text("dmAndroidLaunchSubtitle"), 11, MUTED, false, false);
        subtitle.setMaxLines(1);
        subtitle.setEllipsize(TextUtils.TruncateAt.END);
        content.addView(subtitle);
        addGap(content, 9);

        startItem = buttonItem("▶  " + text("dmAndroidLaunchStart"), 0xff24190f,
                               0xffe8c078, 0xfff2d39f, 44, 16, true);
        startItem.setGravity(Gravity.CENTER);
        startItem.setOnClickListener(v -> startGame());
        content.addView(startItem, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, dp(44)));
        addGap(content, 7);

        LinearLayout choices = new LinearLayout(this);
        choices.setOrientation(LinearLayout.HORIZONTAL);
        profileItem = item(profileLabel(), 12, PARCHMENT, false, true);
        profileItem.setOnClickListener(v -> { handheld = !handheld; applyLabels(); });
        addChoice(choices, profileItem);
        resolutionItem = item(resolutionLabel(), 12, PARCHMENT, false, true);
        resolutionItem.setOnClickListener(v -> {
            resolutionIndex = (resolutionIndex + 1) % RESOLUTION_PRESETS.length;
            applyLabels();
        });
        LinearLayout.LayoutParams resolutionParams = new LinearLayout.LayoutParams(
            0, dp(46), 1f);
        choices.addView(resolutionItem, resolutionParams);
        content.addView(choices);
        addGap(content, 6);

        aspectItem = buttonItem(aspectLabel(), PARCHMENT, 0x55413a32,
                                0x667c6d59, 40, 12, false);
        aspectItem.setOnClickListener(v -> {
            aspect169 = !aspect169;
            String wanted = LaunchSettings.resolutionForAspect(aspect169);
            for (int i = 0; i < RESOLUTION_PRESETS.length; i++)
                if (RESOLUTION_PRESETS[i].equals(wanted)) resolutionIndex = i;
            Log.i("D2RAndroid", "[aspect] launcher selected=" + (aspect169 ? "16:9" : "20:9")
                    + " resolution=" + wanted);
            applyLabels();
        });
        content.addView(aspectItem, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, dp(40)));
        addGap(content, 6);

        LinearLayout options = new LinearLayout(this);
        options.setOrientation(LinearLayout.HORIZONTAL);
        if (turnipAvailable() || panvkAvailable() || malisx2Available()) {
            driverItem = buttonItem(driverLabel(), PARCHMENT, 0x55413a32,
                                    0x667c6d59, 44, 11, false);
            driverItem.setOnClickListener(v -> {
                driverIndex = nextDriverIndex();
                applyLabels();
                writeState();
                Log.i("D2RAndroid", "[driver] launcher selection changed to " + DRIVER_STATES[driverIndex]);
            });
            LinearLayout.LayoutParams driverParams = new LinearLayout.LayoutParams(
                0, dp(44), 1f);
            driverParams.setMarginEnd(dp(4));
            options.addView(driverItem, driverParams);
        }
        overlaySwitch = new Switch(this);
        overlaySwitch.setText(text("dmAndroidLaunchOverlay"));
        overlaySwitch.setTextColor(PARCHMENT);
        overlaySwitch.setTextSize(11);
        overlaySwitch.setGravity(Gravity.CENTER_VERTICAL);
        overlaySwitch.setPadding(0, 0, 0, 0);
        LinearLayout.LayoutParams overlayParams = new LinearLayout.LayoutParams(
            0, dp(44), driverItem == null ? 1f : 1.15f);
        options.addView(overlaySwitch, overlayParams);
        content.addView(options);
        addGap(content, 6);

        importItem = buttonItem(importLabel(), PARCHMENT, 0x55413a32,
                                0x667c6d59, 44, 12, false);
        importItem.setOnClickListener(v -> pickPackage());
        content.addView(importItem, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, dp(44)));
        addGap(content, 5);

        LinearLayout saves = new LinearLayout(this);
        saves.setOrientation(LinearLayout.HORIZONTAL);
        TextView saveExportItem = buttonItem(text("dmAndroidSaveExport"), PARCHMENT,
            0x55332e29, 0x557c6d59, 40, 11, false);
        saveExportItem.setGravity(Gravity.CENTER);
        saveExportItem.setOnClickListener(v -> exportSave());
        LinearLayout.LayoutParams saveParams = new LinearLayout.LayoutParams(0, dp(40), 1f);
        saveParams.setMarginEnd(dp(5));
        saves.addView(saveExportItem, saveParams);
        TextView saveImportItem = buttonItem(text("dmAndroidSaveImport"), PARCHMENT,
            0x55332e29, 0x557c6d59, 40, 11, false);
        saveImportItem.setGravity(Gravity.CENTER);
        saveImportItem.setOnClickListener(v -> confirmImportSave());
        saves.addView(saveImportItem, new LinearLayout.LayoutParams(0, dp(40), 1f));
        content.addView(saves);

        statusView = item("", 11, WARNING, false, false);
        statusView.setMaxLines(2);
        statusView.setEllipsize(TextUtils.TruncateAt.END);
        content.addView(statusView);

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.setClipToPadding(false);
        scroll.addView(content, new ScrollView.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        panel.addView(scroll, new FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        View accent = new View(this);
        GradientDrawable accentShape = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
            new int[]{0x00e8c078, 0xffe8c078, 0x00e8c078});
        accentShape.setCornerRadius(dp(2));
        accent.setBackground(accentShape);
        FrameLayout.LayoutParams accentParams = new FrameLayout.LayoutParams(
            dp(3), ViewGroup.LayoutParams.MATCH_PARENT, Gravity.LEFT);
        accentParams.setMargins(dp(1), dp(20), 0, dp(20));
        panel.addView(accent, accentParams);

        refreshFromDisk();
        setContentView(root);
    }

    @Override protected void onResume() {
        super.onResume();
        if (stepAsideForRunningGame()) return;
        launchStarting = false;
        applyImmersiveMode();
        refreshFromDisk();
    }
    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) applyImmersiveMode();
    }


    // 选包用系统文件选择器：应用不需要任何存储权限，也不必知道文件放在哪。
    // The system picker chooses the package, so the app needs no storage permission and does not care where the
    // file lives. Selecting it again while an import runs asks that import to stop.
    private void pickPackage() {
        if (importing) {
            cancelImport = true;
            setStatus(text("dmAndroidLaunchCancel"), MUTED);
            return;
        }
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        // 部分提供方把 zip 报成 octet-stream，所以类型放宽；否则文件在选择器里是灰的。
        // Some providers report a zip as octet-stream, so the type is left open; otherwise the file greys out.
        intent.setType("*/*");
        try {
            startActivityForResult(intent, PICK_PACKAGE);
        } catch (RuntimeException error) {
            setStatus(String.format(text("dmAndroidLaunchImportFailed"), String.valueOf(error.getMessage())), WARNING);
        }
    }

    // ---------- 存档导入 / 导出 ----------
    // 存档在内部目录 files/save/（角色、共享仓库、Settings 三个目录）；导出就是把它整棵树打成 zip，
    // 导入是"解到临时目录 → 校验 → 原子换入"，失败的导入不会动到现有存档（换入前旧目录还在）。
    // The save lives in the internal files/save/ (character, shared stash, Settings); export zips the whole tree,
    // import is "unpack to a staging directory -> validate -> swap in", and a failed import never touches the
    // existing save (the old directory is still there until the swap).
    private File saveDirectory() { return new File(getFilesDir(), "save"); }

    // 文件名带时间戳，方便留多份；玩家仍可在选择器里改名。| Timestamped so several copies are easy; renameable in the picker.
    private String saveArchiveName() {
        return "d2r-save-" + new SimpleDateFormat("yyyyMMdd-HHmm", Locale.US).format(new Date()) + ".zip";
    }

    private void exportSave() {
        if (!saveDirectory().isDirectory()) { setStatus(text("dmAndroidSaveNone"), WARNING); return; }
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/zip");
        intent.putExtra(Intent.EXTRA_TITLE, saveArchiveName());
        try {
            startActivityForResult(intent, EXPORT_SAVE);
        } catch (RuntimeException error) {
            setStatus(String.format(text("dmAndroidSaveExportFailed"), String.valueOf(error.getMessage())), WARNING);
        }
    }

    private void confirmImportSave() {
        new AlertDialog.Builder(this)
                .setMessage(text("dmAndroidSaveImportConfirm"))
                .setPositiveButton(android.R.string.ok, (dialog, which) -> pickSaveArchive())
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void pickSaveArchive() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        // 与选数据包同样的理由：有的提供方把 zip 报成 octet-stream，类型放宽，否则文件在选择器里是灰的。
        // Same reason as the package picker: some providers report a zip as octet-stream, so the type is left open.
        intent.setType("*/*");
        try {
            startActivityForResult(intent, IMPORT_SAVE);
        } catch (RuntimeException error) {
            setStatus(String.format(text("dmAndroidSaveImportFailed"), String.valueOf(error.getMessage())), WARNING);
        }
    }

    private void writeSaveArchive(Uri target) {
        try (OutputStream out = getContentResolver().openOutputStream(target)) {
            if (out == null) throw new IOException("无法写入所选位置");
            int files = SaveArchive.zip(saveDirectory(), out, System.currentTimeMillis());
            setStatus(String.format(text("dmAndroidSaveExported"), files), PARCHMENT);
        } catch (Exception error) {
            setStatus(String.format(text("dmAndroidSaveExportFailed"), String.valueOf(error.getMessage())), WARNING);
        }
    }

    private void readSaveArchive(Uri source) {
        File staging = new File(getFilesDir(), "save.import");
        SaveArchive.deleteTree(staging);
        try (InputStream in = getContentResolver().openInputStream(source)) {
            if (in == null) throw new IOException("无法读取所选文件");
            int files = SaveArchive.unzip(in, staging);
            SaveArchive.replace(saveDirectory(), staging);
            setStatus(String.format(text("dmAndroidSaveImported"), files), PARCHMENT);
        } catch (Exception error) {
            SaveArchive.deleteTree(staging);
            setStatus(String.format(text("dmAndroidSaveImportFailed"), String.valueOf(error.getMessage())), WARNING);
        }
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (result != RESULT_OK || data == null || data.getData() == null) return;
        if (request == EXPORT_SAVE) { writeSaveArchive(data.getData()); return; }
        if (request == IMPORT_SAVE) { readSaveArchive(data.getData()); return; }
        if (request == PICK_PACKAGE) beginImport(data.getData());
    }

    private void beginImport(Uri uri) {
        importing = true;
        cancelImport = false;
        updateStartState();
        setStatus(String.format(text("dmAndroidLaunchImporting"), 0), MUTED);
        new Thread(() -> {
            try {
                String summary = extract(uri, documentSize(uri));
                runOnUiThread(() -> {
                    packageSummary = summary;
                    writeState();
                    refreshFromDisk();
                    setStatus(summary, PARCHMENT);
                });
            } catch (Exception error) {
                runOnUiThread(() -> setStatus(String.format(text("dmAndroidLaunchImportFailed"), failureText(error)), WARNING));
            } finally {
                importing = false;
                runOnUiThread(this::updateStartState);
            }
        }, "d2r-package-import").start();
    }

    private long documentSize(Uri uri) {
        try (android.database.Cursor cursor = getContentResolver().query(uri, new String[]{OpenableColumns.SIZE}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) return cursor.getLong(0);
        } catch (RuntimeException ignored) {
        }
        return -1;
    }

    // 规则、验收与"最后才写完成标记"的顺序都在 PackageArchive 里（那部分不碰 Android API，能在电脑上跑回归），
    // 这里只负责「流从哪来、进度怎么显示、取消怎么判断、失败怎么讲给玩家听」。
    // The rules, the acceptance check and the write-the-marker-last ordering all live in PackageArchive (which holds
    // no Android API and is regression-tested on a PC). This supplies only where the stream comes from, how progress
    // is shown, how cancellation is noticed, and how a failure is told to the player.
    private String extract(Uri uri, long totalBytes) throws IOException {
        PackageArchive.Counts counts;
        try (InputStream raw = getContentResolver().openInputStream(uri)) {
            if (raw == null) throw new IOException("cannot open the selected file");
            counts = PackageArchive.unpack(raw, packageDirectory(), packageGameDirectory(), totalBytes,
                                           new PackageArchive.Sink() {
                @Override public void progress(final int percent) {
                    runOnUiThread(() -> setStatus(String.format(text("dmAndroidLaunchImporting"), percent), MUTED));
                }
                @Override public boolean cancelled() { return cancelImport; }
            }, LauncherActivity::declaration);
        }
        // 显示**实际解出**的数量。清单自报的数只参与校验、不当结果显示 —— 以前正是把它当结果显示，
        // 才让一个只装了一个文件的包报出"46 个资源、20795 个着色器"。
        // Report what was **actually** unpacked. The manifest's own numbers are only compared, never displayed:
        // displaying them is what once made a package holding one file report "46 assets, 20795 shaders".
        return String.format(text("dmAndroidLaunchImportSummary"), counts.romfs, counts.shaders);
    }

    // 清单的解析交给 org.json（Android 自带）；规则层只要这几个数，所以不依赖任何 JSON 实现。
    // org.json does the parsing. The rule layer wants only these few numbers, so it depends on no JSON library.
    private static PackageArchive.Declaration declaration(byte[] manifest) throws IOException {
        try {
            JSONObject root = new JSONObject(new String(manifest, StandardCharsets.UTF_8));
            JSONObject counts = root.optJSONObject("counts");
            return new PackageArchive.Declaration(
                root.optInt("format", PackageArchive.Declaration.ABSENT),
                counts == null ? PackageArchive.Declaration.ABSENT : counts.optInt("game", PackageArchive.Declaration.ABSENT),
                counts == null ? PackageArchive.Declaration.ABSENT : counts.optInt("romfs", PackageArchive.Declaration.ABSENT),
                counts == null ? PackageArchive.Declaration.ABSENT : counts.optInt("romfs_overlay", PackageArchive.Declaration.ABSENT),
                counts == null ? PackageArchive.Declaration.ABSENT : counts.optInt("shaders", PackageArchive.Declaration.ABSENT));
        } catch (JSONException error) {
            throw new IOException("package manifest is not valid JSON: " + error.getMessage());
        }
    }

    // 规则层抛出的失败带的是「哪条规则」的键，在这里翻成中英文字；表里没这个键就退回异常原文，方便看日志。
    // A failure from the rule layer carries the key of the rule it broke; turn it into text here, falling back to
    // the raw message when the table has no such key so the log stays useful.
    private String failureText(Exception error) {
        if (!(error instanceof PackageArchive.Failure)) return String.valueOf(error.getMessage());
        PackageArchive.Failure failure = (PackageArchive.Failure) error;
        String format = text(failure.key);
        return format.equals(failure.key)
            ? String.valueOf(error.getMessage())
            : String.format(format, failure.arguments);
    }

    private String commitLaunchSettings(String content) {
        final File destination;
        try {
            destination = configFile();
        } catch (RuntimeException error) {
            return error.getMessage() == null ? error.getClass().getSimpleName() : error.getMessage();
        }
        File pending = new File(destination.getParentFile(), destination.getName() + ".pending");
        try (FileOutputStream output = new FileOutputStream(pending, false)) {
            output.write(content.getBytes(StandardCharsets.UTF_8));
            output.getFD().sync();
        } catch (IOException error) {
            pending.delete();
            return error.getMessage();
        }
        try {
            Os.rename(pending.getAbsolutePath(), destination.getAbsolutePath());
            return null;
        } catch (ErrnoException error) {
            pending.delete();
            return error.getMessage();
        }
    }

    private void startGame() {
        if (launchStarting) return;
        if (importing) {
            setStatus(String.format(text("dmAndroidLaunchImporting"), 0), WARNING);
            return;
        }
        if (!packageManifest().isFile()) {
            setStatus(text("dmAndroidLaunchNeedsImport"), WARNING);
            return;
        }
        // Prepare the selected driver first and abort if it cannot be configured. Never silently substitute a different driver.
        String requestedDriver = DRIVER_STATES[driverIndex];
        Log.i("D2RAndroid", "[driver] launch requested=" + requestedDriver);
        setStatus(text("dmAndroidLaunchDriverPreparing"), MUTED);
        String driverProblem = prepareDriver();
        if (driverProblem != null) {
            Log.e("D2RAndroid", "[driver] launch preparation failed requested=" + requestedDriver
                    + " reason=" + driverProblem);
            setStatus(String.format(text("dmAndroidLaunchDriverFailed"), driverProblem), WARNING);
            return;
        }
        Log.i("D2RAndroid", "[driver] launch configuration prepared requested=" + requestedDriver
                + " strict_custom=" + !requestedDriver.equals("system"));
        setStatus("", PARCHMENT);
        StringBuilder content = new StringBuilder();
        content.append("# 由启动界面写入 | written by the launcher\n");
        content.append("debug_overlay=").append(overlaySwitch.isChecked() ? "1" : "0").append('\n');
        content.append("operation_mode=").append(handheld ? "handheld" : "console").append('\n');
        String presentHz = LaunchSettings.read(configFile(), "present_hz");
        content.append("present_hz=").append(presentHz == null ? "60" : presentHz).append('\n');
        if (resolutionIndex < 0) content.append("resolution=").append(customResolution).append('\n');
        else if (!RESOLUTION_PRESETS[resolutionIndex].isEmpty()) content.append("resolution=").append(RESOLUTION_PRESETS[resolutionIndex]).append('\n');
        content.append(LaunchSettings.foreign(configFile(), OWNED_KEYS));
        String settingsProblem = commitLaunchSettings(content.toString());
        if (settingsProblem != null) {
            Log.e("D2RAndroid", "[android-startup] launch settings commit failed; game not started: " + settingsProblem);
            setStatus(settingsProblem, WARNING);
            return;
        }
        Log.i("D2RAndroid", "[android-startup] launch settings committed; starting NativeActivity");
        setStatus("", PARCHMENT);
        launchStarting = true;
        updateStartState();
        try {
            startActivity(new Intent(this, D2RActivity.class));
        } catch (RuntimeException error) {
            launchStarting = false;
            updateStartState();
            Log.e("D2RAndroid", "[android-startup] activity launch failed", error);
            setStatus(String.valueOf(error.getMessage()), WARNING);
        }
    }
}
