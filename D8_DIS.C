#include "debug8.h"

WORD d_opcode;  /* full opcode, will be used in pieces as needed */
BYTE d_opcode_high;  /* MSB */
BYTE d_opcode_low;   /* LSB, also 8-bit constant value */
BYTE d_opcode_str[32];  /* too big, but who cares? */

#define X_REG_FIELD   d_opcode_high & 0x0F
#define Y_REG_FIELD   d_opcode_low >> 4


void disassemble_system()  /* 0... */
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
   if ((d_opcode & 0xFF0) == 0x0C0)  /* SCD */
      {
      sprintf(d_opcode_str,"SCD   %x",d_opcode_low & 0x0F);
      }
   else
      {
      switch (d_opcode & 0xFFF)
         {
         case 0xE0:   /* CLS */
            strcpy(d_opcode_str,"CLS");
            break;
         case 0xEE:  /* RET */
            strcpy(d_opcode_str,"RET");
            break;
         case 0xFB:  /* SCR */
            strcpy(d_opcode_str,"SCR");
            break;
         case 0xFC:  /* SCL */
            strcpy(d_opcode_str,"SCL");
            break;
         case 0xFD:
            strcpy(d_opcode_str,"EXIT");
            break;
         case 0xFE:
            strcpy(d_opcode_str,"LOW");
            break;
         case 0xFF:
            strcpy(d_opcode_str,"HIGH");
            break;
         default:
/*            sprintf(d_opcode_str,"<unknown>  ;%02x%02x",d_opcode_high,d_opcode_low); */
            strcpy(d_opcode_str,"<unknown>");
            break;
         }
      }
   }

void disassemble_jmp()  /* 1... */
   {
   sprintf(d_opcode_str,"JP    %03x",d_opcode & ADDRESS_MASK);
   }

void disassemble_call()  /* 2... */
   {
   sprintf(d_opcode_str,"CALL  %03x",d_opcode & ADDRESS_MASK);
   }

void disassemble_skipeq()  /* 3xkk - SE Vx, byte    Skip next instr if Vx == KK */
   {
   sprintf(d_opcode_str,"SE    V%x, %02x",X_REG_FIELD,d_opcode_low);
   }

void disassemble_skipne()  /* 4xkk - SNE Vx, byte   Skip next instr if Vx != KK */
   {
   sprintf(d_opcode_str,"SNE   V%x, %02x",X_REG_FIELD,d_opcode_low);
   }

