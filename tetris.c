/* Tetris for Terminals (2nd version) on saxonsoc linux
 * 
 * This program implements a (simplified) version of the famous 'Tetris'
 * game with a PIC16F628 connected to a serial terminal (VT52 or higher).
 * Just connect the terminal to the PIC RX/TX ports, select the required
 * communication settings (9600 baud 8N1) on the terminal, and you are
 * ready to go. 
 * 
 * The program assumes an input clock of 1.075 MHz (in order to generate 
 * the correct baud-clock for 9600 baud communication), but it might also
 * work with 1.00 MHz clock if your terminal tolerates deviations from the 
 * exact baud-clock. The low clock rate was chosen to make the program
 * (almost) useable in a simulator; you might want to use 4 MHz or 16 MHz
 * and 19200 or 38400 kbaud for real hardware. This requires changing the
 * RX/TX settings in vt100_initialize().
 * 
 * Use the following keys to control the game:
 * 
 * 'j'        - move current block left
 * 'k'        - rotate current block
 * 'l'        - move current block right
 * 'space'    - drop current block               
 * 'r'        - redraw the screen
 * 's'        - start new game (or give up current game)
 * 'q'        - quit
 * 'f'        - faster
 * 'd'        - slower
 * 
 * Tip: if you run the demo via the Hades simulator, please type at most
 * one command key during each repaint iteration. Otherwise, the 16F628
 * receive buffer will overrun, and the 16F628 simulation model seems to
 * misbehave afterwards - it reacts only to each second keypress afterwards,
 * which makes playing much more difficult :-)
 * 
 * Originally intended for the PIC16F84 with only 68 bytes of RAM,
 * the program uses a very compact representation of the gaming board,
 * with one bit per position (1=occupied 0=empty). All accesses to the
 * gaming board should be via the setPixel() and occupied() functions.
 * A standard gaming board of 24 rows of 10 columns each is used.
 * Note that the decision for a 1-bit representation means that we lose
 * the option to display the original type (color) of the different blocks.
 * Unfortunately, I found no way to tweak the program into the 16F84
 * via the picclite compiler, so I switched to the pincompatible 16F628.
 * The original 1-bit datastructures were kept, though the 16F628 should
 * have enough RAM to support storing the block-type in the gaming board.
 *
 * Similarly, the current block is represented via a 4x4 bitmap stored
 * in the 'current_block<0..3>' variables, where the lower nibble of each
 * byte specify the 4 bits of one row of the current block. New blocks
 * are created via the create_block() and create_random_block() functions,
 * and rotate_block() uses some bit-fiddling to rotate the block inplace
 * by 90 degrees. The current position (row, column) of the current block
 * is maintained in the current_row and current_col variables, and the
 * test_if_block_fits() functions checks whether the block fits within the
 * gaming board bounds and the already placed blocks.
 * 
 * The program enables interrupts for both the timer0 interrupts and
 * the serial-port receiver interrupts (user input via the terminal).
 * The interrupt handler just copies the received input character to the
 * 'command' variable, and sets a bit in the 'state' variable on a timer
 * overflow.
 * On each iteration of the check_handle_command() function, the 'state'
 * variable is checked for the timer overflow bit. If set, the program
 * attempts to let the current block fall down one position and handles
 * the resulting situation (collapsing completed rows, or setting the
 * game-over bit if the block is stuck at the topmost row).
 * Otherwise, the check_handle_command() bit copies the contents of the
 * command variable to a tmp variable, resets the command variable, and
 * then continues to check for and execute the given user command.
 * 
 * Despite using an 'incremental repainting' strategy of first erasing and
 * then overdrawing the current block during the game-play, the performance
 * of the program is still limited by the slow serial connection.
 * A single block movement still needs four VT52 goto commands and writes
 * four characters to erase a block, and another four gotos and writes to
 * redraw the block. This alones takes 40*(1+8+1)*(1/baudrate) seconds - 
 * about 40 msecs at 9600 baud. 
 * While the program uses the 16F628 transmitter register to send characters,
 * it also uses a wait loop in function vt100_putc() to avoid overrunning
 * the transmitter. This is far simpler than trying to maintain a send buffer,
 * but it also means that we are likely to drop user input requests, because
 * the sending is triggered from within the interrupt handler. Also, note
 * that the wait-loop limits in vt100_putc() need to be adapted when changing 
 * the terminal baud-rate or the 16F628 input clock.
 *
 * TODO:
 * The program was written for the free Picclite compiler (9.50PL1), which
 * supports the 16F627 but not the 16F628. This means that the program must
 * fit into the first 1K words of program memory, which severely limits the
 * things we can do. At the moment, there are just seven unused words left
 * free in the program memory...
 * 
 * 
 * If you have access to the full version of the compiler or another compiler
 * for the 16F628, you could try to re-instate the uncommented functions
 * and to add some functions that were omitted due to program-memory size
 * limitiations. For example:
 * - show the 'level' and 'score' info after each new block
 * - distinguish the different block-types via different characters
 * - distinguish the 'score' between block-types
 * - add highlighting of 'collapsing' rows before redrawing the game-board
 * - implement the 'pause' command
 * - make timer0 delays smaller with increasing game level
 * - add high-score list management, could use the data EEPROM
 * - program should run with watchdog timer enabled (needs extra clrwdt)
 * - etc.
 *
 * Note: see http://www.rickard.gunee.com/projects for a really amazing
 * assembly-code version of Tetris for the PIC16F84 - with direct connection
 * to a PAL/NTSC black&white television set, and even including audio!
 * I only learned about that version after writing this C program, however. 
 * Fortunately, data-structures and timing of both programs are quite different.
 * 
 * Note: make sure to set the UART initialization code to match the
 * selected clock rate. You might also have to play with the TMR0
 * reload value in the interrupt handler to optimize the game-play
 * (not too slow, but neither too fast...)
 * 
 * 
 * (C) 2005, 2006 fnh hendrich@informatik.uni-hamburg.de
 * 
 * 2020.10.05 - lcc compiled for saxonsoc
 * 2006.03.23 - change vt100_putc to support 4MHz and 250 kHz clock
 * 2006.03.20 - ifdef stuff to change between 4 MHz and 250 kHz clock
 * 2006.03.06 - change 16F628 to use XT osc. mode at 4.000 MHz
 * 2006.01.04 - reorganize main-loop <-> isr interaction
 * 2005.12.29 - implemented check_remove_complete_rows()
 * 2005.12.28 - first working version (with interrupts)
 * 2005.12.27 - first code (game board drawing etc)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

// graphics

#define CHAR_SPACE  ' '
#define CHAR_WALL   '|'
#define CHAR_ACTIVE 'H'
#define CHAR_FIXED  'X'

// 1-single char, 2-double char, ...
#define DRAW_MULTI  2

typedef unsigned char bit; // compatiblity

#define TCGETS           0x5401
#define TCSETS           0x5402

#define ECHO             0000010
#define ICANON           0000002
#define ISIG             0000001

#define VTIME 5
#define VMIN 6

typedef unsigned char    cc_t;
typedef unsigned int     speed_t;
typedef unsigned int     tcflag_t;

#define NCCS             32

struct termios
{
    tcflag_t c_iflag;                /* input mode flags */
    tcflag_t c_oflag;                /* output mode flags */
    tcflag_t c_cflag;                /* control mode flags */
    tcflag_t c_lflag;                /* local mode flags */
    cc_t c_line;                     /* line discipline */
    cc_t c_cc[NCCS];                 /* control characters */
    speed_t c_ispeed;                /* input speed */
    speed_t c_ospeed;                /* output speed */
