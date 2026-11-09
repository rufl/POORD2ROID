#include "../native/touch_overlay_migration.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static void put(const char *path, const char *text) {
    FILE *f = fopen(path, "w");
    assert(f);
    fputs(text, f);
    assert(fclose(f) == 0);
}

int main(int argc, char **argv) {
    char dir[] = "/tmp/dm-touch-overlay-XXXXXX";
    assert(mkdtemp(dir));
    char data[512], config[1024], archive[1024], marker[1024], strings[1024];
    snprintf(data, sizeof(data), "%s/data", dir);
    assert(mkdir(data, 0700) == 0);
    snprintf(config, sizeof(config), "%s/.build.config", data);
    snprintf(archive, sizeof(archive), "%s/00-00000007.data", data);
    snprintf(marker, sizeof(marker), "%s/ctrl_touch", dir);
    snprintf(strings, sizeof(strings), "%s/touch_strings.tsv", dir);
    const char *touch =
        "root = 255e1ec8492f984d528581956573be5d\n"
        "vfs-1 = b62845b747b0fe73f81ec482588477e2 418ee8cc6eaee67f340007000030b987\n"
        "vfs-1-size = 7845886 3816662\n"
        "build-target-platform = switch\n";
    const char *font =
        "root = 255e1ec8492f984d528581956573be5d\n"
        "vfs-1 = 7e36e0df494eef391388c780d49d2e7e fadcd5c34b962d4a4500070000001376\n"
        "vfs-1-size = 7796846 3794239\n"
        "build-target-platform = switch\n";
    put(config, touch);
    assert(dm_touch_overlay_is_legacy_package(config));
    assert(!dm_touch_overlay_needs_repair(dir, config)); /* archive absent: never guess */
    put(archive, "archive");
    assert(dm_touch_overlay_is_legacy_install(dir, config));
    assert(dm_touch_overlay_needs_repair(dir, config));
    put(marker, "touch");
    put(strings, "strings");
    assert(!dm_touch_overlay_needs_repair(dir, config)); /* complete: idempotent */
    unlink(marker);
    assert(dm_touch_overlay_needs_repair(dir, config)); /* one missing sidecar: repair */
    put(config, font);
    assert(!dm_touch_overlay_is_legacy_package(config)); /* font-only overlay: never switch */
    assert(!dm_touch_overlay_needs_repair(dir, config));
    assert(!dm_touch_overlay_is_legacy_package("/tmp/missing-touch-overlay-config"));

    if (argc == 3) {
        assert(dm_touch_overlay_is_legacy_package(argv[1]));
        assert(!dm_touch_overlay_is_legacy_package(argv[2]));
    } else {
        assert(argc == 1);
    }
    unlink(marker); unlink(strings); unlink(archive); unlink(config); rmdir(data); rmdir(dir);
    puts("legacy touch-overlay migration: match, nonmatch, missing archive, partial repair and idempotency pass");
    return 0;
}
