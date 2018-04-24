/*
	Quan Tran
	Steven Wright

    15 May 2018
*/
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

//Free old memory and returns an array with the double the size of the old one and the same elements
char * doubleSize(char * oldPoint, int ptrlen){
    char * newPoint;
    newPoint = calloc(sizeof(char) , (ptrlen*2));

    strcpy(newPoint, oldPoint);
    free(oldPoint);
   // oldPoint = newPoint;
    return newPoint;
}

int sendall(int s, char *buf, int len){
/*
	A function that sends the entire buf string if the socket is open

    Parameters:
        -s: The socket to send data through
        -buf: The character buffer that is being sent
        -len: The length of the character buffer
    Return:
        - A -1 if a failure and a 0 on success
    int total = 0; // how many bytes we've spent
    int bytesleft = *len; // how many we have left to send
    int n;
*/
	int total = 0; // how many bytes we've spent
	int bytesleft = len; // how many we have left to send
	int n;

    while(total < len){
		fprintf(stderr, "sending\n");
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1){
            break;
        }
        total+= n;
        bytesleft -= n;
    }
    len = total; // return number actually left here

    return n == -1?-1:0; // return -1 on failure, 0 on success
}

int recvNotAll(int socket){
	char * buf;
	int bufLength = 1024;
	buf = calloc(sizeof(char), bufLength);
	int received = recv(socket, buf, bufLength,0);
	fprintf(stderr, "received\n");
	if(received == 0 || received == -1 ){
		return -1;
	}
	fprintf(stderr, "recv len = %d\n", received );
	fprintf(stderr, "recv buf = %s", buf);
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int *argc, char **argv)
{
	int sockfd, numbytes;
	char buf[100];
	struct addrinfo hints, *servinfo, *p;
	int rv;

    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo("chat.freenode.net", "6667", &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

    p = servinfo;
	if ((sockfd = socket(p->ai_family, p->ai_socktype,
		p->ai_protocol)) == -1) {
		perror("client: socket");
	}

	if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
		perror("client: connect");
		close(sockfd);
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

    // if (recvStatus(sockfd) != 220 ) {
	//     perror("Connection Refused");
	//     exit(1);
	// }

	char * msg = "NICK root\r\n";
    int length = 11;
	int n = sendall(sockfd, msg, length);

	recvNotAll(sockfd);

	msg = "USER root 0 * :root\r\n";
	length = 20;
	n = sendall(sockfd, msg, length);

	recvNotAll(sockfd);
	recvNotAll(sockfd);
	recvNotAll(sockfd);

	//recvNotAll(sockfd);

	msg = "NICK root1\r\n";
	length = 12;
	n = sendall(sockfd, msg, length);

	fprintf(stderr, "in main again!\n" );
	recvNotAll(sockfd);
    recvNotAll(sockfd);
    recvNotAll(sockfd);

    close(sockfd);

	return 0;
}
