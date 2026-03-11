CC = gcc
CFLAGS = -W -Wall -Wextra -Werror

TARGET = 3s

SOURCES_DIR = src
SOURCES = $(wildcard $(SOURCES_DIR)/*.c)

OBJECTS_DIR = obj
OBJECTS = $(patsubst $(SOURCES_DIR)/*.c, $(OBJECTS_DIR)/%.o, $(SOURCES))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJECTS_DIR)/%.o: $(SOURCES_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rvf $(OBJECTS_DIR) $(TARGET)

.PHONY: all clean
