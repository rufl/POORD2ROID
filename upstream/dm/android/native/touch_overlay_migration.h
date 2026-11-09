#ifndef DM_TOUCH_OVERLAY_MIGRATION_H
#define DM_TOUCH_OVERLAY_MIGRATION_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Exact signature of the 20:9 touch-layout overlay shipped without sidecars in package v0.6.81. */
static int dm_touch_overlay_is_legacy_package(const char *config_path) {
    FILE *file = fopen(config_path, "r");
    if (!file) return 0;
    int root = 0, archive = 0, sizes = 0, target = 0;
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strcmp(line, "root = 255e1ec8492f984d528581956573be5d")) root = 1;
        else if (!strcmp(line, "vfs-1 = b62845b747b0fe73f81ec482588477e2 418ee8cc6eaee67f340007000030b987")) archive = 1;
        else if (!strcmp(line, "vfs-1-size = 7845886 3816662")) sizes = 1;
        else if (!strcmp(line, "build-target-platform = switch")) target = 1;
    }
    fclose(file);
    return root && archive && sizes && target;
}

static int dm_touch_overlay_is_legacy_install(const char *overlay, const char *config_path) {
    if (!dm_touch_overlay_is_legacy_package(config_path)) return 0;
    char path[1200];
    int n = snprintf(path, sizeof(path), "%s/data/00-00000007.data", overlay);
    return n > 0 && (size_t)n < sizeof(path) && access(path, R_OK) == 0;
}

static int dm_touch_overlay_needs_repair(const char *overlay, const char *config_path) {
    if (!dm_touch_overlay_is_legacy_install(overlay, config_path)) return 0;
    char path[1200];
    int n = snprintf(path, sizeof(path), "%s/ctrl_touch", overlay);
    if (n <= 0 || (size_t)n >= sizeof(path) || access(path, R_OK)) return 1;
    n = snprintf(path, sizeof(path), "%s/touch_strings.tsv", overlay);
    return n <= 0 || (size_t)n >= sizeof(path) || access(path, R_OK);
}
#endif
