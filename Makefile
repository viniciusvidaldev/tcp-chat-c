CC     ?= cc
CFLAGS ?= -std=c23 -Wall -Wextra -O2 -g
LDLIBS ?= -lpthread

BUILD  := build
SHARED := framer.c net.c proto.c

SERVER      := $(BUILD)/server
SERVER_SRCS := server.c chat.c conn.c packet.c sendq.c $(SHARED)

CLIENT      := $(BUILD)/client
CLIENT_SRCS := client.c $(SHARED)

.PHONY: all server client clean

all: server client

server: $(SERVER)
client: $(CLIENT)

$(SERVER): $(SERVER_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) $(SERVER_SRCS) -o $@ $(LDLIBS)

$(CLIENT): $(CLIENT_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) $(CLIENT_SRCS) -o $@ $(LDLIBS)

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
