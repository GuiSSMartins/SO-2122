CC = gcc
CFLAGS = -Wall -g

all: folder server client transfs
server: bin/sdstored
client: bin/sdstore
transfs: bin/nop bin/gcompress bin/gdecompress bin/bcompress bin/bdecompress bin/encrypt bin/decrypt


folder:
	mkdir -p obj
	mkdir -p namedpipe


# --------------
# -- sdstored --
# --------------

bin/sdstored: obj/sdstored.o
	$(CC) $(CFLAGS) $< -o $@

obj/sdstored.o: src/sdstored.c includes/request.h includes/process.h includes/reply.h includes/tprocess.h includes/transfs.h
	$(CC) $(CFLAGS) -c $< -o $@


# --------------
# -- sdstore ---
# --------------

bin/sdstore: obj/sdstore.o
	$(CC) $(CFLAGS) $< -o $@

obj/sdstore.o: src/sdstore.c includes/reply.h includes/request.h 
	$(CC) $(CFLAGS) -c $< -o $@


# ----------------------
# -- transformations ---
# ----------------------

bin/gcompress: obj/gcompress.o
	$(CC) $(CFLAGS) $< -o $@

obj/gcompress.o: bin/gcompress.c
	$(CC) $(CFLAGS) -c $< -o $@

bin/gdecompress: obj/gdecompress.o
	$(CC) $(CFLAGS) $< -o $@

obj/gdecompress.o: bin/gdecompress.c
	$(CC) $(CFLAGS) -c $< -o $@

bin/bcompress: obj/bcompress.o
	$(CC) $(CFLAGS) $< -o $@

obj/bcompress.o: bin/bcompress.c
	$(CC) $(CFLAGS) -c $< -o $@

bin/bdecompress: obj/bdecompress.o
	$(CC) $(CFLAGS) $< -o $@

obj/bdecompress.o: bin/bdecompress.c
	$(CC) $(CFLAGS) -c $< -o $@

bin/encrypt: obj/encrypt.o
	$(CC) $(CFLAGS) $< -o $@

obj/encrypt.o: bin/encrypt.c
	$(CC) $(CFLAGS) -c $< -o $@

bin/decrypt: obj/decrypt.o
	$(CC) $(CFLAGS) $< -o $@

obj/decrypt.o: bin/decrypt.c
	$(CC) $(CFLAGS) -c $< -o $@

bin/nop: obj/nop.o
	$(CC) $(CFLAGS) $< -o $@

obj/nop.o: bin/nop.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	-rm -rf obj/* bin/sdstore bin/sdstored bin/*compress bin/*decompress bin/encrypt bin/decrypt bin/nop namedpipe/*