xanim: main.o gopt.o gopt-errors.o
	g++ -o xanim main.o gopt.o gopt-errors.o -lSDL2 -lSDL2_image -lX11 -lopencv_core -lopencv_videoio -lopencv_imgproc

main.o: main.cpp
	g++ -c main.cpp -ggdb

gopt.o: gopt.c gopt.h
	gcc -c gopt.c

gopt-errors.o: gopt-errors.c gopt.h
	gcc -c gopt-errors.c
