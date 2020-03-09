#include "debug8.h"

BYTE asm_data_len;  /* len of data in asm_data[] */
WORD asm_data[STRING_SIZE+1];  /* big enough for DB / DW, not just 2-byte instruction */
BYTE asm_input_index;
BYTE asm_input[STRING_SIZE+1];
BYTE asm_token_len;
BYTE asm_token[MAX_TOKEN_LEN+1];
   /* LEN+1 so we have room for EOS */
BYTE asm_syntax_error;
WORD asm_base_opcode;
WORD asm_ip;

extern WORD program_load_addr;  /* variable, for ETI-660 support */

#define DB_DW_FLAG_VALUE   0xffff

BYTE asm_is_end_of_token(ch)
   BYTE ch;
   {
   if ((ch == EOS) || (ch == CR) || (ch == LF) || (ch == SPACE) || (ch == TAB)
                   || (ch == COMMA) || (ch == EOS))
      return(TRUE);
   return(FALSE);
   }

void asm_skip_input_whitespace()
   {
   while ((asm_input_index < STRING_SIZE)
              && ((asm_input[asm_input_index] == SPACE)
                      || (asm_input[asm_input_index] == TAB)
                 )
         )
      asm_input_index++;
   }

void asm_get_token()
   {
   asm_skip_input_whitespace();
   for (asm_token_len = 0;
             (asm_token_len < MAX_TOKEN_LEN)
                 && (!asm_is_end_of_token(asm_input[asm_input_index]))
                 && (asm_input_index < STRING_SIZE);
             asm_token_len++, asm_input_index++)
      asm_token[asm_token_len] = asm_input[asm_input_index];
   asm_token[asm_token_len] = EOS;
   }

BYTE asm_skip_comma()
   {
   asm_skip_input_whitespace();
   if ((asm_input_index < STRING_SIZE)
          && (asm_input[asm_input_index] == COMMA))
      {
      asm_input_index++;
      return(TRUE);
      }
   return(FALSE);
   }

BYTE fromhex(ch)
   BYTE ch;
   {
   if (ch > '9')
      return(10 + toupper(ch) - 'A');
   return(ch - '0');
   }

BYTE asm_is_regname()
   {
   if ((asm_token_len == 2)
            && (toupper(asm_token[0]) == 'V')
            && (ishexdigit(asm_token[1]))
      )
      return(TRUE);
   return(FALSE);
   }

BYTE asm_get_regnum()
   {
   BYTE regnum = NUM_REGISTERS + 1;  /* default invalid */

   asm_get_token();
   if (asm_is_regname())
      {
      regnum = fromhex(asm_token[1]);
      }
   return(regnum);
   }

BYTE asm_parse_memory(addr)
   WORD *addr;
   {
   WORD i;

   if ((!asm_token_len) || (asm_token_len > 3))
      return(FALSE);
   for (*addr = 0, i = 0; i < asm_token_len; i++)
      {
      if (!ishexdigit(asm_token[i]))
         return(FALSE);
      *addr = (*addr << 4) | fromhex(asm_token[i]);
      }
   if (*addr >= MEM_SIZE-1)
      return(FALSE);
   return(TRUE);
   }

BYTE asm_parse_address(addr)
   WORD *addr;
   {
   WORD i;

   if ((!asm_token_len) || (asm_token_len > 3))
      return(FALSE);
   for (*addr = 0, i = 0; i < asm_token_len; i++)
      {
      if (!ishexdigit(asm_token[i]))
         return(FALSE);
      *addr = (*addr << 4) | fromhex(asm_token[i]);
      }
   if ((*addr < program_load_addr) || (*addr >= MEM_SIZE-1))
      return(FALSE);  /* MEM_SIZE-1 since instructions are 2 bytes long */
   if (*addr & 1)  /* instructions are 2 bytes so addresses must be EVEN */
      return(FALSE);
   return(TRUE);
   }

BYTE asm_is_byte(val)
   WORD *val;
   {
   WORD i;

   if ((!asm_token_len) || (asm_token_len > 2))
      return(FALSE);
   for (*val = 0, i = 0; i < asm_token_len; i++)
      {
      if (!ishexdigit(asm_token[i]))
         return(FALSE);
      *val = (*val << 4) | fromhex(asm_token[i]);
      }
   return(TRUE);
   }

BYTE asm_scd()
   {
   asm_get_token();
   if ((asm_token_len == 1) && (ishexdigit(asm_token[0])))
      {
      asm_base_opcode |= fromhex(asm_token[0]);
      return(TRUE);
      }
   return(FALSE);
   }

