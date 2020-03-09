#include "debug8.h"
#include "d8_dis.h"
#include "d8_asm.h"
#include "d8sprite.h"

BYTE regs[NUM_REGISTERS];  /* V0 .. VF */
BYTE hp48regs[8];  /* may as well implement these, too... */
WORD pc;  /* program counter */
WORD sp = STACK_START;
          /* Note:  Although stack size is 16 entires, it starts at
           *        memory address 0x01E0, and so has to be a WORD
           */
WORD I_reg;   /* I register */
BYTE memory[MEM_SIZE];
   /* Note:  CHIP-8 code is stored BIG-ENDIAN.  Native x86 code is LITTLE-ENDIAN */
WORD program_load_addr = PROGRAM_START;  /* variable, for ETI-660 support */

BYTE video_memory[VIDEO_MEMORY_SIZE];
    /* +-----------------------------+
     * | (0,0)           (maxcol, 0) |
     * |                             |
     * |                             |
     * |                             |
     * | (0,maxrow)  (maxcol,maxrow) |
     * +-----------------------------+
     */
BYTE super_chip8_video = FALSE;  /* FALSE == 64 x 32, TRUE == 128 x 64 */
BYTE startup_video_super_chip8 = FALSE;


BYTE sound_timer;
BYTE delay_timer;
WORD execution_ticks_counter;
BYTE execution_tick_flag;
BYTE system_clock_count = CHIP8_EXECUTION_TICKS_TO_CLK_TICKS;
BYTE system_clock_divisor = CHIP8_EXECUTION_TICKS_TO_CLK_TICKS;
       /* need CHIP8_EXECUTION_TICKS_TO_CLK_TICKS in a variable for assembly code */
BYTE sixty_hz_divisor = CHIP8_EXECUTION_TICKS_TO_60HZ;
       /* need CHIP8_EXECUTION_TICKS_TO_60HZ in a variable for assembly code */
BYTE sixty_hz_count = CHIP8_EXECUTION_TICKS_TO_60HZ;
BYTE sixty_hz_flag;
WORD sound_frequency = DEFAULT_SOUND_FREQUENCY;

/* following variables are for inline assembly language */
WORD screen_cols = LOWRES_GRAPHICS_COLS;
WORD screen_rows = LOWRES_GRAPHICS_ROWS;
WORD max_screen_cols = LOWRES_GRAPHICS_COLS - 1;
WORD max_screen_rows = LOWRES_GRAPHICS_ROWS - 1;
WORD video_seg = GRAPHICS_SEG;
BYTE white = WHITE;
BYTE black = BLACK;
BYTE wrap_sprites = FALSE;
       /* according to the documentation, it looks like the DRAW
        * command WRAPS the sprite to the start of the next line
        * if the sprite goes beyond the "right edge" of the screen.
        *
        * However, some CHIP8 programs don't work correctly unless
        * the sprite STOPS at the "right edge" of the screen.
        *
        * The default will be to STOP the sprite at the edge of the
        * screen, but this behavior can be toggled.
        */
BYTE use_vy_shift = FALSE;
   /* The documentation on the 8xyE instruction between various
    * sources does not agree.  Sources like devernay.free.fr
    * and craigthomas.ca/blog say that 8xyE is Vx = Vx << 1, VF = carry
    *
    * However, github.com/mattmikolay/chip-8/wiki/CHIP-8-Instruction-Set
    * says that 8xyE is Vx = Vy << 1, VF = carry
    *
    * The Wikipedia article en.wikipedia.org/wiki/CHIP-8 notes this
    * discrepancy (see the Opcode Table notes).
    */
BYTE i_reg_is_incremented = FALSE;
   /* The documentation on the I register is inconsistent.  Some
    * sources (see above) say that the instructions Fx55 (LD [I], Vx)
    * and Fx65 (LD Vx, [I]) do not increment I, other say it does.
    */
BYTE i_reg_sets_carry = FALSE;
   /* An "undocumented" feature is that the ADD I,Vx instruction will
    * will set VF to CARRY.  The standard sources (see above) don't
    * document this.  It is not the standard behavior because destroying
    * VF can cause problems with programs that don't expect it.
    *
    * Wikipedia (en.wikipedia.org/wiki/CHIP-8) notes that the program
    * Spacefight 2091! uses this "undocumented" feature.
    */

WORD opcode;       /* full opcode, will be used in pieces as needed */
BYTE opcode_high;  /* MSB */
BYTE opcode_main;  /* high nibble of opcode MSB shifted 4 right */
BYTE opcode_x;     /* opcode MSB & 0x0f */
BYTE opcode_low;   /* LSB, also 8-bit constant value */
BYTE opcode_y;     /* high nibble of opcode LSB shifted 4 right */
BYTE opcode_subfn; /* secondary opcode function, etc */
BYTE opcode_n;     /* nibble */
WORD opcode_xxx;   /* opcode word & ADDRESS_MASK --> 12 bit value for ADDR and I reg */

WORD execution_ticks_per_instruction = 1;
WORD instructions_per_execution_tick = 1;
WORD instruction_count[4];
BYTE execution_freerun = FALSE;

BYTE hex_sprites[HEX_SPRITES_NUM_BYTES] =
   {
   0xF0,  /* 11110000   xxxx   */
   0x90,  /* 10010000   x  x   */
   0x90,  /* 10010000   x  x   */
   0x90,  /* 10010000   x  x   */
   0xF0,  /* 11110000   xxxx   */

   0x20,  /* 00100000     x    */
   0x60,  /* 01100000    xx    */
   0x20,  /* 00100000     x    */
   0x20,  /* 00100000     x    */
   0x70,  /* 01110000    xxx   */

   0xF0,  /* 11110000   xxxx   */
   0x10,  /* 00010000      x   */
   0xF0,  /* 11110000   xxxx   */
   0x80,  /* 10000000   x      */
   0xF0,  /* 11110000   xxxx   */

   0xF0,  /* 11110000   xxxx   */
   0x10,  /* 00010000      x   */
   0xF0,  /* 11110000   xxxx   */
   0x10,  /* 00010000      x   */
   0xF0,  /* 11110000   xxxx   */

   0x90,  /* 10010000   x  x   */
   0x90,  /* 10010000   x  x   */
   0xF0,  /* 11110000   xxxx   */
   0x10,  /* 00010000      x   */
   0x10,  /* 00010000      x   */

   0xF0,  /* 11110000   xxxx   */
   0x80,  /* 10000000   x      */
   0xF0,  /* 11110000   xxxx   */
   0x10,  /* 00010000      x   */
   0xF0,  /* 11110000   xxxx   */

   0xF0,  /* 11110000   xxxx   */
   0x80,  /* 10000000   x      */
   0xF0,  /* 11110000   xxxx   */
   0x90,  /* 10010000   x  x   */
   0xF0,  /* 11110000   xxxx   */

   0xF0,  /* 11110000   xxxx   */
   0x10,  /* 00010000      x   */
   0x20,  /* 00100000     x    */
   0x40,  /* 01000000    x     */
   0x40,  /* 01000000    x     */

   0xF0,  /* 11110000   xxxx   */
   0x90,  /* 10010000   x  x   */
   0xF0,  /* 11110000   xxxx   */
   0x90,  /* 10010000   x  x   */
   0xF0,  /* 11110000   xxxx   */

   0xF0,  /* 11110000   xxxx   */
   0x90,  /* 10010000   x  x   */
   0xF0,  /* 11110000   xxxx   */
   0x10,  /* 00010000      x   */
   0xF0,  /* 11110000   xxxx   */

   0xF0,  /* 11110000   xxxx   */
   0x90,  /* 10010000   x  x   */
   0xF0,  /* 11110000   xxxx   */
   0x90,  /* 10010000   x  x   */
   0x90,  /* 10010000   x  x   */

   0xE0,  /* 11100000   xxx    */
   0x90,  /* 10010000   x  x   */
   0xE0,  /* 11100000   xxx    */
   0x90,  /* 10010000   x  x   */
   0xE0,  /* 11100000   xxx    */

   0xF0,  /* 11110000   xxxx   */
   0x80,  /* 10000000   x      */
   0x80,  /* 10000000   x      */
   0x80,  /* 10000000   x      */
   0xF0,  /* 11110000   xxxx   */

   0xE0,  /* 11100000   xxx    */
   0x90,  /* 10010000   x  x   */
   0x90,  /* 10010000   x  x   */
   0x90,  /* 10010000   x  x   */
   0xE0,  /* 11100000   xxx    */

   0xF0,  /* 11110000   xxxx   */
   0x80,  /* 10000000   x      */
   0xF0,  /* 11110000   xxxx   */
   0x80,  /* 10000000   x      */
   0xF0,  /* 11110000   xxxx   */

   0xF0,  /* 11110000   xxxx   */
   0x80,  /* 10000000   x      */
   0xF0,  /* 11110000   xxxx   */
   0x80,  /* 10000000   x      */
   0x80   /* 10000000   x      */
   };

BYTE hex_sprites_super[HEX_SPRITES_SUPER_NUM_BYTES] =
   {
   0x3C,  /* 00111100    xxxx   */
   0x7E,  /* 01111110   xxxxxx  */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0x7E,  /* 01111110   xxxxxx  */
   0x3C,  /* 00111100    xxxx   */

   0x18,  /* 00011000     xx    */
   0x38,  /* 00111000    xxx    */
   0x58,  /* 01011000   x xx    */
   0x18,  /* 00011000     xx    */
   0x18,  /* 00011000     xx    */
   0x18,  /* 00011000     xx    */
   0x18,  /* 00011000     xx    */
   0x18,  /* 00011000     xx    */
   0x18,  /* 00011000     xx    */
   0x3C,  /* 00111100    xxxx   */

   0x3E,  /* 00111110    xxxxx  */
   0x7F,  /* 01111111   xxxxxxx */
   0xC3,  /* 11000011  xx    xx */
   0x06,  /* 00000110       xx  */
   0x0C,  /* 00001100      xx   */
   0x18,  /* 00011000     xx    */
   0x30,  /* 00110000    xx     */
   0x60,  /* 01100000   xx      */
   0xFF,  /* 11111111  xxxxxxxx */
   0xFF,  /* 11111111  xxxxxxxx */

   0x3C,  /* 00111100    xxxx   */
   0x7E,  /* 01111110   xxxxxx  */
   0xC3,  /* 11000011  xx    xx */
   0x03,  /* 00000011        xx */
   0x0E,  /* 00001110      xxx  */
   0x0E,  /* 00001110      xxx  */
   0x03,  /* 00000011        xx */
   0xC3,  /* 11000011  xx    xx */
   0x7E,  /* 01111110   xxxxxx  */
   0x3C,  /* 00111100    xxxx   */

   0x06,  /* 00000110       xx  */
   0x0E,  /* 00001110      xxx  */
   0x1E,  /* 00011110     xxxx  */
   0x36,  /* 00110110    xx xx  */
   0x66,  /* 01100110   xx  xx  */
   0xC6,  /* 11000110  xx   xx  */
   0xFF,  /* 11111111  xxxxxxxx */
   0xFF,  /* 11111111  xxxxxxxx */
   0x06,  /* 00000110       xx  */
   0x06,  /* 00000110       xx  */

   0xFF,  /* 11111111  xxxxxxxx */
   0xFF,  /* 11111111  xxxxxxxx */
   0xC0,  /* 11000000  xx       */
   0xC0,  /* 11000000  xx       */
   0xFC,  /* 11111100  xxxxxx   */
   0xFE,  /* 11111110  xxxxxxx  */
   0x03,  /* 00000011        xx */
   0xC3,  /* 11000011  xx    xx */
   0x7E,  /* 01111110   xxxxxx  */
   0x3C,  /* 00111100    xxxx   */

   0x3E,  /* 00111110    xxxxx  */
   0x7C,  /* 01111100   xxxxx   */
   0xC0,  /* 11000000  xx       */
   0xC0,  /* 11000000  xx       */
   0xFC,  /* 11111100  xxxxxx   */
   0xFE,  /* 11111110  xxxxxxx  */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0x7E,  /* 01111110   xxxxxx  */
   0x3C,  /* 00111100    xxxx   */

   0xFF,  /* 11111111  xxxxxxxx */
   0xFF,  /* 11111111  xxxxxxxx */
   0x03,  /* 00000011        xx */
   0x06,  /* 00000110       xx  */
   0x0C,  /* 00001100      xx   */
   0x18,  /* 00011000     xx    */
   0x30,  /* 00110000    xx     */
   0x60,  /* 01100000   xx      */
   0x60,  /* 01100000   xx      */
   0x60,  /* 01100000   xx      */

   0x3C,  /* 00111100    xxxx   */
   0x7E,  /* 01111110   xxxxxx  */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0x7E,  /* 01111110   xxxxxx  */
   0x7E,  /* 01111110   xxxxxx  */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0x7E,  /* 01111110   xxxxxx  */
   0x3C,  /* 00111100    xxxx   */

   0x3C,  /* 00111100    xxxx   */
   0x7E,  /* 01111110   xxxxxx  */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0x7F,  /* 01111111   xxxxxxx */
   0x3F,  /* 00111111    xxxxxx */
   0x03,  /* 00000011        xx */
   0x03,  /* 00000011        xx */
   0x3E,  /* 00111110    xxxxx  */
   0x7C,  /* 01111100   xxxxx   */

   0x18,  /* 00011000     xx    */
   0x3C,  /* 00111100    xxxx   */
   0x66,  /* 01100110   xx  xx  */
   0xC3,  /* 11000011  xx    xx */
   0xFF,  /* 11111111  xxxxxxxx */
   0xFF,  /* 11111111  xxxxxxxx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */

   0xFC,  /* 11111100  xxxxxx   */
   0xFE,  /* 11111110  xxxxxxx  */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xFE,  /* 11111110  xxxxxxx  */
   0xFE,  /* 11111110  xxxxxxx  */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xFE,  /* 11111110  xxxxxxx  */
   0xFC,  /* 11111100  xxxxxx   */

   0x3C,  /* 00111100    xxxx   */
   0x7E,  /* 01111110   xxxxxx  */
   0xC3,  /* 11000011  xx    xx */
   0xC0,  /* 11000000  xx       */
   0xC0,  /* 11000000  xx       */
   0xC0,  /* 11000000  xx       */
   0xC0,  /* 11000000  xx       */
   0xC3,  /* 11000011  xx    xx */
   0x7E,  /* 01111110   xxxxxx  */
   0x3C,  /* 00111100    xxxx   */

   0xFC,  /* 11111100  xxxxxx   */
   0xFE,  /* 11111110  xxxxxxx  */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xC3,  /* 11000011  xx    xx */
   0xFE,  /* 11111110  xxxxxxx  */
   0xFC,  /* 11111100  xxxxxx   */

   0xFF,  /* 11111111  xxxxxxxx */
   0xFF,  /* 11111111  xxxxxxxx */
   0xC0,  /* 11000000  xx       */
   0xC0,  /* 11000000  xx       */
   0xFE,  /* 11111110  xxxxxxx  */
   0xFE,  /* 11111110  xxxxxxx  */
   0xC0,  /* 11000000  xx       */
   0xC0,  /* 11000000  xx       */
   0xFF,  /* 11111111  xxxxxxxx */
   0xFF,  /* 11111111  xxxxxxxx */

   0xFF,  /* 11111111  xxxxxxxx */
   0xFF,  /* 11111111  xxxxxxxx */
   0xC0,  /* 11000000  xx       */
   0xC0,  /* 11000000  xx       */
   0xFC,  /* 11111100  xxxxxx   */
   0xFC,  /* 11111100  xxxxxx   */
   0xC0,  /* 11000000  xx       */
   0xC0,  /* 11000000  xx       */
   0xC0,  /* 11000000  xx       */
   0xC0   /* 11000000  xx       */
   };


BYTE keypad[NUM_KEYS] =  /*  Note: These values are read from PORT 60H when a key is pressed */
   {
   HEX_0_KEY,  /* 0x53  num-.     */
   HEX_1_KEY,  /* 0x47  num-7     */
   HEX_2_KEY,  /* 0x48  num-8     */
   HEX_3_KEY,  /* 0x49  num-9     */
   HEX_4_KEY,  /* 0x4B  num-4     */
   HEX_5_KEY,  /* 0x4C  num-5     */
   HEX_6_KEY,  /* 0x4D  num-6     */
   HEX_7_KEY,  /* 0x4F  num-1     */
   HEX_8_KEY,  /* 0x50  num-2     */
   HEX_9_KEY,  /* 0x51  num-3     */
   HEX_A_KEY,  /* 0x52  num-0     */
   HEX_B_KEY,  /* 0x1C  num-Enter */
   HEX_C_KEY,  /* 0x35  num-/     */
   HEX_D_KEY,  /* 0x37  num-*     */
   HEX_E_KEY,  /* 0x4A  num--     */
   HEX_F_KEY   /* 0x4E  num-+     */
   };

