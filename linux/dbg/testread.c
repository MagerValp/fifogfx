#include <stdio.h>
#include <string.h>
#include <sysexits.h>


void usage(void) {
    puts("Usage: testread [-x]");
}


char scrcode(unsigned char b) {
    char c = (char)(b & 0x1f);

    switch (b & 0x60) {
        case 0x00:
        return c + 0x60;

        case 0x20:
        return c + 0x20;

        case 0x40:
        return c + 0x40;

        case 0x60:
        return '.';
    }
}

typedef enum {
    MODE_PETSCII,
    MODE_HEX
} mode_t;

int main(int argc, char *argv[]) {
    unsigned char buf[65536];
    int x, y;
    mode_t mode = MODE_PETSCII;

    if (argc == 2) {
        if (strcmp(argv[1], "-x")) {
            mode = MODE_HEX;
        } else {
            usage();
            return EX_USAGE;
        }
    } else if (argc != 1) {
        usage();
        return EX_USAGE;
    }

    FILE *fifogfx = fopen("/dev/fifogfx0", "rb");
    if (fifogfx == NULL) {
        perror("/dev/fifogfx0");
        return 1;
    }
    size_t count = fread(buf, sizeof(buf), 1, fifogfx);
    //printf("Read %d bytes from fifogfx\n", sizeof(buf) * count);

    char line[200];
    if (mode == MODE_HEX) {
        for (y = 0; y < 25; y++) {
            for (x = 0; x < 40; x++) {
                sprintf(&line[x * 2], "%02x", buf[0x0400 + y * 40 + x]);
            }
            line[80] = ' ';
            line[81] = ' ';
            for (x = 0; x < 40; x++) {
                sprintf(&line[x + 82], "%c", scrcode(buf[0x0400 + y * 40 + x]));
            }
            puts(line);
        }
    } else {
         for (y = 0; y < 25; y++) {
            for (x = 0; x < 40; x++) {
                sprintf(&line[x], "%c", scrcode(buf[0x0400 + y * 40 + x]));
            }
            puts(line);
        }
    }
}