#define _HAVE_STRUCT_TERMIOS_C_ISPEED 1
#define _HAVE_STRUCT_TERMIOS_C_OSPEED 1
};
struct termios orig_termios, current_termios;

#define ROWS  ((unsigned char) 24)
#define COLS  ((unsigned char) 10)

#define PAINT_FIXED  ((unsigned char) 2)
#define PAINT_ACTIVE ((unsigned char) 1)
#define ERASE        ((unsigned char) 0)

#define XOFFSET ((unsigned char) 3)
#define XLIMIT  ((unsigned char) (XOFFSET+COLS))

static unsigned char board[30];         // the main game-board

static unsigned char current_block0;    // bit-pattern of the current block,
static unsigned char current_block1;    // with one four-bit bitmap stored
static unsigned char current_block2;    // in the lower nibble of each of these
static unsigned char current_block3;    // variables

static signed char current_row;         // row of the current block
static signed char current_col;         // col of the current block

static unsigned char lines;
static unsigned char level;
static unsigned char score;

#define STATE_IDLE   ((unsigned char) 0x1)
#define TIMEOUT      ((unsigned char) 0x2)
#define NEEDS_INIT   ((unsigned char) 0x4) 
#define GAME_OVER    ((unsigned char) 0x8) 

#define CMD_NONE     ((unsigned char) 0)
#define CMD_LEFT     ((unsigned char) 'j')
#define CMD_RIGHT    ((unsigned char) 'l')
#define CMD_ROTATE   ((unsigned char) 'k')
#define CMD_DROP     ((unsigned char) ' ')
#define CMD_REDRAW   ((unsigned char) 'r')
#define CMD_START    ((unsigned char) 's')
#define CMD_QUIT     ((unsigned char) 'q')

