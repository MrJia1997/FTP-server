CUR_DIR = $(shell pwd)

OBJECTS = server.o server_functions.o global.o commands.o


server: $(OBJECTS)
	cc -o server $(OBJECTS)

server.o: server.h
server_functions.o: server.h
global.o: server.h
commands.o: server.h

.PHONY: clean
clean:
	-rm server $(OBJECTS)

