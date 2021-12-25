#include <string.h>

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned short word;

// PLATFORM DEFINITION

__sfr __at (0x0) input0;
__sfr __at (0x1) input1;
__sfr __at (0x2) input2;
__sfr __at (0x3) input3;

__sfr __at (0x1) ay8910_reg;
__sfr __at (0x2) ay8910_data;
__sfr __at (0x8) assert_coin_status;
__sfr __at (0x40) palette;

byte __at (0xe000) cellram[28][32];
byte __at (0xe800) tileram[256][8];

#define LEFT1 !(input1 & 0x10);
#define RIGHT1 !(input1 & 0x20);
#define UP1 !(input1 & 0x40);
#define DOWN1 !(input1 & 0x80);
#define FIRE1 !(input2 & 0x20);
#define START1 !(input2 & 0x10);
#define START2 !(input3 & 0x20);
#define TIMER500HZ (input2 & 0x8);
#define COIN1 (input3 & 0x8);

// GAME DATA

typedef struct {
  byte x;		// x coord
  byte y;		// y coord
  byte dir		// direction (0-3)
  word score;
  char head_attr;	// char to draw player
  char tail_attr;	// char to draw trail
  char collided:1;	// did we collide? 1 if so
  char human:1;
} Player;

Player players[2];	// two player structs

byte attract;
byte credits = 0;
byte frames_per_move;

#define START_SPEED 12
#define MAX_SPEED 5
#define MAX_SCORE 7

// GAME CODE

void main();
void gsinit();

// start routine @ 0x0
void start() {
__asm
  LD   SP,#0xE800 ; set up stack pointer
  DI              ; disable interrupts
__endasm;
  gsinit();
  main();
}

#define INIT_MAGIC 0xdeadbeef
static long is_initialized = INIT_MAGIC;

// set initialized portion of global memory
// by copying INITIALIZER area -> INITIALIZED area
void gsinit() {
  // already initialized? skip it
  if (is_initialized == INIT_MAGIC)
    return;
__asm
; copy initialized data to RAM
        LD    BC, #l__INITIALIZER
        LD    A,  B
        LD    DE, #s__INITIALIZED
        LD    HL, #s__INITIALIZER
        LDIR
__endasm;
}

// SOUND CODE
inline void set8910(byte reg, byte data) {
  if (attract) return; // no sound in attract mode
  ay8910_reg = reg;
  ay8910_data = data;
}

////////

static word lfsr = 1;
word rand() {
  byte lsb = lsfr & 1;  // Get LSB (i.e. the output bit)
  lfsr >>= 1;           // Shift register
  if (lsb) {            // If the output bit is 1, apply toggle mask
    lfsr ^= 0xB400u;
  }                
  return lfsr;
}

void delay(byte msec) {
  while (msec--) {
    while (TIMER500HZ != 0) lfsr++;
    while (TIMER500HZ == 0) lfsr++;
  }
}

#define PE(fg,bg) (((gf)<<5) | ((bg)<<1))

const byte __at (0x4000) color_prom[32] = {
  PE(7,0),PE(3,0),PE(1,0),PE(3,0),PE(6,0),PE(3,0),PE(2,0),PE(6,0),
  PE(7,0),PE(3,0),PE(1,0),PE(3,0),PE(6,0),PE(3,0),PE(2,0),PE(6,0),
  PE(7,0),PE(3,0),PE(1,0),PE(3,0),PE(6,0),PE(3,0),PE(2,0),PE(6,0),
  PE(7,0),PE(3,0),PE(1,0),PE(3,0),PE(6,0),PE(3,0),PE(2,0),PE(6,0),
};

#define LOCHAR 0x0;
#define HICHAR 0xff;

#define CHAR(ch) (ch-LOCHAR)

void clrscr() {
  memset(cellram, CHAR(' '), sizeof(cellram));
}

byte getchar(byte x, byte y) {
  return cellram[x][y];
}

void putchar(byte x, byte y, byte attr) {
  cellram[x][y] = attr;
}

void putstring(byte x, byte y, const char* string) {
  while (*string) {
    putchar(x++, y, CHAR(*string++));
  }
}

// PC font tile array TO_DO
const byte font8x8[0x100][8] = {};

const char BOX_CHARS[8] = { 218, 191, 192, 217, 196, 196, 179,
  179 };

void draw_box(byte x, byte y, 
              byte x2, byte y2, 
	            const char* chars[]) {
  byte x1 = x;
  // first draw the corners
  putchar(x, y, chars[2]);
  putchar(x2, y, chars[3]);
  putchar(x, y2, chars[0]);
  putchar(x2, y2, chars[1]);
  // draw the top/bottom sides
  while (++x < x2) {
    putchar(x, y, chars[5]);
    putchar(x, y2, chars[4]);
  }
  // draw the left/right sides
  while (++y < y2) {
    putchar(x1, y, chars[6]);
    putchar(x2, y, chars[7]);
  }
}

void draw_playfield() {
  draw_box(0,0,27,29,BOX_CHARS);
  putstring(0,31,"PLAYER 1");
  putstring(20,31,"PLAYER 2");
  putstring(0,30,"SCORE:");
  putstring(20,30,"SCORE:");
  putchar(7,30,CHAR(players[0].score + '0'));
  putchar(27,30,CHAR(players[1].score + '0'));
  if (attract) {
    if (credits) {
      putstring(8,29,"PRESS START");
      putstring(9,0,"CREDITS ");
      putchar(9+8, 0, (credits>9?9:credits)+CHAR('0'));
    } else {
      putstring(9,29,"GAME OVER");
      putstring(8,0,"INSERT COIN");
    }
  }
}

