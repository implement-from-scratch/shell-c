CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -I../include
LDFLAGS = 

# Source files
PARSER_SRC = ../src/parser.c
SHELL_SRC = ../src/shell.c

# Test executables
TEST_PARSER = test_parser
TEST_MEMORY = test_memory

# Default target
all: $(TEST_PARSER) $(TEST_MEMORY)

# Parser tests
$(TEST_PARSER): test_parser.c $(PARSER_SRC)
	$(CC) $(CFLAGS) -o $(TEST_PARSER) test_parser.c $(PARSER_SRC) $(LDFLAGS)

# Memory tests
$(TEST_MEMORY): test_memory.c $(PARSER_SRC)
	$(CC) $(CFLAGS) -o $(TEST_MEMORY) test_memory.c $(PARSER_SRC) $(LDFLAGS)

# Run all tests
test: all
	@echo "Running all tests..."
	@./$(TEST_PARSER) && ./$(TEST_MEMORY) && echo "All tests passed!"

# Clean build artifacts
clean:
	rm -f $(TEST_PARSER) $(TEST_MEMORY)

.PHONY: all test clean