BYTE alt_keypad[NUM_KEYS] =  /*  Note: These values are read from PORT 60H when a key is pressed */
   {
   ALT_HEX_0_KEY,  /* 0x2D  x */
   ALT_HEX_1_KEY,  /* 0x02  1 */
   ALT_HEX_2_KEY,  /* 0x03  2 */
   ALT_HEX_3_KEY,  /* 0x04  3 */
   ALT_HEX_4_KEY,  /* 0x10  q */
   ALT_HEX_5_KEY,  /* 0x11  w */
   ALT_HEX_6_KEY,  /* 0x12  e */
   ALT_HEX_7_KEY,  /* 0x1E  a */
   ALT_HEX_8_KEY,  /* 0x1F  s */
   ALT_HEX_9_KEY,  /* 0x20  d */
   ALT_HEX_A_KEY,  /* 0x2C  z */
   ALT_HEX_B_KEY,  /* 0x2E  c */
   ALT_HEX_C_KEY,  /* 0x05  4 */
   ALT_HEX_D_KEY,  /* 0x13  r */
   ALT_HEX_E_KEY,  /* 0x21  f */
   ALT_HEX_F_KEY   /* 0x2F  v */
   };

BYTE chip8_keys[NUM_KEYS];
BYTE keyboard_int_flag;
BYTE key_state[128];  /* 0 if UP, otherwise pressed */
BYTE anykey();

BYTE use_alt_keypad;

WORD errorcode = ERROR_NONE;  /* need this as a global */

WORD peekw();

BYTE *chip8_error[] =
   {
   "Stack Overflow",
   "Stack Underflow",
   "Unknown Opcode",
   "PC out of range"
   };

BYTE num_sticky_breakpoints;
BYTE sticky_breakpoints_disabled;
breakpoints_t sticky_breakpoints[MAX_BREAKPOINTS];
BYTE num_go_breakpoints;
breakpoints_t go_breakpoints[MAX_BREAKPOINTS];
BYTE use_proceede_breakpoint;
breakpoints_t proceede_breakpoint;

/* CHIP-8 opcodes
 *
 *   00Cn - SCD nibble     SUPERCHIP instruction, scroll screen <nibble> lines DOWN
 *                         i.e. screen[i+nibble][] = screen[i][]
 *   00E0 - CLS            clear screen
 *   00EE - RET            return from subroutine
 *   00FB - SCR            SUPERCHIP instruction, scroll screen 4 pixels right
 *   00FC - SCL            SUPERCHIP instruction, scroll screen 4 pixels left
 *   00FD - EXIT           SUPERCHIP instruction, exit interpreter
 *   00FE - LOW            set LOWRES (128 x 32) graphics
 *   00FF - HIGH           set HIGHRES (256 x 64) graphics
 *   0nnn - SYS addr       <not implemented>
 *   1nnn - JP addr        JMP nnn
 *   2nnn - CALL addr      CALL nnn
 *   3xkk - SE Vx, byte    Skip next instr if Vx == KK
 *   4xkk - SNE Vx, byte   Skip next instr if Vx != KK
 *   5xy0 - SE Vx, Vy      Skip next instr if Vx == Vy
 *   6xkk - LD Vx, byte    Vx = KK
 *   7xkk - ADD Vx, byte   Vx += KK
 *   8xy0 - LD Vx, Vy      Vx = Vy
 *   8xy1 - OR Vx, Vy      Vx |= Vy
 *   8xy2 - AND Vx, Vy     Vx &= Vy
 *   8xy3 - XOR Vx, Vy     Vx ^= Vy
 *   8xy4 - ADD Vx, Vy     Vx += Vy, VF set to 1 if result > 0xff, else 0
 *   8xy5 - SUB Vx, Vy     Vx -= Vy, VF set to 1 if Vx >= Vy, else 0
 *   8xy6 - SHR Vx, 1      Vx >>= 1, VF set to 1 if carry, else 0
 *   8xy7 - SUBN Vx, Vy    Vx = Vy - Vx, VF set to 1 if Vy >= Vx, else 0
 *   8xyE - SHL Vx, 1      Vx <<= 1, VF set to 1 if carry, else 0
 *   9xy0 - SNE Vx, Vy     skip next instr if Vx != Vy
 *   Annn - LD I, addr     set I to NNN
 *   Bnnn - JP V0, addr    JMP to NNN + V0
 *   Cxkk - RND Vx, byte   Vx = <random> & KK
 *   Dxyn - DRW Vx, Vy, nibble  draw sprite at I at col=VX, row=VY for <nibble>
 *                              bytes.  VF set to 1 if screen pixels changed, else 0
 *                              If nibble == 0, draw 16-bit (highres) SPRITE
 *   Ex9E - SKP Vx         skip next instr if key stored in Vx is pressed
 *   ExA1 - SKNP Vx        skip next instr if key stored in Vx is not pressed
 *   Fx07 - LD Vx, DT      Vx = Delay_Timer
 *   Fx0A - LD Vx, K       Wait for key, then Vx = key
 *   Fx15 - LD DT, Vx      Delay_Timer = Vx
 *   Fx18 - LD ST, Vx      Sound_timer = Vx
 *   Fx1E - ADD I, Vx      I += Vx
 *   Fx29 - LD F, Vx       I = &system_sprite[Vx]
 *   Fx30 - LD HF, Vx      I = &highres_system_sprite[Vx]
 *   Fx33 - LD B, Vx       I[0] = BCD_HIGH(Vx), I[1] = BCD_MID(Vx), I[2] = BCD_LOW(Vx)
 *   Fx55 - LD [I], Vx     Save V0 .. Vx into memory at I .. I+x
 *   Fx65 - LD Vx, [I]     Load V0 .. Vx from memory at I .. I+x
 *   Fx75 - LD R, Vx       Save V0 .. Vx (x<8) to HP48 flags
 *   Fx85 - LD Vx, R       Load V0 .. Vx (x<8) from HP48 flags
 */
void instr_jmp(),
     instr_call(),
     instr_skipeq(),
     instr_skipne(),
     instr_skipregeq(),
     instr_skipregne(),
     instr_movkk(),
     instr_addkk(),
     instr_mov(),
     instr_or(),
     instr_and(),
     instr_xor(),
     instr_addreg(),
     instr_subreg(),
     instr_revsubreg(),
     instr_shr(),
     instr_shl(),
     instr_movi(),
     instr_jmpv0(),
     instr_rand(),
     instr_draw();

void instr_system();   /* opcode 0... collective processing */
void instr_math();     /* opcode 8... collective processing */
void instr_keys();     /* opcode E... collective processing */
void instr_memory();   /* opcode F... collective processing */  /* mostly memory... */

void (*primary_opcode[16])() =
   {
   instr_system,    /* 0... */
   instr_jmp,       /* 1... */
   instr_call,      /* 2... */
   instr_skipeq,    /* 3... */
   instr_skipne,    /* 4... */
   instr_skipregeq, /* 5... */
   instr_movkk,     /* 6... */
   instr_addkk,     /* 7... */
   instr_math,      /* 8... */
   instr_skipregne, /* 9... */
   instr_movi,      /* A... */
   instr_jmpv0,     /* B... */
   instr_rand,      /* C... */
   instr_draw,      /* D... */
   instr_keys,      /* E... */
   instr_memory     /* F... */  /* mostly memory... */
   };

void instr_bad_opcode();   /* unimmplemented math opcodes */
void (*math_opcode[16])() =
   {
   instr_mov,          /* ...0 */
   instr_or,           /* ...1 */
   instr_and,          /* ...2 */
   instr_xor,          /* ...3 */
   instr_addreg,       /* ...4 */
   instr_subreg,       /* ...5 */
   instr_shr,          /* ...6 */
   instr_revsubreg,    /* ...7 */
   instr_bad_opcode,   /* ...8 */
   instr_bad_opcode,   /* ...9 */
   instr_bad_opcode,   /* ...A */
   instr_bad_opcode,   /* ...B */
   instr_bad_opcode,   /* ...C */
   instr_bad_opcode,   /* ...D */
   instr_shl,          /* ...E */
   instr_bad_opcode    /* ...F */
   };

BYTE dbg_input_index;
BYTE dbg_input[STRING_SIZE+1];
BYTE dbg_token_len;
BYTE dbg_token[MAX_TOKEN_LEN+1];
   /* LEN+1 so we have room for EOS */
void dbg_proceede();
void dbg_go();
void dbg_trace();
void dbg_breakpoint();
void dbg_disassemble();
void dbg_assemble();
void dbg_edit_sprite();
void dbg_create_file();
void dbg_load_file();
void dbg_enter_bytes();
void dbg_fill_memory();
void dbg_reg();
void dbg_dump_memory();
void dbg_show_where();
void dbg_save_state();
void dbg_load_state();
void dbg_help();
void dbg_cmdline_k();
void dbg_cmdline_f();
void dbg_cmdline_v();
void dbg_cmdline_t();
void dbg_cmdline_i();
void dbg_cmdline_s();
void dbg_cmdline_l();
void dbg_cmdline_w();
void dbg_cmdline_m();
void dbg_cmdline_x();
void dbg_cmdline_y();
void dbg_cmdline_z();
void dbg_quit();
dbgcmds_t dbg_commands[DBG_CMD_LIST_SIZE] =
   {
      {  "P",    PROCEEDE,     dbg_proceede },
      {  "G",    GO,           dbg_go },
      {  "T",    TRACE,        dbg_trace },
      {  "B",    BREAKPOINT,   dbg_breakpoint },
      {  "U",    DISASSEMBLE,  dbg_disassemble },
      {  "A",    ASSEMBLE,     dbg_assemble },
      {  "S",    EDIT_SPRITE,  dbg_edit_sprite },
      {  "C",    CREATE_FILE,  dbg_create_file },
      {  "L",    LOAD_FILE,    dbg_load_file },
      {  "E",    ENTER_BYTES,  dbg_enter_bytes },
      {  "F",    FILL_MEMORY,  dbg_fill_memory },
      {  "R",    REG,          dbg_reg },
      {  "D",    DUMP_MEMORY,  dbg_dump_memory },
      {  "W",    SHOW_WHERE,   dbg_show_where },
      {  "SAVE", SAVE_STATE,   dbg_save_state },
      {  "LOAD", LOAD_STATE,   dbg_load_state },
      {  "H",    HELP,         dbg_help },
      { "-K",    CMDLINE_K,    dbg_cmdline_k },
      { "-F",    CMDLINE_F,    dbg_cmdline_f },
      { "-V",    CMDLINE_V,    dbg_cmdline_v },
      { "-T",    CMDLINE_T,    dbg_cmdline_t },
      { "-I",    CMDLINE_I,    dbg_cmdline_i },
      { "-S",    CMDLINE_S,    dbg_cmdline_s },
      { "-L",    CMDLINE_L,    dbg_cmdline_l },
      { "-W",    CMDLINE_W,    dbg_cmdline_w },
      { "-M",    CMDLINE_M,    dbg_cmdline_m },
      { "-X",    CMDLINE_X,    dbg_cmdline_x },
      { "-Y",    CMDLINE_Y,    dbg_cmdline_y },
      { "-Z",    CMDLINE_Z,    dbg_cmdline_z },
      {  "Q",    QUIT,         dbg_quit }
   };
WORD dbg_instruction_limit[4];
WORD dbg_cursor_r, dbg_cursor_c;

#define STATE_FILE_SIGNATURE_LEN  10
BYTE state_file_signature[] = "DEBUG8 001";
                            /* 123456789A */
      /* change this whenever you change the contents of the state file */
      /* use HEX numbers for more revisions */
      /* signature must be unique in these 10 characters for ALL later
       * versions of DEBUG8.
       */

BYTE screen_mirror[VESA_GRAPHICS_TEXT_ROWS][VESA_GRAPHICS_TEXT_COLS];
   /* BIOS scroll doesn't work in VESA graphics, do things the "hard way" */


WORD dbg_asm_pc;
WORD dbg_disassemble_pc;
WORD dbg_dump_addr;

BYTE breakpoint_instr_name[NAMED_BREAKPOINT_LEN];
WORD breakpoint_instr_name_len;
BYTE breakpoint_this_instr_name[NAMED_BREAKPOINT_LEN];

WORD where_queue[WHERE_QUEUE_SIZE];
WORD where_queue_index;
WORD where_queue_entries;



DWORD start_time, end_time;

main(argc,argv)
   int argc;
   char *argv[];
   {
   int i, j;

   for (i = 1; i < argc; i++)
      {
      if (argv[i][0] == '-')
         {
         switch(toupper(argv[i][1]))
            {
            case 'K':
               use_alt_keypad = TRUE;
               break;
            case 'F':
               execution_freerun = TRUE;
               break;
            case 'V':
               set_high_video();
               startup_video_super_chip8 = TRUE;
               break;
            case 'T':
               if (!isdigit(argv[i][2]))
                  help();
               sscanf(&argv[i][2],"%d",&j);
               if (j)
                  execution_ticks_per_instruction = j;
               break;
            case 'I':
               if (!isdigit(argv[i][2]))
                  help();
               sscanf(&argv[i][2],"%d",&j);
               if (j)
                  instructions_per_execution_tick = j;
               break;
            case 'S':
               if (!isdigit(argv[i][2]))
                  help();
               sscanf(&argv[i][2],"%d",&j);
               if ((j >= 100) && (j <= 1000))
                  sound_frequency = j;
               break;
            case 'L':
               if (!isdigit(argv[i][2]))
                  help();
               sscanf(&argv[i][2],"%d",&j);
               if ((j == PROGRAM_START) || (j == ETI660_PROGRAM_START))
                  program_load_addr = j;
               break;
            case 'W':
               wrap_sprites = TRUE;
               break;
            case 'M':
               use_vy_shift = TRUE;
               i_reg_is_incremented = TRUE;
               break;
            case 'X':
               use_vy_shift = TRUE;
               break;
            case 'Y':
               i_reg_is_incremented = TRUE;
               break;
            case 'Z':
               i_reg_sets_carry = TRUE;
               break;
            default:
               help();  /* this does not return */
               break;
            }
         for (j = i; j < argc-1; j++)
            strcpy(argv[j],argv[j+1]);
         argc--;
         i--;  /* remove this cmdline parameter */
         }
      }
   if (argc == 2)
      {
      if ((i = open(argv[1], 0)) == -1)
         help();
      j = read(i,&memory[program_load_addr],MEM_SIZE - program_load_addr);
      close(i);
      if (j < MIN_PROGRAM_SIZE)
         {
         puts("Not a valid CHIP8 program.\n");
         exit(1);
         }
      }
   j = peekw(SYSTEM_CLOCK_TICK_SEG, SYSTEM_CLOCK_TICK_OFS);
   i = 0;
#ifdef NEVERDEF
   while (i < 18)
      {
      if (peekw(SYSTEM_CLOCK_TICK_SEG, SYSTEM_CLOCK_TICK_OFS) != j)
         {
         j = peekw(SYSTEM_CLOCK_TICK_SEG, SYSTEM_CLOCK_TICK_OFS);
         i++;
         }
      if (csts())   /* if key waiting */
         if (ci() == ESC_KEY)  /* get key */
            exit(0);
      }
#endif
   setup_chip8();
   menu();
   reset_system();

   exit(errorcode);
   }

BYTE breakpoint_check(addr, len)
   WORD addr, len;
        /* TRUE if OK to continue (not a breakpoint), else FALSE */
        /* set errorcode to ERROR_BREAK on a breakpoint */
   {
   WORD i, b;

   if (use_proceede_breakpoint)
      {
      for (i = 0; i < len; i++)
         {
         if (((addr + i) >= proceede_breakpoint.start)
                   && ((addr + i) <= proceede_breakpoint.end))
            {
                 /* don't worry about hit count for this breakpoint */
            errorcode = ERROR_BREAK;
            return(FALSE);
            }
         }
      }
   for (b = 0; b < num_go_breakpoints; b++)
      {
      for (i = 0; i < len; i++)
         {
         if (((addr + i) >= go_breakpoints[b].start)
                  && ((addr + i) <= go_breakpoints[b].end))
            break;  /* exit loop so only 1 "hit" is counted for the entire range */
         }
      if (i < len)  /* if we hit a breakpoint */
         {
         if (++go_breakpoints[b].hits >= go_breakpoints[b].count)
            {
            errorcode = ERROR_BREAK;
            return(FALSE);
            }
         }
      }
   if (sticky_breakpoints_disabled)
      return(TRUE);  /* breakpoints remembered but "OFF", do not check! */
   for (b = 0; b < num_sticky_breakpoints; b++)
      {
      for (i = 0; i < len; i++)
         {
         if (((addr + i) >= sticky_breakpoints[b].start)
                  && ((addr + i) <= sticky_breakpoints[b].end))
            break;  /* exit loop so only 1 "hit" is counted for the entire range */
         }
      if (i < len)  /* if we hit a breakpoint */
         {
         if (++sticky_breakpoints[b].hits >= sticky_breakpoints[b].count)
            {
            errorcode = ERROR_BREAK;
            return(FALSE);
            }
         }
      }
   return(TRUE);
   }

