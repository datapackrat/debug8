#include "debug8.h"

#define UP_LEFT       0x147
#define UP            0x148
#define UP_RIGHT      0x149
#define LEFT          0x14B
#define PIXEL_TOGGLE  0x14C
#define RIGHT         0x14D
#define DOWN_LEFT     0x14F
#define DOWN          0x150
#define DOWN_RIGHT    0x151

#define SPRITE_BOX_PIXELS_PER_COL  8  /* 7 x 7 pixel + 1 border pixel */
#define SPRITE_BOX_PIXELS_PER_ROW  8  /* 7 x 7 pixel + 1 border pixel */

#define MAX_SPRITE_ROWS  16
#define MAX_SPRITE_COLS  16

#define MAX_SPRITE_SIZE   (MAX_SPRITE_ROWS * (MAX_SPRITE_COLS / 8))
   /* == 32,  16 rows * (16 cols / 8bits-per-byte) */

BYTE sprite[MAX_SPRITE_SIZE];
BYTE sprite_rows;
BYTE sprite_cols;
BYTE bytes_per_sprite_row;

BYTE current_pixel_r, current_pixel_c;


sprite_cursor_left()
   {
   if (current_pixel_c)  /* LEFT */
      current_pixel_c--;
   else
      current_pixel_c = sprite_cols - 1;
   }

sprite_cursor_right()
   {
   if (++current_pixel_c == sprite_cols)  /* RIGHT */
      current_pixel_c = 0;
   }

sprite_cursor_up()
   {
   if (current_pixel_r)  /* UP */
      current_pixel_r--;
   else
      current_pixel_r = sprite_rows - 1;
   }

sprite_cursor_down()
   {
   if (++current_pixel_r == sprite_rows)  /* DOWN */
      current_pixel_r = 0;
   }

spixel(r, c, color)
   WORD r, c, color;
      /* every pixel in sprite editor is moved to origin of spite editor box */
   {
   pixel(SPRITE_EDIT_START_PIXEL_ROW + r, SPRITE_EDIT_START_PIXEL_COL + c, color);
   }

sprite_pixel(r, c, color)
   WORD r, c, color;
   {
   WORD r1, c1;

   for (r1 = r; r1 < (r + SPRITE_BOX_PIXELS_PER_ROW - 1); r1++)
      for (c1 = c; c1 < (c + SPRITE_BOX_PIXELS_PER_COL - 1); c1++)
         spixel(r1, c1, color);
   }

sprite_cursor(r, c, color)
   WORD r, c, color;
   {
 /*  +--------------
  *  | . . . x . . .
  *  | . . . x . . .
  *  | . . . x . . .
  *  | x x x x x x x
  *  | . . . x . . .
  *  | . . . x . . .
  *  | . . . x . . .
  */
   WORD i;

   for (i = 0; i < (SPRITE_BOX_PIXELS_PER_COL-1); i++)
      {
      spixel(r + i, c + (SPRITE_BOX_PIXELS_PER_COL-1)/2, color);
      spixel(r + (SPRITE_BOX_PIXELS_PER_COL-1)/2, c + i, color);
      }
   }

