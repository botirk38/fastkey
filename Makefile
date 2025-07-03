CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -g -D_GNU_SOURCE
LDFLAGS = -lpthread

SRC_DIR = app
TEST_DIR = tests
OBJ_DIR = obj
TEST_OBJ_DIR = test_obj

SOURCES = $(wildcard $(SRC_DIR)/*.c)
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.c=$(TEST_OBJ_DIR)/%.o)

# Exclude main.c from library objects for testing
LIB_OBJECTS = $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS))

TARGET = fastkey
TEST_TARGET = test_runner

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS) $(LIB_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(LIB_OBJECTS) -o $@ $(LDFLAGS)

$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(TEST_OBJ_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TEST_OBJ_DIR) $(TARGET) $(TEST_TARGET)

.PHONY: all clean test