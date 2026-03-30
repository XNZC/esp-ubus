CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -L /usr/local/lib

INCLUDE_PATHS = -I/usr/local/include
EXTRA = -lserialport -lubus -lblobmsg_json -lubox

TARGET=../ubus-esp

BUILD_DIR=../build

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $(TARGET) $(BUILD_DIR)/*.o $(EXTRA)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE_PATHS) -c $< -o $(BUILD_DIR)/$@