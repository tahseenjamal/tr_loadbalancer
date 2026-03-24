CC := gcc

CFLAGS_COMMON := -Wall -Wextra -std=c11
CFLAGS_DEBUG  := $(CFLAGS_COMMON) -O0 -g
CFLAGS_REL    := $(CFLAGS_COMMON) -O2

LDFLAGS := -lsctp

TARGET := fe_router

SRC := \
	main.c \
	config.c \
	net.c \
	m3ua.c \
	sccp.c \
	tcap.c \
	redis_store.c \
	tr_registry.c \
	upstream.c \
	router.c

OBJ := $(SRC:.c=.o)

.PHONY: all debug release clean run

all: release

release: CFLAGS := $(CFLAGS_REL)
release: $(TARGET)

debug: CFLAGS := $(CFLAGS_DEBUG)
debug: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
