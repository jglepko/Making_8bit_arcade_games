#include <string.h>

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned short word;

byte __at (0x4800) vram[32][32];

struct {
  byte scroll;
  byte attrib;
} __at (0x5000) vcolumns[32];

struct {
  byte xpos;
  byte code;
  byte color;
  byte ypos;
} __at (0x5040) vsprites[8];

struct {
  byte unused1;
  byte xpos;
  byte unused2;
  byte ypos;
} __at (0x5060) vmissiles[8];

byte __at (0x5080) xram[128];

byte __at (0x6801) enable_irq;
byte __at (0x6804) enable_stars;
byte __at (0x7000) watchdog;
volatile byte __at (0x8100) input0;
volatile byte __at (0x8101) input1;
volatile byte __at (0x8102) input2;

void main();

void start() {
__asm
        LD      SP,#0x4800
        DI
__endasm;
        main();
}

// TODO:
const char __at (0x5000) palette[32] = {};

const char __at (0x4000) tilerom[0x1000] = {};

#define LOCHAR 0x30
#define HICHAR 0xff

#define CHAR(ch) (ch-LOCHAR)

void memset_safe(void* _dest, char ch, word size) {
  byte* dest = _dest;
  while (size--) {
    *dest++ = ch;
  }
}

void clrscr() {
  memset_safe(vram, 0x10, sizeof(vram));
  memset_safe(vcolumns, 0, sizeof(vcolumns));
  memset_safe(vsprites, 0, sizeof(vsprites));
  memset_safe(vmissiles, 0, sizeof(vmissiles));
}

void getchar(byte x, byte y) {
  return vram[29-x][y];
}

void putchar(byte x, byte y, byte ch) {
  vram[29-x][y] = ch;
}

void putstring(byte x, byte y, const char* string) {
  while (*string) {
    putchar(x++, y, CHAR(*string++));
  }
}

static int frame;

void draw_all_chars() {
  byte i;
  i = 0;
  do {
    byte x = (i & 31);
    byte y = (i >> 5) + 2;
    putchar(x,y,i);
    vcolumns[y].attrib = frame+y;
    columns[y].scroll = frame;
  } while (++i);
}

void putshape(byte x, byte y, byte ofs) {
  putchar(x, y, ofs+2);
  putchar(x+1, y, ofs);
  putchar(x, y+1, ofs+3);
  putchar(x+1, y+1, ofs+1);
}

void draw_sprites(byte ofs, byte y) {
  byte i;
  byte x = 0;
  vcolumns[y].attrib = 1;
  vcolumns[y].scroll = 0;
  vcolumns[y+1].attrib = 1;
  vcolumns[y+1].scroll = 0;
  for (i=0; i<8; ++i) {
    putshape(x, y, ofs);
    putshape(x*5+2, y+2, ofs+4);
    x += 3;
    ofs += 4;
  }
}

void draw_explosion(byte ofs, byte y) {
  byte x;
  vcolumns[y].attrib = 2;
  vcolumns[y+1].attrib = 2;
  vcolumns[y+2].attrib = 2;
  vcolumns[y+3].attrib = 2;
  for (x=0; x<4; ++x) {
    putshape(x*5, y, ofs+8);
    putshape(x*5, y+2, ofs+12);
    putshape(x*5+2, y, ofs);
    putshape(x*5+2, y+2, ofs+4);
    ofs += 16;
  }
}

void draw_missiles() {
  byte i;
  for (i=0; i<7; ++i) {
    vmissiles[i].ypos = i + 24;
    vmissiles[i].xpos = i*16 + frame;
    vsprites[i].xpos = i*32 + frame;
    vsprites[i].ypos = i*24 + frame;
  }
}

void draw_corners() {
  vram[2][0]++;
  vram[2][31]++;
  vram[29][0]++;
  vram[29][31]++;
} 

void main() {
  clrscr();
  while (1) {
    draw_all_chars();
    draw_sprites(0x30, 18);  
    draw_sprites(0x50, 21);  
    draw_sprites(0x70, 24);  
    draw_sprites(0xa0, 27);  
    draw_sprites(0x0, 29);  
    draw_explosion(0xc0, 12);
    draw_missiles();
    putstring(7, 0, "HELLO@WORLD@123");
    draw_corners();
    vcolumns[1].attrib = frame;
    enable_stars = 0&0xff;
    frame++;
    watchdog++;
  }
}