void disassemble_skipregeq()  /* 5xy0 - SE Vx, Vy      Skip next instr if Vx == Vy */
   {
   sprintf(d_opcode_str,"SE    V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_movkk()  /* 6xkk - LD Vx, byte    Vx = KK */
   {
   sprintf(d_opcode_str,"LD    V%x, %02x",X_REG_FIELD,d_opcode_low);
   }

void disassemble_addkk()  /* 7xkk - ADD Vx, byte   Vx += KK */
   {
   sprintf(d_opcode_str,"ADD   V%x, %02x",X_REG_FIELD,d_opcode_low);
   }

void disassemble_mov()  /* 8xy0 - LD Vx, Vy      Vx = Vy */
   {
   sprintf(d_opcode_str,"LD    V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_or()  /* 8xy1 - OR Vx, Vy      Vx |= Vy */
   {
   sprintf(d_opcode_str,"OR    V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_and()  /* 8xy2 - AND Vx, Vy     Vx &= Vy */
   {
   sprintf(d_opcode_str,"AND   V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_xor()  /* 8xy3 - XOR Vx, Vy     Vx ^= Vy */
   {
   sprintf(d_opcode_str,"XOR   V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_addreg()  /* 8xy4 - ADD Vx, Vy     Vx += Vy, VF set to 1 if result > 0xff, else 0 */
   {
   sprintf(d_opcode_str,"ADD   V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_subreg()  /* 8xy5 - SUB Vx, Vy     Vx -= Vy, VF set to 1 if Vx >= Vy, else 0 */
   {
   sprintf(d_opcode_str,"SUB   V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_shr()  /* 8xy6 - SHR Vx, 1      Vx >>= 1, VF set to 1 if carry, else 0 */
   {
   sprintf(d_opcode_str,"SHR   V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_revsubreg()  /* 8xy7 - SUBN Vx, Vy    Vx = Vy - Vx, VF set to 1 if Vy >= Vx, else 0 */
   {
   sprintf(d_opcode_str,"SUBN  V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_shl()  /* 8xyE - SHL Vx, 1      Vx <<= 1, VF set to 1 if carry, else 0 */
   {
   sprintf(d_opcode_str,"SHL   V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_bad_opcode()  /* 8..z */
   {
/*   sprintf(d_opcode_str,"<unknown>  ;%02x%02x",d_opcode_high,d_opcode_low); */
   strcpy(d_opcode_str,"<unknown>");
   }

void disassemble_skipregne()  /* 9xy0 - SNE Vx, Vy     skip next instr if Vx != Vy */
   {
   sprintf(d_opcode_str,"SNE   V%x, V%x",X_REG_FIELD,Y_REG_FIELD);
   }

void disassemble_movi()  /* Annn - LD I, addr     set I to NNN */
   {
   sprintf(d_opcode_str,"LD    I, %03x",d_opcode & ADDRESS_MASK);
   }

void disassemble_jmpv0()  /* Bnnn - JP V0, addr    JMP to NNN + V0 */
   {
   sprintf(d_opcode_str,"JP    V0, %03x",d_opcode & ADDRESS_MASK);
   }

void disassemble_rand()  /* Cxkk - RND Vx, byte   Vx = <random> & KK */
   {
   sprintf(d_opcode_str,"RND   V%x, %02x",X_REG_FIELD,d_opcode_low);
   }

void disassemble_draw()  /* Dxyn - DRW Vx, Vy, nibble  draw sprite at I at
                    * col=VX, row=VY for <nibble> bytes.
                    * VF set to 1 if screen pixels changed, else 0
                    */
   {
   sprintf(d_opcode_str,"DRW   V%x, V%x, %x",X_REG_FIELD,Y_REG_FIELD,d_opcode_low & 0x0F);
   }

void disassemble_keys()  /* Ex9E - SKP Vx         skip next instr if key stored in Vx is pressed
                          * ExA1 - SKNP Vx        skip next instr if key stored in Vx is not pressed
                          */
   {
   if (d_opcode_low == 0x9E)  /* skip if key Vx pressed */
      {
      sprintf(d_opcode_str,"SKP   V%x",X_REG_FIELD);
      }
   else if (d_opcode_low == 0xA1)  /* skip if key Vx not pressed */
      {
      sprintf(d_opcode_str,"SKNP  V%x",X_REG_FIELD);
      }
   else
/*      sprintf(d_opcode_str,"<unknown>  ;%02x%02x",d_opcode_high,d_opcode_low); */
      strcpy(d_opcode_str,"<unknown>");
   }

void disassemble_memory()  /* mostly memory... */
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
 */
   {
   int temp;

   switch (d_opcode_low)
      {
      case 0x07:  /* Fx07 - LD Vx, DT      Vx = Delay_Timer */
         sprintf(d_opcode_str,"LD    V%x, DT",X_REG_FIELD);
         break;
      case 0x0A:  /* Fx0A - LD Vx, K       Wait for key, then Vx = key */
         sprintf(d_opcode_str,"LD    V%x, K",X_REG_FIELD);
         break;
      case 0x15:  /* Fx15 - LD DT, Vx      Delay_Timer = Vx */
         sprintf(d_opcode_str,"LD    DT, V%x",X_REG_FIELD);
         break;
      case 0x18:  /* Fx18 - LD ST, Vx      Sound_timer = Vx */
         sprintf(d_opcode_str,"LD    ST, V%x",X_REG_FIELD);
         break;
      case 0x1e:  /* Fx1E - ADD I, Vx      I += Vx */
         sprintf(d_opcode_str,"ADD   I, V%x",X_REG_FIELD);
         break;
      case 0x29:  /* Fx29 - LD F, Vx       I = &system_sprite[Vx] */
         sprintf(d_opcode_str,"LD    F, V%x",X_REG_FIELD);
         break;
      case 0x30:  /* Fx30 - LD HF, Vx      I = &highres_system_sprite[Vx] */
         sprintf(d_opcode_str,"LD    HF, V%x",X_REG_FIELD);
         break;
      case 0x33:  /* Fx33 - LD B, Vx       I[0] = BCD_HIGH(Vx), I[1] = BCD_MID(Vx), I[2] = BCD_LOW(Vx) */
         sprintf(d_opcode_str,"LD    B, V%x",X_REG_FIELD);
         break;
      case 0x55:  /* Fx55 - LD [I], Vx     Save V0 .. Vx into memory at I .. I+x */
         sprintf(d_opcode_str,"LD    [I], V%x",X_REG_FIELD);
         break;
      case 0x65:  /* Fx65 - LD Vx, [I]     Load V0 .. Vx from memory at I .. I+x */
         sprintf(d_opcode_str,"LD    V%x, [I]",X_REG_FIELD);
         break;
      case 0x75:  /* Fx75 - LD R, Vx       Save V0 .. Vx (x<8) to HP48 flags */
         sprintf(d_opcode_str,"LD    R, V%x",X_REG_FIELD);
         break;
      case 0x85:  /* Fx85 - LD Vx, R       Load V0 .. Vx (x<8) from HP48 flags */
         sprintf(d_opcode_str,"LD    V%x, R",X_REG_FIELD);
         break;
      default:
/*         sprintf(d_opcode_str,"<unknown>  ;%02x%02x",d_opcode_high,d_opcode_low); */
         strcpy(d_opcode_str,"<unknown>");
         break;
      }
   }

void (*math_disassemble[16])() =
   {
   disassemble_mov,          /* ...0 */
   disassemble_or,           /* ...1 */
   disassemble_and,          /* ...2 */
   disassemble_xor,          /* ...3 */
   disassemble_addreg,       /* ...4 */
   disassemble_subreg,       /* ...5 */
   disassemble_shr,          /* ...6 */
   disassemble_revsubreg,    /* ...7 */
   disassemble_bad_opcode,   /* ...8 */
   disassemble_bad_opcode,   /* ...9 */
   disassemble_bad_opcode,   /* ...A */
   disassemble_bad_opcode,   /* ...B */
   disassemble_bad_opcode,   /* ...C */
   disassemble_bad_opcode,   /* ...D */
   disassemble_shl,          /* ...E */
   disassemble_bad_opcode    /* ...F */
   };

void disassemble_math()  /* 8... */
   {
   (*math_disassemble[d_opcode_low & 0x0f])();
   }

void (*primary_disassemble[16])() =
   {
   disassemble_system,    /* 0... */
   disassemble_jmp,       /* 1... */
   disassemble_call,      /* 2... */
   disassemble_skipeq,    /* 3... */
   disassemble_skipne,    /* 4... */
   disassemble_skipregeq, /* 5... */
   disassemble_movkk,     /* 6... */
   disassemble_addkk,     /* 7... */
   disassemble_math,      /* 8... */
   disassemble_skipregne, /* 9... */
   disassemble_movi,      /* A... */
   disassemble_jmpv0,     /* B... */
   disassemble_rand,      /* C... */
   disassemble_draw,      /* D... */
   disassemble_keys,      /* E... */
   disassemble_memory     /* F... */  /* mostly memory... */
   };

void disassemble(ip, d)
   WORD ip;
   BYTE *d;
   {
   WORD i;

   d_opcode_str[0] = 0;  /* start with null string */
   *((BYTE *)&d_opcode + 1) = d_opcode_high = memory[ip++];
      /* 1st CHIP-8 instr byte is high byte, to high-byte of x86 var */
   *(BYTE *)&d_opcode = d_opcode_low = memory[ip++];
   (*primary_disassemble[d_opcode_high >> 4])();
   for (i = strlen(d_opcode_str); i < 16; i++)
      strcat(d_opcode_str," ");
   sprintf(&d_opcode_str[16],";%02x%02x",d_opcode_high,d_opcode_low);
   strcpy(d,d_opcode_str);
   }
