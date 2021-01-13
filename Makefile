LDFLAGS = -lSDL2 -lSDL2_image -lX11 -lopencv_core -lopencv_videoio -lopencv_imgproc
CC = g++ -std=c++14
BIN = xanim

$(BIN): main.o gopt.o gopt-errors.o
	$(CC) -o $(BIN) main.o gopt.o gopt-errors.o $(LDFLAGS)
main.o: main.cpp
	$(CC) -c main.cpp -ggdb

gopt.o: gopt.c gopt.h
	$(CC) -c gopt.c

gopt-errors.o: gopt-errors.c gopt.h
	$(CC) -c gopt-errors.c

install: $(BIN)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(BIN) $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(BIN)

uninstall:
	rm $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -f $(BIN) *.o
