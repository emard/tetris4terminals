#!/bin/sh
PATH=/opt/riscv32_lcc/lcc/bin/:$PATH
lcc tetris.c -o tetris
chmod +x tetris
