CC=gcc
CFLAGS=-g
LFLAGS=-lrt -lpthread

OUT_DIR=build
BIN_DIR=bin

SRCS=$(wildcard *.c)
OBJS=$(patsubst %.c,$(OUT_DIR)/%.o,$(SRCS))

all:both
	@echo done

$(OUT_DIR)/%.o:%.c
	@echo compiling $^...
	@mkdir -p $(OUT_DIR)
	$(CC) -c $< $(CFLAGS) -o $@

clean:
	rm -rf $(OUT_DIR)/
	rm -rf $(BIN_DIR)/

both:$(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(OUT_DIR)/server.o -o $(BIN_DIR)/server $(LFLAGS)
