#include "debug8.h"
#define SHOW(str)  debug_window_puts_nl(str)

WORD help_pause()
   {
   WORD key;

   dbg_cursor_r = VESA_GRAPHICS_TEXT_ROWS - 1;
   curpos(dbg_cursor_r, dbg_cursor_c = 0);
   puts("Esc to stop, any other key to continue");
   key = getkey();
   curpos(dbg_cursor_r, dbg_cursor_c);
   puts("                                      ");
/*   next_debug_window_line(); */
   return(key);
   }

void help_cls()
   {
   erase_debug_window();
   dbg_cursor_r = DEBUG_WINDOW_START_TEXT_ROW;
   dbg_cursor_c = DEBUG_WINDOW_START_TEXT_COL;
   curpos(dbg_cursor_r, dbg_cursor_c);
   }

void dbg_help()
   {
   help_cls();
   SHOW("G -- Go (run program)  Use Esc to stop.");
   SHOW("   G [=pc] [start1[,end1][=count1]] ... [start5[,end5][=count5]]");
   SHOW("P -- Proceede.  Same as Go, but set breakpoint at next instruction.");
   SHOW("T -- Trace program.  Use Esc to stop tracing multiple instructions.");
   SHOW("   T [=pc] [number_of_instructions_to_trace]");
   SHOW("   DT and ST are not updated during Trace");
   SHOW("B -- manage Breakpoints.  Entering a new set clears the current set.");
   SHOW("   B by itself toggles current list ON and OFF.");
   SHOW("   B LIST -- shows current breakpoints");
   SHOW("   B [\"instr\"] [start1[,end1][=count1]] ... [start5[,end5][=count5]]");
   SHOW("U -- Unassemble (Disassemble).  U [start_pc [number_of_instructions]]");
   SHOW("A -- Assemble.  A [starting_pc]");
   SHOW("S -- edit Sprite. S memory_address num_rows [template_address]");
   SHOW("   Use keypad to navigate, '5' or space to toggle bit, I to invert");
   SHOW("   C to clear, F to fill, S to save, or Esc to exit without saving.");
   if (help_pause() == ESC_KEY)
      return;
   help_cls();
   SHOW("D -- Dump memory.  D [memory_address [len]]");
   SHOW("R -- Change register value.  R reg_name value");
   SHOW("   Where reg_name is V0 .. VF, PC, I, SP, DT, or ST.");
   SHOW("E -- Enter value(s) into memory");
   SHOW("   E starting_address value1 [value2 [value3 ...]]]");
   SHOW("F -- Fill memory with value(s)");
   SHOW("   F starting_address ending_address value1 [value2 [value3 ...]]]");
   SHOW("W -- Where was I?  Show the last 16 (10 hex) instructions executed.");
   SHOW("L -- Load program.  L [program_name]");
   SHOW("   Load program and reset interpreter.  If no name given, just reset.");
   SHOW("C -- Create (save) program.  C program_name [start_address [end_address]]");
   SHOW("SAVE -- Save debuger state.  SAVE filename");
   SHOW("   Save all memory, graphics, registers, and state to reload later.");
   SHOW("   Allows multiple debugging sessions from the same initial state.");
   SHOW("LOAD -- Load debugger state.  LOAD filename");
   if (help_pause() == ESC_KEY)
      return;
   help_cls();
   SHOW("Q -- Quit the debugger.");
   SHOW("-F -- toggle Freerun (run as fast as possible)");
   SHOW("-T -- set Ticks (2040Hz) per execution cycle.  -T [cycle_count]");
   SHOW("-I -- set Instruction count per execution cycle. -I [instruction_count]");
   SHOW("-K -- toggle normal / alternate keypad.  Normal keypad is the PC keypad");
   SHOW("   Alternate keypad is 1..4/q..r/a..f/z..v");
   SHOW("-V -- toggle startup mode High-res / Standard video");
   SHOW("-S -- Sound frequency.  -S frequency");
   SHOW("  Default is 330Hz, range is 100Hz (64 hex) to 1000 Hz (3E8 hex)");
   SHOW("-L -- set Load address.  -L address");
   SHOW("  Valid values are 200 or 600 (ETI660 compatible).");
   SHOW("-W -- toggle Wrap sprites at edge of screen.");
   if (help_pause() == ESC_KEY)
      return;
   help_cls();
   SHOW("-M -- toggle Mikolay's instruction behavior.");
   SHOW("   SHL/SHR Vx,Vy shift Vy and store it in Vx.");
   SHOW("   LD [I],Vx and LD Vx,[I] increment I");
   SHOW("   This is the same as using the -X and -Y commands together.");
   SHOW("-X -- toggle Mikolay's SHL/SHR instruction behavior");
   SHOW("   SHL/SHR Vx,Vy shift Vy and store it in Vx.");
   SHOW("-Y -- toggle Mikolay's I register increment");
   SHOW("   LD [I],Vx and LD Vx,[I] increment I");
   SHOW("-Z -- toggle carry from ADD I,Vx");
   }