BYTE asm_done()
   {
   return(TRUE);
   }

BYTE asm_jmp()  /* need to distinguish JP xxx from JP V0,xxx */
   {
   WORD addr;
   BYTE result;

   asm_get_token();
   if (strcmpi(asm_token,"V0") == 0)  /* indexed jump */
      {
      if (!asm_skip_comma())
         return(FALSE);
      asm_base_opcode = 0xb000;
      asm_get_token();  /* token is now dest address */
      }
   result = asm_parse_address(&addr);
   asm_base_opcode |= addr;
   return(result);
   }

BYTE asm_call()
   {
   WORD addr;
   BYTE result;

   asm_get_token();
   result = asm_parse_address(&addr);
   asm_base_opcode |= addr;
   return(result);
   }

BYTE asm_se()  /* need to distinguish SE VX,byte from SE VX,VY */
   {
   BYTE xreg, yreg;

   if ((xreg = asm_get_regnum()) > NUM_REGISTERS)
      return(FALSE);
   if (!asm_skip_comma())
      return(FALSE);
   if ((yreg = asm_get_regnum()) > NUM_REGISTERS)
      {
      if (!asm_is_byte(&yreg))
         return(FALSE);
      asm_base_opcode = 0x3000 | (xreg << 8) | yreg;
         /* asm_se() and asm_sne() could be one routine except for this value */
      return(TRUE);
      }
   asm_base_opcode |= (xreg << 8) | (yreg << 4);
   return(TRUE);
   }

BYTE asm_sne()  /* need to distinguish SNE VX,byte from SNE VX,VY */
   {
   BYTE xreg, yreg;

   if ((xreg = asm_get_regnum()) > NUM_REGISTERS)
      return(FALSE);
   if (!asm_skip_comma())
      return(FALSE);
   if ((yreg = asm_get_regnum()) > NUM_REGISTERS)
      {
      if (!asm_is_byte(&yreg))
         return(FALSE);
      asm_base_opcode = 0x4000 | (xreg << 8) | yreg;
         /* asm_se() and asm_sne() could be one routine except for this value */
      return(TRUE);
      }
   asm_base_opcode |= (xreg << 8) | (yreg << 4);
   return(TRUE);
   }

