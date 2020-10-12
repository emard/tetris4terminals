# Tetris for Terminals

This is minimalistic tetris for unix with VT100 (color/mono) or VT52 (mono) terminals,
ultra-light dependencies, no curses, no printf, terminal control sequences hardcoded inside.

Code is based on slightly updated:
[Pic16F628 Tetris for Terminals](https://tams.informatik.uni-hamburg.de/applets/hades/webdemos/95-dpi/pic16f628-tetris/tetris.html)
and adapted to compile with LCC native compiler running on saxonsoc.
Can be also compiled with GCC.

Few usage examples:
    
     make                : compile with GCC for normal unix
    ./build.sh           : compile with LCC for saxonsoc linux
    ./tetris             : default tetris for VT100 color
    ./tetris -h          : print options and key usage
    ./tetris > /dev/tty1 : take input from stdin, draw on /dev/tty1 terminal

# Screenshot

    ||                    ||
    ||                    ||
    ||                    ||
    ||                    ||
    ||                    ||
    ||        HH          ||
    ||        HHHHHH      ||
    ||                    ||
    ||                    ||
    ||                    ||
    ||                    ||
    ||  XX                ||
    ||XXXXXX    XXXXXXXX  ||
    ||XXXXXXXXXXXXXX  XXXX||
    ||XXXXXXXX  XXXXXXXX  ||
    ||XXXX  XXXXXX    XX  ||
    ||XXXXXXXX  XXXXXXXXXX||
    ||  XXXXXXXXXXXX  XXXX||
    ||XXXXXXXX  XXXXXXXXXX||
    ||XXXX  XXXXXXXX  XXXX||
    ||XXXXXXXX  XX    XXXX||            Level: 01 
    ||  XX  XXXXXXXXXXXXXX||            Score: 0010
    ||  XXXXXXXX    XXXX  ||
    ||||||||||||||||||||||||

![Sceenshot](/pic/tetris.png)
