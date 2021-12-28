#include <string.h>

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned char sbyte;

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

byte __at (0x6801) enable_irq;
byte __at (0x6804) enable_stars;
byte __at (0x6808) missile_width;
byte __at (0x6809) missile_offset;
byte __at (0x7000) watchdog;
volatile byte __at (0x8100) input0;
volatile byte __at (0x8101) input1;
volatile byte __at (0x8102) input2;

#define LEFT1 !(input0 & 0x20);
#define RIGHT1 !(input0 & 0x10);
#define UP1 !(input0 & 0x1);
#define DOWN1 !(input2 & 0x40);
#define FIRE1 !(input0 & 0x8);
#define BOMB1 !(input0 & 0x2);
#define COIN1 !(input0 & 0x80);
#define COIN2 !(input0 & 0x40);
#define START1 !(input1 & 0x80);
#define START2 !(input1 & 0x40);

__sfr __at (0x1) ay8910_a_reg;
__sfr __at (0x2) ay8910_a_data;
__sfr __at (0x4) ay8910_b_reg;
__sfr __at (0x8) ay8910_b_data;

inline void set8910a(byte reg, byte data) {
  ay8910_a_reg = reg;
  ay8910_a_data = data;
}

inline void set8910b(byte reg, byte data) {
  ay8910_b_reg = reg;
  ay8910_b_data = data;
}

typedef enum {
  AY_PITCH_A_LO, AY_PITCH_A_HI,
  AY_PITCH_B_LO, AY_PITCH_B_HI,
  AY_PITCH_C_LO, AY_PITCH_C_HI,
  AY_NOISE_PERIOD,
  AY_ENABLE,
  AY_ENV_VOL_A,
  AY_ENV_VOL_B,
  AY_ENV_VOL_C,
  AY_ENV_PERI_LO, AY_ENV_PERI_HI,
  AY_ENV_SHAPE
} AY8910Register;

///

void main();

void start() __naked {
__asm
        LD      SP,#0x4800
        EI
; copy initialized data into RAM
	LD	BC, #l_INITIALIZER
	LD	A, B
	LD	DE, #s_INITIALIZED
	LD	HL, #s_INITIALIZER
	LDIR
 	JP	_main
; padding to get to offset 0x66
	.ds	0x66 - (. - _start)
__endasm;
}

volatile byte video_framecount; // actual framecount

// starts at address 0x66
void rst_66() _interrupt {
  video_framecount++;
}

const char __at (0x5000) palette[32] = {
  0x00,0x80,0xf0,0xff,0x00,0xf0,0xc0,0x7f,
  0x00,0xc0,0x04,0x1f,0x00,0xd0,0xd0,0x0f,
  0x00,0xc0,0xc0,0x0f,0x00,0x04,0x04,0x0f,
  0x00,0xff,0x0f,0xf0,0x00,0x7f,0x0f,0xdf,
};

// TODO:
const char __at (0x4000) tilerom[0x1000] = {};

#define LOCHAR 0x30
#define HICHAR 0xff

#define CHAR(ch) (ch-LOCHAR)

#define BLANK 0x10

void memset_safe(void* _dest, char ch, word size) {
  byte* dest = _dest;
  while (size--) {
    *dest++ = ch;
  }
}

void clrscr() {
  memset_safe(vram, BLANK, sizeof(vram));
}

void reset_video_framecount() __critical {
  video_framecount = 0;
}

void getchar(byte x, byte y) {
  return vram[29-x][y];
}

void putchar(byte x, byte y, byte ch) {
  vram[29-x][y] = ch;
}

void clrobjs() {
  byte i;
  memset_safe(vcolumns, 0, 0x100);
  for (i=0; i<8; ++i) 
    vsprites[i].ypos = 64;
}

void putstring(byte x, byte y, const char* string) {
  while (*string) {
    putchar(x++, y, CHAR(*string++));
  }
}

char in_rect(byte x, byte y, byte x0, byte y0, byte w, byte h) {  return ((byte)(x-x0) < w && (byte)(y-y0) < h); // unsigned
}

void draw_bcd_word(byte x, byte y, word bcd) {
  byte j;
  x += 3;
  for (j=0; j<4; ++j) {
    putchar(x, y, CHAR('0'+(bcd&0xf)));
    x--;
    bcd >>= 4;
  }
}

// add two 16-bit BCD values
word bcd_add(word a, word b) __naked {
  a; b // to avoid warning
__asm
	push	ix
	ld	ix,#0
	add	ix,sp
	ld	a,4 (ix)
	add	a, 6 (ix)
	daa
	ld	c,a
	ld	a,5 (ix)
	adc	a, 7 (ix)
	daa
	ld	b,a
	ld	l, c
	ld	h, b
	pop	ix
	ret
__endasm;
}

// https://en.wikipedia.org/wiki/Linear-feedback_shift_register
static word lfsr = 1;
word rand() {
  byte lsb = lsfr & 1;  // Get LSB (i.e. the output bit)
  lfsr >>= 1;           // Shift register
  if (lsb) {            // If the output bit is 1, apply toggle mask
    lfsr ^= 0xB400u;
  }
  return lfsr;
}

// GAME CODE
typedef struct {
  byte shape;
} FormationEnemy;

