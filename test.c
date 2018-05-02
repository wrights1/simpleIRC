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
#include <regex.h>

#include <arpa/inet.h>

#define DEBUG 0
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

void init(int socket){
	/*
	send bootstrap NICK and USER messages
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
}

void parse_response(char* buf){
	/*
	takes raw server response and prints user-friendly things to the terminal

	*/
		regex_t regex;
		int reti;
		int privmsg = 0;
		int part = 0;
		int join = 0;
		regmatch_t pmatch[4];

		/* Compile regular expression to capture username and message content  */
		reti = regcomp(&regex, "^:(.*)!.* ([a-zA-Z]*) .*:(.*)$", REG_ICASE|REG_EXTENDED);
		// if (reti) {
		// 	fprintf(stderr, "Could not compile regex\n");
		// 	exit(1);
		// }

		/* Execute regular expression to extract matches */

		reti = regexec(&regex, buf, 4, pmatch, 0);
		if (!reti) {
			int cmdlen = pmatch[2].rm_eo - pmatch[2].rm_so;
			char * cmd = (char*) calloc(cmdlen,1);
			memcpy(cmd, buf + pmatch[2].rm_so, cmdlen);
			//fprintf(stderr, "cmd = %s\n", cmd);

			if (strcmp("PART", cmd) == 0 )
				part = 1;
			if (strcmp("PRIVMSG", cmd) == 0 )
				privmsg = 1;
			if (strcmp("JOIN", cmd) == 0)
				join = 1;

			int nicklen = pmatch[1].rm_eo - pmatch[1].rm_so;
			char * nick = (char*) calloc(nicklen,1);
			memcpy(nick, buf + pmatch[1].rm_so, nicklen);

			int contentlen = (pmatch[3].rm_eo - pmatch[3].rm_so)-1;
			char * content = (char*) calloc(contentlen,1);
			memcpy(content, buf + pmatch[3].rm_so, contentlen);

			if (part){
				fprintf(stderr, " ~~~~ %s left the channel. ~~~~\n", nick);
			}
			if (privmsg) {
				fprintf(stderr, "%s: %s\n", nick, content);
			}
			if (join) {
				fprintf(stderr, "~~~~ %s has joined the channel. ~~~~\n", nick);
			}

			free(nick);
			free(content);
			free(cmd);
		}

		else if (reti != REG_NOMATCH) {
			fprintf(stderr, "Regex match failed\n");
			exit(1);
		}

		if (!privmsg && !part && !join){
			fprintf(stderr, "%s", buf);
		}

		regfree(&regex);
}

int chat(int socket){
	/*
		whenever there is response from server, it will be output without
		having to call a function or check that socket

		the only 2 sockets being monitored by select on the client side are
		stdin and the IRC server socket (input and output)
	*/

	/* ~~~~~~~~~~~~~~~~~~~~~~~~~  CURRENT FEATURES ~~~~~~~~~~~~~~~~~~~~~~~~
		- /CLOSE
			- leaves channel
		- /QUIT
			- disconnects from server and closes program
		- /JOIN
		- any real IRC command can be sent with /<command> and will work if
		  properly formatted according to RFCs
		- if no "/", input is sent as PRIVMSG to current channel/buffer
	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	/* TODO
		- send PINGs and handle PONG replies (or send PONGs and handle PING replies, check RFCs)
			- these prevent server timeout, client will disconnect after 240(?) seconds if no ping recieved
		- UI/formatting to make more readable
		- any other client responsibilities?
		- memory management?
	*/

	    //send initial NICK and USER commands
		init(socket);

        fd_set readfds;
		char* channel = NULL;

        while (1){
            FD_ZERO(&readfds);
            FD_SET(STDIN, &readfds);
            FD_SET(socket, &readfds);
			if (channel != NULL){
				fprintf(stderr, "[%s] ", channel);
			}
			else{
				fprintf(stderr, "[simpleIRC] ");
			}

            select(socket+1, &readfds, NULL, NULL, NULL);

            if (FD_ISSET(STDIN, &readfds)){
                // user input
				size_t bufSize = 255;
                char *str = (char*) calloc(bufSize,1);
                getline(&str,&bufSize,stdin);

				if (str[0] == '/' ) {
					// is command, send exactly what user typed but without / at beginning
					char line[256];
					strcpy(line, str);  // copy of str to be tokenized
					char * command = NULL;
					command = strtok(line," \n");
					if (command != NULL){
						command = command + sizeof(char); //remove  "/"
						if (strcmp("join",command) == 0 || strcmp("JOIN",command) == 0){
							char channel_line[256];
							strcpy(channel_line, str);
							channel = strtok(channel_line,"#");
							channel = strtok(NULL,"#"); // only update channel var on join command
							if ( channel != NULL) {
								channel[strlen(channel)-1] = 0; // remove newline
								//fprintf(stderr, "channel = %s\n", channel);
							}
							str = str + sizeof(char); // remove leading "/"
							sendall(socket, str, strlen(str));
							fprintf(stderr, " ======================== JOINED CHANNEL %s ======================= \n", channel );
						}
						else if ( channel != NULL && (strcmp("close",command) == 0 || strcmp("CLOSE",command) == 0)){
							int partlen = strlen(channel)+8;
							char part[partlen];
							snprintf(part, partlen, "PART #%s",channel);
							part[partlen-2] = 13; // 13 = 0x0D = \r
							part[partlen-1] = 10; // 10 = =x0A = \n
							sendall(socket, part, partlen);
							fprintf(stderr, " ======================== CLOSED CHANNEL %s ======================= \n", channel );
							/*
								TODO????
								keep track of previous channels so you can join channels from other channels (or maybe just don't allow JOIN at all if already in a channel)
							*/
							channel = NULL;
						}
						else if (strcmp("quit",command) == 0 || strcmp("QUIT",command) == 0){
							str = str + sizeof(char);
							sendall(socket, str, strlen(str));
							exit(0);
						}
						else {
							str = str + sizeof(char); // remove "/", send unmodified input
							sendall(socket, str, strlen(str));
						}
					}
				}
				else{
					// is message, send as PRIVMSG to channel (must keep track of current channel)
					if (channel != NULL){
						int prefixlen = strlen(channel)+12;
						char prefix[prefixlen];
						snprintf(prefix, prefixlen, "PRIVMSG #%s :",channel);
						strncat(prefix, str, strlen(str));
						prefix[strlen(prefix)-1] = 13; // 13 = 0x0D = \r
						prefix[strlen(prefix)] = 10; // 10 = =x0A = \n
						sendall(socket, prefix, strlen(str) + prefixlen);
					}
					else{
                		sendall(socket, str, strlen(str));
					}
				}
				//free(str);
            }
            if (FD_ISSET(socket, &readfds)){
                // server response
                char * buf;
                int bufLength = 1024;
                buf = calloc(sizeof(char), bufLength);
                int received = recv(socket, buf, bufLength,0);
				if (received > 0){
					parse_response(buf);
				}
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