// high-scores: 1 point per new block, 20 points per completed row
#define SCORE_PER_BLOCK  ((unsigned char) 1)
#define SCORE_PER_ROW    ((unsigned char) 20)

static unsigned char  state;
static unsigned char  command;

/**
 * utility function to calculate (1 << nbits)
 */
unsigned char power_of_two( unsigned char nbits ) {
  unsigned char mask;
  
  mask = (1 << nbits);
  return mask;	
}


/**
 * utility function that returns the bit-patterns of the seven 
 * predefined types (shapes) of Tetris blocks via ROM lookups.
 * This method returns the block in the standard orientation only.
 * We use a 4x4 matrix packed into two unsigned chars as
 * (first-row << 4 | second-row), (third-row << 4 | fourth-row).
 */ 
unsigned char getRawBlockPattern( unsigned char index ) {
  switch( index ) {
  	          // brown square 2x2
    case 0:   return  0x06; // 0b00000110;
    case 1:   return  0x60; // 0b01100000;
  	
  	          // brown square 2x2
    case 2:   return  0x06; // 0b00000110;
    case 3:   return  0x60; // 0b01100000;

              // yellow cursor-type block    
    case 4:   return  0x0E; // 0b00001110;
    case 5:   return  0x40; // 0b01000000;

              // red 1x4 block
    case 6:   return  0x44; // 0b01000100;
    case 7:   return  0x44; // 0b01000100;

              // blue 2+2 shifted block
    case 8:   return  0x0C; // 0b00001100;
    case 9:   return  0x60; // 0b01100000;

              // green=2+2 shifted block (inverse to blue)
    case 10:  return  0x06; // 0b00000110;
    case 11:  return  0xC0; // 0b11000000;

              // cyan=3+1 L-shaped block
    case 12:  return  0x0E; // 0b00001110;
    case 13:  return  0x20; // 0b00100000;

              // lilac 1+3 L-shaped block
    case 14:  return  0x0E; // 0b00001110;
    case 15:  return  0x80; // 0b10000000;
    
              // we will not arrive here, but the compiler wants this
    default:  return  0xFF; // 0b11111111;
  }
}


/**
 * fill the current_block0..3 variables with the bit pattern of the
 * selected block type (0,1..7).
 * 
 * Implementation note: the first version of the code used an array
 * current_block[4], which made for clean C sources but very clumsy
 * assembly code after compiling. Switching to four separate variables
 * with two utility functions reduced the code size and made the program
 * fit into the 16F627...
 */