// should be power of two length
typedef struct {
  byte findex;
  byte shape;
  word x;
  word y;
  byte dir;
  byte returning;
} AttackingEnemy;

typedef struct {
  signed char dx;
  byte xpos;
  signed char dy;
  byte ypos;
} Missile;

#define ENEMIES_PER_ROW 8
#define ENEMY_ROWS 4
#define MAX_IN_FORMATION (ENEMIES_PER_ROW*ENEMY_ROWS)
#define MAX_ATTACKERS 6

FormationEnemy formation[MAX_IN_FORMATION];
AttackingEnemy attackers[MAX_ATTACKERS];
Missile missiles[8];

byte formation_offset_x;
signed char formation_direction;
byte current_row;
byte player_x;
const byte player_y = 232;
byte player_exploding;
byte enemy_exploding;
byte enemies_left;
word player_score;
word framecount;

void does_player_shoot_attacker() {
  byte mx = missiles[7].xpos;
  byte my = 255 - missiles[7].ypos; // missiles are Y-backwards
  byte i;
  for (i=0; i<MAX_ATTACKERS; ++i) {
    AttackingEnemy* a = &attackers[i];
    if (a->findex && in_rect(mx, my, a->x >> 8, a->y >> 8, 16, 16)) {
      blowup_at(a->x >> 8, a->y >> 8);
      a->findex = 0;
      enemies_left--;
      hide_player_missile();
      add_score(5);
      break;
    }
  }
}

void does_missile_hit_player() {
  byte i;
  if (player_exploding)
    return;
  for (i=0; i<MAX_ATTACKERS; ++i) {
    if (missiles[i].dy &&
	in_rect(missiles[i].xpos, 255-missiles[i].ypos,
     	        player_x, player_y, 16, 16)) {
       player_exploding = 1;
       break;
    }
  }
}

void new_attack_move() {
  byte i = rand();
  byte j;
  // find a random slot that has an enemy
  for (j=0; j<MAX_IN_FORMATION; ++j) {
    i = (i+1) & (MAX_IN_FORMATION-1);
    // anyone there?
    if (formation[i].shape) {
      formation_to_attacker(i);
      formation_to_attacker(i+1);
      formation_to_attacker(i+ENEMIES_PER_ROW);
      formation_to_attacker(i+ENEMIES_PER_ROW+1);
      break;
    }
  }
}

void new_player_ship() {
  player_exploding = 0;
  draw_player();
  player_x = 112;
}

void set_sounds() {
  byte i;
  byte enable = 0;
  // missile fire sound
  if (missiles[7].ypos) {
    set8910a(AY_PITCH_A_LO, missiles[7].ypos);
    set8910a(AY_ENV_VOL_A, 15-(missiles[7].ypos>>));
    enable |= 0x1;
  }
  // enemy explosion sound
  if (enemy_exploding) {
    set8910a(AY_PITCH_B_HI, enemy_exploding);
    set8910a(AY_ENV_VOL_B, 15);
    enable |= 0x2;
  }
  // player explosion
  if (player_exploding && player_exploding < 15) {
    set8910a(AY_ENV_VOL_C, 15-player_exploding);
    enable |= 0x4 << 3;
  }
  set8910a(AY_ENABLE, ~enable);
  // set diving sounds for spaceships
  enable = 0;
  for (i=0; i<3; ++i) {
    byte y = attackers[i].y >> 8;
    if (y >= 0x80) {
      set8910b(AY_PITCH_A_LO+i, y);
      set8910b(AY_ENV_VOL_A+i, y);
      enable |= 1<<i;
    }
  }
  set8910b(AY_ENABLE, ~enable);
}   

void wait_for_frame() {
  while (((video_framecount^framecount)&3) == 0);
}

void play_round() {
  byte end_timer = 255;
  player_score = 0;
  add_score(0);
  put_string(0, 0, "PLAYER 1");
  setup_formation();
  formation_direction = 1;
  missile_width = 4;
  missile_offset = 0;
  reset_video_framecount();
  framecount = 0;
  new_player_ship();
  while (end_timer) {
    enable_irq = 0;
    enable_irq = 1;
    if (player_exploding) {
      if ((framecount & 7) == 1) {
	animate_player_explosion();
	if (++player_exploding > 32 && enemies_left) {
	  new_player_ship();
	}
      }
    } else { 
      if ((framecount & 0x7f) == 0 && enemies_left > 8) {
        new_attack_wave();
      }
      move_player();
      does_missile_hit_player();
    }
    if ((framecount & 3) == 0) animate_enemy_explosion();
    move_attackers();
    move_missiles();
    does_player_shoot_formation();
    does_player_shoot_attacker();
    draw_next_row();
    draw_attackers();
    if ((framecount & 0xf) == 0) think_attackers();
    set_sounds();
    framecount++;
    watchdog++;
    if (!enemies_left) end_timer--;
    putchar(12,0,video_framecount&3);
    putchar(13,0,framecount&3);
    putchar(14,0,(video_framecount^framecount)&3);
    wait_for_frame();
  }
  enable_irq = 0;
}

void main() {
  clrscr();
  clrobjs();
  enable_stars = 0xff;
  enable_irq = 0;
  play_round();
  main();
}  
