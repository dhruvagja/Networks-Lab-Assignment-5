lib: msocket.o
	ar rcs libmsocket.a msocket.o

msocket.o: msocket.c msocket.h
	gcc -c msocket.c

initmsocket: lib initmsocket.c msocket.h
	gcc initmsocket.c -o initmsocket -L. -lmsocket

user1: user1.c msocket.o
	gcc user1.c -o user1 -L. -lmsocket

user2: user2.c msocket.o
	gcc user2.c -o user2 -L. -lmsocket

runinitmsocket: initmsocket 
	./initmsocket

runuser1: user1
	./user1

runuser2: user2
	./user2

clean:
	rm -f *.o *.a initmsocket user1 user2 msocket.o