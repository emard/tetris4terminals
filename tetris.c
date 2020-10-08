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
 * RX/TX settings in terminal_initialize().
 * 
 * Use the following keys to control the game:
 * 
 * 'j'        - move current block left
 * 'l'        - move current block right
 * 'k'        - rotate current block counter-clockwise
 * 'i'        - rotate current block clockwise
 * 'space'    - drop current block
 * 'r'        - redraw the screen
 * 's'        - start new game (or give up current game)
 * 'q'        - quit
 * ctrl-'c'   - quit
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
#include <time.h>

// graphics chars printed
#define CHAR_SPACE  ' '
#define CHAR_WALL   '|'
#define CHAR_FLOOR  '|'
#define CHAR_ACTIVE 'H'
#define CHAR_FIXED  'X'

// 1-single char (for 8x8 font), 2-double char (for 8x16 font) ...
unsigned char DRAW_multi = 2;

// switch VT100 to VT52 mode and use VT52 controls
// 0: VT100 default (may have color)
// 1: VT100->VT52 (no color)
unsigned char VT52_mode = 0;

// VT100 color or mono
// 0: VT100 monochrome
// 1: VT100 color
unsigned char VT100_color = 1;

// VT100 terminal scroll
// 0: redraw board (fixed blocks loose color after scrolling)
// 1: VT100 scroll (fixed blocks keep color after scrolling)
unsigned char VT100_scroll = 1;

// max level
//  9: (default), 0.1s delay between steps
// 10: difficult, no delay between steps (max terminal speed)
unsigned char MAX_level = 9;

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

// saxonsoc needs this
struct timespec_local {
  time_t tv_sec;
  long tv_nsec;
};
struct timespec_local time_now;

long time_now_ms, time_next_ms;
long step_ms = 200; // 0.2s

#define MS_WRAPAROUND 10000
#define MS_TIMEOUT    1500

#define ROWS  ((unsigned char) 24) // must be divisible by 8
#define COLS  ((unsigned char) 10)

#define ROWSD ((unsigned char) 23) // last N rows of active gamefield displayed
#define ROW0  (ROWS-ROWSD) // first row displayed
#define ROWNEW 0 // ROW in memory where new piece appears

#define PAINT_FIXED  ((unsigned char) 2)
#define PAINT_ACTIVE ((unsigned char) 1)
#define ERASE        ((unsigned char) 0)

#define XOFFSET ((unsigned char) 3)
#define XLIMIT  ((unsigned char) (XOFFSET+COLS))

#define BOARD_BYTES ((ROWS>>3)*10)

static unsigned char board[BOARD_BYTES];         // the main game-board

static unsigned char current_index;     // index of the current block (for colorization)
static unsigned char current_block0;    // bit-pattern of the current block,
static unsigned char current_block1;    // with one four-bit bitmap stored
static unsigned char current_block2;    // in the lower nibble of each of these
static unsigned char current_block3;    // variables
static unsigned char current_rotation;

static signed char current_row;         // row of the current block
static signed char current_col;         // col of the current block

static unsigned char lines;
static unsigned char level;
static unsigned int  score;

#define STATE_IDLE   ((unsigned char) 0x1)
#define TIMEOUT      ((unsigned char) 0x2)
#define NEEDS_INIT   ((unsigned char) 0x4) 
#define GAME_OVER    ((unsigned char) 0x8) 

#define CMD_NONE     ((unsigned char) 0)
#define CMD_LEFT     ((unsigned char) 'j')
#define CMD_RIGHT    ((unsigned char) 'l')
#define CMD_ROTATE_CCW ((unsigned char) 'k')
#define CMD_ROTATE_CW  ((unsigned char) 'i')
#define CMD_DROP     ((unsigned char) ' ')
#define CMD_REDRAW   ((unsigned char) 'r')
#define CMD_START    ((unsigned char) 's')
#define CMD_QUIT     ((unsigned char) 'q')
#define CMD_CTRLC    ((unsigned char) 'C'-'@')

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

// VT100 block colors (first 8 matters)
unsigned char index2color[] = {
 47,  // white box 2x2
 103, // yellow box 2x2
 45,  // lilac T-shape
 46,  // cyan straight 1x4
 42,  // green S-shape
 41,  // red Z-shape
 43,  // orange L-shape
 44,  // blue J-shape
 100, // gray
 101, // bright red
 102, // bright green
 103, // yellow
 104, // bright blue
 105, // bright lilac
 106, // bright cyan
};

