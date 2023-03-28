all: screenshot.c
	gcc -lX11 -lXcursor -O3 -lpng -o ss screenshot.c
