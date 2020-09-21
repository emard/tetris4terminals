#ifndef _TETRIS_H_
#define _TETRIS_H_

#endif //_TETRIS_H_

typedef unsigned char bit;

// utility functions
// 
void error( void );
unsigned char power_of_two( unsigned char nbits );


// current block creation and manipulation
//
unsigned char getRawBlockPattern( unsigned char index );
void create_block( unsigned char index );
void create_random_block( void );
unsigned char getBlockNibble( unsigned char i );
void updateBlockNibble( unsigned char i, unsigned char mask );
void rotate_block( void );
bit getBlockPixel( unsigned char row, unsigned char col );


// game board manipulation and access
//
void setPixel( unsigned char row, unsigned char col, unsigned char val );
bit occupied( unsigned char row, unsigned char col );
void clear_board( void );
bit test_if_block_fits( void );
void copy_block_to_gameboard( void );
bit is_complete_row( unsigned char r );
void remove_row( unsigned char row );
void check_remove_completed_rows( void );


// move current block 
//
void cmd_move_left( void );
void cmd_move_right( void );
void cmd_rotate( void );
void cmd_move_down( void );
void check_handle_command( void );


// RS-232 communication and drawing stuff
//
void vt100_initialize( void );
void vt100_enter_vt52_mode( void );
void vt100_putc( unsigned char ch );
void vt100_goto( unsigned char row, unsigned char col );
void vt100_cursor_home( void );
unsigned char vt100_hex( unsigned char val );
void vt100_xtoa( unsigned char val );

void display_board( void );
void display_block( unsigned char paintMode );
void display_score( void );

// main init
// 
void init( void );





