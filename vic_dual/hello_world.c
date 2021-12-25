#include <stdio.h>
#include <string.h>

void main();

void start() {
__asm
  LD   SP,#0xE800 ; set up stack pointer
  DI    	  ; disable interrupts
__endasm;

  main();
}

char __at (0xe000) cellram[32][32];
char __at (0xe800) tileram[256][8];

char cursor_x;
char cursor_y;

const char font8x8[0x100][8] = {};

void clrscr() {
  memset(cellram, 0, sizeof(cellram));
}

void setup_stdio() {
  memcpy(tileram, font8x8, sizeof(font8x8));
  cursor_x = 0;
  cursor_y = 0;
  clrscr();
}

void scrollup() {
  char i;
  memmove(&cellram[0][1], &cellram[0][0], sizeof(cellram)-1);
  for (i=0; i<32; ++i)
    cellram[i][0] = 0;
}

void newline() {
  if (cursor_y >= 31) {
    scrollup();
  }  else {
    cursor_y++;
  }
}

int putchar(int ch) {
  switch (ch) {
    case '\n':
      newline();
    case '\r':
      cursor_x = 0;
      return 0;
  }
  cellram[cursor_x][31-cursor_y] = ch;
  cursor_x++;
  if (cursor_x >= 29) {
    newline();
    cursor_x = 0;
  }
}

void main() {
  unsigned char byteval = 123;
  signed char charval = 123;
  short shortval = 12345;
  setup_stdio();
  printf("HELLO WORLD!\n");
  while (1) {
    printf("char %d byte %u sh %d\n",
      charval++, byteval++, shortval++);
  }
}
