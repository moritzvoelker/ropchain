TARGET = ropchain
OPT_LVL = g
CC = clang
CC_FLAGS = -O$(OPT_LVL) -g -Wall -Wextra -Wpedantic -fno-stack-protector -o $(TARGET)

SRC = $(wildcard *.c *.S)

.PHONY = clean

TARGET: $(SRC)
	$(CC) $(CC_FLAGS) $(SRC)

clean:
	rm $(TARGET)
	rm stack.bin
