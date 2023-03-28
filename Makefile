all: screenshot.c
	gcc -lX11 -lXcursor -O3 -lpng -o min-ss screenshot.c
