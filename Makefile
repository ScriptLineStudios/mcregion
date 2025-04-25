mcregion: main.c
	gcc -Wall -Wextra -pedantic -o mcregion main.c libnbt/test.o cJSON/cJSON.c -lm -O3 -g