build: time_client time_server

time_client: time_client.c libmsocket.a mysocket.h
	gcc -o time_client time_client.c -L. -lmsocket -lpthread -Wall

time_server: time_server.c libmsocket.a mysocket.h
	gcc -o time_server time_server.c -L. -lmsocket -lpthread -Wall

libmsocket.a: mysocket.o
	ar rcs libmsocket.a mysocket.o
	ranlib libmsocket.a

mysocket.o: mysocket.c mysocket.h
	gcc -c mysocket.c -Wall -lpthread 

clean:
	-rm time_client time_server mysocket.o libmsocket.a