draw_sprite_editor_box()
   {
   WORD r, c, color;
   WORD i;

   for (r = 0; r <= sprite_rows; r++)  /* use "<=" so we get final row, too */
      {
      for (c = 0; c <= sprite_cols * SPRITE_BOX_PIXELS_PER_COL; c++)  /* use "<=" so we get final col, too */
         spixel(r * SPRITE_BOX_PIXELS_PER_ROW, c, CYAN);
      }
   for (c = 0; c <= sprite_cols; c++)  /* use "<=" so we get final col, too */
      {
      for (r = 0; r <= sprite_rows * SPRITE_BOX_PIXELS_PER_ROW; r++)
         spixel(r, c * SPRITE_BOX_PIXELS_PER_COL, CYAN);
      }
   for (r = 0; r < sprite_rows; r++)
      {
      for (c = 0; c < sprite_cols; c++)
         {
         if (sprite[r * bytes_per_sprite_row + ((c > 7) ? 1 : 0)] & (0x80 >> (c & 0x07)))
            color = WHITE;
         else
            color = BLACK;
         sprite_pixel(r * SPRITE_BOX_PIXELS_PER_ROW + 1,
                      c * SPRITE_BOX_PIXELS_PER_COL + 1,
                      color);
           /* r+1, c+1 because r, c is location of Upper-Left corner of frame */
         }
      }
   r = current_pixel_r * SPRITE_BOX_PIXELS_PER_ROW;
   c = current_pixel_c * SPRITE_BOX_PIXELS_PER_COL;
   if (sprite[current_pixel_r * bytes_per_sprite_row + ((current_pixel_c > 7) ? 1 : 0)] & (0x80 >> (current_pixel_c & 0x07)))
      color = BLACK;  /* invert colors to show cursor */
   else
      color = WHITE;
   sprite_cursor(r + 1,c + 1,color);
        /* r+1, c+1 because r, c is location of Upper-Left corner of frame */
   for (i =0, r = 0; r < sprite_rows; r++, i += bytes_per_sprite_row)
         /* i = index into sprite[] */
      {
      curpos(r + DEBUG_WINDOW_START_TEXT_ROW, 30);
      if (bytes_per_sprite_row == 1)
         printf("%02x",sprite[i]);
      else
         printf("%02x %02x",sprite[i],sprite[i+1]);
      }
   }

void edit_sprite(numrows, addr, copyfrom)
   WORD numrows, addr, copyfrom;
   {
   WORD i, j;
   WORD key = 0;

   current_pixel_r = 0;
   current_pixel_c = 0;
   bytes_per_sprite_row = 1;
   sprite_cols = 8;  /* default */
   if ((sprite_rows = numrows) == 16)
      {
      sprite_cols = 16;
      bytes_per_sprite_row = 2;
      }
   memmove(&memory[addr],sprite,sprite_rows * bytes_per_sprite_row);
   if (copyfrom != 0xffff)
      memmove(&memory[copyfrom],sprite,sprite_rows * bytes_per_sprite_row);
   draw_sprite_editor_box();
   do
      {
      if (keytest())
         {
         switch (key = upcase(getkey()))
            {
            case SPACE:
            case PIXEL_TOGGLE:
               i = current_pixel_r * bytes_per_sprite_row;
               j = (current_pixel_c > 7) ? 1 : 0;
               sprite[i + j] ^= (0x80 >> (current_pixel_c & 0x07));
               break;
            case UP_LEFT:
               sprite_cursor_up();
               sprite_cursor_left();
               break;
            case UP:
               sprite_cursor_up();
               break;
            case UP_RIGHT:
               sprite_cursor_up();
               sprite_cursor_right();
               break;
            case LEFT:
               sprite_cursor_left();
               break;
            case RIGHT:
               sprite_cursor_right();
               break;
            case DOWN_LEFT:
               sprite_cursor_down();
               sprite_cursor_left();
               break;
            case DOWN:
               sprite_cursor_down();
               break;
            case DOWN_RIGHT:
               sprite_cursor_down();
               sprite_cursor_right();
               break;
            case 'I':  /* invert sprite */
               for (i = 0; i < MAX_SPRITE_SIZE; i++)
                  sprite[i] = ~sprite[i];
               break;
            case 'C':  /* clear sprite */
               for (i = 0; i < MAX_SPRITE_SIZE; i++)
                  sprite[i] = 0;
               break;
            case 'F':  /* fill sprite */
               for (i = 0; i < MAX_SPRITE_SIZE; i++)
                  sprite[i] = 0xff;
               break;
            }
         draw_sprite_editor_box();
         }
      } while ((key != ESC_KEY) && (key != 'S'));
   if (key == 'S')
      {
      for (i = 0; i < sprite_rows * bytes_per_sprite_row; i++)
         memory[addr+i] = sprite[i];
      }
   }
