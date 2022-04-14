all: server client
server: bin/sdstored
client: bin/sdstore

bin/sdstored: obj/sdstored.o
	gcc -g obj/sdstored.o -o bin/sdstored
obj/sdstored.o: src/sdstored.c
	gcc -Wall -g -c src/sdstored.c obj/sdstored.o
bin/sdstore: obj/sdstore.o
	gcc -g obj/sdstore.o -o bin/sdstore
obj/sdstore.o: src/sdstore.c
	gcc -Wall -g -c src/sdstore.c obj/sdstore.o

clean:
	rm obj/* tmp/* bin/{sdstore,sdstored}