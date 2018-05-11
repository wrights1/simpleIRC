all:
	gcc -g -o client client.c
	gcc -g -o server server.c linked_list.c

clean:
	rm -Rf client server