/* each block in 4 rotations
 * utility lookup table that returns the bit-patterns of the seven
 * predefined types (shapes) of Tetris blocks via ROM lookups.
 * We use a 4x4 matrix packed into two unsigned chars as
 * (first-row << 4 | second-row), (third-row << 4 | fourth-row).
 * note on the screen it will be mirrored left-right
*/
unsigned char rotated_block_pattern[] = {
  // yellow square 2x2, all rotations the same
  0x06, //  0:0b 0000 0110
  0x60, //  1:0b 0110 0000

  0x06, //  2:0b 0000 0110
  0x60, //  3:0b 0110 0000

  0x06, //  4:0b 0000 0110
  0x60, //  5:0b 0110 0000

  0x06, //  6:0b 0000 0110
  0x60, //  7:0b 0110 0000

  // yellow square 2x2, all rotations the same
  0x06, //  8:0b 0000 0110
  0x60, //  9:0b 0110 0000

  0x06, // 10:0b 0000 0110
  0x60, // 11:0b 0110 0000

  0x06, // 12:0b 0000 0110
  0x60, // 13:0b 0110 0000

  0x06, // 14:0b 0000 0110
  0x60, // 15:0b 0110 0000

  // lilac T-shape block
  // xxx
  //  x
  0x0E, // 16:0b 0000 1110
  0x40, // 17:0b 0100 0000

  0x4C, // 18:0b 0100 1100
  0x40, // 19:0b 0100 0000

  0x4E, // 20:0b 0100 1110
  0x00, // 21:0b 0000 0000

  0x46, // 22:0b 0100 0110
  0x40, // 23:0b 0100 0000

  // cyan 1x4 block
  0x44, // 24:0b 0100 0100
  0x44, // 25:0b 0100 0100

  0x0F, // 26:0b 0000 1111
  0x00, // 27:0b 0000 0000

  0x44, // 28:0b 0100 0100
  0x44, // 29:0b 0100 0100

  0x0F, // 30:0b 0000 1111
  0x00, // 31:0b 0000 0000

  // red 2+2 shifted block
  // xx
  //  xx
  0x0C, // 32:0b 0000 1100
  0x60, // 33:0b 0110 0000

  //  x
  // xx
  // x
  0x02, // 34:0b 0000 0010
  0x64, // 35:0b 0110 0100

  // xx
  //  xx
  0x0C, // 36:0b 0000 1100
  0x60, // 37:0b 0110 0000

  //  x
  // xx
  // x
  0x02, // 38:0b 0000 0010
  0x64, // 39:0b 0110 0100

  // green 2+2 shifted block (inverse to red)
  //  xx
  // xx
  0x06, // 40:0b 0000 0110
  0xC0, // 41:0b 1100 0000

  // x
  // xx
  //  x
  0x04, // 42:0b 0000 0100
  0x62, // 43:0b 0110 0010

  //  xx
  // xx
  0x06, // 44:0b 0000 0110
  0xC0, // 45:0b 1100 0000

  // x
  // xx
  //  x
  0x04, // 46:0b 0000 0100
  0x62, // 47:0b 0110 0010

  // blue 3+1 L-shaped block
  // xxx
  //  .x
  0x0E, // 48:0b 0000 1110
  0x20, // 49:0b 0010 0000

  //   x
  //  .x
  //  xx
  0x02, // 50:0b 0000 0010
  0x26, // 51:0b 0010 0110

  //
  // x.
  // xxx
  0x00, // 52:0b 0000 0000
  0x8E, // 53:0b 1000 1110

  // xx
  // x.
  // x 
  0x0C, // 54:0b 0000 1100
  0x88, // 55:0b 1000 1000

  // orange 1+3 L-shaped block
  // xxx
  // x.
  0x0E, // 56:0b 0000 1110
  0x80, // 57:0b 1000 0000

  //  xx
  //  .x
  //   x
  0x06, // 58:0b 0000 0110
  0x22, // 59:0b 0010 0010

  //  .x
  // xxx
  0x00, // 60:0b 0000 0000
  0x2E, // 61:0b 0010 1110

  // x
  // x.
  // xx
  0x08, // 62:0b 0000 1000
  0x8C, // 63:0b 1000 1100
};


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
void create_rotated_block( unsigned char index, unsigned char rotation ) {
  unsigned char tmp;
  unsigned char i;
  
  i = (index<<3) + ((rotation&3)<<1);

  tmp = rotated_block_pattern[i];
  current_block0 = (tmp & 0xf0) >> 4;
  current_block1 = (tmp & 0x0f);

  tmp = rotated_block_pattern[i+1];
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
  unsigned char x = 0;
  while(x == 0)
    x = rand() & 7;
  // x is now random 1-7
  create_rotated_block(x,0);
  current_index = x;
  current_rotation = 0;
}


