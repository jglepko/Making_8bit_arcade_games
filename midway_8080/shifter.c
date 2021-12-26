include <string.h>

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned short word;

__sfr __at (0x2) bitshift_offset;
volatile __sfr __at (0x3) bitshift_read;
__sfr __at (0x4) bitshift_value;
__sfr __at (0x2) watchdog_strobe;

byte __at (0x2400) vidmem[224][32]; // 256x224x1 video memory

void main();
// start routine @ 0x0
// set stack pointer, enable interrupts
void start() {
__asm
        LD      SP,#0x2400
        DI
__endasm;
        main();
}

// TODO:
const byte bitmap1[] = {};

void draw_shifted_sprite(const byte* src, byte x, byte y) {
  byte i,j;
  byte* dest = &vidmem[x][y>>3];
  byte w = *src++;
  byte h = *src++;
  bitshift_offset = y & 7;
  for (j=0; j<h; ++j) {
    bitshift_value = 0;
    for (i=0; i<w; ++i) {
      bitshift_value = *src++;
      *dest++ = bitshift_read;
    }
    bitshift_value = 0;
    *dest++ = bitshift_read;
    dest += 31-w; 
  }
}

void clrscr() {
  memset(vidmem, 0, sizeof(vidmem));
}

void main() {
  byte x;
  // TODO: clear memory
  clrscr();
  for (x=0; x<255; ++x) {
    draw_shifted_sprite(bitmap1, x, x);
    watchdog_strobe++;
  }
  main();
}
