#include <stdio.h>
#include <string.h>
#include <sysexits.h>


void usage(void) {
    puts("Usage: testread [-x]");
}


int main(int argc, char *argv[]) {
    unsigned char buf[65536];
    int x, y;

    FILE *fifogfx = fopen("/dev/fifogfx0", "rb");
    if (fifogfx == NULL) {
        perror("/dev/fifogfx0");
        return 1;
    }
    size_t count = fread(buf, sizeof(buf), 1, fifogfx);
    //printf("Read %d bytes from fifogfx\n", sizeof(buf) * count);

    if (buf[0x03fc] != 0xf1 || buf[0x03fd] != 0xf0) {
        puts("No F1F0 marker at $03fc");
        return 2;
    }
    
    int addr = buf[0x03fe] | (buf[0x03ff] << 8);
    printf("Display list at $%04x:\n", addr);
    count = 0;
    do {
        printf("  $%04x: ", addr);
        int op = buf[addr++];
        int arg = buf[addr++];
        if (op == 0x80 && arg == 0x00) {
            puts("END");
            break;
        } else if (op & 0x80) {
            int hpos = (op >> 1) & 0x3f;
            int vpos = ((op & 1) << 8) | arg;
            printf("WAIT %d, %d\n", hpos, vpos);
        } else {
            printf("MOVE $%02x, #$%02x\n", op, arg);
        }
    } while (++count < 100);
}