void set_low_video()  /* shared with menu() */
   {
   super_chip8_video = FALSE;
   screen_cols = LOWRES_GRAPHICS_COLS;
   screen_rows = LOWRES_GRAPHICS_ROWS;
   max_screen_cols = LOWRES_GRAPHICS_COLS - 1;
   max_screen_rows = LOWRES_GRAPHICS_ROWS - 1;
   memfill(video_memory, sizeof(video_memory), BLACK);
   }

void set_high_video()  /* shared with menu() and main() */
   {
   super_chip8_video = TRUE;
   screen_cols = GRAPHICS_COLS;
   screen_rows = GRAPHICS_ROWS;
   max_screen_cols = GRAPHICS_COLS - 1;
   max_screen_rows = GRAPHICS_ROWS - 1;
   memfill(video_memory, sizeof(video_memory), BLACK);
   }

void instr_system()  /* 0... */
   {
/*   00Cn - SCD nibble     SUPERCHIP instruction, scroll screen <nibble> lines DOWN
 *                         i.e. screen[i+nibble][] = screen[i][]
 *   00E0 - CLS            clear screen
 *   00EE - RET            return from subroutine
 *   00FB - SCR            SUPERCHIP instruction, scroll screen 4 pixels right
 *   00FC - SCL            SUPERCHIP instruction, scroll screen 4 pixels left
 *   00FD - EXIT           SUPERCHIP instruction, exit interpreter
 *   00FE - LOW            SUPERCHIP instrction, set low graphics
 *   00FF - HIGH           SUPERCHIP instrctino, set high graphics
 */
   if ((opcode & 0xFF0) == 0x0C0)  /* SCD */
      {
      if (opcode_n = opcode_low & 0x0F)
         {
         scroll_screen_down();
         update_screen();
         }
      }
   else
      {
      switch (opcode & 0xFFF)
         {
         case 0xE0:   /* CLS */
            memfill(video_memory, sizeof(video_memory), BLACK);
            update_screen();
            break;
         case 0xEE:  /* RET */
            if (sp == STACK_START)
               errorcode = ERROR_STACK_UNDERFLOW;
            else
               {
                 /* here, we have
                 *   memory[sp-2] = return MSB
                 *   memory[sp-1] = return LSB
                 */
               if (breakpoint_check(sp+1,2))
                  {
                  *((BYTE *) &pc) = memory[++sp];
                  *(((BYTE *) &pc)+1) = memory[++sp];
                  }
               }
            break;
         case 0xFB:  /* SCR */
            scroll_screen_right();
            update_screen();
            break;
         case 0xFC:  /* SCL */
            scroll_screen_left();
            update_screen();
            break;
         case 0xFD:  /* EXIT */
            errorcode = ERROR_EXIT;
            break;
         case 0xFE:  /* LOW */
            set_low_video();  /* shared with menu() */
            break;
         case 0xFF:  /* HIGH */
            set_high_video();  /* shared with menu() and main() */
            break;
         default:
            errorcode = ERROR_UNKNOWN_OPCODE;
            break;
         }
      }
   }

void instr_jmp()  /* 1... */
   {
   pc = opcode & ADDRESS_MASK;
   }

void instr_call()  /* 2... */
   {
   if (sp == (STACK_START - STACK_SIZE * 2))
      {
      errorcode = ERROR_STACK_OVERFLOW;
      pc -= 2;  /* back to start of instruction */
      }
   else
      {
      if (breakpoint_check(sp-1,2))
         {
         memory[sp--] = *(((BYTE *)&pc) + 1);
         memory[sp--] = *((BYTE *)&pc);
         pc = opcode & ADDRESS_MASK;
         }
      }
   }

void instr_skipeq()  /* 3xkk - SE Vx, byte    Skip next instr if Vx == KK */
   {
   if (regs[X_REG_OPCODE_FIELD] == opcode_low)
      {
      pc += 2;  /* skip next instruction */
#ifdef CHECK_PC_LIMITS
      pc &= ADDRESS_MASK;  /* not sure if this is needed / wanted */
#endif
      }
   }

void instr_skipne()  /* 4xkk - SNE Vx, byte   Skip next instr if Vx != KK */
   {
   if (regs[X_REG_OPCODE_FIELD] != opcode_low)
      {
      pc += 2;  /* skip next instruction */
#ifdef CHECK_PC_LIMITS
      pc &= ADDRESS_MASK;  /* not sure if this is needed / wanted */
#endif
      }
   }

void instr_skipregeq()  /* 5xy0 - SE Vx, Vy      Skip next instr if Vx == Vy */
   {
   if (opcode_low & 0x0f)
      errorcode = ERROR_UNKNOWN_OPCODE;
   if (regs[X_REG_OPCODE_FIELD] == regs[Y_REG_OPCODE_FIELD])
      {
      pc += 2;  /* skip next instruction */
#ifdef CHECK_PC_LIMITS
      pc &= ADDRESS_MASK;  /* not sure if this is needed / wanted */
#endif
      }
   }

void instr_movkk()  /* 6xkk - LD Vx, byte    Vx = KK */
   {
   regs[X_REG_OPCODE_FIELD] = opcode_low;
   }

void instr_addkk()  /* 7xkk - ADD Vx, byte   Vx += KK */
   {
   regs[X_REG_OPCODE_FIELD] += opcode_low;
   }

void instr_mov()  /* 8xy0 - LD Vx, Vy      Vx = Vy */
   {
   regs[X_REG_OPCODE_FIELD] = regs[Y_REG_OPCODE_FIELD];
   }

void instr_or()  /* 8xy1 - OR Vx, Vy      Vx |= Vy */
   {
   regs[X_REG_OPCODE_FIELD] |= regs[Y_REG_OPCODE_FIELD];
   }

void instr_and()  /* 8xy2 - AND Vx, Vy     Vx &= Vy */
   {
   regs[X_REG_OPCODE_FIELD] &= regs[Y_REG_OPCODE_FIELD];
   }

void instr_xor()  /* 8xy3 - XOR Vx, Vy     Vx ^= Vy */
   {
   regs[X_REG_OPCODE_FIELD] ^= regs[Y_REG_OPCODE_FIELD];
   }

void instr_addreg()  /* 8xy4 - ADD Vx, Vy     Vx += Vy, VF set to 1 if result > 0xff, else 0 */
   {
   WORD temp;

   temp = (WORD)regs[opcode_x = X_REG_OPCODE_FIELD] + (WORD)regs[Y_REG_OPCODE_FIELD];
      /* DeSmet -- unless regs[] are explicitly typecast, this will be done
       *           as a BYTE addition then promoted to WORD, and we lose our
       *           carry information.
       */
   regs[opcode_x] = (BYTE)temp;
   regs[VF_REG] = (BYTE)(temp >> 8);
   }

void instr_subreg()  /* 8xy5 - SUB Vx, Vy     Vx -= Vy, VF set to 1 if Vx >= Vy, else 0 */
   {
   WORD temp;

   temp = (WORD)regs[opcode_x = X_REG_OPCODE_FIELD] - (WORD)regs[Y_REG_OPCODE_FIELD];
      /* DeSmet -- unless regs[] are explicitly typecast, this will be done
       *           as a BYTE subtraction then promoted to WORD, and we lose our
       *           carry information.
       */
   regs[opcode_x] = (BYTE)temp;
   regs[VF_REG] = (BYTE)(temp >> 8) + 1;
   }

void instr_shr()  /* 8xy6 - SHR Vx, 1      Vx >>= 1, VF set to 1 if carry, else 0 */
   {
   if (use_vy_shift)
      {
      regs[VF_REG] = regs[opcode_y = Y_REG_OPCODE_FIELD] & 1;
      regs[X_REG_OPCODE_FIELD] = regs[opcode_y] >> 1;
      }
   else
      {
      regs[VF_REG] = regs[opcode_x = X_REG_OPCODE_FIELD] & 1;
      regs[opcode_x] >>= 1;
      }
   }

void instr_revsubreg()  /* 8xy7 - SUBN Vx, Vy    Vx = Vy - Vx, VF set to 1 if Vy >= Vx, else 0 */
   {
   WORD temp;

   temp = (WORD)regs[Y_REG_OPCODE_FIELD] - (WORD)regs[opcode_x = X_REG_OPCODE_FIELD];
      /* DeSmet -- unless regs[] are explicitly typecast, this will be done
       *           as a BYTE subtraction then promoted to WORD, and we lose our
       *           carry information.
       */
   regs[opcode_x] = (BYTE)temp;
   regs[VF_REG] = (BYTE)(temp >> 8) + 1;
   }

void instr_shl()  /* 8xyE - SHL Vx, 1      Vx <<= 1, VF set to 1 if carry, else 0 */
   {
   if (use_vy_shift)
      {
      regs[VF_REG] = regs[opcode_y = Y_REG_OPCODE_FIELD] >> 7;
      regs[X_REG_OPCODE_FIELD] = regs[opcode_y] << 1;
      }
   else
      {
      regs[VF_REG] = regs[opcode_x = X_REG_OPCODE_FIELD] >> 7;
      regs[opcode_x] <<= 1;
      }
   }

void instr_bad_opcode()  /* 8..z */
   {
   errorcode = ERROR_UNKNOWN_OPCODE;
   }

void instr_math()  /* 8... */
   {
   (*math_opcode[opcode_low & 0x0f])();
   }

void instr_skipregne()  /* 9xy0 - SNE Vx, Vy     skip next instr if Vx != Vy */
   {
   if (opcode_low & 0x0f)
      errorcode = ERROR_UNKNOWN_OPCODE;
   if (regs[X_REG_OPCODE_FIELD] != regs[Y_REG_OPCODE_FIELD])
      {
      pc += 2;  /* skip next instruction */
#ifdef CHECK_PC_LIMITS
      pc &= ADDRESS_MASK;  /* not sure if this is needed / wanted */
#endif
      }
   }

void instr_movi()  /* Annn - LD I, addr     set I to NNN */
   {
   I_reg = opcode & ADDRESS_MASK;
   }

void instr_jmpv0()  /* Bnnn - JP V0, addr    JMP to NNN + V0 */
   {
   pc = (opcode & ADDRESS_MASK) + (WORD)regs[0];
   }

void instr_rand()  /* Cxkk - RND Vx, byte   Vx = <random> & KK */
   {
   regs[X_REG_OPCODE_FIELD] = rand() & opcode_low;
   }

void instr_draw()  /* Dxyn - DRW Vx, Vy, nibble  draw sprite at I at
                    * col=VX, row=VY for <nibble> bytes.
                    * VF set to 1 if screen pixels changed, else 0
                    */
   {
   BYTE spritedata;
   WORD sprite_16;
   WORD i, row, col, c;
   BYTE *p;

   if (!(opcode_n = opcode_low & 0x0F))
      opcode_n = 16;  /* 8 x 16 or 16 x 16 */
   regs[VF_REG] = WHITE;
   col = regs[X_REG_OPCODE_FIELD] & max_screen_cols;
   row = regs[opcode_low >> 4] & max_screen_rows;
   if (opcode_n + row > screen_rows)
      opcode_n = screen_rows - row;
   if (!breakpoint_check(I_reg,opcode_n))
      return;
   p = &video_memory[row * screen_cols];
   i = I_reg;
   if (!(opcode_low & 0x0F))  /* 16 x 16 sprite */
      {
      while (opcode_n)
         {
         *((BYTE *)&sprite_16 + 1) = memory[i++];
            /* 1st CHIP-8 memory is big-endian, to high byte of x86 var */
         *(BYTE *)&sprite_16 = memory[i++];
         if (wrap_sprites)
            {
            for (c = col; sprite_16; sprite_16 <<= 1, c = (c + 1) & max_screen_cols)
               {
               if (sprite_16 & 0x8000)
                  regs[VF_REG] &= (p[c] ^= WHITE);
               }
            }
         else
            {
            for (c = col; (sprite_16) && (c < screen_cols); sprite_16 <<= 1, c++)
               {
               if (sprite_16 & 0x8000)
                  regs[VF_REG] &= (p[c] ^= WHITE);
               }
            }
         opcode_n--;
         p += screen_cols;
         }
      }
   else
      {
      while (opcode_n)
         {
         if (wrap_sprites)
            {
            for (c = col, spritedata = memory[i++]; spritedata; spritedata <<= 1, c = (c + 1) & max_screen_cols)
               {
               if (spritedata & 0x80)
                  regs[VF_REG] &= (p[c] ^= WHITE);
               }
            }
         else
            {
            for (c = col, spritedata = memory[i++]; (spritedata) && (c < screen_cols); spritedata <<= 1, c++)
               {
               if (spritedata & 0x80)
                  regs[VF_REG] &= (p[c] ^= WHITE);
               }
            }
         opcode_n--;
         p += screen_cols;
         }
      }
   regs[VF_REG] = (regs[VF_REG] == WHITE) ? FALSE : TRUE;
   update_screen();
   }

void instr_keys()  /* Ex9E - SKP Vx         skip next instr if key stored in Vx is pressed
                    * ExA1 - SKNP Vx        skip next instr if key stored in Vx is not pressed
                    */
   {
   WORD temp;

   if (opcode_low == 0x9E)  /* skip if key Vx pressed */
      {
      if (((temp = regs[X_REG_OPCODE_FIELD]) < 0x10)  /* valid key number */
               && (key_state[chip8_keys[temp]]))
         {
         pc += 2;  /* skip next instr */
#ifdef CHECK_PC_LIMITS
         pc &= ADDRESS_MASK;  /* not sure if this is needed / wanted */
#endif
         }
      }
   else if (opcode_low == 0xA1)  /* skip if key Vx not pressed */
      {
      if (((temp = regs[X_REG_OPCODE_FIELD]) < 0x10)  /* valid key number */
               && (!key_state[chip8_keys[temp]]))
         {
         pc += 2;  /* skip next instr */
#ifdef CHECK_PC_LIMITS
         pc &= ADDRESS_MASK;  /* not sure if this is needed / wanted */
#endif
         }
      }
   else
      errorcode = ERROR_UNKNOWN_OPCODE;
   }