void rotate_block( char r ) {
  current_rotation = (current_rotation+r)&3;
  create_rotated_block(current_index, current_rotation);
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

void vt100_default_color()
{
  vt100_putc( 27 );
  vt100_putc( '[' );
  vt100_putc( '4' );
  vt100_putc( '9' );
  vt100_putc( 'm' );
  vt100_putc( 27 );
  vt100_putc( '[' );
  vt100_putc( 'm' );
}

void vt100_default_scroll_region()
{
  vt100_putc( 27 );
  vt100_putc( '[' );
  vt100_putc( '1' );
  vt100_putc( ';' );
  vt100_putc( '2' );
  vt100_putc( '4' );
  vt100_putc( 'r' );
}

void vt100_full_screen()
{
  vt100_default_scroll_region();
  // cursor down
  vt100_putc( 27 );
  vt100_putc( '[' );
  vt100_putc( '2' );
  vt100_putc( '4' );
  vt100_putc( ';' );
  vt100_putc( '1' );
  vt100_putc( 'H' );
}

void reset_terminal_mode()
{
  int r;
  if(VT52_mode)
    vt100_exit_vt52_mode();
  else
  {
    if(VT100_color)
      vt100_default_color();
    if(VT100_scroll)
      vt100_full_screen();
  }
  r = ioctl(0, TCSETS, &orig_termios);
}


/**
 * initialize the serial communication parameters.
 * We also put some timer initialization here. 
 */
void terminal_initialize( void ) {
  int r;
  char buf[2];

  buf[1] = 0;

  r = ioctl(0, TCGETS, &orig_termios);
  current_termios = orig_termios;

  current_termios.c_lflag &= ~(ECHO | ICANON | ISIG);

  current_termios.c_cc[VMIN] = 0;
  current_termios.c_cc[VTIME] = 1; // *0.1s timeout

  r = ioctl(0, TCSETS, &current_termios);

  atexit(reset_terminal_mode);
}


/** 
 * request a VT100 cursor-home command on the terminal via tx.
 * We send the VT100/VT52 command 'ESC H'.
 */
void vt100_cursor_home( void ) {
  if(VT52_mode)
  {
    vt100_putc( 27 );    // ESC
    vt100_putc( 'H' );
  }
  else
  {
    vt100_putc( 27 );    // ESC
    vt100_putc( '[' );
    vt100_putc( 'H' );
  }
}


/** clear screen
 */
void vt100_clear_screen(void)
{
  vt100_cursor_home();
  if(VT52_mode)
  {
    vt100_putc(27);
    vt100_putc('J');  // clear to end of screen
  }
  else
  {
    vt100_putc( 27 );    // ESC
    vt100_putc( '[' );
    vt100_putc( 'J' );  // clear to end of screen
  }
}


void vt100_beep(void)
{
  vt100_putc(7);
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
  c = (val / 10) & 0x0f;
  vt100_putc( vt100_hex( c ));
  c = (val % 10);
  vt100_putc( vt100_hex( c ));
}


void vt100_itoa( unsigned char val ) {
  unsigned char c;
  if(val >= 10)
  {
    c = (val / 10) & 0x0f;
    vt100_putc( vt100_hex( c ));
  }
  c = (val % 10);
  vt100_putc( vt100_hex( c ));
}

/**
 * move the VT100 cursor to the given position.
 * This is done by sending 'ESC Y l c'
 * NOTE: VT52 expects an offset of 32 for the l and c values.
 */
void vt100_goto( unsigned char row, unsigned char col )
{
  if(VT52_mode)
  {
    vt100_putc( 27 );   // ESC
    vt100_putc( 'Y' );  // ESC-Y
    vt100_putc( (row+32) );
    vt100_putc( (col+32) );
  }
  else
  {
    vt100_putc( 27 );    // ESC
    vt100_putc( '[' );
    vt100_itoa(row+1);
    vt100_putc( ';' );
    vt100_itoa(col+1);
    vt100_putc( 'H' );  // set cursor
  }
}

void vt100_bgcolor(unsigned char color)
{
  vt100_putc( 27 );
  vt100_putc( '[' );
  if(color >= 100)
    vt100_putc( '5' );
  vt100_putc( 'm' );
  vt100_putc(27);    // ESC
  vt100_putc('[');
  if(color >= 100)
  {
    vt100_putc('1');
    color -= 100;
  }
  vt100_xtoa(color);
  vt100_putc('m');  // set color
}


void vt100_scroll_region_down(unsigned char b)
{
  vt100_cursor_home(); // to top of region

  vt100_putc(27);    // ESC
  vt100_putc('[');
  vt100_putc('1');
  vt100_putc( ';' );
  vt100_itoa(b+1);
  vt100_putc( 'r' );  // set region

  vt100_putc(27);
  vt100_putc('M');   // at top of region, scroll down

  vt100_default_scroll_region();
}


void vt100_erase_to_end_of_line(void)
{
    vt100_putc(27);
    vt100_putc('[');
    vt100_putc('K');
}


void block_color(unsigned char paintMode)
{
  unsigned char bgcolor;
  if(VT52_mode == 0)
  {
    if(VT100_color)
    {
      bgcolor = 49; // default color (black)
      bgcolor = paintMode == PAINT_ACTIVE || paintMode == PAINT_FIXED ? index2color[current_index] : 49;
      vt100_bgcolor(bgcolor);
    }
  }
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

  block_color(paintMode);
  for( i=0; i < 4; i++ ) {
    rr = current_row + i;
    if (rr < ROW0 || rr >= ROWS) continue; // out of range

    for ( j=0; j < 4; j++ ) {
      cc = XOFFSET + current_col + j;
      if (cc >= XLIMIT) continue; // out of range
      if (getBlockPixel(i,j)) {
        vt100_goto( rr-ROW0, cc*DRAW_multi );
        if     (paintMode == PAINT_ACTIVE) draw = CHAR_ACTIVE;
        else if (paintMode == PAINT_FIXED) draw = CHAR_FIXED;
        else                               draw = CHAR_SPACE;
        for(k = 0; k < DRAW_multi; k++)
          vt100_putc(draw);
      }
    }
  	
    // HACK:
    // extra goto because neither xterm/seycon nor Winxp hyperterm
    // understand the VT52 "cursor off" command, and the blinking
    // cursor at the end of a block is really annoying...
    // a "real" VT100/VT52 does work fine without this.
    vt100_cursor_home();
  }
} // display_block


/** 
 * display the current game board position on the terminal.
 * This method redraws the whole gaming board including borders.
 * Use another call to display_block() to also draw the current block.
 */
void display_board( unsigned char rows ) {
  unsigned char r,c,k;
  unsigned char ch;

  vt100_cursor_home();
  for( r=0; r < rows; r++ ) {

    vt100_goto( r, 2*DRAW_multi );

    for(k = 0; k < DRAW_multi; k++)
      vt100_putc( CHAR_WALL );            // one row of the board: border, data, border

    for( c=0; c < COLS; c++ ) {
      ch = occupied(r+ROW0,c) ? CHAR_FIXED : CHAR_SPACE;
      for(k = 0; k < DRAW_multi; k++)
        vt100_putc( ch );
    }
    for(k = 0; k < DRAW_multi; k++)
      vt100_putc( CHAR_WALL );
    
    // a real VT52 wants both linefeed (10) and carriage-return (13).
    // We don't want a linefeed after the last row, because the terminal
    // would scroll up then... anyway, we use vt100_goto() now.
    // 
    // vt100_putc( 13 );
    // vt100_putc( 10 );
  }

  if(r == ROWSD)
  {
    // print floor
    vt100_goto( r, 2*DRAW_multi );
    for(k = 0; k < DRAW_multi*(COLS+2); k++)
      vt100_putc( CHAR_FLOOR );
  }
}

void erase_score( void ) {
  unsigned char i, j;
  for(j = 20; j < 22; j++)
  {
    vt100_goto(j, 40);
    vt100_erase_to_end_of_line();
  }
}

/**
 * display the current level and score values on the terminal.
 */
void display_score( void ) {
  vt100_goto( 20, 40 );
#if 1
  vt100_putc( 'L' );
  vt100_putc( 'e' );
  vt100_putc( 'v' );
  vt100_putc( 'e' );
  vt100_putc( 'l' );
  vt100_putc( ':' );
  vt100_putc( ' ' );
#endif
  vt100_xtoa( level );

  vt100_putc( ' ' );
#if 1
  vt100_goto( 21, 40 );
  vt100_putc( 'S' );
  vt100_putc( 'c' );
  vt100_putc( 'o' );
  vt100_putc( 'r' );
  vt100_putc( 'e' );
  vt100_putc( ':' );
  vt100_putc( ' ' );
#endif
  vt100_xtoa( (score/100) );
  vt100_xtoa( (score%100) );
  vt100_putc( '\n' );
}


/**
 * check for completed rows and remove them from the gaming board.
 */
void check_remove_completed_rows( void ) {
  unsigned char r, removed;

  removed = 0;
  for( r=0; r < ROWS; r++ ) {
    if (is_complete_row(r)) {
      removed++;
      break;
    }
  }

  if(VT52_mode == 0)
  {
    if(VT100_color)
      vt100_default_color();
    if(VT100_scroll)
      if(removed)
        erase_score();
  }

  removed = 0;
  for( r=0; r < ROWS; r++ ) {
    if (is_complete_row(r)) {
      removed++;
      remove_row( r );
      if(VT52_mode == 0)
        if(VT100_scroll)
          vt100_scroll_region_down(r-ROW0);
      if(++lines == 1)
      {
        lines = 0;
        if(level < MAX_level)
        {
          level++;
          step_ms = (step_ms*3)>>2; // *3/4
        }
      }
    }
  }
  
  if (removed) {
    vt100_beep();
    block_color(ERASE);
    if(VT52_mode == 0)
    {
      if(VT100_scroll)
        // redraw scrolled out walls on top
        display_board(removed);
      else
        display_board(ROWSD);
      if(VT100_color)
        vt100_default_color();
    }
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
void cmd_rotate( char r ) {
  rotate_block(r);
  if (test_if_block_fits()) return;
  
  // if we arrive here, the block doesn't fit,
  // and we undo the rotation by three more rotations...
  rotate_block(-r);
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
  
  if (current_row <= ROWNEW+2) { // already stuck right on top
  	state |= GAME_OVER;
  }
  
  // if we arrive here, the block doesn't fit,
  // and we have to copy it to the game board.
  // we also need a new random block...
  current_row--;
  display_block( PAINT_FIXED );  // this is now stuck
  
  copy_block_to_gameboard();
  check_remove_completed_rows(); // repaints all when necessary

  current_row = ROWNEW;
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

// wraparound 10000 ms (10s)
int time_ms()
{
  clock_gettime(0, &time_now); // reads time
  return (time_now.tv_sec%10)*1000 + time_now.tv_nsec/1000000;
}

int time_diff_ms()
{
  return (MS_WRAPAROUND + time_next_ms - time_ms()) % MS_WRAPAROUND;
}

void set_read_timeout(void)
{
  int time_diff;
  time_diff = time_diff_ms();
  if(time_diff > MS_TIMEOUT) // if(time_ms()>time_next_ms)
  {
    current_termios.c_cc[VTIME] = 0;
    time_next_ms = time_ms(); // CPU too slow -> skew
  }
  else
    current_termios.c_cc[VTIME] = time_diff / 100; // ms->0.1s
  ioctl(0, TCSETS, &current_termios);
}

void set_read_next_time(void)
{
  time_next_ms = (time_next_ms + step_ms) % MS_WRAPAROUND;
}

void init_game( void ) {
  if(VT52_mode)
    vt100_enter_vt52_mode();
  else
    if(VT100_color)
      vt100_default_color();
  vt100_clear_screen();
  clear_board();
  display_board(ROWSD);

  current_row = ROWNEW;
  current_col = 4;
  create_random_block();
  score = SCORE_PER_BLOCK;  // one block created right now
  lines = 0;
  level = 1;
  time_next_ms = time_ms();
  step_ms = 1000; // initial step 1s
  set_read_next_time();

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

  if(command == CMD_QUIT || command == CMD_CTRLC)
    exit(0);

  // if the game is over, we only react to the 's' restart command.
  // 
  if (gameover()) {
    step_ms = 1000; // reduce CPU usage during game over
    if (command == CMD_START) {
  	  init_game();	
    }
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

  switch( tmp )
  {
    case CMD_NONE: // idle
      break;

    case CMD_LEFT: // try to move the current block left
      display_block( ERASE );
      cmd_move_left();
      display_block( PAINT_ACTIVE );
      break;

    case CMD_ROTATE_CCW: // try to rotate the current block
      display_block( ERASE );
      cmd_rotate(1);
      display_block( PAINT_ACTIVE );
      break;

    case CMD_ROTATE_CW: // try to rotate the current block
      display_block( ERASE );
      cmd_rotate(-1);
      display_block( PAINT_ACTIVE );
      break;

    case CMD_RIGHT: // try to move the current block right
      display_block( ERASE );
      cmd_move_right();
      display_block( PAINT_ACTIVE );
      break;

    case CMD_DROP: // drop the current block
      display_block( ERASE );
      for( ; test_if_block_fits(); )
        current_row++;
	            
      // now one step back, then paint the block in its
      // final position.
      current_row--;
      display_block( PAINT_FIXED );

      // this checks for completed rows and does the
      // major cleanup and redraw if necessary.
      cmd_move_down();
      break;

    case CMD_REDRAW: // redraw everything
      if(VT52_mode == 0)
        if(VT100_color)
          vt100_default_color();
      vt100_clear_screen();
      display_board(ROWSD);
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

/*
void read_time()
{
  static long tprev;
  long tnow;
  long tdiff;
  tnow = time_ms();
  //tprev = time_prev.tv_nsec;
  tdiff = (MS_WRAPAROUND+tnow-tprev) % MS_WRAPAROUND;
  tprev = tnow;
  vt100_cursor_home();
  printf("%15d", tdiff);
  //printf("%15d %15d", time_now.tv_nsec, time_next.tv_nsec-time_now.tv_nsec);
}
*/


void isr( void ) {
  int tim[3];
  int r;
  int mode;
  int tnow;
  static int tnext, tprev;
  int tdiff;
  char buf[2];
  set_read_timeout();
  r = read(0, buf, 1);
  if (r > 0)
    command = buf[0];
  else
    command = 0;
  tdiff = time_diff_ms();
  if(tdiff > MS_TIMEOUT) // time_ms()>time_next_ms
  {
    set_read_next_time();
    if(command == 0)
      state |= TIMEOUT; // timeout ignores command
  }
  fflush(stdout);
}


int main(int argc, char *argv[])
{
  struct timespec_local tp;

  for (; argc>1 && argv[1][0]=='-'; argc--, argv++)
  {
    switch (argv[1][1])
    {
      case 'v': // VT100 -> VT52
        VT52_mode = 1;
        VT100_scroll = 0;
        VT100_color = 0;
        break;

      case 's': // VT100 no scroll controls (redraw board instead)
        VT100_scroll = 0;
        break;

      case 'm': // VT100 no color (monochrome)
        VT100_color = 0;
        break;

      case 'c': // single-width chars (good for 8x8 font)
        DRAW_multi = 1;
        break;

      case 'r': // randomize, each run new random sequence
        srand(time_ms());
        break;

      case 'x': // max level 10, max terminal speed
        MAX_level = 10;
        break;
      
      case 'h':
        puts("options:");
        puts(" -v  : VT100->VT52 mode (no colors)");
        puts(" -m  : VT100 monochrome (no colors)");
        puts(" -s  : VT100 no scroll controls (remove line by redrawing monochrome board)");
        puts(" -c  : single-char width for 8x8 font (instead of double-char for 8x8 font)");
        puts(" -r  : each run new random sequence (instead of always the same sequence)");
        puts(" -x  : enable level 10, max terminal speed, difficult/impossible to play");
        puts("use the following keys to control the game:");
        puts(" 'j' : move current block left");
        puts(" 'l' : move current block right");
        puts(" 'k' : rotate current block counter-clockwise");
        puts(" 'i' : rotate current block clockwise");
        puts(" ' ' : drop current block");
        puts(" 'r' : redraw the screen");
        puts(" 's' : start new game (or give up current game)");
        puts(" 'q' : quit (same as ctrl-c)");
        exit(0);
        break;
    }
  }

  terminal_initialize();  // setup the rx/tx and timer parameters
  init_game();            // initialize the game-board and stuff

  for( ;; ) {
  	check_handle_command();
  	isr();
  }
  return 0;
}
