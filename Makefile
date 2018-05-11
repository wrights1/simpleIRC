all:
	gcc -g -o client client.c 
	gcc -g -o server server.c

clean:
	rm -Rf client server
