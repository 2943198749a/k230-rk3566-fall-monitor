CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
TARGET ?= build/k230_status_client

all: $(TARGET)

$(TARGET): src/status_client.c
	mkdir -p build
	$(CC) $(CFLAGS) $< -o $@ -lm

test: $(TARGET)
	python3 tests/test_client.py

.PHONY: all test