void create_block( unsigned char index ) {
  unsigned char tmp;

  tmp = getRawBlockPattern(index+index);
  current_block0 = (tmp & 0xf0) >> 4;
  current_block1 = (tmp & 0x0f);

  tmp = getRawBlockPattern(index+index+1);
  current_block2 = (tmp & 0xf0) >> 4;
  current_block3 = (tmp & 0x0f);
}


/**
 * create a new randomly-chosen block.
 * Note that rand() doesn't seem to return zero at all,
 * so the seven types of blocks are indexed via values 1..7
 * instead of 0..6.
 */
void create_random_block( void ) {
  unsigned char x;
  x = rand() & 0x7;
  create_block( x );	
}


/**
 * helper function to access one nibble (row) of the current block.
 */
unsigned char getBlockNibble( unsigned char i ) {
  unsigned char tmp;
  switch( i ) {
  	case 0: tmp = current_block0; break;
  	case 1: tmp = current_block1; break;
  	case 2: tmp = current_block2; break;
  	case 3: tmp = current_block3; break;
  }
  return tmp;
}


/**
 * helper function for rotate_block()
 */
void updateBlockNibble( unsigned char i, unsigned char mask ) {
  switch( i ) {
    case 0: current_block0 |= mask; break;
    case 1: current_block1 |= mask; break;  	
    case 2: current_block2 |= mask; break;
    case 3: current_block3 |= mask; break;  	
  }
}


/**
 * rotate the current_block data by 90 degrees (counterclockwise).
 * We use an inplace rotation that uses the upper nibbles of the
 * current_block0..3 variables as intermediate storage.
 * 
 * 

 * (- - - -   a b c d)           (a b c d   d h l p)
 * (- - - -   e f g h)           (e f g h   c g k o)
 * (- - - -   i j k l)   =>      (i j k l   b f j n)
 * (- - - -   m n o p)           (m n o p   a e i m)
 * 

 */ 
void rotate_block( void ) {
  unsigned char i, j;

  // move lower nibble to upper nibbles
  current_block0 = current_block0 << 4;
  current_block1 = current_block1 << 4;
  current_block2 = current_block2 << 4;
  current_block3 = current_block3 << 4;

  // fill in the rotated bits
  for( i=0; i < 4; i++ ) {
    for( j=0; j < 4; j++ ) {
      if ((getBlockNibble(i) & power_of_two(j+4)) != 0) {
         updateBlockNibble( j, power_of_two(3-i) );
      }
    }
  }

  // clear upper nibbles again. This is not strictly required,
  // and uncommented for now to conserve program memory...
  // for( i=0; i < 4; i++ ) {
  //  current_block[i] = current_block[i] & 0x0f;
  // }
}


/**
 * check whether the current block has a pixel at position(row,col)
 */
bit getBlockPixel( unsigned char row, unsigned char col ) {
  //return (current_block[row] & power_of_two(col) != 0;
  unsigned char tmp;
  tmp = getBlockNibble( row );
  return (tmp & power_of_two(col)) != 0;
}


/**
 * accessor function for the gaming board position at (row,col).
 * Use val=1 for occupied and val=0 for empty places.
 */
void setPixel( unsigned char row, unsigned char col, unsigned char val ) {
  unsigned char index;
  unsigned char mask;
  
  index  = (row >> 3)*10 + col;
  mask   = power_of_two(row & 0x7);
  
  if (val == 0)   board[index] &= ((unsigned char) ~mask);
  else            board[index] |= mask;
}


/**
 * check whether the gaming board position at (row,col) is occupied.
 */
bit occupied( unsigned char row, unsigned char col ) {
  unsigned char index;
  unsigned char mask;

  index = (row >> 3)*10 + col;
  mask  = power_of_two(row & 0x7);

  return (board[index] & mask) != 0;
}


/**
 * clear the whole gaming board.
 */
void clear_board( void ) {
  unsigned char r, c;
  for( r=0; r < ROWS; r++ ) {
  	for( c=0; c < COLS; c++ ) {
  	  setPixel( r, c, 0 );	
  	}
  }	
}


