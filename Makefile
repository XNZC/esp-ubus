CC = gcc

SRC_DIR = src
BUILD_DIR = build

TARGET=ubus-esp

.PHONY: clean src

all: $(BUILD_DIR) src

src:
	$(MAKE) -C $(SRC_DIR)

$(BUILD_DIR):
	@mkdir build

clean:
	@rm -f $(TARGET)
	@rm -rf $(BUILD_DIR)
