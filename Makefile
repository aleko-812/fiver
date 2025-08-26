# Makefile for fiver CLI tool

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS =

# Source files
SOURCES = fiver.c storage_system.c delta_algorithm.c rolling_hash.c hash_table.c
TARGET = fiver

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# Clean build artifacts
clean:
	rm -f $(TARGET) *.o

# Install to system (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Uninstall from system
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Run automated tests
test: $(TARGET)
	@echo "Running automated tests..."
	./test_track_command.sh

# Development target with debug info
debug: CFLAGS += -DDEBUG -g3
debug: $(TARGET)

# Release target with optimizations
release: CFLAGS += -O3 -DNDEBUG
release: clean $(TARGET)

# Show help
help:
	@echo "Available targets:"
	@echo "  all      - Build the fiver executable (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install to /usr/local/bin/"
	@echo "  uninstall- Remove from /usr/local/bin/"
	@echo "  test     - Run basic CLI tests"
	@echo "  debug    - Build with debug information"
	@echo "  release  - Build optimized release version"
	@echo "  help     - Show this help message"

.PHONY: all clean install uninstall test debug release help