/**
 * create a randomly populated gaming board.  
 */
/* Uncommented to save program memory space.
void random_board( void ) {
  unsigned char r, c, val;
  
  for( r=0; r < ROWS; r++ ) {
  	for( c=0; c < COLS; c++ ) {
  	  val = rand() & 0x01;
  	  setPixel( r, c, val );	
  	}
  }	
}
*/


/**
 * check whether the current block fits at the position given by
 * (current_row, current_col).
 * Returns 1 if the block fits, and 0 if not.
 */
bit test_if_block_fits( void ) {
  unsigned char i, j;
  signed char ccj;
  for( i=0; i < 4; i++ ) {
  	for( j=0; j < 4; j++ ) {
  	  if (getBlockPixel(i,j)) {
  	  	ccj = current_col + j;
  	  	if (ccj < 0) return 0; // too far left
  	  	if (ccj >= 10)  return 0; // too far right
  	  	if (current_row+i >= ROWS) return 0; // too low
  	  	
  	  	if (occupied(current_row+i, ccj)) return 0;
  	  }	
  	}
  }
  
  return 1; // block fits
}


/**
 * copy the bits from the current block at its current position 
 * to the gaming board. This means to fix the current 'foreground'
 * block into the static 'background' gaming-board pattern.
 */
void copy_block_to_gameboard( void ) {
  unsigned char i, j;
  unsigned char cri, ccj;

  for( i=0; i < 4; i++ ) {
  	cri = current_row + i;
  	for( j=0; j < 4; j++ ) {
  	  ccj = current_col + j;
  	  if (getBlockPixel(i,j)) {
  	  	setPixel( cri, ccj, 0x01 );
  	  }	
  	}
  }
}


/**
 * check whether the specified game-board row (0=top,23=bottom)
 * is complete (all bits set) or not.
 */
bit is_complete_row( unsigned char r ) {
  unsigned char c;
  
  for( c=0; c < COLS; c++ ) {
    if (!occupied(r,c))	return 0;
  }	
  
  return 1;
}


/**
 * remove one (presumed complete) row from the game-board,
 * so that all rows above the specified row drop one level.
 */
void remove_row( unsigned char row ) {
  unsigned char r, c, tmp;
  
  for( c=0; c < COLS; c++ ) {
  	for( r=row; r>0; r-- ) {
  	  tmp = occupied( r-1, c );
  	  setPixel( r, c, tmp );
  	}

    // finally, clear topmost pixel
    setPixel( 0, c, 0 );  	
  }
}


/** 
 * transmit the given character via tx to the terminal.
 * We assume that we can immediately put the new data into the transmit
 * buffer register TXREG. Afterwards, we should wait until the TRMT
 * (TXSTA<1>) bit is 1 to indicate that TSR is empty again.
 * Unfortunately, this doesn't work reliably with the Hades 16F628
 * simulation model, so that we use an explicit wait loop instead.
 * You will have to update the wait loop limit when chaning the
 * RS232 communication parameters (baudrate) and PIC input clock
 * frequency.
 */
void vt100_putc( unsigned char ch ) {
  putchar(ch);
  fflush(stdout);
}


/**
 * send "ESC [ ? 2 l" to put a VT100 into VT52 mode
 * and "ESC f" to hide the cursor.
 */
void vt100_enter_vt52_mode() {
  // VT100: enter VT52 mode
  vt100_putc( 27 );	 
  vt100_putc( '[' );
  vt100_putc( '?' );
  vt100_putc( '2' );
  vt100_putc( 'l' );
  
  // VT52: cursor off
  vt100_putc( 27 );
  vt100_putc( 'f' );
}


void vt100_exit_vt52_mode( void ) {
  vt100_putc( 27 );
  vt100_putc( '<' );
}


void reset_terminal_mode()
{
  int r;
  vt100_exit_vt52_mode();
  r = ioctl(0, TCSETS, &orig_termios);
}


