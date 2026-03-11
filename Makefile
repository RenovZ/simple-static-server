TARGET = 3s

all: $(TARGET)

$(TARGET): build.zig src/main.zig
	zig build -Doptimize=ReleaseSafe
	ln -sf zig-out/bin/$(TARGET) $(TARGET)

clean:
	rm -rf .zig-cache zig-out $(TARGET)

.PHONY: all clean
