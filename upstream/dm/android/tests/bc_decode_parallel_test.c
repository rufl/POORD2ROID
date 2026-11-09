#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../gfx/tex/nvn_bcdec.h"

int main(void) {
    enum { WIDTH = 513, HEIGHT = 513, DEST_PITCH = WIDTH * 4 + 20 };
    const size_t block_pitch = ((WIDTH + 3u) / 4u) * 8u;
    const size_t block_rows = (HEIGHT + 3u) / 4u;
    const size_t source_size = block_pitch * block_rows;
    uint8_t *source = calloc(source_size, 1);
    uint8_t *destination = malloc((size_t)DEST_PITCH * HEIGHT);
    if (!source || !destination) return 2;

    // BC1 RGBA: red endpoint, green endpoint, and selector zero for all pixels.
    for (size_t offset = 0; offset < source_size; offset += 8) {
        source[offset] = 0x00;
        source[offset + 1] = 0xf8;
        source[offset + 2] = 0xe0;
        source[offset + 3] = 0x07;
    }
    memset(destination, 0xa5, (size_t)DEST_PITCH * HEIGHT);
    if (nvn_bc_decode(0x42, source, block_pitch, WIDTH, HEIGHT, destination, DEST_PITCH)) {
        fprintf(stderr, "BC1 parallel decode rejected input\n");
        return 3;
    }
    for (unsigned y = 0; y < HEIGHT; y++) {
        const uint8_t *row = destination + (size_t)y * DEST_PITCH;
        for (unsigned x = 0; x < WIDTH; x++) {
            const uint8_t *pixel = row + x * 4;
            if (pixel[0] != 255 || pixel[1] != 0 || pixel[2] != 0 || pixel[3] != 255) {
                fprintf(stderr, "BC1 pixel mismatch at %u,%u: %u,%u,%u,%u\n", x, y,
                        pixel[0], pixel[1], pixel[2], pixel[3]);
                return 4;
            }
        }
        for (unsigned x = WIDTH * 4; x < DEST_PITCH; x++) {
            if (row[x] != 0xa5) {
                fprintf(stderr, "BC1 row padding overwritten at %u,%u\n", x, y);
                return 5;
            }
        }
    }
    free(destination);
    free(source);
    puts("BC1 parallel decode PASS: 513x513, block-row partitioning and destination padding");
    return 0;
}