void instr_memory()  /* mostly memory... */
/*   Fx07 - LD Vx, DT      Vx = Delay_Timer
 *   Fx0A - LD Vx, K       Wait for key, then Vx = key
 *   Fx15 - LD DT, Vx      Delay_Timer = Vx
 *   Fx18 - LD ST, Vx      Sound_timer = Vx
 *   Fx1E - ADD I, Vx      I += Vx
 *   Fx29 - LD F, Vx       I = &system_sprite[Vx]
 *   Fx30 - LD HF, Vx      I = &highres_system_sprite[Vx]
 *   Fx33 - LD B, Vx       I[0] = BCD_HIGH(Vx), I[1] = BCD_MID(Vx), I[2] = BCD_LOW(Vx)
 *   Fx55 - LD [I], Vx     Save V0 .. Vx into memory at I .. I+x
 *   Fx65 - LD Vx, [I]     Load V0 .. Vx from memory at I .. I+x
 *   Fx75 - LD R, Vx       Save V0 .. Vx (x<8) to HP48 flags
 *   Fx85 - LD Vx, R       Load V0 .. Vx (x<8) from HP48 flags
 */
   {
   int temp;

   switch (opcode_low)
      {
      case 0x07:  /* Fx07 - LD Vx, DT      Vx = Delay_Timer */
         regs[X_REG_OPCODE_FIELD] = delay_timer;
         break;
      case 0x0A:  /* Fx0A - LD Vx, K       Wait for key, then Vx = key */
         if ((temp = anykey()) > 0x0F)
            pc -= 2;
               /* our "wait for key" is to keep executing this instruction.
                * We need to return to the main loop to do things like update
                * timers.
                */
         else
            regs[X_REG_OPCODE_FIELD] = temp;
         break;
      case 0x15:  /* Fx15 - LD DT, Vx      Delay_Timer = Vx */
         delay_timer = regs[X_REG_OPCODE_FIELD];
         break;
      case 0x18:  /* Fx18 - LD ST, Vx      Sound_timer = Vx */
         if (sound_timer = regs[X_REG_OPCODE_FIELD])
            sound_on();
         else
            sound_off();
         break;
      case 0x1e:  /* Fx1E - ADD I, Vx      I += Vx */
         I_reg += regs[X_REG_OPCODE_FIELD];
         if (i_reg_sets_carry)
            {
            if (I_reg >= MEM_SIZE)
               regs[VF_REG] = 1;
            else
               regs[VF_REG] = 0;
            }
         I_reg &= ADDRESS_MASK;
         break;
      case 0x29:  /* Fx29 - LD F, Vx       I = &system_sprite[Vx] */
         I_reg = HEX_SPRITES_LOCATION + (regs[X_REG_OPCODE_FIELD] & 0x0F) * HEX_SPRITES_ROWS;
         break;
      case 0x30:  /* Fx30 - LD HF, Vx      I = &highres_system_sprite[Vx] */
         I_reg = HEX_SPRITES_SUPER_LOCATION + (regs[X_REG_OPCODE_FIELD] & 0x0F) * HEX_SPRITES_SUPER_ROWS;
         break;
      case 0x33:  /* Fx33 - LD B, Vx       I[0] = BCD_HIGH(Vx), I[1] = BCD_MID(Vx), I[2] = BCD_LOW(Vx) */
         if (breakpoint_check(I_reg,3))
            {
            memory[I_reg] = (temp = regs[X_REG_OPCODE_FIELD]) / 100;
            memory[I_reg + 1] = (temp / 10) % 10;
            memory[I_reg + 2] = temp % 10;
            }
         break;
      case 0x55:  /* Fx55 - LD [I], Vx     Save V0 .. Vx into memory at I .. I+x */
         opcode_x = X_REG_OPCODE_FIELD;
         if (breakpoint_check(I_reg,opcode_x+1))  /* +1, save at least 1 reg */
            {
            for (temp = 0; temp <= opcode_x; temp++)
               memory[I_reg + temp] = regs[temp];
            if (i_reg_is_incremented)
               I_reg = (I_reg + opcode_x) & ADDRESS_MASK;
            }
         break;
      case 0x65:  /* Fx65 - LD Vx, [I]     Load V0 .. Vx from memory at I .. I+x */
         opcode_x = X_REG_OPCODE_FIELD;
         if (breakpoint_check(I_reg,opcode_x+1))  /* +1, restore at least 1 reg */
            {
            for (temp = 0; temp <= opcode_x; temp++)
               regs[temp] = memory[I_reg + temp];
            if (i_reg_is_incremented)
               I_reg = (I_reg + opcode_x) & ADDRESS_MASK;
            }
         break;
      case 0x75:  /* Fx75 - LD R, Vx       Save V0 .. Vx (x<8) to HP48 flags */
         if ((opcode_x = X_REG_OPCODE_FIELD) > 7)
            errorcode = ERROR_UNKNOWN_OPCODE;
         else
            {
            for (temp = 0; temp <= opcode_x; temp++)
               hp48regs[temp] = regs[temp];
            }
         break;
      case 0x85:  /* Fx85 - LD Vx, R       Load V0 .. Vx (x<8) from HP48 flags */
         if ((opcode_x = X_REG_OPCODE_FIELD) > 7)
            errorcode = ERROR_UNKNOWN_OPCODE;
         else
            {
            for (temp = 0; temp <= opcode_x; temp++)
               regs[temp] = hp48regs[temp];
            }
         break;
      default:
         errorcode = ERROR_UNKNOWN_OPCODE;
         break;
      }
   }

void dbg_skip_input_whitespace()
   {
   while ((dbg_input_index < STRING_SIZE)
              && ((dbg_input[dbg_input_index] == SPACE)
                       || (dbg_input[dbg_input_index] == TAB)
                       || (dbg_input[dbg_input_index] == CR)
                       || (dbg_input[dbg_input_index] == LF)
                 )
         )
      dbg_input_index++;
   }

BYTE dbg_is_end_of_token(ch)
   BYTE ch;
   {
   if ((ch == EOS)
        || (ch == CR)
        || (ch == LF)
        || (ch == SPACE)
        || (ch == TAB)
        || (ch == COMMA)
        || (ch == '=')
      )
      return(TRUE);
   return(FALSE);
   }

WORD dbg_parse()
   {
   WORD inquote;  /* allow double/single quoted string to be parsed as
                   * one token
                   */
   WORD whichquote;  /* double or single quote */

   inquote = FALSE;
   dbg_token_len = 0;
   dbg_token[0] = EOS;
   dbg_skip_input_whitespace();
   for (dbg_token_len = 0;
             ((dbg_token_len < MAX_TOKEN_LEN)
                 && ((!dbg_is_end_of_token(dbg_input[dbg_input_index]))
                        || ((dbg_token_len == 0) && (dbg_input[dbg_input_index] == '='))
                           /* it is OK for token to start with '=', even
                            * though for other purposes it is end of token
                            */
                    )
                 && (dbg_input_index < STRING_SIZE)
             ) || (inquote)
                        ; dbg_token_len++, dbg_input_index++)
      {
      if ((dbg_input[dbg_input_index] == '\"')
               || (dbg_input[dbg_input_index] == '\''))
         {
         if (inquote)
            {
            if (dbg_input[dbg_input_index] == whichquote)
               inquote = FALSE;
            }
         else
            {
            inquote = TRUE;
            whichquote = dbg_input[dbg_input_index];
            }
         }
      dbg_token[dbg_token_len] = dbg_input[dbg_input_index];
      }
   dbg_token[dbg_token_len] = EOS;
   if ((!dbg_input[dbg_input_index]) && (dbg_token_len == 0))
       /* if only spaces, etc, were left */
      return(FALSE);
   return(TRUE);
   }

WORD ishexdigit(ch)
   BYTE ch;
   {
   if ((ch >= '0') && (ch <= '9'))
      return(TRUE);
   if ((toupper(ch) >= 'A') && (toupper(ch) <= 'F'))
      return(TRUE);
   return(FALSE);
   }

WORD hexdigit(ch)  /* we know passed ch is a valid hex digit */
   BYTE ch;
   {
   if (ch > '9')
      return(toupper(ch) - 'A' + 10);
   return(ch - '0');
   }

BYTE tohex(val)
   WORD val;
   {
   if (val > 9)
      return('A'-10+val);
   return('0'+val);
   }

WORD is_hexword(str,val)
   BYTE *str;
   WORD *val;
   {
   WORD i = 0;

   *val = 0;
   while (str[i])
      {
      if (i > 3)
         return(FALSE);
      if (!ishexdigit(str[i]))
         return(FALSE);
      *val = (*val << 4) | hexdigit(str[i]);
      i++;
      }
   if (i == 0)  /* a blank string was passed */
      return(FALSE);
   return(TRUE);
   }

WORD is_hexbyte(str,val)
   BYTE *str;
   WORD *val;
   {
   WORD i = 0;

   *val = 0;
   while (str[i])
      {
      if (i > 1)
         return(FALSE);
      if (!ishexdigit(str[i]))
         return(FALSE);
      *val = (*val << 4) | hexdigit(str[i]);
      i++;
      }
   if (i == 0)  /* a blank string was passed */
      return(FALSE);
   return(TRUE);
   }

WORD is_address(str,addr)
   BYTE *str;
   WORD *addr;
   {
   if (!is_hexword(str,addr))
      return(FALSE);
   if ((*addr < program_load_addr)
         || (*addr >= MEM_SIZE)
      )
      return(FALSE);
   return(TRUE);
   }

WORD is_memory(str,addr)
   BYTE *str;
   WORD *addr;
   {
   if (!is_hexword(str,addr))
      return(FALSE);
   if (*addr >= MEM_SIZE)
      return(FALSE);
   return(TRUE);
   }

WORD is_pc(str,addr)
   BYTE *str;
   WORD *addr;
   {
   if (!is_address(str,addr))
      return(FALSE);
   if (*addr & 1)   /* all instructions are 2 bytes, addresses must be even */
      return(FALSE);
   return(TRUE);
   }

debug_window_puts(str)
   BYTE *str;
      /* use dbg_cursor_r,dbg_cursor_c as indices into screen_mirror[][] */
   {
   WORD i = 0;

   while (str[i])
      screen_mirror[dbg_cursor_r][dbg_cursor_c++] = str[i++];
   puts(str);
   }

BYTE *mygets(str)
   BYTE *str;
   {
   BYTE buf[STRING_SIZE];
   BYTE prompt[2];
   BYTE blank[2];
   WORD i = 0;
   WORD ch;
   WORD start_r, start_c;  /* starting screen location for ESC */

   start_r = dbg_cursor_r;
   start_c = dbg_cursor_c;
   prompt[0] = '_';
   prompt[1] = EOS;
   blank[0] = SPACE;
   blank[1] = EOS;
   do
      {
      curpos(dbg_cursor_r,dbg_cursor_c);
      puts(prompt);
      curpos(dbg_cursor_r,dbg_cursor_c);
      ch = getkey();
      curpos(dbg_cursor_r,dbg_cursor_c);
      puts(blank);
      curpos(dbg_cursor_r,dbg_cursor_c);
      if (ch == TAB)
         ch = SPACE;  /* make it easy on ourselves... */
      if (ch == ESC_KEY)  /* erase line */
         {
         memfill(buf,i,' ');  /* spaces */
         buf[i] = EOS;
         curpos(dbg_cursor_r = start_r,dbg_cursor_c = start_c);
         debug_window_puts(buf);  /* erase current typed line */
         curpos(dbg_cursor_r = start_r,dbg_cursor_c = start_c);
         i = 0;
         }
      if (ch == BS)
         {
         if (i)
            {
            curpos(dbg_cursor_r,--dbg_cursor_c);
            debug_window_puts(blank);
            curpos(dbg_cursor_r,--dbg_cursor_c);
            i--;
            }
         }
      else if ((ch != CR)
                 && (ch < 0x100)  /* not CR, and not FN key (etc) */
                 && (ch >= ' ')  /* and is regular ASCII */
              )
         {
         if (i < VESA_GRAPHICS_TEXT_COLS-2)
            {
            *(str+i) = ch;
            *(str+i+1) = EOS;
            debug_window_puts(str+i);
            i++;
            }
         }
      } while (ch != CR);
   *(str+i) = EOS;
   }

redraw_debug_window()
   {
   BYTE str[STRING_SIZE];  /* a little extra space never hurt... */
   WORD r, c;

   for (r = DEBUG_WINDOW_START_TEXT_ROW; r < VESA_GRAPHICS_TEXT_ROWS; r++)
      {
      for (c = 0; c < VESA_GRAPHICS_TEXT_COLS-1; c++)
         str[c] = screen_mirror[r][c];
      str[c] = EOS;
      curpos(r,0);
      puts(str);
      }
   curpos(dbg_cursor_r, dbg_cursor_c);
   }

scroll_debug_window()
   {
   memmove(&screen_mirror[DEBUG_WINDOW_START_TEXT_ROW+1][0],
           &screen_mirror[DEBUG_WINDOW_START_TEXT_ROW][0],
           VESA_GRAPHICS_TEXT_COLS * (VESA_GRAPHICS_TEXT_ROWS - DEBUG_WINDOW_START_TEXT_ROW - 1));
   memfill(&screen_mirror[VESA_GRAPHICS_TEXT_MAX_ROW][0],VESA_GRAPHICS_TEXT_COLS,' ');
      /* memfill() to erase bottom text line */
   redraw_debug_window();
   }

show_debug_screen()
   {
   WORD i, j, k;
   BYTE str[STRING_SIZE+32];  /* too big, but who cares? */

   sprintf(str,"--> %03x ",pc);
   for (i = 0, j = pc; i < DISASSEMBLY_WINDOW_NUM_ROWS; i++)
      {
      curpos(DISASSEMBLY_WINDOW_START_TEXT_ROW + i, DISASSEMBLY_WINDOW_START_TEXT_COL);
      if (j < MEM_SIZE)
         disassemble(j, &str[8]);
      while (strlen(str) < 32)
         strcat(str," ");
      puts(str);
      j += 2;
      if (j < MEM_SIZE)
         sprintf(str,"    %03x ",j);
      else
         str[0] = EOS;
      }
   for (i = 0; i < 8; i++)
      {
      curpos(REGISTER_WINDOW_START_TEXT_ROW, REGISTER_WINDOW_START_TEXT_COL+i*7);
      sprintf(str,"V%x=%02x  ",i,regs[i]);
      puts(str);
      }
   sprintf(str," PC=%03x  I=%03x  SP=%03x",pc,I_reg,sp);
   puts(str);
   for (i = 8, j = 0; i < 16; i++, j++)
      {
      curpos(REGISTER_WINDOW_START_TEXT_ROW+1, REGISTER_WINDOW_START_TEXT_COL+j*7);
      sprintf(str,"V%x=%02x  ",i,regs[i]);
      puts(str);
      }
   sprintf(str," DT=%02x   ST=%02x",delay_timer,sound_timer);
   puts(str);
   curpos(REGISTER_WINDOW_START_TEXT_ROW+2, REGISTER_WINDOW_START_TEXT_COL);
   puts("Stack:");
   for (i = sp, j = 0; i < STACK_START; i += 2, j++)
      {
      sprintf(str," %x%02x",memory[i+2],memory[i+1]);
      puts(str);
      }
   for ( ; j < STACK_SIZE; j++)
      puts("    ");  /* erase old contents */
   curpos(dbg_cursor_r,dbg_cursor_c);
   }

next_debug_window_line()
   {
   dbg_cursor_c = 0;
   if (++dbg_cursor_r == VESA_GRAPHICS_TEXT_ROWS)
      {
      dbg_cursor_r--;
      scroll_debug_window();
      }
   curpos(dbg_cursor_r, dbg_cursor_c);
   }

debug_window_puts_nl(str)
   BYTE *str;
   {
   debug_window_puts(str);
   next_debug_window_line();
   }

get_debug_str(prompt,str)
   BYTE *prompt, *str;
   {
   debug_window_puts(prompt);
   mygets(str);
   }
 
reset_chip8()
   {
   reset_run_regs();
   memfill(&memory[PROGRAM_START], MEM_SIZE-PROGRAM_START-2, 0);
      /* reset video, too */
   if (super_chip8_video)
      set_high_video();  /* shared with instr_system() */
   else
      set_low_video();  /* shared with instr_system() and main() */
   update_screen();
   }

dbg_command_error()
   {
   WORD i;

   for (i = 0; i < dbg_input_index; i++)
      debug_window_puts(" ");
   debug_window_puts_nl("^ ???");
   }

WORD setup_instruction_breakpoint()
   {
   WORD i = strlen(dbg_token);
   WORD j;
   WORD state;

   if (breakpoint_instr_name[0])  /* instruction name breakpoint already set */
      return(FALSE);
   if ((i < 2) || (dbg_token[i-1] != '"'))  /* not really a quoted string */
      return(FALSE);
   for (i = 1, j = 0, state = 0; dbg_token[i] != '"'; i++, j++)
      {
      breakpoint_instr_name[j] = toupper(dbg_token[i]);
      if ((state == 0) && (breakpoint_instr_name[j] == ' '))  /* end of instr name */
         {
         if ((!j)  /* if expecting an instr name but it's a space */
                 || (j > 5))  /* or instr name is too long */
            return(FALSE);
         while (j < 5)
            breakpoint_instr_name[++j] = ' ';  /* pad to standard len */
         state = 1;  /* expecting an argument */
         }
      if ((breakpoint_instr_name[j] == ',')
               && (dbg_token[i+1] != ' '))  /* disassembly uses squence ", " */
         breakpoint_instr_name[++j] = ' ';  /* pad to standard len */
      }
   breakpoint_instr_name[j] = EOS;
   return(TRUE);
   }

void dbg_proceede()
   {
   use_proceede_breakpoint = TRUE;
        /* set breakpoint after current instruction */
   dbg_go();  /* the rest is just the GO command */
   }

