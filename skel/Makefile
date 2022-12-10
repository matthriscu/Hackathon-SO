CC=gcc
CFLAGS=-fPIC -Wall -Wextra -g
LDFLAGS=-ldl
OUTPUT_DIR=$(abspath ../checker/output/)
CFLAGS+=-DOUTPUT_TEMPLATE="\"$(OUTPUT_DIR)/out-XXXXXX\""
LDLIBS=./libipc.so

build: outdir server client

outdir:
	@mkdir -p $(OUTPUT_DIR)

server: server.o libipc.so
	$(CC) -o $@ $< $(LDLIBS) $(LDFLAGS)

client: client.o libipc.so
	$(CC) -o $@ $< $(LDLIBS) $(LDFLAGS)

server.o: server.c
	$(CC) $(CFLAGS) -o $@ -c $<

client.o: client.c
	$(CC) $(CFLAGS) -o $@ -c $<

libipc.so: ipc.o
	$(CC) -shared -o $@ $^

ipc.o: ipc.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -rf server client *.o libipc.so