BYTE asm_ld()  /* there are multiple LD instructions */
/*  In instruction order,
 *   6xkk - LD Vx, byte    Vx = KK
 *   8xy0 - LD Vx, Vy      Vx = Vy
 *   Annn - LD I, addr     set I to NNN
 *   Fx07 - LD Vx, DT      Vx = Delay_Timer
 *   Fx0A - LD Vx, K       Wait for key, then Vx = key
 *   Fx15 - LD DT, Vx      Delay_Timer = Vx
 *   Fx18 - LD ST, Vx      Sound_timer = Vx
 *   Fx29 - LD F, Vx       I = &system_sprite[Vx]
 *   Fx30 - LD HF, Vx      I = &highres_system_sprite[Vx]
 *   Fx33 - LD B, Vx       I[0] = BCD_HIGH(Vx), I[1] = BCD_MID(Vx), I[2] = BCD_LOW(Vx)
 *   Fx55 - LD [I], Vx     Save V0 .. Vx into memory at I .. I+x
 *   Fx65 - LD Vx, [I]     Load V0 .. Vx from memory at I .. I+x
 *   Fx75 - LD R, Vx       Save V0 .. Vx (x<8) to HP48 flags
 *   Fx85 - LD Vx, R       Load V0 .. Vx (x<8) from HP48 flags
 *
 *  Let's change this around
 *   8xy0 - LD Vx, Vy      Vx = Vy
 *   6xkk - LD Vx, byte    Vx = KK
 *   Fx07 - LD Vx, DT      Vx = Delay_Timer
 *   Fx0A - LD Vx, K       Wait for key, then Vx = key
 *   Fx65 - LD Vx, [I]     Load V0 .. Vx from memory at I .. I+x
 *   Fx85 - LD Vx, R       Load V0 .. Vx (x<8) from HP48 flags
 *   Annn - LD I, addr     set I to NNN
 *   Fx15 - LD DT, Vx      Delay_Timer = Vx
 *   Fx18 - LD ST, Vx      Sound_timer = Vx
 *   Fx29 - LD F, Vx       I = &system_sprite[Vx]
 *   Fx30 - LD HF, Vx      I = &highres_system_sprite[Vx]
 *   Fx33 - LD B, Vx       I[0] = BCD_HIGH(Vx), I[1] = BCD_MID(Vx), I[2] = BCD_LOW(Vx)
 *   Fx55 - LD [I], Vx     Save V0 .. Vx into memory at I .. I+x
 *   Fx75 - LD R, Vx       Save V0 .. Vx (x<8) to HP48 flags
 */
   {
   WORD xreg;  /* need this as a word */
   BYTE yreg;
   BYTE token[MAX_TOKEN_LEN+1];  /* save area for first token */
   BYTE result;
   WORD addr;

   xreg = asm_get_regnum();  /* parse, but do not evaluate yet */
   if (!asm_skip_comma())  /* there is always a comma between two arguments */
      return(FALSE);
   if (xreg > NUM_REGISTERS)
      {
      if (strcmpi(asm_token,"I") == 0)  /* must be LD I,xxx */
         {
         asm_get_token();
         result = asm_parse_memory(&addr);
         asm_base_opcode = 0xa000 | addr;
         return(result);
         }
      strcpy(token,asm_token);  /* save current token */
      if ((xreg = asm_get_regnum()) > NUM_REGISTERS)
         return(FALSE);
           /* all remaining LD are <dest>,VX
            * So, if next arg is not a reg, invalid
            */
      xreg <<= 8;  /* move to correct position in instruction */
      if (strcmpi(token,"DT") == 0)
         {
         asm_base_opcode = 0xf015 | xreg;
         return(TRUE);
         }
      if (strcmpi(token,"ST") == 0)
         {
         asm_base_opcode = 0xf018 | xreg;
         return(TRUE);
         }
      if (strcmpi(token,"F") == 0)
         {
         asm_base_opcode = 0xf029 | xreg;
         return(TRUE);
         }
      if (strcmpi(token,"HF") == 0)
         {
         asm_base_opcode = 0xf030 | xreg;
         return(TRUE);
         }
      if (strcmpi(token,"B") == 0)
         {
         asm_base_opcode = 0xf033 | xreg;
         return(TRUE);
         }
      if (strcmpi(token,"[I]") == 0)
         {
         asm_base_opcode = 0xf055 | xreg;
         return(TRUE);
         }
      if (strcmpi(token,"R") == 0)
         {
         if (xreg > (7 << 8))
            return(FALSE);
         asm_base_opcode = 0xf075 | xreg;
         return(TRUE);
         }
      return(FALSE);
      }
   /* Here, xreg is the value of a valid register in the first argument.
    * What is the second argument?
    */
   xreg <<= 8;  /* move to correct position in instruction */
   if ((yreg = asm_get_regnum()) > NUM_REGISTERS)  /* not a register */
      {
      if (!asm_is_byte(&yreg))  /* not a byte value */
         {
         if (strcmpi(asm_token,"DT") == 0)
            {
            asm_base_opcode = 0xf007 | xreg;
            return(TRUE);
            }
         if (strcmpi(asm_token,"K") == 0)
            {
            asm_base_opcode = 0xf00a | xreg;
            return(TRUE);
            }
         if (strcmpi(asm_token,"[I]") == 0)
            {
            asm_base_opcode = 0xf065 | xreg;
            return(TRUE);
            }
         if (strcmpi(asm_token,"R") == 0)
            {
            if (xreg > (7 << 8))
               return(FALSE);
            asm_base_opcode = 0xf085 | xreg;
            return(TRUE);
            }
         return(FALSE);
         }
      /* 2nd opcode is a byte value */
      asm_base_opcode = 0x6000 | xreg | yreg;
      return(TRUE);
      }
   /* 2nd opcode is a register */
   asm_base_opcode = 0x8000 | xreg | (yreg << 4);
   return(TRUE);
   }

BYTE asm_add()  /* there are multiple ADD instructions */
/*   7xkk - ADD Vx, byte   Vx += KK
 *   8xy4 - ADD Vx, Vy     Vx += Vy, VF set to 1 if result > 0xff, else 0
 *   Fx1E - ADD I, Vx      I += Vx
 */
   {
   WORD xreg, yreg;

   xreg = asm_get_regnum();  /* parse, but do not evaluate yet */
   if (!asm_skip_comma())  /* there is always a comma between two arguments */
      return(FALSE);
   if (xreg > NUM_REGISTERS)
      {
      if (strcmpi(asm_token,"I") != 0)  /* not ADD I,Vx */
         return(FALSE);
      if ((xreg = asm_get_regnum()) > NUM_REGISTERS)
         return(FALSE);
      asm_base_opcode = 0xf01e | (xreg << 8);
      return(TRUE);
      }
   if ((yreg = asm_get_regnum()) > NUM_REGISTERS)  /* not a register */
      {
      if (!asm_is_byte(&yreg))  /* not a byte value */
         return(FALSE);
      asm_base_opcode = 0x7000 | (xreg << 8) | yreg;
      return(TRUE);
      }
   asm_base_opcode = 0x8004 | (xreg << 8) | (yreg << 4);
   return(TRUE);
   }