void dbg_go()
   {
   WORD valid;
   WORD num;
   WORD addr, addrflag;
   WORD b_start, b_end, b_count;
   BYTE message[STRING_SIZE];

   for (num = 0; num < MAX_BREAKPOINTS; num++)
                 /* reset all breakpoint hit counts */
      {
      sticky_breakpoints[num].hits = 0;
      go_breakpoints[num].hits = 0;
      }
   addrflag = FALSE;
   valid = TRUE;  /* don't have to have any parameters */
   if (dbg_parse())  /* can be either start PC or breakpoint */
      {
      if (dbg_token[0] == '=')  /* if a starting address given */
         {
/*          if (valid = is_pc(&dbg_token[1],&addr)) */
         if (valid = is_address(&dbg_token[1],&addr))
            {
            addrflag = TRUE;
               /* do not immediately change PC, could be an error
                * later in the command.
                */
            dbg_parse();
            }
         }
      }
   num_go_breakpoints = 0;
      /* set breakpoints */
   while ((valid)
              && (dbg_token_len)
              && (num_go_breakpoints < MAX_BREAKPOINTS)
         )
      {
      b_count = 0;
      if (valid = is_memory(dbg_token,&b_start))
         {
         dbg_skip_input_whitespace();
         if (dbg_input[dbg_input_index] == ',')  /* breakpoint end location */
            {
            dbg_input_index++;  /* skip comma */
            if (valid = dbg_parse())
               {
               if ((!is_memory(dbg_token,&b_end))
                    || (b_start > b_end)  /* end must be >= start */
                  )
                  valid = FALSE;
               else
                  dbg_skip_input_whitespace();
                       /* skip whitespace so we can check for hit count */
               }
            }
         else
            b_end = b_start;
         if ((valid) && (dbg_input[dbg_input_index] == '='))
                      /* breakpoint hit count */
            {
            dbg_input_index++;  /* skip equal sign */
            if (valid = dbg_parse())
               {
               valid = is_hexword(dbg_token,&b_count);
               }
            }
         }
      if (valid)
         {
         go_breakpoints[num_go_breakpoints].start = b_start;
         go_breakpoints[num_go_breakpoints].end = b_end;
         go_breakpoints[num_go_breakpoints++].count = b_count;
         dbg_parse();  /* get next token, if any */
         }
      }
   if (valid)
      valid = !dbg_parse();  /* if we're not looking for a token but one exists, error */
   if (!valid)
      dbg_command_error();
   else
      {
      if (addrflag)
         pc = addr;  /* only change PC when entire command is vaild */
      if (use_proceede_breakpoint)
            /* set breakpoint location here, in case we specified the PC */
         {
         proceede_breakpoint.start =
         proceede_breakpoint.end = pc + 2;
         }
      run_chip8();
      if (errorcode == ERROR_BREAK)
         debug_window_puts_nl("Breakpoint");
      if (end_time < start_time)
         end_time += 1572480L;  /* 86400 seconds/day * 18.2 ticks/second */
      end_time = (DWORD)(((float)(end_time - start_time) + 0.91) / 1.82);
                     /* integer execution time in seconds * 10 */
      sprintf(message,"%04x %04x %04x instructions in %.1f seconds\n",
               instruction_count[2],
               instruction_count[1],
               instruction_count[0],
               (float)end_time / 10.0  /* round 1/10ths of a second */
               );
      debug_window_puts_nl(message);
      if (errorcode >= ERROR_STACK_OVERFLOW)
         {
         debug_window_puts(chip8_error[errorcode - ERROR_STACK_OVERFLOW]);
         if (errorcode == ERROR_UNKNOWN_OPCODE)
            {
            sprintf(message,"   opcode = %04x\n",opcode);
            debug_window_puts(message);
            }
         next_debug_window_line();
         }
      }
   errorcode = ERROR_NONE;  /* reset for next time */
   use_proceede_breakpoint = FALSE;  /* don't care if this is GO or PROCEEDE */
   num_go_breakpoints = 0;  /* clear GO breakpoints after command */
   }

void dbg_trace()
   {
   WORD valid;
   WORD len;
   WORD addr, addrflag;
   WORD b;
   BYTE message[STRING_SIZE];

   addrflag = FALSE;
   len = 1;  /* default: trace 1 instruction */
   valid = TRUE;  /* don't have to have any parameters */
   if (dbg_parse())  /* can be either start PC or trace count */
      {
      if (dbg_token[0] == '=')  /* if a starting address given */
         {
/*          if (valid = is_pc(&dbg_token[1],&addr)) */
         if (valid = is_address(&dbg_token[1],&addr))
            {
            addrflag = TRUE;
               /* do not immediately change PC, could be an error
                * later in the command.
                */
            dbg_parse();
            }
         }
      }
   if ((valid) && (dbg_token_len))  /* must be trace count */
      {
      if ((!is_hexword(dbg_token,&len))
                 || (len == 0))
         valid = FALSE;
      }
   if (valid)
      valid = !dbg_parse();  /* if we're not looking for a token but one exists, error */
   if (!valid)
      dbg_command_error();
   else
      {
      if (addrflag)
         pc = addr;  /* only change PC when entire command is vaild */
      b = sticky_breakpoints_disabled;  /* remember original value */
      sticky_breakpoints_disabled = TRUE;
      do
         {
         trace_chip8();
         show_debug_screen();
         if (keytest())
            if (getkey() == ESC_KEY)
               len = 1;  /* abort trace */
         } while ((--len) && (errorcode < ERROR_EXIT));
      sticky_breakpoints_disabled = b;
      if (errorcode >= ERROR_STACK_OVERFLOW)
         {
         debug_window_puts(chip8_error[errorcode - ERROR_STACK_OVERFLOW]);
         if (errorcode == ERROR_UNKNOWN_OPCODE)
            {
            sprintf(message,"   opcode = %04x\n",opcode);
            debug_window_puts(message);
            }
         next_debug_window_line();
         }
      }
   errorcode = ERROR_NONE;  /* reset for next time */
   }

void dbg_breakpoint()
   {
   WORD valid;
   WORD num;
   WORD b_start[MAX_BREAKPOINTS], b_end[MAX_BREAKPOINTS], b_count[MAX_BREAKPOINTS];
   BYTE message[STRING_SIZE];

   if (!dbg_parse())  /* if no arguments, toggle breakpoint status */
      {
      if ((!num_sticky_breakpoints) && (!breakpoint_instr_name[0]))
         {
         sticky_breakpoints_disabled = TRUE;
         debug_window_puts_nl("no sticky breakpoints");
         }
      else if (sticky_breakpoints_disabled)
         {
         sticky_breakpoints_disabled = FALSE;
         breakpoint_instr_name_len = strlen(breakpoint_instr_name);
         debug_window_puts_nl("Stick breakpoints enabled");
         }
      else
         {
         sticky_breakpoints_disabled = TRUE;
         breakpoint_instr_name_len = 0;
         debug_window_puts_nl("Stick breakpoints disabled");
         }
      return;  /* and done */
      }
   if (strcmpi(dbg_token,"list") == 0)
      {
      if ((!num_sticky_breakpoints) && (!breakpoint_instr_name_len))
         debug_window_puts_nl("No sticky breakpoints");
      if (num_sticky_breakpoints)
         {
         for (num = 0; num < num_sticky_breakpoints; num++)
            {
            sprintf(message,"%3x",sticky_breakpoints[num].start);
            if (sticky_breakpoints[num].start != sticky_breakpoints[num].end)
               sprintf(&message[strlen(message)],",%3x",sticky_breakpoints[num].end);
            if (sticky_breakpoints[num].count > 1)
               sprintf(&message[strlen(message)],"=%2x",sticky_breakpoints[num].count);
            strcat(message,"  ");
            debug_window_puts(message);
            }
         next_debug_window_line();
         }
      if (breakpoint_instr_name_len)
         {
         debug_window_puts("Break on instruction ");
         debug_window_puts_nl(breakpoint_instr_name);
         }
      return;
      }
   valid = TRUE;
   num = 0;
   breakpoint_instr_name_len = 0;
   breakpoint_instr_name[0] = EOS;  /* clear named breakpoint */
      /* set breakpoints */
   while ((valid)
              && (dbg_token_len)
              && (num < MAX_BREAKPOINTS)
         )
      {
      if (dbg_token[0] == '"')  /* if breakpoint on instruction */
         {
         if (valid = setup_instruction_breakpoint())
            dbg_parse();  /* get next token, if any */
         }
      else
         {
         b_count[num] = 0;
         if (valid = is_memory(dbg_token,&b_start[num]))
            {
            dbg_skip_input_whitespace();
            if (dbg_input[dbg_input_index] == ',')  /* breakpoint end location */
               {
               dbg_input_index++;  /* skip comma */
               if (valid = dbg_parse())
                  {
                  if ((!is_memory(dbg_token,&b_end[num]))
                       || (b_start[num] > b_end[num])  /* end must be >= start */
                     )
                     valid = FALSE;
                  else
                     dbg_skip_input_whitespace();
                          /* skip whitespace so we can check for hit count */
                  }
               }
            else
               b_end[num] = b_start[num];
            if ((valid) && (dbg_input[dbg_input_index] == '='))
                         /* breakpoint hit count */
               {
               dbg_input_index++;  /* skip equal sign */
               if (valid = dbg_parse())
                  {
                  valid = is_hexword(dbg_token,&b_count[num]);
                  }
               }
            }
         if (valid)
            {
            dbg_parse();  /* get next token, if any */
            num++;
            }
         }  /* if (instruction_breakpoint) ... else */
      }  /* while(...) */
   if ((valid)
         && (((num == MAX_BREAKPOINTS) && (dbg_token_len))
                || (dbg_parse())
            )
      )  /* if we're not looking for a token but one exists, error */
      valid = FALSE;
   if ((!valid)
         || ((num == 0) && (!breakpoint_instr_name[0]))
      )
      {
      breakpoint_instr_name_len = 0;
      breakpoint_instr_name[0] = EOS;  /* clear named breakpoint */
      dbg_command_error();
      }
   else
      {
      for (num_sticky_breakpoints = 0; num_sticky_breakpoints < num; num_sticky_breakpoints++)
         {
         sticky_breakpoints[num_sticky_breakpoints].start = b_start[num_sticky_breakpoints];
         sticky_breakpoints[num_sticky_breakpoints].end = b_end[num_sticky_breakpoints];
         sticky_breakpoints[num_sticky_breakpoints].count = b_count[num_sticky_breakpoints];
         }
      if (num)
         sticky_breakpoints_disabled = FALSE;
      breakpoint_instr_name_len = strlen(breakpoint_instr_name);
      }
   }

void dbg_disassemble()
   {
   WORD valid;
   WORD len;
   WORD addr;
   BYTE message[STRING_SIZE];

   len = VESA_GRAPHICS_TEXT_ROWS - DEBUG_WINDOW_START_TEXT_ROW - 1;
   valid = TRUE;  /* don't have to have any parameters */
   if (dbg_parse())  /* first paramter:  starting address */
      {
/*       if (valid = is_pc(dbg_token,&addr)) */
      if (valid = is_address(dbg_token,&addr))
         {
         dbg_disassemble_pc = addr;
         dbg_parse();
         }
      }
   if ((valid) && (dbg_token_len))  /* if another token */
      {
      if ((!is_hexword(dbg_token,&len))
                || (len == 0)
                || (len > MEM_SIZE)  /* don't OK a value that will
                                      * wrap around in memory.
                                      */
                || ((dbg_disassemble_pc + len*2) > MEM_SIZE)
         )
         valid = FALSE;
      }
   if (!valid)
      dbg_command_error();
   else
      {
      while ((len) && (dbg_disassemble_pc < MEM_SIZE))
         {
         sprintf(message,"%03x ",dbg_disassemble_pc);
         disassemble(dbg_disassemble_pc, &message[4]);
         debug_window_puts_nl(message);
         dbg_disassemble_pc += 2;
         len--;
         if (keytest())
            if (getkey() == ESC_KEY)
               len = 0;  /* abort trace */
         }
      if (dbg_disassemble_pc >= MEM_SIZE)
         dbg_disassemble_pc = program_load_addr;
      }
   }

void dbg_assemble()
   {
   WORD valid;
   WORD len;
   WORD addr;
   BYTE message[STRING_SIZE];

   valid = TRUE;  /* don't have to have any parameters */
   if (dbg_parse())  /* only parameter: starting address */
      {
/*       if (valid = is_pc(dbg_token,&addr)) */
      if (valid = is_address(dbg_token,&addr))
         dbg_asm_pc = addr;
      }
   if (!valid)
      dbg_command_error();
   else
      {
      do
         {
         sprintf(message,"%03x ",dbg_asm_pc);
         get_debug_str(message,dbg_input);
         next_debug_window_line();
         if (dbg_input[0])
            {
            if (assemble(dbg_asm_pc,dbg_input,&len))
               dbg_asm_pc += len;
            else
               dbg_command_error();
            show_debug_screen();
            if (dbg_asm_pc >= MEM_SIZE)
               {
               dbg_asm_pc = program_load_addr;
               dbg_input[0] = EOS;  /* terminate assembler */
               }
            }
         } while (dbg_input[0]);  /* until a blank line */
      }
   }

void erase_debug_window()
   {
   WORD r;
   BYTE message[STRING_SIZE];

   memfill(message,VESA_GRAPHICS_TEXT_COLS-2,' ');
   message[VESA_GRAPHICS_TEXT_COLS-2] = EOS;
   for (r = DEBUG_WINDOW_START_TEXT_ROW; r < VESA_GRAPHICS_TEXT_ROWS; r++)
      {
      curpos(r,0);
      puts(message);
      }
   }

void dbg_edit_sprite()
   {
   WORD valid;
   WORD addr, addr2;
   WORD num;
   WORD len;
   WORD r;

   num = 8;  /* default:  8-bit wide sprite */
   addr2 = 0xffff;  /* flag: no copy-from address */
   if (valid = dbg_parse())  /* address of sprite in memory */
      {
      valid = is_memory(dbg_token,&addr);
           /* Note: Not is_address(), allow editing of system sprites */
      }
   if ((valid) && (valid = dbg_parse()))  /* must have height */
      {
      if ((!is_hexword(dbg_token,&len))
                || (len == 0)
                || (len > 16)
         )
         valid = FALSE;
      }
   if ((valid) && (dbg_parse()))  /* copy-from address */
      {
      valid = is_memory(dbg_token,&addr2);
          /* Note: Not is_address(), allow editing of system sprites */
       }
   if (valid)
      valid = !dbg_parse();  /* if we're not looking for a token but one exists, error */
   if (len < 16)  /* 1-byte wide sprite */
      {
      if ((addr + len) >= MEM_SIZE)
         valid = FALSE;
      if ((addr2 != 0xffff) && ((addr2 + len) >= MEM_SIZE))
         valid = FALSE;
      }
   else  /* 2-byte wide sprite */
      {
      if ((addr + len + len) >= MEM_SIZE)
         valid = FALSE;
      if ((addr2 != 0xffff) && ((addr2 + len + len) >= MEM_SIZE))
         valid = FALSE;
      }
   if (!valid)
      dbg_command_error();
   else
      {
         /* first, erase debug window so there is somewhere to
          * draw the sprite editor box
          */
      erase_debug_window();
      edit_sprite(len, addr, addr2);
      redraw_debug_window();
      }
   }

void dbg_create_file()
   {
   WORD valid;
   WORD addr, addr2;
   WORD i;
   BYTE name[STRING_SIZE];

   addr = program_load_addr;
   addr2 = MEM_SIZE - 1;
   if (valid = dbg_parse())
      {
      strcpy(name,dbg_token);  /* save filename for now */
      if (dbg_parse())  /* is there a starting address? */
         {
         if (valid = is_address(dbg_token,&addr))
            {
            if (dbg_parse())  /* is there an ending address? */
               {
               if ((!is_address(dbg_token,&addr2))
                    || (addr2 <= addr))  /* end must be > start */
                  valid = FALSE;
               }
            }
         }
      }
   if (!valid)
      dbg_command_error();
   else
      {
      if ((i = open(name,0)) != -1)  /* if file exists */
               /* use i as a file handle */
         {
         close(i);
         get_debug_str("File exists.  Overwrite it? ",dbg_input);
         if (toupper(dbg_input[0]) != 'Y')
            valid = FALSE;
         next_debug_window_line();
         }
      if (valid)  /* OK to create or overwrite */
         {
         addr2 = addr2 - addr + 1;  /* length to write */
         if ((i = creat(name)) == -1)
            debug_window_puts_nl("Cannot create file");
         else
            {
            write(i,&memory[addr],addr2);
            close(i);
            }
         }
      }
   }

void dbg_load_file()
   {
   WORD valid;
   WORD len;
   WORD i;

   valid = TRUE;
   if (dbg_parse())  /* if we have a filename */
      {
      if ((i = open(dbg_token,0)) == -1)  /* if file does not exist */
               /* use i as a file handle */
         {
         debug_window_puts_nl("Cannot open file");
         valid = FALSE;
         }
      else
         {
         len = read(i,&memory[program_load_addr],MEM_SIZE - program_load_addr);
         close(i);
         if (len < MIN_PROGRAM_SIZE)
            {
            debug_window_puts_nl("Not a valid CHIP8 program");
            memfill(&memory[PROGRAM_START], MEM_SIZE-PROGRAM_START, 0);
            }
         }
      }
   if (valid)
      {
      reset_run_regs();
      if (startup_video_super_chip8)
         set_high_video();  /* shared with instr_system() */
      else
         set_low_video();  /* shared with instr_system() and main() */
      update_screen();
      }
   }

