#include <string.h>

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned short word;

__sfr __at (0x6) watchdog_strobe;

byte __at (0x2400) vidmem[224][32]; // 256x224x1 video memory

void main();
// start routine @ 0x0
// set stack pointer
void start() {
__asm
	LD	SP,#0x2400
	DI	
__endasm;
	main();
}

/// GRAPHICS FUNCTIONS

void clrscr() {
  memset(vidmem, 0, sizeof(vidmem));
}

inline void xor_pixel(byte x, byte y) {
  byte* dest = &vidmem[x][y>>3];
  *dest ^= 0x1 << (y&7);
}

void draw_vline(byte x, byte y1, byte y2) {
  byte yb1 = y1/8;
  byte yb2 = y2/8;
  byte* dest = &vidmem[x][yb1];
  signed char nbytes = yb2 - yb1;
  *dest++ ^= 0xff << (y1&7);
  if (nbytes > 0) {
    while (--nbytes > 0) {
      *dest++ ^= 0xff;
    }
    *dest ^= 0xff >> (~y2&7);
  } else {
    *--dest ^= 0xff << ((y2+1)&7);
  }
}

#define LOCHAR 0x20
#define HICHAR 0x5e

// TODO:
const byte font8x8[HICHAR-LOCHAR+1][8] = {};
  
void draw_char(char ch, byte x, byte y) {
  byte i;
  const byte* src = &font8x8[(ch-LOCHAR)][0];
  byte* dest = &vidmem[x*8][y];
  for (i=0; i<8; ++i) {
    *dest = *src;
    dest += 32;
    src += 1;
  }
}

void draw_string(const char* str, byte x, byte y) {
  do {
    byte ch = *str++;
    if (!ch) break;
    draw_char(ch, x, y);
    x++;
  } while (1);
}

////

void draw_font() {
  byte i = LOCHAR;
  do {
    draw_char(i, i&15, 31-(i>>4));
    draw_vline(i, i, i*2);
    xor_pixel(i*15, i);
  } while (++i != HICHAR);
}

void main() {
  clrscr();
  draw_font();
  draw_string("HELLO WORLD", 0, 0);
  while (1) watchdog_strobe = 0;
}
