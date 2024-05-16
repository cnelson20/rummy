all: RUMMY.PRG

CC = cl65
FLAGS = -t cx16 -Cl -Ois -m game.map

RUMMY.PRG: game.c game.h helper.s helper.h
	$(CC) $(FLAGS) -o $@ game.c helper.s

