CC := gcc
CFLAGS := -ggdb -O0 -Wall -Wextra -ldl -Werror -Wno-unused-parameter
PROD_CFLAGS := -ggdb -O0 -Wall -Wextra -ldl
LDFLAGS := -L/usr/local/lib -ltermbox2
SOURCES := editor.c main.c buffer.c cursor.c window.c
OBJECTS := $(SOURCES:.c=.o)
EXECUTABLE := nv

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $^ $(LDFLAGS) -o $@

debug:
	gdb --args ./$(EXECUTABLE) main.c window.c

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all --log-file="valgrind" ./$(EXECUTABLE) main.c

run: $(EXECUTABLE)
	./$(EXECUTABLE) main.c window.c

.PHONY: all clean run valgrind
