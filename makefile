CC = clang

CFLAGS = -std=gnu99 -Wall -Wfloat-equal -c

LDFLAGS =
EXECUTABLE = server


all: $(EXECUTABLE)

$(EXECUTABLE): server.o
	$(CC) $(LDFLAGS) $^ -o $(EXECUTABLE)

server.o: server.c
	$(CC) $(CFLAGS) $< -o $@



clean:
	@rm *.o $(EXECUTABLE)