void level_speed(unsigned char level)
{
  current_termios.c_cc[VTIME] = level < 10 ? 10-level : 1; // *0.1s timeout
  ioctl(0, TCSETS, &current_termios);
}


/**
 * initialize the serial communication parameters.
 * We also put some timer initialization here. 
 */
void vt100_initialize( void ) {
  int r;
  //int mode;
  char buf[2];

  buf[1] = 0;

  r = ioctl(0, TCGETS, &orig_termios);
  current_termios = orig_termios;

  current_termios.c_lflag &= ~(ECHO | ICANON /* | ISIG */);

  current_termios.c_cc[VMIN] = 0;
  current_termios.c_cc[VTIME] = 4; // *0.1s timeout

  r = ioctl(0, TCSETS, &current_termios);

}


/**
 * move the VT100 cursor to the given position.
 * This is done by sending 'ESC Y l c'
 * NOTE: VT52 expects an offset of 32 for the l and c values.
 */
void vt100_goto( unsigned char row, unsigned char col ) {
  vt100_putc( 27 );   // ESC
  vt100_putc( 'Y' );  // ESC-Y
  vt100_putc( (row+32) );
  vt100_putc( (col+32) );
}


/** 
 * request a VT100 cursor-home command on the terminal via tx.
 * We send the VT100/VT52 command 'ESC H'.
 */
void vt100_cursor_home( void ) {
  vt100_putc( 27 );    // ESC
  vt100_putc( 'H' );
}


/** clear screen
 */
void vt100_clear_screen(void)
{
  vt100_cursor_home();
  vt100_putc(27);
  vt100_putc('J');  // clear to end of screen
}


/**
 * helper function to convert a hex-digit into a character 0..9 a..f
 */
unsigned char vt100_hex( unsigned char val ) {
  if (val <= 9) return '0' + val;
  else          return ('a'-10) + val;	
}


/**
 * print the given integer value as two hex-digits.
 */
void vt100_xtoa( unsigned char val ) {
  unsigned char c;
  c = val >> 4;
  vt100_putc( vt100_hex( c ));
  c = val & 0x0f;
  vt100_putc( vt100_hex( c ));
}


/**
 * display (or erase) the current block at its current position,
 * depending on whether the paintMode parameter is PAINT_ACTIVE,
 * PAINT_FIXED, or ERASE.
 * This method just paints the active pixels from the block,
 * but nothing else (no game board, no borders, no score).
 * To achieve the best performance, we use the VT52 cursor
 * positioning commands via vt100_goto() for each of the
 * (always four) visible pixels of the block.
 */
void display_block( unsigned char paintMode ) {
  unsigned char i, j, k, ch;
  unsigned char rr, cc;
  char draw;
  
  for( i=0; i < 4; i++ ) {
    rr = current_row + i;
    if (rr >= ROWS) continue; // out of range

    for ( j=0; j < 4; j++ ) {
      cc = XOFFSET + current_col + j;
      if (cc >= XLIMIT) continue; // out of range
      if (getBlockPixel(i,j)) {
        vt100_goto( rr, cc*DRAW_MULTI );
        if     (paintMode == PAINT_ACTIVE) draw = CHAR_ACTIVE;
        else if (paintMode == PAINT_FIXED) draw = CHAR_FIXED;
        else                               draw = CHAR_SPACE;
        for(k = 0; k < DRAW_MULTI; k++)
          vt100_putc(draw);
      }
    }
  	
    // HACK:
    // extra goto because neither xterm/seycon nor Winxp hyperterm
    // understand the VT52 "cursor off" command, and the blinking
    // cursor at the end of a block is really annoying...
    // a "real" VT100/VT52 does work fine without this.
    vt100_goto( 1, 1 ); 
  }
} // display_block


/** 
 * display the current game board position on the terminal.
 * This method redraws the whole gaming board including borders.
 * Use another call to display_block() to also draw the current block.
 */