void dbg_enter_bytes()
   {
   WORD valid;
   WORD len;
   WORD addr;
   BYTE data[STRING_SIZE];

   if (valid = dbg_parse())
      valid = is_address(dbg_token,&addr);
   if (valid)
      {
      len = 0;
      while ((valid) && (dbg_parse()))
         {
         valid = is_hexbyte(dbg_token,&data[len]);
            /* are all value valid numbers? */
         len++;
         }
      }
   if ((addr + len) >= (MEM_SIZE+1))
      valid = FALSE;
   if ((!valid) || (!len))
      dbg_command_error();
   else
      {
      memmove(data,&memory[addr],len);
      }
   }

void dbg_fill_memory()
   {
   WORD valid;
   WORD addr, addr2;
   WORD len;
   WORD i;
   BYTE data[STRING_SIZE];

   if (valid = dbg_parse())
      valid = is_address(dbg_token,&addr);
   if (valid)
      {
      if (valid = dbg_parse())
         {
         if ((!is_address(dbg_token,&addr2))
                     || (addr2 < addr))  /* need END >= START */
            valid = FALSE;
         }
      }
   if (valid)
      {
      len = 0;
      while ((valid) && (dbg_parse()))
         {
         valid = is_hexbyte(dbg_token,&data[len]);
            /* are all value valid numbers? */
         len++;
         }
      }
   if ((!valid) || (!len))
      dbg_command_error();
   else
      {
      i = 0;
      while (addr <= addr2)
         {
         memory[addr++] = data[i++];
         if (i == len)
            i = 0;
         }
      }
   }

void dbg_reg()
   {
   WORD valid;
   WORD addr;
   WORD num;
   WORD i;

   if (valid = dbg_parse())
      {
      if ((toupper(dbg_token[0]) == 'V')
               && (ishexdigit(dbg_token[1]))
               && (!dbg_token[2])
         )
         {
         i = hexdigit(dbg_token[1]);
         if ((valid = dbg_parse())
                   && (valid = is_hexbyte(dbg_token,&num))
            )
            regs[i] = num;
         }
      else
         {
         if (strcmpi(dbg_token,"PC") == 0)
            {
            if ((valid = dbg_parse())
/*                     && (valid = is_pc(dbg_token,&addr)) */
                    && (valid = is_address(dbg_token,&addr))
               )
               pc = addr;
            }
         else if (strcmpi(dbg_token,"I") == 0)
            {
            if ((valid = dbg_parse())
                     && (valid = is_memory(dbg_token,&addr))
               )
               I_reg = addr;
            }
         else if (strcmpi(dbg_token,"SP") == 0)
            {
            if ((valid = dbg_parse())
                    && (valid = is_memory(dbg_token,&addr))
               )
               {
               if ((!(addr & 1))
                         && (addr >= STACK_END)
                         && (addr <= STACK_START)
                  )
                  sp = addr;
               else
                  valid = FALSE;
               }
            }
         else if (strcmpi(dbg_token,"DT") == 0)
            {
            if ((valid = dbg_parse())
                    && (valid = is_hexbyte(dbg_token,&num))
               )
               delay_timer = num;
            }
         else if (strcmpi(dbg_token,"ST") == 0)
            {
            if ((valid = dbg_parse())
                    && (valid = is_hexbyte(dbg_token,&num))
               )
               sound_timer = num;
            }
         else
            valid = FALSE;
         }
      }
   if (!valid)
      dbg_command_error();
   }

void dbg_dump_memory()
   {
   WORD valid;
   WORD addr, addrflag;
   WORD addr2;
   WORD len;
   WORD num;
   WORD i;
   BYTE message[STRING_SIZE];

   len = 32;  /* default: 2 lines of 16 bytes */
   addr = dbg_dump_addr;
   addrflag = FALSE;
   valid = TRUE;  /* don't have to have any parameters */
   if (dbg_parse())  /* starting address */
      {
      if (valid = is_memory(dbg_token,&addr))
         addrflag = TRUE;
      if ((valid) && (dbg_parse()))
         {
         if ((!is_hexword(dbg_token,&len))
                 || ((addr+len) > MEM_SIZE))
            valid = FALSE;
         }
      }
   if ((!valid) || ((addr + len) > MEM_SIZE))
      dbg_command_error();
   else
      {
      if (addrflag)
         dbg_dump_addr = addr;
      num = 0;  /* count of bytes dumped */
      while (num < len)
         {
         sprintf(message,"%03x                         -                       ",dbg_dump_addr & 0xfff0);
                       /* template */
                       /* "000  00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00   ................" */
                       /*  000000000011111111112222222222333333333344444444445555555555666666666677 */
                       /*  012345678901234567890123456789012345678901234567890123456789012345678901 */
         i = 5 + (dbg_dump_addr & 0x000f) * 3;  /* index into line */
         while ((i < 52) && (num < len))
            {
            message[i++] = tohex(memory[dbg_dump_addr] >> 4);
            message[i++] = tohex(memory[dbg_dump_addr++] & 0x0f);
            i++;
            num++;
/*
 *          j = memory[dbg_dump_addr];
 *          message[55 + (dbg_dump_addr & 0x0f)] = ((j >= SPACE) && (j <= '~')) ? j : '.';
 *          message[i++] = tohex(j >> 4);
 *          message[i++] = tohex(j & 0x0f);
 *          i++;
 *          num++;
 *          dbg_dump_addr++;
 */
            }
         debug_window_puts_nl(message);
         }
      }
   }

void dbg_show_where()
   {
   WORD num;
   WORD i;
   BYTE message[STRING_SIZE];

   if (!where_queue_entries)
      {
      debug_window_puts_nl("empty where queue");
      }
   else
      {
      if (where_queue_entries < WHERE_QUEUE_SIZE)
         i = 0;
      else
         i = where_queue_index;  /* next to be overwritten is the oldest */
      for (num = 0; num < where_queue_entries; num++)
         {
         sprintf(message,"%03x ",where_queue[i]);
         disassemble(where_queue[i], &message[4]);
         i = (i + 1) & (WHERE_QUEUE_SIZE - 1);
         debug_window_puts_nl(message);
         }
      }
   }

void dbg_save_state()
   {
   WORD valid;
   WORD len;
   WORD handle;
   BYTE file[STRING_SIZE];

   if (!dbg_parse())
      dbg_command_error();
   else
      {
      strcpy(file,dbg_token);  /* save filename */
      if ((handle = open(file,0)) != -1)  /* if file exists */
         {
         close(handle);
         get_debug_str("File exists.  Overwrite it? ",dbg_input);
         if (toupper(dbg_input[0]) != 'Y')
            valid = FALSE;
         next_debug_window_line();
         }
      if (valid)  /* OK to create or overwrite */
         {
         if ((handle = creat(file)) == -1)
            debug_window_puts_nl("Cannot create file");
         else
            {
            if (valid = (write(handle,state_file_signature,STATE_FILE_SIGNATURE_LEN) == STATE_FILE_SIGNATURE_LEN))
               valid = write(handle,memory,MEM_SIZE) == MEM_SIZE;
            if (valid)
               valid = write(handle,video_memory,VIDEO_MEMORY_SIZE) == VIDEO_MEMORY_SIZE;
            if (valid)
               valid = write(handle,regs,sizeof(regs)) == sizeof(regs);
            if (valid)
               valid = write(handle,hp48regs,sizeof(hp48regs)) == sizeof(hp48regs);
            if (valid)
               valid = write(handle,&pc,sizeof(pc)) == sizeof(pc);
            if (valid)
               valid = write(handle,&sp,sizeof(sp)) == sizeof(sp);
            if (valid)
               valid = write(handle,&I_reg,sizeof(I_reg)) == sizeof(I_reg);
            if (valid)
               valid = write(handle,&sound_timer,sizeof(sound_timer)) == sizeof(sound_timer);
            if (valid)
               valid = write(handle,&delay_timer,sizeof(delay_timer)) == sizeof(delay_timer);
            if (valid)
               valid = write(handle,&program_load_addr,sizeof(program_load_addr)) == sizeof(program_load_addr);
            if (valid)
               valid = write(handle,&super_chip8_video,sizeof(super_chip8_video)) == sizeof(super_chip8_video);
            close(handle);
            if (!valid)
               {
               unlink(file);
               debug_window_puts_nl("Unable to write file.  Disk full?");
               }
            }
         }
      }
   }

void dbg_load_state()
   {
   WORD valid;
   WORD len;
   WORD handle;
   WORD load_addr;
   BYTE super8_video;
   BYTE file[STRING_SIZE];

   if (!dbg_parse())
      dbg_command_error();
   else
      {
      strcpy(file,dbg_token);  /* save filename */
      if ((handle = open(file,0)) == -1)  /* if file exists */
         {
         debug_window_puts_nl("Cannot open file");
         }
      else
         {
         if (valid = (read(handle,file,STATE_FILE_SIGNATURE_LEN) == STATE_FILE_SIGNATURE_LEN))
                /* can reuse file here */
            {
            file[STATE_FILE_SIGNATURE_LEN] = EOS;  /* proper string for compare */
            if (strcmp(file,state_file_signature) != 0)
               valid = FALSE;
            }
         if (valid)
            valid = read(handle,memory,MEM_SIZE) == MEM_SIZE;
         if (valid)
            valid = read(handle,video_memory,VIDEO_MEMORY_SIZE) == VIDEO_MEMORY_SIZE;
         if (valid)
            valid = read(handle,regs,sizeof(regs)) == sizeof(regs);
         if (valid)
            valid = read(handle,hp48regs,sizeof(hp48regs)) == sizeof(hp48regs);
         if (valid)
            valid = read(handle,&pc,sizeof(pc)) == sizeof(pc);
         if (valid)
            valid = read(handle,&sp,sizeof(sp)) == sizeof(sp);
         if (valid)
            valid = read(handle,&I_reg,sizeof(I_reg)) == sizeof(I_reg);
         if (valid)
            valid = read(handle,&sound_timer,sizeof(sound_timer)) == sizeof(sound_timer);
         if (valid)
            valid = read(handle,&delay_timer,sizeof(delay_timer)) == sizeof(delay_timer);
         if (valid)
            valid = read(handle,&load_addr,sizeof(program_load_addr)) == sizeof(program_load_addr);
         if (valid)
            valid = read(handle,&super8_video,sizeof(super_chip8_video)) == sizeof(super_chip8_video);
         close(handle);
         if ((!valid)
              || ((load_addr != PROGRAM_START)
                    && (load_addr != ETI660_PROGRAM_START))
              || (super8_video > 1)  /* not TRUE or FALSE */
            )
            {
            debug_window_puts_nl("invalid state file");
            reset_chip8();
            }
         else
            {
            program_load_addr = load_addr;
            if (super_chip8_video = super8_video)
               {
               screen_cols = GRAPHICS_COLS;
               screen_rows = GRAPHICS_ROWS;
               max_screen_cols = GRAPHICS_COLS - 1;
               max_screen_rows = GRAPHICS_ROWS - 1;
               }
            else
               {
               screen_cols = LOWRES_GRAPHICS_COLS;
               screen_rows = LOWRES_GRAPHICS_ROWS;
               max_screen_cols = LOWRES_GRAPHICS_COLS - 1;
               max_screen_rows = LOWRES_GRAPHICS_ROWS - 1;
               }
            update_screen();
            }
         }
      }
   }

void dbg_cmdline_k()
   {
   if (use_alt_keypad)
      {
      use_alt_keypad = FALSE;
      debug_window_puts_nl("Normal keypad");
      }
   else
      {
      use_alt_keypad = TRUE;
      debug_window_puts_nl("Alternate keypad");
      }
   setup_keypad();
   }

void dbg_cmdline_f()
   {
   if (execution_freerun)
      {
      execution_freerun = FALSE;
      debug_window_puts_nl("Freerun OFF");
      }
   else
      {
      execution_freerun = TRUE;
      debug_window_puts_nl("Freerun ON");
      }
   }

void dbg_cmdline_v()
   {
   if (startup_video_super_chip8)
      {
      startup_video_super_chip8 = FALSE;
      set_low_video();  /* shared with instr_system() */
      debug_window_puts_nl("Super Chip8 video OFF");
      }
   else
      {
      startup_video_super_chip8 = TRUE;
      set_high_video();  /* shared with instr_system() and main() */
      debug_window_puts_nl("Super Chip8 video ON");
      }
   update_screen();
   }

void dbg_cmdline_t()
   {
   WORD num;
   BYTE message[STRING_SIZE];

   if (!dbg_parse())
      {
      sprintf(message,"%x ticks (2040 Hz) per instruction cycle",execution_ticks_per_instruction);
      debug_window_puts_nl(message);
      }
   else
      {
      if ((!is_hexword(dbg_token,&num)) || (num == 0))
         dbg_command_error();
      else
         execution_ticks_per_instruction = num;
      }
   }

void dbg_cmdline_i()
   {
   WORD num;
   BYTE message[STRING_SIZE];

   if (!dbg_parse())
      {
      sprintf(message,"%x instructions per instruction cycle",instructions_per_execution_tick);
      debug_window_puts_nl(message);
      }
   else
      {
      if ((!is_hexword(dbg_token,&num)) || (num == 0))
         dbg_command_error();
      else
         instructions_per_execution_tick = num;
      }
   }

void dbg_cmdline_s()
   {
   WORD num;

   if ((!dbg_parse())
             || (!is_hexword(dbg_token,&num))
             || (num < 100)
             || (num > 1000)
      )
      dbg_command_error();
   else
      {
      sound_frequency = num;
      setup_sound((WORD)(TIMER_CLOCK_BASE / sound_frequency));
      }
   }

void dbg_cmdline_l()
   {
   WORD num;

   if ((!dbg_parse())
             || (!is_hexword(dbg_token,&num))
             || ((num != PROGRAM_START)
                  && (num != ETI660_PROGRAM_START))
      )
      dbg_command_error();
   else
      {
      program_load_addr = num;
      reset_chip8();
      }
   }

void dbg_cmdline_w()
   {
   if (wrap_sprites)
      {
      wrap_sprites = FALSE;
      debug_window_puts_nl("Sprite wrap OFF");
      }
   else
      {
      wrap_sprites = TRUE;
      debug_window_puts_nl("Sprite wrap ON");
      }
   }

void dbg_cmdline_m()
   {
   if (use_vy_shift)
      {
      use_vy_shift = FALSE;
      i_reg_is_incremented = FALSE;
      debug_window_puts_nl("Mikolay's opcodes OFF:");
      debug_window_puts_nl("      SHR Vx, Vy and SHL Vx, Vy -- shift the Vx reg");
      debug_window_puts_nl("      LD [I], Vx and LD Vx, [I] -- I unchanged");
      }
   else
      {
      use_vy_shift = TRUE;
      i_reg_is_incremented = TRUE;
      debug_window_puts_nl("Mikolay's opcodes ON:");
      debug_window_puts_nl("      SHR Vx, Vy and SHL Vx, Vy -- shift the Vy reg, result in Vx");
      debug_window_puts_nl("      LD [I], Vx and LD Vx, [I] -- increment I");
      }
   }

void dbg_cmdline_x()
   {
   if (use_vy_shift)
      {
      use_vy_shift = FALSE;
      debug_window_puts_nl("SHR Vx, Vy and SHL Vx, Vy -- shift the Vx reg, Vy not used");
      }
   else
      {
      use_vy_shift = TRUE;
      debug_window_puts_nl("SHR Vx, Vy and SHL Vx, Vy -- shift the Vy reg, result in Vx");
      }
   }

void dbg_cmdline_y()
   {
   if (i_reg_is_incremented)
      {
      i_reg_is_incremented = FALSE;
      debug_window_puts_nl("LD [I], Vx and LD Vx, [I] do not increment I");
      }
   else
      {
      i_reg_is_incremented = TRUE;
      debug_window_puts_nl("LD [I], Vx and LD Vx, [I] increment I");
      }
   }

void dbg_cmdline_z()
   {
   if (i_reg_sets_carry)
      {
      i_reg_sets_carry = FALSE;
      debug_window_puts_nl("ADD I, Vx does not set carry in VF");
      }
   else
      {
      i_reg_sets_carry = TRUE;
      debug_window_puts_nl("ADD I, Vx sets carry in VF");
      }
   }

void dbg_quit()
   {
     /* don't need to do anything, just need a function to point to */
   }

menu()
   {
   WORD command;
   WORD i;

   memfill(screen_mirror, sizeof(screen_mirror), ' ');
      /* don't want screen_mirror to be full of End-Of-String */
   dbg_cursor_r = DEBUG_WINDOW_START_TEXT_ROW;
   dbg_cursor_c = DEBUG_WINDOW_START_TEXT_COL;
   curpos(dbg_cursor_r, dbg_cursor_c);
   debug_window_puts_nl("DEBUG8  Mar 8th, 2020   Use H for help");
   for (i = 0; i < 258; i++)
      {
      pixel(128,i,CYAN);
      pixel(129,i,CYAN);
      }
   for (i = 0; i < 129; i++)
      {
      pixel(i,256,CYAN);
      pixel(i,257,CYAN);
      }
   do
      {
      show_debug_screen();
      get_debug_str(">",dbg_input);
      next_debug_window_line();
      dbg_input_index = 0;
      command = BLANK_LINE;  /* not quit! */
      if (dbg_parse())
         {
         for (command = 0; command < DBG_CMD_LIST_SIZE; command++)
            if (strcmpi(dbg_token,dbg_commands[command].name) == 0)
               break;
         if (command == DBG_CMD_LIST_SIZE)
            dbg_command_error();
         else
            (*dbg_commands[command].cmd_fn)();
         }
      }  while (command != QUIT);
   }

