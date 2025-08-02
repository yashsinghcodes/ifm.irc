CC=clang
CFLAGS= -Wall -O2

SRC=ifm.c
OBJ=$(SRC:.c=.o)
TARGET=app

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