void display_board( void ) {
  unsigned char r,c,k;
  unsigned char ch;

  vt100_cursor_home();
  for( r=0; r < ROWS; r++ ) {

    vt100_goto( r, 2*DRAW_MULTI );

    for(k = 0; k < DRAW_MULTI; k++)
      vt100_putc( CHAR_WALL );            // one row of the board: border, data, border

    for( c=0; c < COLS; c++ ) {
      ch = occupied(r,c) ? CHAR_FIXED : CHAR_SPACE;
      for(k = 0; k < DRAW_MULTI; k++)
        vt100_putc( ch );
    }
    for(k = 0; k < DRAW_MULTI; k++)
      vt100_putc( CHAR_WALL );
    
    // a real VT52 wants both linefeed (10) and carriage-return (13).
    // We don't want a linefeed after the last row, because the terminal
    // would scroll up then... anyway, we use vt100_goto() now.
    // 
    // vt100_putc( 13 );
    // vt100_putc( 10 );
  }
}


/**
 * display the current level and score values on the terminal.
 */
void display_score( void ) {
  vt100_goto( 20, 40 );
/* uncommented to free some program memory for more important things...
  vt100_putc( 'L' );
  vt100_putc( 'e' );
  vt100_putc( 'v' );
  vt100_putc( 'e' );
  vt100_putc( 'l' );
  vt100_putc( ':' );
  vt100_putc( ' ' );
*/
  vt100_xtoa( level );
  
  vt100_putc( ' ' );
/*
  vt100_goto( 21, 40 );
  vt100_putc( 'S' );
  vt100_putc( 'c' );
  vt100_putc( 'o' );
  vt100_putc( 'r' );
  vt100_putc( 'e' );
  vt100_putc( ':' );
  vt100_putc( ' ' );
*/
  vt100_xtoa( (score>>8) );
  vt100_xtoa( (score) );
//vt100_putc( '\n' );

}


/**
 * check for completed rows and remove them from the gaming board.
 */
void check_remove_completed_rows( void ) {
  unsigned char r, flag;
  
  flag = 0;
  for( r=0; r < ROWS; r++ ) {
  	if (is_complete_row(r)) {
  	  flag = 1;
  	  remove_row( r );
          if(++lines == 3)
          {
            lines = 0;
            if(level < 9)
              level_speed(++level);
          }
  	}
  }
  
  if (flag) {
  	display_board();
  	display_score();
  }
}


/**
 * try to move the current block left.
 * This function updates the current_col variable, but does not
 * repaint the current block.
 */
void cmd_move_left( void ) {
  current_col --;
  if (test_if_block_fits()) return;
  
  // if we arrive here, the block doesn't fit,
  // and we simply undo the column change.
  current_col ++;	
}


/**
 * try to move the current block right.
 * This function updates the current_col variable, but does not
 * repaint the current block.
 */
void cmd_move_right( void ) {
 current_col++;
  if (test_if_block_fits()) return;
  
  // if we arrive here, the block doesn't fit,
  // and we simply undo the column change.
  current_col --;	
}	


/**
 * try to rotate the current block.
 * This function updates the current_block(0..3) variables, 
 * but does not repaint the current block.
 */
void cmd_rotate( void ) {
  rotate_block();
  if (test_if_block_fits()) return;
  
  // if we arrive here, the block doesn't fit,
  // and we undo the rotation by three more rotations...
  rotate_block();
  rotate_block();
  rotate_block();	
}


/**
 * attempt to move the current block one position down.
 * If it doesn't fit, copy the current block to the gaming board
 * and check for completed rows. If any completed rows are found,
 * check_remove_completed_rows() also automatically repaints the 
 * whole gaming board.
 * Finally, we create a new random block (and let our caller
 * handle repainting the new block).
 */
void cmd_move_down( void ) {
  current_row++;
  if (test_if_block_fits()) return; // fits
  
  if (current_row <= 2) { // already stuck right on top
  	state |= GAME_OVER;
  }
  
  // if we arrive here, the block doesn't fit,
  // and we have to copy it to the game board.
  // we also need a new random block...
  current_row--;
  display_block( PAINT_FIXED );  // this is now stuck
  
  copy_block_to_gameboard();
  check_remove_completed_rows(); // repaints all when necessary

  current_row = 0;
  current_col = 4;
  
  create_random_block();
  score += SCORE_PER_BLOCK;
  
  display_block( PAINT_ACTIVE );
}


