build: time_client time_server

time_client: time_client.c mysocket.o
	gcc -o time_client time_client.c mysocket.o -lpthread -Wall 

time_server: time_server.c mysocket.o
	gcc -o time_server time_server.c mysocket.o -lpthread -Wall

mysocket.o: mysocket.c mysocket.h
	gcc -c mysocket.c -Wall -lpthread 

clean:
	-rm -f time_client time_server mysocket.o