run_chip8()
   {
   takeover_keyboard_int();
   takeover_clock_tick(CHIP8_EXECUTION_TICKS_8253_DIVISOR);
   dbg_instruction_limit[0] = 0xffff;
   dbg_instruction_limit[1] = 0xffff;
   dbg_instruction_limit[2] = 0xffff;
   dbg_instruction_limit[3] = 0xffff;  /* forever */
   if (sound_timer)
      sound_on();
   errorcode = ERROR_NONE;
   *((WORD *)&start_time + 1) = peekw(SYSTEM_CLOCK_TICK_SEG, SYSTEM_CLOCK_TICK_OFS + 2);
   *(WORD *)&start_time = peekw(SYSTEM_CLOCK_TICK_SEG, SYSTEM_CLOCK_TICK_OFS);
   execute_chip8();
   *((WORD *)&end_time + 1) = peekw(SYSTEM_CLOCK_TICK_SEG, SYSTEM_CLOCK_TICK_OFS + 2);
   *(WORD *)&end_time = peekw(SYSTEM_CLOCK_TICK_SEG, SYSTEM_CLOCK_TICK_OFS);
   sound_off();
   reset_keyboard_int();
   reset_clock_tick();
   }

trace_chip8()
   {
   WORD freerun_save;
   WORD bp_name_len_save;

   dbg_instruction_limit[0] = 1;
   dbg_instruction_limit[1] = 0;
   dbg_instruction_limit[2] = 0;
   dbg_instruction_limit[3] = 0;
   errorcode = ERROR_NONE;
   freerun_save = execution_freerun;
   execution_freerun = TRUE;
   bp_name_len_save = breakpoint_instr_name_len;
   breakpoint_instr_name_len = 0;
      /* Since tick is not running, don't wait for tick! */
   execute_chip8();
   execution_freerun = freerun_save;
   breakpoint_instr_name_len = bp_name_len_save;
   sound_off();
   }

execute_chip8()
   {
   static BYTE instr_count;  /* put "local" in regular RAM for speed */
   WORD starting_pc;

   instruction_count[0] =
   instruction_count[1] =
   instruction_count[2] =
   instruction_count[3] = 0;
   while (errorcode == ERROR_NONE)  /* also use BREAK to get out */
      {
      if ((execution_freerun) || (execution_ticks_counter >= execution_ticks_per_instruction))
                /* >= in case of execution_ticks_per_instruction == 0 */
         {
#asm
   cli       ;do not disturb
#endasm
         execution_ticks_counter = 0;
#asm
   sti       ;ints are OK again
#endasm
         instr_count = 0;  /* how many instructions this tick */
              /* x86 is little-endian, CHIP-8 memory is big-endian */
         while (TRUE)  /* use BREAK to get out */
            {
            starting_pc = pc;  /* remember this for where queue and breakpoints */
            if (breakpoint_instr_name_len)  /* if we have a breakpoint string */
               {
               disassemble(pc, breakpoint_this_instr_name);
               if (strncmp(breakpoint_this_instr_name, breakpoint_instr_name, breakpoint_instr_name_len) == 0)
                  {
                  errorcode = ERROR_BREAK;
                  break;
                  }
               }
            if (!breakpoint_check(pc,2))
               break;
            if ((pc < program_load_addr) || (pc > MAX_PC))
               {
               errorcode = ERROR_PC_RANGE;
               break;
               }
            *((BYTE *)&opcode + 1) = opcode_high = memory[pc++];
               /* 1st CHIP-8 instr byte is high byte, to high-byte of x86 var */
            *(BYTE *)&opcode = opcode_low = memory[pc++];
#ifdef CHECK_PC_LIMITS
            pc &= ADDRESS_MASK;  /* not sure if this is needed / wanted */
#endif
            (*primary_opcode[opcode_high >> 4])();
            if (errorcode == ERROR_NONE)
               {
#asm
   add  word instruction_count_,1   ;do a 64-bit INC
   adc  word instruction_count_[2],0
   adc  word instruction_count_[4],0
   adc  word instruction_count_[6],0
#endasm
               where_queue[where_queue_index++] = starting_pc;
                    /* if we actually executed the instruction, remember it */
               where_queue_index &= WHERE_QUEUE_SIZE - 1;
                    /* WHERE_QUEUE_SIZE is a power of 2 */
               if (where_queue_entries < WHERE_QUEUE_SIZE)
                  where_queue_entries++;
               if (qword_ge(instruction_count, dbg_instruction_limit))
                       /* if we already have an error, don't overwrite it */
                       /* 64-bit:  if (instruction_count >= dbg_instruction_limit) */
                  {
                  errorcode = ERROR_BREAK;  /* exit outer loop, too! */
                  break;
                  }
               }
            else
               pc = starting_pc;  /* back to start of instruction, so we can see it */

            if ((execution_freerun) || (++instr_count >= instructions_per_execution_tick))
               break;
            }
         }
      if (sixty_hz_flag)
         {
         sixty_hz_flag = FALSE;
         if (sound_timer)
            {
            if (--sound_timer == 0)
               sound_off();
            }
         if (delay_timer)
            delay_timer--;
         if (key_state[ESC_KEY_SCANCODE])
            {
            errorcode = ERROR_EXIT;  /* not really an error, just convenient */
            }
         if (keyboard_int_flag)
            {
            keyboard_int_flag = FALSE;
            clear_keyboard_queue();
            }
              /* don't use these keypresses, just keep keyboard queue empty */
         }  /* sixty_hz_flag */
      }  /* while (errorcode == ERROR_NONE) */
   }

setup_keypad()
   {
   WORD i;

   if (use_alt_keypad)
      {
      for (i = 0; i < NUM_KEYS; i++)
         chip8_keys[i] = alt_keypad[i];
      }
   else
      {
      for (i = 0; i < NUM_KEYS; i++)
         chip8_keys[i] = keypad[i];
      }
   }

reset_run_regs()
   {
   WORD i;

   pc = program_load_addr;
   dbg_asm_pc = program_load_addr;
   dbg_disassemble_pc = program_load_addr;
   dbg_dump_addr = program_load_addr;
   sp = STACK_START;
   where_queue_index = 0;
   where_queue_entries = 0;
   sound_timer = 0;
   delay_timer = 0;
   for (i = 0; i < NUM_REGISTERS; i++)
      regs[i] = 0;
   }

setup_chip8()
   {
   WORD i;

   for (i = 0; i < HEX_SPRITES_NUM_BYTES; i++)
      memory[HEX_SPRITES_LOCATION + i] = hex_sprites[i];
   for (i = 0; i < HEX_SPRITES_SUPER_NUM_BYTES; i++)
      memory[HEX_SPRITES_SUPER_LOCATION + i] = hex_sprites_super[i];

   setup_keypad();

   srand(peekw(SYSTEM_CLOCK_TICK_SEG, SYSTEM_CLOCK_TICK_OFS));

   reset_run_regs();

   setup_sound((WORD)(TIMER_CLOCK_BASE / sound_frequency));
   sound_off();

   vesamode(VESA_GRAPHICS_640X480X256);
     /* Note: We want 640x480 for debug screen, and 256-color (8bits/pixel)
      *       is over 20 times faster than 16-color (4bits/pixel in bitplanes)
      */
   }

reset_system()
   {
   sound_off();

   videomode(TEXT_MODE);
   }

setup_sound(divisor)
   WORD divisor;
   {
#asm
   cli     ;DO NOT DISTURB!
   mov  al,0b6h             ;8253 control data:
                  ; 10.. .... = channel 2
                  ; ..11 .... = read/write LSB/MSB
                  ; .... 011. = generate square wave
                  ; .... ...0 = binary counting
   out   43h,al            ;set 8253 control port
   jmp   s_delay1          ;waste time
s_delay1:
   mov  ax,word [bp+4]     ;get divisor
   out   42h,al            ;set low byte of clock divisor
   jmp   s_delay2          ;waste time
s_delay2:
   mov   al,ah             ;get high byte
   out   42h,al            ;set high byte of divisor
   sti                     ;ints are OK again
#endasm
   }

sound_on()
   {
#asm
   in   al,061h    ;get speaker port
   or   al,3       ;turn on speaker
   out  061h,al    ;set speaker port
#endasm
   }

sound_off()
   {
#asm
   in   al,061h    ;get speaker port
   and  al,0fch    ;turn off speaker
   out  061h,al    ;set speaker port
#endasm
   }

takeover_keyboard_int()  /* includes INT 9 handler */
   {
#asm
   push ds                         ;save our DS
   mov  ax,ds                      ;get ds
   mov  word cs:dssave,ax          ;save DS for interrupt use
   xor  ax,ax                      ;get a 0
   mov  ds,ax                      ;address INT seg
   mov  si,word [24h]              ;get current int ofs
   mov  di,word [26h]              ;get current int seg
   mov  word cs:int9ofs,si         ;save ofs
   mov  word cs:int9seg,di         ;and seg
   mov  word [24h],offset int9here ;set new INT9 handler
   mov  word [26h],cs              ;and new INT9 seg
   pop  ds
   jmp  takeover_keyboard_int_end

dssave:  dw 0                      ;shared with INT 8 (timer) code
int9ofs: dw 0
int9seg: dw 0

int9here:
   pushf                           ;save flags
   push ax                         ;and regs
   push bx
   push ds
   mov  ds,word cs:dssave          ;get program's DS
   in   al,60h                     ;get keyboard port
   mov  bl,al                      ;copy key to addressing reg
   and  bx,7fh                     ;BX = index
   test al,80h                     ;highbit set (key release?)
   jz   kbd_int_key_down           ;no, skip this
   mov  byte key_state_[bx],0      ;this key not pressed
   jmp  kbd_int_continue           ;done for now
kbd_int_key_down:
   mov  byte keyboard_int_flag_,1  ;set flag: empty keyboard queue
   mov  byte key_state_[bx],1      ;this key is pressed
kbd_int_continue:
   pop  ds
   pop  bx
   pop  ax
   lcall word cs:int9ofs           ;do original INT 9
   iret                            ;and done

takeover_keyboard_int_end:
#endasm
   }

reset_keyboard_int()
   {
#asm
   push ds                         ;save our DS
   xor  ax,ax                      ;get a 0
   mov  ds,ax                      ;address INT seg
   mov  si,word cs:int9ofs         ;get ofs
   mov  di,word cs:int9seg         ;and seg
   mov  word [24h],si              ;save int ofs
   mov  word [26h],di              ;and int seg
   pop  ds
#endasm
   memfill(key_state,sizeof(key_state),0);  /* no leftover keypresses */
   }

takeover_clock_tick(divisor)  /* includes INT 8 handler */
   WORD divisor;
   {
#asm
   cli     ;DO NOT DISTURB!
   push ds                         ;save our DS
   xor  ax,ax                      ;get a 0
   mov  ds,ax                      ;address INT seg
   mov  si,word [20h]              ;get current int ofs
   mov  di,word [22h]              ;get current int seg
   mov  word cs:int8ofs,si         ;save ofs
   mov  word cs:int8seg,di         ;and seg
   mov  word [20h],offset int8here ;set new INT8 handler
   mov  word [22h],cs              ;and new INT89 seg
   pop  ds
   mov  al,36h            ;8253 control data:
               ; 00.. .... = channel 0
               ; ..11 .... = read/write LSB/MSB
               ; .... 011. = generate square wave
               ; .... ...0 = binary counting
   out  43h,al            ;set 8253 control port
   jmp  delay1              ;waste time
delay1:
   mov  ax,word [bp+4]      ;get divisor
   out  40h,al            ;set low byte of clock divisor
   jmp  delay2              ;waste time
delay2:
   mov  al,ah            ;get high byte
   out  40h,al            ;set high byte of divisor
   sti                      ;ints are OK again
   jmp  takeover_tick_end   ;skip data storage

int8ofs: dw 0
int8seg: dw 0

;
;  Interrupt handler follows
;
int8here:
   push ds                  ;save ds
   push ax                  ;and ax
   mov  ds,word cs:dssave   ;get our ds
   inc  word execution_ticks_counter_  ;count this tick
   mov  byte execution_tick_flag_,1    ;set tick flag
   dec  byte sixty_hz_count_           ;update 60Hz counter
   jnz  int8_60hz_not_done             ;not expired, skip this
   mov  al,byte sixty_hz_divisor_      ;get counter value
   mov  byte sixty_hz_count_,al        ;reset counter
   mov  byte sixty_hz_flag_,1          ;set flag for interpreter
int8_60hz_not_done:
   dec  byte system_clock_count_       ;update 18.2Hz counter
   jnz  int8_system_clock_not_done     ;not expired, skip this
   mov  al,byte system_clock_divisor_  ;get counter value
   mov  byte system_clock_count_,al    ;reset counter
   pushf                               ;save flags (simulate interrupt)
   lcall word cs:int8ofs               ;far CALL to original INT 8, DeSmet style
int8_system_clock_not_done:
   mov  al,20h              ;EOI signal
   out  20h,al              ;send EOI to 8259
   pop  ax                  ;restore ax
   pop  ds                  ;and ds
   iret                     ;and done
;
;  End of Interrupt Handler
;

takeover_tick_end:
#endasm
   }

reset_clock_tick()
   {
#asm
   push ds                         ;save our DS
   xor  ax,ax                      ;get a 0
   mov  ds,ax                      ;address INT seg
   mov  si,word cs:int8ofs         ;get ofs
   mov  di,word cs:int8seg         ;and seg
   mov  word [20h],si              ;save int ofs
   mov  word [22h],di              ;and int seg
   pop  ds
   mov  al,36h                     ;8253 control data:
               ; 00.. .... = channel 0
               ; ..11 .... = read/write LSB/MSB
               ; .... 011. = generate square wave
               ; .... ...0 = binary counting
   out  43h,al                     ;set 8253 control port
   jmp  delay10                    ;waste time
delay10:
   xor  ax,ax                      ;get divisor of 65536
   out  40h,al                     ;set low byte of clock divisor
   jmp  delay11                    ;waste time
delay11:
   out  40h,al                     ;set high byte of divisor
#endasm
   }

videomode(mode)
   int mode;
   {
#asm
   mov  ah,0               ;fn = set video mode
   mov  al,byte [bp+4]     ;get new mode
   int  10h                ;call BIOS
#endasm
   }

vesamode(mode)
   int mode;
   {
#asm
   mov  ax,4f02h           ;fn = set VESA video mode
   mov  bx,word [bp+4]     ;get video mode
   int  10h                ;call BIOS
#endasm
   }

scrollwindow(ul_row, ul_col, lr_row, lr_col, numrows)
   WORD ul_row, ul_col, lr_row, lr_col, numrows;
      /* bp+4    bp+6    bp+8    bp+10   bp+12 */
   {
#asm
   mov  ah,6               ;fn = scroll
   mov  al,byte [bp+12]    ;number of lines
   mov  bh,0               ;attribute = BLACK
   mov  ch,byte [bp+4]     ;UL row
   mov  cl,byte [bp+6]     ;UL col
   mov  dh,byte [bp+8]     ;LR row
   mov  dl,byte [bp+10]    ;LR col
   int  10h
#endasm
   }

pokew(seg,ofs,value)
   unsigned int seg,ofs,value;
   {
#asm
   mov  es,word [bp+4]  ;get seg
   mov  bx,word [bp+6]  ;get ofs
   mov  ax,word [bp+8]  ;get value
   mov  word es:[bx],ax ;poke the word
   push ds              ;current seg
   pop  es              ;to es
#endasm
   }

unsigned int peekw(seg,ofs)
   unsigned int seg,ofs;
   {
#asm
   mov  es,word [bp+4]  ;get seg
   mov  bx,word [bp+6]  ;get ofs
   mov  ax,word es:[bx] ;get value
   push ds              ;current seg
   pop  es              ;to es
#endasm
   }

memmove(src, dest, size)
   BYTE *src, *dest;
   WORD size;
   {
#asm
   mov  si,word [bp+4]            ;get source addr
   mov  di,word [bp+6]            ;get dest addr
   mov  cx,word [bp+8]            ;get bytes to move
   cld                            ;auto-inc string primitives
   shr  cx,1                      ;to words, CY flag is "odd byte count"
   db   0f3h                      ;REPZ, DeSmet doesn't do it right
   movsw                          ;move memory
   jnc memmove_done               ;if no "odd byte", skip this
   movsb                          ;move the last byte
memmove_done:
#endasm
   }

