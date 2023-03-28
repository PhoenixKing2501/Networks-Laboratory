libmsocket.a: mysocket.o
	ar rcs libmsocket.a mysocket.o
	ranlib libmsocket.a

mysocket.o: mysocket.c mysocket.h
	gcc -c mysocket.c -Wall -lpthread 

clean:
	-rm mysocket.o libmsocket.a
