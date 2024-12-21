
# Directories
BIN_DIR = bin
LIB_DIR = lib
INC_DIR = include

# Compiler and flags
CC = cc
CFLAGS = -Wall -Wextra -std=c99 -g -I$(INC_DIR)

# Source and object files
LIB_SOURCES = $(wildcard $(LIB_DIR)/*.c)
LIB_OBJECTS = $(LIB_SOURCES:$(LIB_DIR)/%.c=$(BIN_DIR)/%.o)

MAIN_SOURCE = main.c
MAIN_OBJECT = $(BIN_DIR)/main.o

# Target executable
TARGET = $(BIN_DIR)/clox

# Ensure bin directory exists
$(shell mkdir $(BIN_DIR))

# Build the executable
$(TARGET): $(LIB_OBJECTS) $(MAIN_OBJECT)
	$(CC) $(CFLAGS) -o $@ $^

# Compile main.c
$(MAIN_OBJECT): $(MAIN_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile library object files
$(BIN_DIR)/%.o: $(LIB_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(BIN_DIR)/*.o $(TARGET)