memfill(dest, size, value)
   BYTE *dest;
   WORD size;
   BYTE value;
   {
#asm
   mov  di,word [bp+4]            ;get dest addr
   mov  cx,word [bp+6]            ;get byte count
   mov  al,byte [bp+8]            ;get value
   mov  ah,al                     ;make it a WORD
   cld                            ;auto-inc string primitives
   shr  cx,1                      ;byte count to WORDS, CY flag is "odd byte count"
   db   0f3h                      ;REPZ, DeSmet doesn't do it right
   stosw                          ;fill memory
   jnc  memfill_done              ;if no "odd byte", skip this
   stosb                          ;fill the last byte
memfill_done:
#endasm
   }

clear_keyboard_queue()
   {
#asm
clear_keyboard_queue_loop:
   mov  ax,1100h     ;fn = check extended keypress
   int  16h          ;call BIOS
   jz   clear_keyboard_queue_done  ;nope, we're done
   mov  ax,1000h     ;fn = get extended keypress
   int  16h          ;call BIOS
   jmp  clear_keyboard_queue_loop  ;more keys, keep going
clear_keyboard_queue_done:
#endasm
   }

WORD keytest()
   {
#asm
   mov  ax,1100h    ;fn = check extended keypress
   int  16h         ;call BIOS
   mov  ax,0        ;default: no keypress waiting
   jz   keytest_end ;no keys, skip this
   inc  ax          ;keypress waiting
keytest_end:
#endasm
   }

WORD getkey()
   {
#asm
   mov  ax,1000h     ;fn = get extended keypress
   int  16h          ;call BIOS
   cmp  al,0         ;fnkey, etc?
   jz   getkey_fnkey ;yes, skip this
   mov  ah,0         ;ignore scancode
   jmp  getkey_end   ;and done
getkey_fnkey:
   mov  al,ah        ;set scancode = "key"
   mov  ah,1         ;set extended keypress flag
getkey_end:
#endasm
   }


WORD upcase(ch)
   WORD ch;
       /* since getkey() can return values > 0x7f, use this to get
        * upper case.
        */
   {
   if ((ch >= 'a') && (ch <= 'z'))
      return(ch + 'A' - 'a');
   return(ch);
   }

BYTE anykey()
   {
   WORD i;

   for (i = 0; i < NUM_KEYS; i++)
      if (key_state[chip8_keys[i]])
         return(i);
   return(0xff);
   }

scroll_screen_down()
      /* Note: We know the screen always has an even number of ROWS and
       *       COLUMNS, so we can do WORD moves without worry about
       *       an odd number of bytes.
       */
   {
   if (super_chip8_video)  /* 128 columns, 64 rows */
      {
#asm
   mov  di,offset video_memory_[8190]    ;ES:DI pointer to end of video memory
   mov  si,di            ;and a copy to SI
   xor  ax,ax            ;clear ax
   mov  ah,byte opcode_n_ ;ax = lines to move * 256
   shr  ax,1             ;/2 for lines to move * 128 (128 == bytes per line)
   mov  cx,8192          ;video memory size
   sub  cx,ax            ;bytes to move
   shr  cx,1             ;words to move
   sub  si,ax            ;DS:SI = end of video memory to move
   std                   ;auto-dec string primitives
   db   0f3h             ;REP
   movsw                 ;move video memory down
   mov  cx,ax            ;number of bytes that were moved
   shr  cx,1             ;number of words moved == number of words to fill
   mov  al,byte black_   ;get fill color
   mov  ah,al            ;as a WORD
   db   0f3h             ;REP
   stosw                 ;erase lines that were moved down
#endasm
      }
   else  /* 64 columns, 32 rows */
      {
      if (opcode_n == 1)
         return;
          /* we must move a full line (no 1/2 lines).  If we are only
           * scolling 1 line, 1/2 of that is a "null move", do nothing.
           *
           * I am not going to do like some CHIP-8 interpreters and move
           * 1/2 a line (i.e. start in the middle of a line)
           */
#asm
   mov  di,offset video_memory_[2046]    ;ES:DI pointer to end of video memory
   mov  si,di              ;and a copy to SI
   xor  ax,ax              ;clear ax
   mov  ah,byte opcode_n_  ;ax = lines to move * 256
   shr  ax,1               ;/2 = lines to move * 128
   shr  ax,1               ;/2 = lines to move * 64
   shr  ax,1               ;/2 = lines to move * 32
                           ;== (lines to move / 2) * bytes per line
                           ;1/2 the sceen resolution, 1/2 the lines
   mov  cx,2048            ;video memory size
   sub  cx,ax              ;bytes to move
   shr  cx,1               ;words to move
   sub  si,ax              ;DS:SI = end of video memory to move
   std                     ;auto-dec string primitives
   db   0f3h               ;REP
   movsw                   ;move video memory down
   mov  cx,ax              ;number of bytes that were moved
   shr  cx,1               ;number of words moved == number of words to fill
   mov  al,byte black_     ;get fill color
   mov  ah,al              ;as a WORD
   db   0f3h               ;REP
   stosw                   ;erase lines that were moved down
#endasm
      }
   }

update_screen()
    /* Note: We know the screen always has an even number of ROWS and
     *       COLUMNS, so we can do WORD moves without worrying about
     *       an odd number of bytes.
     */
    /* Note: We know the window size and granularity size are 64K, so I'm
     *       just going to use that without bothering to call VESA function
     *       4F01 to get window size and granularity size and set variables...
     */
   {
#asm
   push es        ;save ES (just in case)
   mov  ax,4f05h  ;VESA memory control
   xor  bx,bx     ;BH = 0, select video memory window.  BL=0, page A
   xor  dx,dx     ;window address 0 (granularity units, 64K each)
   int  10h       ;call BIOS
   pop  es        ;restore ES
#endasm
   if (super_chip8_video)  /* 128 columns, 64 rows */
      {
       /* We want enough "free" screen space for debugging info, so
        * 64 rows * (2 x 2 block is one pixel) take 128 rows, and add 2
        * rows of blank space so 160 pixels out of 480 are used.
        */
#asm
   push es                           ;save current es
   mov  bx,offset video_memory_      ;bx = video data
   mov  es,word video_seg_           ;get target video seg
   xor  di,di                        ;es:di = video memory
   mov  dl,64                        ;video lines
   cld                               ;auto-inc string primitives
update_screen_super_1:
   mov  dh,2                         ;pixels are 2 x 2, line count
update_screen_super_2:
   mov  si,bx                        ;ds:si = video data
   mov  cx,128                       ;bytes per line
update_screen_super_3:
   lodsb                             ;get video data
   stosb                             ;target pixel is 2 x 2
   stosb
   loop  update_screen_super_3       ;do entire row
   or    di,di                       ;at end of 64K segment?
              ;Note: 640 * 102 + 256 == 65536, wrap 16-bit register to 0
   jnz   update_screen_super_4       ;NZ, not time for next window
   push  es                          ;save regs
   push  ax
   push  bx
   push  dx
   mov   ax,4f05h                    ;fn = VESA memory control
   xor   bx,bx                       ;BH = 0, select video memory window.  BL=0, page A
   mov   dx,1                        ;window address 1 (granularity units, 64K each)
   int   10h                         ;call BIOS
   pop   dx                          ;restore regs
   pop   bx
   pop   ax
   pop   es
update_screen_super_4:
   add   di,640-256                  ;point to next line in video seg
   dec   dh                          ;count this pixel line
   jnz   update_screen_super_2       ;do next line of 2 x 2 pixel
   add   bx,128                      ;offset of next line of video data
   dec   dl                          ;count this line
   jnz   update_screen_super_1       ;do all lines
   pop   es                          ;restore old es
#endasm
      }
   else  /* 64 columns, 32 rows */
      {
       /* We want enough "free" screen space for debugging info, so
        * 32 rows * (4 x 4 block is one pixel) take 128 rows, and add 2
        * rows of blank space so 160 pixels out of 480 are used.
        */
#asm
   push es                           ;save current es
   mov  bx,offset video_memory_      ;bx = video data
   mov  es,word video_seg_           ;get target video seg
   xor  di,di                        ;es:di = video memory
   mov  dl,32                        ;video lines
   cld                               ;auto-inc string primitives
update_screen_chip8_1:
   mov  dh,4                         ;pixels are 4 x 4, line count
update_screen_chip8_2:
   mov  si,bx                        ;ds:si = video data
   mov  cx,64                        ;bytes per line
update_screen_chip8_3:
   lodsb                             ;get video data
   mov  ah,al                        ;as a WORD
   stosw                             ;target pixel is 4 x 4
   stosw
   loop  update_screen_chip8_3       ;do entire row
   or    di,di                       ;at end of 64K segment?
              ;Note: 640 * 102 + 256 == 65536, wrap 16-bit register to 0
   jnz   update_screen_chip8_4       ;NZ, not time for next window
   push  es                          ;save regs
   push  ax
   push  bx
   push  dx
   mov   ax,4f05h                    ;fn = VESA memory control
   xor   bx,bx                       ;BH = 0, select video memory window.  BL=0, page A
   mov   dx,1                        ;window address 1 (granularity units, 64K each)
   int   10h                         ;call BIOS
   pop   dx                          ;restore regs
   pop   bx
   pop   ax
   pop   es
update_screen_chip8_4:
   add   di,640-256                  ;point to next line in video seg
   dec   dh                          ;count this pixel line
   jnz   update_screen_chip8_2       ;do next line of 4 x 4 pixel
   add   bx,64                       ;point to next line in video memory
   dec   dl                          ;count this line
   jnz   update_screen_chip8_1       ;do all lines
   pop   es                          ;restore old es
#endasm
      }
   }

scroll_screen_right()
   {
   if (super_chip8_video)  /* 128 columns, 64 rows */
      {
    /* for all rows,
     *  c0..c3=BLACK, c4..c127 = old_c0..old_c123
     */
#asm
   mov  bx,offset video_memory_[127]  ;destination (row 0)
   mov  dl,64                         ;rows to process
   std                                ;auto-dec string primitives
   mov  al,byte black_                ;get fill color
   mov  ah,al                         ;as a WORD
scroll_right_super:
   mov  di,bx                         ;es:di = destination
   lea  si,[di-4]                     ;ds:si = source
   mov  cx,62                         ;words to move ((128-4)/2)
   db   0f3h                          ;REPZ
   movsw                              ;move video data
   stosw                              ;fill first 4 bytes
   stosw
   add  bx,128                        ;offset in next rown
   dec  dl                            ;count this row
   jnz  scroll_right_super            ;do all rows
#endasm
      }
   else  /* 64 columns, 32 rows */
      {
    /* for all rows,
     *  c0..c1=BLACK, c2..c63 = old_c0..old_c61
     */
#asm
   mov  bx,offset video_memory_[63]   ;destination (row 0)
   mov  dl,32                         ;rows to process
   std                                ;auto-dec string primitives
   mov  al,byte black_                ;get fill color
   mov  ah,al                         ;as a WORD
scroll_right_chip8:
   mov  di,bx                         ;es:di = destination
   lea  si,[di-2]                     ;ds:si = source
   mov  cx,31                         ;words to move ((64-2)/2)
   db   0f3h                          ;REPZ
   movsw                              ;move video data
   stosw                              ;fill first 2 bytes
   add  bx,64                         ;offset in next row
   dec  dl                            ;count this row
   jnz  scroll_right_chip8            ;do all rows
#endasm
      }
   }

scroll_screen_left()
   {
   if (super_chip8_video)  /* 128 columns, 64 rows */
      {
    /* for all rows,
     *  c0 .. c123 --> old_c4..old_c123, c124..c127=BLACK
     */
#asm
   mov  bx,offset video_memory_       ;destination (row 0)
   mov  dl,64                         ;rows to process
   cld                                ;auto-inc string primitives
   mov  al,byte black_                ;get fill color
   mov  ah,al                         ;as a WORD
scroll_left_super:
   mov  di,bx                         ;es:di = destination
   lea  si,[di+4]                     ;ds:si = source
   mov  cx,62                         ;words to move ((128-4)/2)
   db   0f3h                          ;REPZ
   movsw                              ;move video data
   stosw                              ;fill last 4 bytes
   stosw
   add  bx,128                        ;offset in next row
   dec  dl                            ;count this row
   jnz  scroll_left_super             ;do all rows
#endasm
      }
   else  /* 64 columns, 32 rows */
      {
    /* for all rows,
     *  c0 .. c61 --> old_c2..old_c63, c62..c63=BLACK
     */
#asm
   mov  bx,offset video_memory_       ;destination (row 0)
   mov  dx,32                         ;rows to process
   cld                                ;auto-inc string primitives
   mov  al,byte black_                ;get fill color
   mov  ah,al                         ;as a WORD
scroll_left_chip8:
   mov  di,bx                         ;es:di = destination
   lea  si,[di+2]                     ;ds:si = source
   mov  cx,31                         ;words to move ((128-4)/2)
   db   0f3h                          ;REPZ
   movsw                              ;move video data
   stosw                              ;fill last 2 bytes
   add  bx,64                         ;offset in next row
   dec  dl                            ;count this row
   jnz  scroll_left_chip8             ;do all rows
#endasm
      }
   }

curpos(r,c)
   int r,c;
   {
#asm
   mov  ah,2            ;fn = set cursor position
   mov  bx,0            ;page 0
   mov  dh,byte [bp+4]  ;get row
   mov  dl,byte [bp+6]  ;get col
   int  10h
#endasm
   }

pixel(r,c,color)
   WORD r,c,color;
   {
#asm
   mov  ah,0ch
   mov  al,byte [bp+8]
   mov  bx,0
   mov  cx,word [bp+6]
   mov  dx,word [bp+4]
   int  10h
#endasm
   }

int qword_ge(first,second)   /* 64-bit: TRUE if first >= second, else FALSE */
   WORD *first, *second;
       /* BP+4   BP+6 */
   {
#asm
   mov  si,word [bp+4]   ;get ptr to 64-bit long (stored little-endian)
   mov  di,word [bp+6]   ;ptr to 2nd 64-bit long
   mov  ax,word [si]
   sub  ax,word [di]
   mov  ax,word [si+2]
   sbb  ax,word [di+2]
   mov  ax,word [si+4]
   sbb  ax,word [di+4]
   mov  ax,word [si+6]
   sbb  ax,word [di+6]
   mov  ax,1             ;default to TRUE
   jae  qword_ge_done    ;if NC, that's all
   dec  ax               ;set FALSE
qword_ge_done:
#endasm
   }

/* write what should be a library routine */
WORD strcmpi(str1,str2)
   BYTE *str1, *str2;
   {
   while ((*str1) && (*str2))
      {
      if (toupper(*str1) != toupper(*str2))
         return(1);  /* Oops! */
      str1++;
      str2++;
      }
   if (*str1 == *str2)  /* make sure both are at EOS */
      return(0);
   return(1);
   }

help()
   {
   puts("DEBUG8 [options] [chip8_program]\n");
   puts("Options:\n");
   puts("   -K  : Use Alternate keypad (1..4/q..r/a..f/z..v) instead of PC keypad.\n");
   puts("   -F  : execution Freerun (run code as fast as possible)\n");
   puts("   -V  : Default High-res video (128 x 64) instead of standard (64 x 32)\n");
   puts("   -Tn : Execution ticks (2040 ticks/second) to wait before executing\n");
   puts("         CHIP8 instructions.  Default is 1.\n");
   puts("   -In : CHIP8 instructions to execute every time execution tick timer\n");
   puts("         expires.  Default is 1.\n");
   puts("   -Sn : Sound frequency.  Default is 330Hz.  Range is 100 to 1000.\n");
   puts("   -Ln : Load address for program.  Valid values are 512 and 1536.\n");
   puts("         Default value is 512 (0x0200).\n");
   puts("   -W  : Wrap sprites that go beyond the right edge of the screen.\n");
   puts("         Default is to truncate sprites at the right edge of the screen.\n");
   puts("   -M  : Use Mikolay's instruction behaviors (equivalent to using both\n");
   puts("         -X and -Y options).\n");
   puts("   -X  : SHR Vx,Vy and SHL Vx,Vy -- shift Vy register and store it in Vx\n");
   puts("         Default is that Vx is shifted.\n");
   puts("   -Y  : LD [I],Vx and LD Vx,[I] instructions DO update the I register.\n");
   puts("         Default is that they DO NOT update the I register.\n");
   puts("   -Z  : ADD I,Vx DOES sets carry in VF\n");
   puts("         Default is that carry is NOT set in VF.\n");
   exit(10);
   }
