package org.d2r.android.port;

public final class LaunchAspectTest {
    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    public static void main(String[] args) {
        check(LaunchSettings.is169Resolution("1280x720"), "1280x720 selects standard 16:9");
        check(LaunchSettings.is169Resolution("1920x1080"), "1920x1080 selects standard 16:9");
        check(!LaunchSettings.is169Resolution("1280x576"), "1280x576 selects extended 20:9");
        check(!LaunchSettings.is169Resolution("1920x864"), "1920x864 selects extended 20:9");
        check(!LaunchSettings.is169Resolution(""), "empty legacy resolution follows the wide default");
        check(!LaunchSettings.is169Resolution(null), "missing legacy resolution follows the wide default");
        check(!LaunchSettings.is169Resolution("invalid"), "malformed custom text does not crash restoration");
        check(LaunchSettings.resolutionForAspect(true).equals("1600x900"), "16:9 docked base; handheld becomes 1280x720");
        check(LaunchSettings.resolutionForAspect(false).equals("1600x720"), "20:9 docked base; handheld becomes 1280x576");
        System.out.println("Launcher aspect toggle: restore, wide/16:9 selection and malformed-input handling pass");
    }
}