/**
 * debugging display of the current block outside the gaming-board
 * area. 
 *
void display_test_block( void ) {
  unsigned char i, j, ch;
  
  for( i=0; i < 4; i++ ) {
  	vt100_goto( i+11, 30 );
  	for ( j=0; j < 4; j++ ) {
  	  ch = (getBlockPixel(i,j)) ? 'X' : '.';
  	  vt100_putc( ch );
  	}
  }
}
*/


void init_game( void ) {
  vt100_enter_vt52_mode();
  vt100_clear_screen();
  clear_board();
  display_board();

  current_row = 0;
  current_col = 4;
  create_random_block();
  score = SCORE_PER_BLOCK;  // one block created right now
  lines = 0;
  level = 1;
  level_speed(level);
  
  state = STATE_IDLE;
  command = CMD_NONE;
  display_block( PAINT_ACTIVE );
}


bit gameover( void ) {
  return (state & GAME_OVER) != 0;	
}


bit timeout( void ) {
  return (state & TIMEOUT) != 0;	
}


void check_handle_command( void ) {
  unsigned char tmp;
  
  if(command == CMD_QUIT)
    exit(0);
  
  // if the game is over, we only react to the 's' restart command.
  // 
  if (gameover()) {
  	if (command == CMD_START) {
  	  init_game();	
  	}
  	display_score(); // flickers somewhat :-(
  	return;
  }
	
  // first check game status and a possible timeout. 
  // (we don't want the user to block timeouts by just overflowing
  // us with keystrokes :-))
  //
  if (timeout()) {
  	display_block( ERASE );
    cmd_move_down();
    display_block( PAINT_ACTIVE );
    state |= TIMEOUT;
    state ^= TIMEOUT; // reset timeout flag
    return;
  }

  // now, check whether we have a command. If so, handle it.
  //  
  tmp = command;
  command = CMD_NONE;
  
  switch( tmp ) {
  	
    case CMD_NONE: // idle
    
                break;

  	case CMD_LEFT: // try to move the current block left
  	            display_block( ERASE );
  	            cmd_move_left();
  	            display_block( PAINT_ACTIVE );
  	            break;

  	case CMD_ROTATE: // try to rotate the current block
  	            display_block( ERASE );
  	            cmd_rotate();
  	            display_block( PAINT_ACTIVE );
  	            break;

  	case CMD_RIGHT: // try to move the current block right
  	            display_block( ERASE );
  	            cmd_move_right();
  	            display_block( PAINT_ACTIVE );
                break;

  	case CMD_DROP: // drop the current block
  	            display_block( ERASE );
  	            for( ; test_if_block_fits(); ) {
  	              current_row++;
  	            }
  	            
  	            // now one step back, then paint the block in its
  	            // final position.
  	            current_row--;
  	            display_block( PAINT_FIXED );

                // this checks for completed rows and does the
                // major cleanup and redraw if necessary.
  	            cmd_move_down();
  	            break;

  	case CMD_REDRAW: // redraw everything
  	            display_board();
  	            display_score();
  	            display_block( PAINT_ACTIVE );
  	            break;

  	case CMD_START: // quit and (re-) start the game
  	            init_game();
  	            break;
  	            
    default:
  	            // do nothing
  	            break;
  	}
}

void isr( void ) {
  int tim[3];
  int r;
  int mode;
  char buf[2];
  r = read(0, buf, 1);
  if (r > 0)
  {
    command = buf[0];
  } 
  else
  { 
    command = 0;
    state |= TIMEOUT;
  }
  fflush(stdout);
}


void main(void)
{
  vt100_initialize();  // setup the rx/tx and timer parameters
  init_game();              // initialize the game-board and stuff

  for( ;; ) {
  	check_handle_command();
  	isr();
  }
}