BYTE asm_vxvy()
/*   8xy1 - OR Vx, Vy      Vx |= Vy
 *   8xy2 - AND Vx, Vy     Vx &= Vy
 *   8xy3 - XOR Vx, Vy     Vx ^= Vy
 *   8xy5 - SUB Vx, Vy     Vx -= Vy, VF set to 1 if Vx >= Vy, else 0
 *   8xy7 - SUBN Vx, Vy    Vx = Vy - Vx, VF set to 1 if Vy >= Vx, else 0
 */
   {
   WORD xreg, yreg;

   if ((xreg = asm_get_regnum()) > NUM_REGISTERS)
      return(FALSE);
   if (!asm_skip_comma())  /* there is always a comma between two arguments */
      return(FALSE);
   if ((yreg = asm_get_regnum()) > NUM_REGISTERS)
      return(FALSE);
   asm_base_opcode |= (xreg << 8) | (yreg << 4);
   return(TRUE);
   }

BYTE asm_shift()
/*   8xy6 - SHR Vx         Vx >>= 1, VF set to 1 if carry, else 0 
 *   8xyE - SHL Vx         Vx <<= 1, VF set to 1 if carry, else 0
 *
 *   Can also be
 *       SHR Vx, 1
 *       SHR Vx, Vy
 *       SHL Vx, 1
 *       SHL Vx, Vy
 */
   {
   WORD xreg, yreg = 0;

   if ((xreg = asm_get_regnum()) > NUM_REGISTERS)
      return(FALSE);
   if (asm_skip_comma())  /* if a comma, is 2nd argument valid? */
      {
      if ((yreg = asm_get_regnum()) > NUM_REGISTERS)
         {
         if (strcmpi(asm_token,"1"))
            return(FALSE);
         yreg = 0;
         }
      }
   asm_base_opcode |= (xreg << 8) | (yreg << 4);
   return(TRUE);
   }

BYTE asm_vx()
/*   Ex9E - SKP Vx         skip next instr if key stored in Vx is pressed
 *   ExA1 - SKNP Vx        skip next instr if key stored in Vx is not pressed
 */
   {
   WORD xreg;

   if ((xreg = asm_get_regnum()) > NUM_REGISTERS)
      return(FALSE);
   asm_base_opcode |= (xreg << 8);
   return(TRUE);
   }

BYTE asm_rnd()
  /* Cxkk - RND Vx, byte   Vx = <random> & KK */
   {
   WORD xreg, val;

   if ((xreg = asm_get_regnum()) > NUM_REGISTERS)
      return(FALSE);
   if (!asm_skip_comma())  /* there is always a comma between two arguments */
      return(FALSE);
   if (!asm_is_byte(&val))  /* not a byte value */
      return(FALSE);
   asm_base_opcode |= (xreg << 8) | val;
   return(TRUE);
   }

BYTE asm_draw()
/* Dxyn - DRW Vx, Vy, nibble  draw sprite at I at */
   {
   WORD xreg, yreg;

   if ((xreg = asm_get_regnum()) > NUM_REGISTERS)
      return(FALSE);
   if (!asm_skip_comma())  /* there is always a comma between two arguments */
      return(FALSE);
   if ((yreg = asm_get_regnum()) > NUM_REGISTERS)
      return(FALSE);
   if (!asm_skip_comma())  /* there is always a comma between two arguments */
      return(FALSE);
   asm_get_token();
   if ((asm_token_len == 1) && (ishexdigit(asm_token[0])))
      {
      asm_base_opcode |= (xreg << 8) | (yreg << 4) | fromhex(asm_token[0]);
      return(TRUE);
      }
   return(FALSE);
   }

BYTE asm_bytes()  /* data bytes, typically sprites */
   {
   WORD val;

   asm_base_opcode = DB_DW_FLAG_VALUE;  /* flag value:  data is in asm_data[] */
   asm_data_len = 0;
   do
      {
      asm_get_token();
      if (!asm_is_byte(&val))
         return(FALSE);
      asm_data[asm_data_len++] = val;
      if (asm_data_len >= STRING_SIZE)
         return(FALSE);
      if ((asm_ip + asm_data_len) > MEM_SIZE)  /* don't assemble past end of memory */
         return(FALSE);
      } while (asm_skip_comma());  /* if a comma, expect another value */
   return(TRUE);
   }

