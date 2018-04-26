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

#define DEBUG 1
#define STDIN 0  // file descriptor for standard input

int sendall(int s, char *buf, int len){
/*
	A function that sends the entire buf string if the socket is open

    Parameters:
        -s: The socket to send data through
        -buf: The character buffer that is being sent
        -len: The length of the character buffer
    Return:
        - A -1 if a failure and a 0 on success
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

// int recvNotAll(int socket){
// 	char * buf;
// 	int bufLength = 1024;
// 	buf = calloc(sizeof(char), bufLength);
//     int received = 0;
//     while (received != 0 || received != -1)
//     {
//         fprintf(stderr, "about to receive\n");
//         int received = recv(socket, buf, bufLength,0);
//         fprintf(stderr, "received\n");
//         if(received == 0 || received == -1 ){
//             return -1;
//         }
//         fprintf(stderr, "recv buf = %s", buf);
//     }
//     return 0;
// }

int chat(int socket){
    /*
    sends initial NICK and USER commands, now must get input from stdin
    */
        char * user = getlogin();// gets current unix username
        int nicklen = strlen(user)+8;
        char nickmsg[nicklen];
        snprintf(nickmsg, nicklen, "NICK %s\r\n", user);
        int n = sendall(socket, nickmsg, nicklen-1);

        #if DEBUG
        fprintf(stderr, "USERNAME = %s\n", user );
        fprintf(stderr, "sizeof(user) = %lu\n", strlen(user));
        fprintf(stderr, "MSG = %s\n", nickmsg);
        #endif

        int userlen = (strlen(user)*2)+13;
        char usermsg[userlen];
        snprintf(usermsg, userlen, "USER %s 0 * :%s\r\n", user, user);
        n = sendall(socket, usermsg, userlen-1);

        #if DEBUG
        fprintf(stderr, "MSG = %s\n", usermsg);
        #endif

        // whenever there is response from server, it will be output without
        // having to call a function or check that socket

        // the only 2 sockets being monitored by select on the client side are
        // stdin and the IRC server socket (input and output)

        fd_set readfds;

        while (1){
            FD_ZERO(&readfds);
            FD_SET(STDIN, &readfds);
            FD_SET(socket, &readfds);

            select(socket+1, &readfds, NULL, NULL, NULL);

            if (FD_ISSET(STDIN, &readfds)){
                fprintf(stderr, "User input\n");
                char *str;
                size_t bufSize = 255;
                getline(&str,&bufSize,stdin);
                //fprintf(stderr, "%s\n", str);
                sendall(socket, str, strlen(str));
            }
            if (FD_ISSET(socket, &readfds)){
                fprintf(stderr, "server response\n");
                char * buf;
                int bufLength = 1024;
                buf = calloc(sizeof(char), bufLength);
                int received = recv(socket, buf, bufLength,0);
                fprintf(stderr, "%s", buf);
                free(buf);
            }
        }
        return 0;
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

	chat(sockfd);

    close(sockfd);

	return 0;
}
