#include "../native/startup_gate.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(!dm_android_startup_ready(0, 0));
    assert(!dm_android_startup_ready(1, 0)); /* Surface may arrive before onResume. */
    assert(!dm_android_startup_ready(0, 1)); /* onResume may arrive before the surface. */
    assert(dm_android_startup_ready(1, 1));
    puts("Android startup gate: waits for both resumed activity and native window");
    return 0;
}