BYTE asm_words()   /* data words, typically sprites */
   {
   WORD val;
   WORD i;

   asm_base_opcode = DB_DW_FLAG_VALUE;  /* flag value:  data is in asm_data[] */
   asm_data_len = 0;
   do
      {
      asm_get_token();
      if ((!asm_token_len) || (asm_token_len > 4))
         return(FALSE);
      for (val = 0, i = 0; i < asm_token_len; i++)
         {
         if (!ishexdigit(asm_token[i]))
            return(FALSE);
         val = (val << 4) | fromhex(asm_token[i]);
         }
      asm_data[asm_data_len++] = (val >> 8);  /* CHIP-8 is big-endian */
      asm_data[asm_data_len++] = val & 0xff;
      if (asm_data_len >= STRING_SIZE-1)
         return(FALSE);
      if ((asm_ip + asm_data_len) > MEM_SIZE)  /* don't assemble past end of memory */
         return(FALSE);
      } while (asm_skip_comma());  /* if a comma, expect another value */
   return(TRUE);
   }


typedef struct
   {
   BYTE *instr_name;
   WORD opcode_base;
   BYTE (*asm_fn)();
   } asm_info;

#define NUM_BASE_INSTRS   27
asm_info chip8_asm[NUM_BASE_INSTRS] =
   {
      { "SCD",  0x00c0, asm_scd },
      { "CLS",  0x00e0, asm_done },
      { "RET",  0x00ee, asm_done },
      { "SCR",  0x00fb, asm_done },
      { "SCL",  0x00fc, asm_done },
      { "EXIT", 0x00fd, asm_done },
      { "LOW",  0x00fe, asm_done },
      { "HIGH", 0x00ff, asm_done },
      { "JP",   0x1000, asm_jmp },  /* need to distinguish JP xxx from JP V0,xxx */
      { "CALL", 0x2000, asm_call },
      { "SE",   0x5000, asm_se },  /* need to distinguish SE VX,byte from SE VX,VY */
      { "LD",   0x6000, asm_ld },  /* there are multiple LD instructions */
      { "ADD",  0x7000, asm_add },  /* there are multiple ADD instructions */
      { "OR",   0x8001, asm_vxvy },
      { "AND",  0x8002, asm_vxvy },
      { "XOR",  0x8003, asm_vxvy },
      { "SUB",  0x8005, asm_vxvy },
      { "SHR",  0x8006, asm_shift },
      { "SUBN", 0x8007, asm_vxvy },
      { "SHL",  0x800e, asm_shift },
      { "SNE",  0x9000, asm_sne },  /* need to distinguish SNE VX,byte from SNE VX,VY */
      { "RND",  0xc000, asm_rnd },
      { "DRW",  0xd000, asm_draw },
      { "SKP",  0xe09e, asm_vx },
      { "SKNP", 0xe0a1, asm_vx },
      { "DB",   0x0000, asm_bytes },  /* data bytes, typically sprites */
      { "DW",   0x0000, asm_words }   /* data words, typically sprites */
   };

BYTE assemble(ip, str, len)
   WORD ip;
   BYTE *str;
   WORD *len;
      /* must return LEN of assembled string, DB / DW can be more than the
       * 2 bytes of an instruction.
       */
   {
   WORD which_instruction;
   BYTE result = FALSE;
   WORD i;

   strcpy(asm_input,str);
   asm_input_index = 0;
   asm_ip = ip;
   asm_get_token();
   for (which_instruction = 0; which_instruction < NUM_BASE_INSTRS; which_instruction++)
      if (strcmpi(asm_token,chip8_asm[which_instruction].instr_name) == 0)
         break;
   if (which_instruction < NUM_BASE_INSTRS)
      {
      asm_base_opcode = chip8_asm[which_instruction].opcode_base;
      result = (*chip8_asm[which_instruction].asm_fn)();
      }
   if (result)
      {
      if (asm_base_opcode != DB_DW_FLAG_VALUE)
         {
         if ((ip + 2) > MEM_SIZE)
            result = FALSE;
         else
            {
            memory[ip++] = asm_base_opcode >> 8;  /* CHIP-8 is big-endian */
            memory[ip] = asm_base_opcode & 0xff;
            *len = 2;
            }
         }
      else
         {
         if ((ip + asm_data_len) > MEM_SIZE)
            result = FALSE;
         else
            {
            for (i = 0; i < asm_data_len; i++)
               memory[ip + i] = asm_data[i];  /* bytes already big-endian */
            *len = asm_data_len;
            }
         }
      }
   return(result);
   }
