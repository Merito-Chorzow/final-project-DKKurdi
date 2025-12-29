CC=gcc
CFLAGS=-std=c11 -O0 -g -Wall -Wextra -Wpedantic -Iinclude

SRC=$(wildcard src/*.c) $(wildcard app/*.c) $(wildcard drivers/*.c)
OUTDIR=build
OUT=$(OUTDIR)/app

all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: all
	./$(OUT)

clean:
	rm -rf $(OUTDIR)

.PHONY: all run clean
