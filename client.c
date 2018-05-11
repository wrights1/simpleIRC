/*
    ~~~~~ simpleIRC ~~~~~
    Steven Wright and Quan Tran
    May 15, 2018
    client.c
        IRC client to connect to server, register with,
        and chat in

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

struct global_state {
	char *nickname;
	int nickname_registered;
	char* channel;
};
typedef struct global_state global_state_t;

/*
	A function that sends the entire buf string if the socket is open

    Parameters:
        -s: The socket to send data through
        -buf: The character buffer that is being sent
        -len: The length of the character buffer
    Return:
        - A -1 if a failure and a 0 on success
*/
int sendall(int s, char *buf, int len){
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

void init(int socket, char *user){
	/*
	send bootstrap NICK and USER messages
	*/
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

void parse_response(int socket, char* buf, global_state_t *state){
	/*
	takes raw server response and prints user-friendly things to the terminal

	*/

		//fprintf(stderr, "buf: %s\n", buf);
		if (strcmp(buf,"REGISTERED") == 0 ){
			fprintf(stderr, "%s is already registered, do you want to (1) enter the password, or (2) pick a new nickname?\n",
				state->nickname );
			fprintf(stderr, "[Enter 1 or 2]: ");
			size_t asize = 256;
			char *answer = (char*) calloc(asize,1);
			getline(&answer, &asize,stdin);
			answer[strlen(answer)-1] = 0;
			while (strcmp(answer,"1") != 0 && strcmp(answer,"2") != 0 ) {
				fprintf(stderr, "[Enter 1 or 2]: ");
				getline(&answer, &asize,stdin);
				answer[strlen(answer)-1] = 0;
			}
			if (strcmp(answer,"1")==0){
				fprintf(stderr, "[simpleIRC] Password: ");
				char *password= (char*) calloc(asize,1);
				getline(&password, &asize,stdin);
				while (strlen(password) < 6) {
					fprintf(stderr, "[simpleIRC] Passwords can't be less than 6 characters. Password: ");
					getline(&password, &asize,stdin);
				}
				password[strlen(password)-1] = 0;

				char *loginFmt = "LOGIN %s %s\r\n";
				char *loginCmd = calloc(1024, sizeof(char));
				sprintf(loginCmd, loginFmt, state->nickname, password);
				sendall(socket, loginCmd, strlen(loginCmd));
			} else if (strcmp(answer, "2") == 0) {
				fprintf(stderr, "[simpleIRC] Choose a nickname: ");
				size_t nickSize = 255;
				getline(&state->nickname,&nickSize,stdin);

				state->nickname[strlen(state->nickname) - 1] = 0;
				if (strlen(state->nickname) == 0) {
					//free(state->nickname);
					state->nickname = getlogin();
				}

				char *nickCmd = calloc(1024, sizeof(char));
				sprintf(nickCmd, "NICK %s\r\n", state->nickname);
				sendall(socket, nickCmd, strlen(nickCmd));
			}

			return;
		}
		else if (strcmp(buf,"NOT REGISTERED") == 0 ){
			size_t asize = 1024;
			char *password = (char *) calloc(asize, 1);
			fprintf(stderr, "You must register this nickname. Choose a password: ");
			getline(&password, &asize, stdin);
			while (strlen(password) < 6 ){
				fprintf(stderr, "[simpleIRC] Your password can't be less than 6 characters. Choose a better password: ");
				getline(&password, &asize, stdin);
			}
			password[strlen(password) - 1] = 0;

			char *email = (char *) calloc(asize, 1);
			fprintf(stderr, "[simpleIRC] Enter your email address: ");
			getline(&email, &asize, stdin);
			while (strcmp(email,"\n") == 0 ) {
				fprintf(stderr, "[simpleIRC] Enter a valid email address: ");
				getline(&email, &asize, stdin);
			}
			email[strlen(email) - 1] = 0;

			char *registerCmd = calloc(1024, sizeof(char));
			sprintf(registerCmd, "REGISTER %s %s %s\r\n", state->nickname, email, password);
			sendall(socket, registerCmd, strlen(registerCmd));

			return;
		}
		else if (strcmp(buf, "TOKEN") == 0 || strcmp(buf, "WRONG TOKEN") == 0) {
			fprintf(stderr, "We sent you an email with a verification code in it. Enter it here: ");
			char *token = (char *) calloc(256, 1);
			size_t n = 256;
			getline(&token, &n, stdin);
			while (strlen(token) < 2){
				fprintf(stderr, "[simpleIRC] Invalid token. Try again: ");
				getline(&token, &n, stdin);
			}
			token[strlen(token) - 1] = 0;

			char *tokenCmd = calloc(n, sizeof(char));
			sprintf(tokenCmd, "TOKEN %s %s\r\n", state->nickname, token);
			sendall(socket, tokenCmd, strlen(tokenCmd));

			return;
		}
		else if (strcmp(buf,"RIGHT TOKEN")==0){
			fprintf(stderr, "Nickname registered.\n");
			state->nickname_registered = 1;
			return;
		}
		else if (strcmp(buf,"RIGHT PASSWORD")==0){
			fprintf(stderr, "Logged in successfully as %s.\n" ,state->nickname);
			return;
		}
		else if(strcmp(buf,"WRONG PASSWORD")==0){
			size_t asize = 1024;
			fprintf(stderr, "Password: ");
			char *password = (char *) calloc(asize, 1);
			getline(&password, &asize,stdin);
			while (strlen(password) < 6) {
				fprintf(stderr, "Passwords can't be less than 6 characters. Password: ");
				getline(&password, &asize,stdin);
			}
			password[strlen(password)-1] = 0;

			char *loginFmt = "LOGIN %s %s\r\n";
			char *loginCmd = calloc(1024, sizeof(char));
			sprintf(loginCmd, loginFmt, state->nickname, password);
			sendall(socket, loginCmd, strlen(loginCmd));
			return;
		}
		else if (strcmp(buf,"USER LOGGED IN") == 0) {
			fprintf(stderr, "User already logged in.\n");
			fprintf(stderr, "[simpleIRC] Choose a different nickname: ");
			size_t nickSize = 255;
			getline(&state->nickname,&nickSize,stdin);

			state->nickname[strlen(state->nickname) - 1] = 0;
			if (strlen(state->nickname) == 0) {
				memset(state->nickname, 0, sizeof(state->nickname));
				strcpy(state->nickname, getlogin());
			}

			char *nickCmd = calloc(1024, sizeof(char));
			sprintf(nickCmd, "NICK %s\r\n", state->nickname);
			sendall(socket, nickCmd, strlen(nickCmd));

			return;
		}

		int pmret;
		int cmdflag = 0;
		int i;
		char *expressions[3] = { "^:(.*)!.* ([a-zA-Z]*) (#?[a-zA-Z0-9]*) :(.*)$",
		 "^:(.*)!.* ([a-zA-Z]*) #(.*)$",
		 "^:(.*)!.* ([a-zA-Z]*) :(.*)$" };

		for ( i = 0 ; i < 3; i++){
			regex_t pm_regex;
			regmatch_t pm_match[5];

			/* Compile regular expression to capture nickname, command, and message content  */
			pmret = regcomp(&pm_regex, expressions[i], REG_ICASE|REG_EXTENDED);
			if (pmret) {
				fprintf(stderr, "Could not compile regex\n");
				exit(1);
			}

			/* Execute regular expression to extract matches */
			pmret = regexec(&pm_regex, buf, 5, pm_match, 0);
			if (!pmret) {
				int cmdlen = pm_match[2].rm_eo - pm_match[2].rm_so;
				char * cmd = (char*) calloc(cmdlen,1);
				memcpy(cmd, buf + pm_match[2].rm_so, cmdlen);
				//fprintf(stderr, "cmd = %s\n", cmd);

				int nicklen = pm_match[1].rm_eo - pm_match[1].rm_so;
				char * nick = (char*) calloc(nicklen,1);
				memcpy(nick, buf + pm_match[1].rm_so, nicklen);

				int contentlen;
				char * content;
				int recipientlen;
				char *recipient;
				if (i == 0){
					recipientlen = (pm_match[3].rm_eo - pm_match[3].rm_so);
					recipient = (char*) calloc(recipientlen,1);
					memcpy(recipient, buf + pm_match[3].rm_so, recipientlen);

					contentlen = (pm_match[4].rm_eo - pm_match[4].rm_so)-1;
					content = (char*) calloc(contentlen,1);
					memcpy(content, buf + pm_match[4].rm_so, contentlen);


					if (strcmp(recipient,state->nickname) == 0){
						fprintf(stderr, "Private message from %s. Only you can see this message.\n", recipient);
					}
				}
				else {
					contentlen = (pm_match[3].rm_eo - pm_match[3].rm_so)-1;
					content = (char*) calloc(contentlen,1);
					memcpy(content, buf + pm_match[3].rm_so, contentlen);
				}



				if (strcmp("PRIVMSG", cmd) == 0 ){
					fprintf(stderr, "%s: %s\n", nick, content);
					cmdflag = 1;
					break;
				}
				if (strcmp("PART", cmd) == 0 ) {
					fprintf(stderr, "~~~~~~~~ %s left the channel ~~~~~~~~\n", nick);
					cmdflag = 1;
					break;
				}
				if (strcmp("JOIN", cmd) == 0){
					fprintf(stderr, "~~~~~~~~ %s has joined the channel ~~~~~~~~\n", nick);
					cmdflag = 1;
					break;
				}
				if (strcmp("NICK", cmd) == 0 ){
					//fprintf(stderr, "nick = %s\n", nick );
					//fprintf(stderr, "content = %s\n", content);
					fprintf(stderr,  "%s is now known as: %s\n", nick, content);
					cmdflag = 1;
					break;
				}

				free(nick);
				free(content);
				free(cmd);
			}

			else if (pmret != REG_NOMATCH) {
				fprintf(stderr, "Regex match failed\n");
				exit(1);
			}

			regfree(&pm_regex);
		}

		if (!cmdflag){
			fprintf(stderr, "%s", buf);
		}
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
		global_state_t *state = (global_state_t *) calloc(sizeof(global_state_t), 1);
		state->nickname = (char *) calloc(sizeof(char), 1024);
		state->nickname_registered = 0;
		state->channel = "simpleIRC";

		fprintf(stderr, "[simpleIRC] Choose a nickname: ");
		size_t nickSize = 255;
		getline(&state->nickname,&nickSize,stdin);

		state->nickname[strlen(state->nickname) - 1] = 0;
		if (strlen(state->nickname) == 0) {
			memset(state->nickname, 0, sizeof(state->nickname));
			strcpy(state->nickname, getlogin());
		}

		init(socket, state->nickname);

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
					char line[512];
					strcpy(line, str);  // copy of str to be tokenized
					char * command = NULL;
					command = strtok(line," \n");
					if (command != NULL){
						command = command + sizeof(char); //remove  "/"
						if (strcmp("join",command) == 0 || strcmp("JOIN",command) == 0){
							if (channel != NULL ){
								fprintf(stderr, "You are already in %s. You must leave this channel to join another one.\n", channel );
							}
							else {
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
								fprintf(stderr, " ============================== JOINED CHANNEL %s ============================== \n", channel );
							}
						}
						else if ( channel != NULL && (strcmp("close",command) == 0 || strcmp("CLOSE",command) == 0)){
							int partlen = strlen(channel)+8;
							char part[partlen];
							snprintf(part, partlen, "PART #%s",channel);
							part[partlen-2] = 13; // 13 = 0x0D = \r
							part[partlen-1] = 10; // 10 = =x0A = \n
							sendall(socket, part, partlen);
							fprintf(stderr, " ============================== CLOSED CHANNEL %s ============================== \n", channel );
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
						// else if (strcmp("nick", command) == 0 || strcmp("NICK", command) == 0) {
						// 	sendall(socket, "QUIT :simpleIRC 1.0.0", 5);
						// 	str = str + sizeof(char);
						// 	sendall(socket, str, strlen(str));
						// }
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
					parse_response(socket, buf, state);
				}
                free(buf);
            }
        }
        return 0;
}

int main(int argc, char **argv)
{
	int sockfd, numbytes;
	struct addrinfo hints, *servinfo, *p;
	int rv;

    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo("localhost", "6667", &hints, &servinfo)) != 0) {
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
		exit(1);
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