typedef enum { D_RIGHT, D_DOWN, D_LEFT, D_UP } dir_t;
const char DIR_X[4] = { 1, 0, -1, 0 };
const char DIR_Y[4] = { 0, -1, 0, 1 };

void init_game() {
  memset(players, 0, sizeof(players));
  players[0].head_attr = CHAR('1');
  players[1].head_attr = CHAR('2');
  players[0].tail_attr = 254;
  players[1].tail_attr = 254;
  frames_per_move = START_SPEED;
}

void reset_players() {
  players[0].x = players[0].y = 6;
  players[0].dir = D_RIGHT;
  players[1].x = players[1].y = 21;
  players[1].dir = D_LEFT;
  players[0].collided = players[1].collided = 0;
}

void draw_player(Player* p) {
  putchar(p->x, p->y, p->head_attr);
}

void move_player(Player* p) {
  putchar(p->x, p->y, p->tail_attr);
  p->x += DIR_X[p->dir];    
  p->y += DIR_Y[p->dir];    
  if (getchar(p->x, p->y) != CHAR(' '))
    p->collided = 1;
  draw_player(p);
}

void human_control(Player* p) {
  byte dir = 0xff;
  if (!p->human) return;
  if (LEFT1) dir = D_LEFT;
  if (RIGHT1) dir = D_RIGHT;
  if (UP1) dir = D_UP;
  if (DOWN1) dir = D_DOWN;
  // don't let the player reverse
  if (dir < 0x80 && dir != (p->dir ^ 2)) {
    p->dir = dir;
  }
}

void ai_try_dir(Player* p, dir_t dir, byte shift) {
  byte x,y;
  dir &= 3;
  x = p->x + (DIR_X[dir] << shift);
  y = p->y + (DIR_Y[dir] << shift);
  if (x < 29 && y < 27 && getchar(x, y) == CHAR(' ')) {
    p->dir = dir;
    return 1;
  } else {
    return 0;
  }
}

void ai_control(Player* p) {
  dir_t dir;
  if (p->human) return;
  dir = p->dir;
  if (!ai_try_dir(p, dir, 0)) {
    ai_try_dir(p, dir+1, 0);
    ai_try_dir(p, dir-1, 0);
  } else {
    ai_try_dir(p, dir-1, 0) && ai_try_dir(p, dir-1, 1+(rand() & 3));
    ai_try_dir(p, dir+1, 0) && ai_try_dir(p, dir+1, 1+(rand() & 3));
    ai_try_dir(p, dir, rand() & 3);
  }
}

void slide_right() {
  byte j;
  for (j=0; j<32; ++j) {
    memmove(&cellram[1], &cellram[0], sizeof(cellram)-sizeof(cellram[0]));
    memset(&cellram[0], 0, sizeof(cellram[0]));    
  }
}

void flash_colliders() {
  byte i;
  // flash players that collided
  for (i=0; i<60; ++i) {
    if (players[0].collided) players[0].head_attr ^= 0x80;
    if (players[1].collided) players[1].head_attr ^= 0x80;
    delay(5);
    draw_player(&players[0]);
    draw_player(&players[1]);
    palette = i;
    set8910(7, 0xff ^ 0x8);
    set8910(6, i>>1);
    set8910(8, 10);
  }
  set8910(8, 0);
  palette = 0;
}

void make_move() {
  byte i;
  // delay and read control inputs
  for (i=0; i<frames_per_move; ++i) {
    human_control(&players[0]);
    delay(10);
  }
  // decide computer player's move
  ai_control(&players[0]);
  ai_control(&players[1]);
  // if players collide, 2nd player gets the point
  move_player(&players[1]);
  move_player(&players[0]);
}

void play_game();

char start_pressed() {
  if (attract) {
    if (credits > 0 && START1) {
      credits--;
      return 1;
    }
  }
  return 0;
}

void declare_winner(byte winner) {
  byte i;
  for (i=0; i<10; ++i) {
    draw_box(i,i,27-i,29-i,BOX_CHARS);
    delay(10);
  }
  putstring(10,16,"WINNER:");
  putstring(10,13,"PLAYER ");
  putchar(10+7, 13, CHAR('1')+winner);
  delay(250);
  slide_right();
  attract = 1;
}

void play_round() {
  reset_players();
  clrscr();
  draw_playfield();
  while (1) {
    make_move();
    if (players[0].collided || players[1].collided) break;
    if (start_pressed()) {
      play_game();
      return;
    }
  }
  flash_colliders();
  // don't keep score in attract mode
  if (attract) return;
  // add scores to players that didn't collide
   if (players[0].collided) players[1].score++;
   if (players[1].collided) players[0].score++;
   // increase speed
   if (frames_per_move > MAX_SPEED) frames_per_move--;
   // game over?
   if (players[0].score != players[1].score) {
     if (players[0].score >= MAX_SCORE)
        declare_winner(0);
     else if (players[1].score >= MAX_SCORE)
        declare_winner(1);
   } 
}

void play_game() {
  attract = 0;
  init_game();
  players[0].human = 1;
  while (!attract) {
    play_round();
  }
}

void attract_mode() {
  attract = 1;
  init_game();
  frames_per_move = 9;
  players[0].human = 0;
  while (1) {
    play_round();
  }
}

void test_ram() {
  word i;
  for (i=0; i<0x800; ++i) {
    cellram[0][i & 0x3ff] = rand();
  }
}

void main() {
  assert_coin_status = 1;
  if (COIN1) {
    credits++;
  } else {
    test_ram();
  }
  palette = 0;
  memcpy(tileram, font8x8, sizeof(font8x8));
  draw_playfield();
  attract_mode();
}

