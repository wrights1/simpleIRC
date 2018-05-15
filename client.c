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

#define DEBUG 1
#define STDIN 0  // file descriptor for standard input

struct global_state {
	char *nickname;
	char *pending_nickname;
	int nickname_registered;
	char* channel;
	char* pending_channel;
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

void init(global_state_t *state, int socket, char *user){
	/*
	send bootstrap NICK and USER messages
	*/
	int nicklen = strlen(user)+8;
	char *nickmsg = calloc(nicklen, sizeof(char));
	sprintf(nickmsg, "NICK %s\r\n", user);
	int n = sendall(socket, nickmsg, strlen(nickmsg));
	strcpy(state->pending_nickname, user);

	free(nickmsg);

	char *userFmt = "USER %s 0 * :%s\r\n";
	char *userMsg = calloc(strlen(userFmt) + strlen(user) * 2, 1);
	sprintf(userMsg, "USER %s 0 * :%s\r\n", user, user);
	n = sendall(socket, userMsg, strlen(userMsg));

	free(userMsg);
}

void strip_newline(char *buf) {
    int i = strlen(buf) - 1;
    while (i >= 0) {
        if (buf[i] == '\n' || buf[i] == '\r' || buf[i] == ' ') {
            buf[i] = '\0';
            i -= 1;
        } else {
            break;
        }
    }
}

char *recv_all(int socket) {
    int n = 1024;
    int len = 0;
    char *buf = calloc(sizeof(char), n);

    while (1) {
        char c[1];
        int nbytes = recv(socket, c, 1, 0);
        if (nbytes == 0) {
            break;
        }
		if (c[0] == 0) {
			continue;
		}

        buf[len] = c[0];
        len += 1;
        if (c[0] == '\n') {
            break;
		}

        if (len == n - 2) {
            n *= 2;
            buf = realloc(buf, n);
        }
    }
    buf[len] = 0;

    return buf;
}

void to_lower(char *str)
{
	int i;
	for (i = 0; i < strlen(str); i++) {
		str[i] = tolower(str[i]);
	}
}

/*
    parses client messages and respondes accordingly to each one.
/*
	takes raw server response and prints user-friendly things to the terminal
*/
void parse_response(int socket, char* buf, global_state_t *state){
	strip_newline(buf);

	if (strcmp(buf,"REGISTERED") == 0 ){
		state->nickname_registered = 0;

		fprintf(stderr, "%s is already registered, do you want to (1) enter" 
			" the password, or (2) pick a new nickname?\n",
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
				fprintf(stderr, "[simpleIRC] Passwords can't be less than 6 "
				"characters. Password: ");
				getline(&password, &asize,stdin);
			}
			password[strlen(password)-1] = 0;

			char *loginFmt = "LOGIN %s %s\r\n";
			char *loginCmd = calloc(1024, sizeof(char));
			sprintf(loginCmd, loginFmt, state->pending_nickname, password);
			sendall(socket, loginCmd, strlen(loginCmd));
		} else if (strcmp(answer, "2") == 0) {
			fprintf(stderr, "[simpleIRC] Choose a nickname: ");
			size_t nickSize = 255;
			getline(&state->pending_nickname,&nickSize,stdin);

			state->pending_nickname[strlen(state->pending_nickname) - 1] = 0;
			if (strlen(state->pending_nickname) == 0) {
				strcpy(state->pending_nickname, "cs375");
			}

			char *nickCmd = calloc(1024, sizeof(char));
			sprintf(nickCmd, "NICK %s\r\n", state->pending_nickname);
			sendall(socket, nickCmd, strlen(nickCmd));
		}

		return;
	}
	else if (strcmp(buf,"NOT REGISTERED") == 0 ){
		state->nickname_registered = 0;

		size_t asize = 1024;
		char *password = (char *) calloc(asize, 1);
		fprintf(stderr, "You must register this nickname. Choose a password: ");
		getline(&password, &asize, stdin);
		while (strlen(password) < 6 ){
			fprintf(stderr, "[simpleIRC] Your password can't be less than 6 "
			"characters. Choose a better password: ");
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
		sprintf(registerCmd, "REGISTER %s %s %s\r\n", state->pending_nickname, email, password);
		sendall(socket, registerCmd, strlen(registerCmd));

		return;
	}
	else if (strcmp(buf, "TOKEN") == 0 || strcmp(buf, "WRONG TOKEN") == 0) {
		state->nickname_registered = 0;

		fprintf(stderr, "We sent you an email with a verification code in it."
		" Enter it here: ");
		char *token = (char *) calloc(256, 1);
		size_t n = 256;
		getline(&token, &n, stdin);
		while (strlen(token) < 2){
			fprintf(stderr, "[simpleIRC] Invalid token. Try again: ");
			getline(&token, &n, stdin);
		}
		token[strlen(token) - 1] = 0;

		char *tokenCmd = calloc(n, sizeof(char));
		sprintf(tokenCmd, "TOKEN %s %s\r\n", state->pending_nickname, token);
		sendall(socket, tokenCmd, strlen(tokenCmd));

		return;
	}
	else if(strcmp(buf,"WRONG PASSWORD")==0){
		state->nickname_registered = 0;

		size_t asize = 1024;
		fprintf(stderr, "Incorrect. Password: ");
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
		state->nickname_registered = 0;

		fprintf(stderr, "User already logged in.\n");
		fprintf(stderr, "[simpleIRC] Choose a different nickname: ");
		size_t nickSize = 255;
		getline(&state->nickname,&nickSize,stdin);

		state->nickname[strlen(state->nickname) - 1] = 0;
		if (strlen(state->nickname) == 0) {
			memset(state->nickname, 0, sizeof(state->nickname));
			strcpy(state->nickname, "cs375");
		}

		char *nickCmd = calloc(1024, sizeof(char));
		sprintf(nickCmd, "NICK %s\r\n", state->nickname);
		sendall(socket, nickCmd, strlen(nickCmd));

		return;
	}
	else if(strcmp(buf,"NOT FOUND") == 0) {
		fprintf(stderr, "No such nick/channel. Message not sent. \n" );
		return;
	}

	int pmret;
	int cmdflag = 0;
	int i;
	char *expressions[3] = { "^:(.*)!.* ([a-zA-Z]*) (#?[a-zA-Z0-9]*) :(.*)$",
		"^:(.*)!.* ([a-zA-Z]*) #?(.*)$",
		"^:(.*)!.* ([a-zA-Z]*) :(.*)$"};

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

				contentlen = (pm_match[4].rm_eo - pm_match[4].rm_so);
				content = (char*) calloc(contentlen,1);
				memcpy(content, buf + pm_match[4].rm_so, contentlen);


				if (strcmp(recipient,state->nickname) == 0){
					fprintf(stderr, "Private message from %s. "
					"Only you can see this message.\n", recipient);
				}
			}
			else {
				contentlen = pm_match[3].rm_eo - pm_match[3].rm_so;
				content = (char*) calloc(contentlen,1);
				memcpy(content, buf + pm_match[3].rm_so, contentlen);
			}
			if (strcmp("PRIVMSG", cmd) == 0 ){
				fprintf(stderr, "%s: %s\n", nick, content);
				cmdflag = 1;
				break;
			}
			if (strcmp("PART", cmd) == 0 ) {
				if (strcmp(nick, state->nickname) == 0) {
					fprintf(stderr, " ============================== "
						"LEFT CHANNEL %s ============================== \n", 
						state->channel );
					strcpy(state->channel, "");
				} else {
					fprintf(stderr, "~~~~~~~~ %s left the channel ~~~~~~~~\n", nick);
				}
				cmdflag = 1;
				break;
			}
			if (strcmp("JOIN", cmd) == 0){
				if (strcmp(nick, state->nickname) == 0) {
					strcpy(state->channel, state->pending_channel);

					char *namesFmt = "NAMES %s\r\n";
					char *namesCmd = calloc(1024, strlen("NAMES ") + strlen(state->channel));
					sprintf(namesCmd, namesFmt, state->channel);
					sendall(socket, namesCmd, strlen(namesCmd));
					fprintf(stderr, " ============================== "
						"JOINED CHANNEL %s ============================== \n", 
						state->channel );
				} else {
					fprintf(stderr, "~~~~~~~~ %s has joined the channel ~~~~~~~~\n", 
						nick);
				}
				cmdflag = 1;
				break;
			}
			if (strcmp("NICK", cmd) == 0 ){
				// If the nick being changed is ours or we haven't registered
				// then update our nick to the new one
				if (strcmp(state->nickname, nick) == 0) {
					fprintf(stderr, "Logged in successfully as %s.\n", 
						state->pending_nickname);
					strcpy(state->nickname, content + 1);
				} else if (state->nickname_registered == 0) {
					fprintf(stderr, "Logged in successfully as %s.\n", 
						state->pending_nickname);
					strcpy(state->nickname, content + 1);
				} else {
					fprintf(stderr,  "%s is now known as: %s\n", nick, content + 1);
				}
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
		char *bufCopy = calloc(strlen(buf)+1,1);
		strcpy(bufCopy, buf);
		char * command = strtok(bufCopy," ");
		if (strcmp(command, "PING")==0){
			//fprintf(stderr, "pong'd\n");
			sendall(socket, "PONG :hello\r\n\0", 14);
		} else {
			fprintf(stderr, "%s\n", buf);
		}

		free(bufCopy);
	}
}

void join_channel(global_state_t *state, int socket, char *command)
{
	if (strcmp(state->channel,"") != 0 ){
		fprintf(stderr, "You are already in %s. "
		"You must leave this channel to join another one.\n"
		, state->channel );
	} else {
		// get the channel's name
		char *channel_line = calloc(strlen(command) + 1, sizeof(char));
		strcpy(channel_line, command);
		char *channel = strtok(channel_line," ");
		channel = strtok(NULL,""); 

		if (strcmp(channel,"") != 0) {
			// remove newline
			channel[strlen(channel)-1] = 0; 
			// Save the name of the channel the user wants to join
			strcpy(state->pending_channel, channel);

			command = command + sizeof(char); // remove leading "/"
			sendall(socket, command, strlen(command));
		} else {
			fprintf(stderr, "Channel name cannot be empty!");
		}

		free(channel_line);
	}
}

int chat(int socket){
	/*
		whenever there is response from server, it will be output without
		having to call a function or check that socket

		the only 2 sockets being monitored by select on the client side are
		stdin and the IRC server socket (input and output)
	*/

	global_state_t *state = (global_state_t *) calloc(sizeof(global_state_t), 1);
	state->nickname = calloc(sizeof(char), 1024);
	state->pending_nickname = calloc(sizeof(char), 1024);
	state->nickname_registered = 1;
	state->channel = calloc(256,1);
	state->pending_channel = calloc(256, 1);

	fprintf(stderr, "[simpleIRC] Choose a nickname: ");
	size_t nickSize = 255;
	getline(&state->nickname,&nickSize,stdin);

	state->nickname[strlen(state->nickname) - 1] = 0;
	if (strlen(state->nickname) == 0) {
		memset(state->nickname, 0, sizeof(state->nickname));
		//fprintf(stderr, "LOGIN = %s\n", getlogin());
		//strcpy(state->nickname, getlogin());
		strcpy(state->nickname, "cs375");
	}

	init(state, socket, state->nickname);

	fd_set readfds;
	while (1){
		FD_ZERO(&readfds);
		FD_SET(STDIN, &readfds);
		FD_SET(socket, &readfds);
		if (strcmp(state->channel,"") == 0){
			fprintf(stderr, "[simpleIRC] ");
		}
		else{
			fprintf(stderr, "[%s] ", state->channel);
		}

		select(socket+1, &readfds, NULL, NULL, NULL);

		if (FD_ISSET(STDIN, &readfds)){
			// user input
			size_t bufSize = 255;
			char *str = (char*) calloc(bufSize,1);
			getline(&str,&bufSize,stdin);

			if (strlen(str) > 1 && str[0] == '/' ) {
				// is command, send exactly what user typed but without / at beginning
				char line[512];
				strcpy(line, str);  // copy of str to be tokenized
				char * command = NULL;
				command = strtok(line," \n");
				if (command != NULL){
					command = command + sizeof(char); //remove  "/"
					to_lower(command);

					if (strcmp("join",command) == 0) {
						join_channel(state, socket, str);
					}
					else if (strcmp(state->channel,"") != 0  && (strcmp("close",command) == 0 || strcmp("CLOSE",command) == 0)){
						int partlen = strlen(state->channel)+8;
						char part[partlen];
						snprintf(part, partlen, "PART %s\r\n",state->channel);
						part[partlen-2] = 13; // 13 = 0x0D = \r
						part[partlen-1] = 10; // 10 = =x0A = \n
						sendall(socket, part, partlen);
					}
					else if (strcmp("quit",command) == 0 || strcmp("QUIT",command) == 0){
						str = str + sizeof(char);
						sendall(socket, str, strlen(str));
						exit(0);
					}
					else if (strcmp("names",command) == 0 || strcmp("NAMES",command) == 0){
						char *namesFmt = "NAMES %s\r\n";
						char *namesCmd = calloc(1024, strlen("NAMES ") + strlen(state->channel));
						sprintf(namesCmd, namesFmt, state->channel);
						// sends "NAMES <currentChannelName>"
						sendall(socket, namesCmd, strlen(namesCmd));

					}
					else if (strcmp("channels",command) == 0 || strcmp("CHANNELS",command) == 0){
						sendall(socket, "CHANNELS \r\n", 11);
					}
					else if (strcmp("nick", command) == 0 || strcmp("NICK", command) == 0) {
						char *new_nick = strtok(NULL, "\n");
						state->nickname_registered = 0;
						strcpy(state->pending_nickname, new_nick);

						str = str + sizeof(char); // remove "/", send unmodified input
						sendall(socket, str, strlen(str));
					}
					else {
						str = str + sizeof(char); // remove "/", send unmodified input
						sendall(socket, str, strlen(str));
					}
				}
			}
			else if (strlen(str) > 1) {
				// is message, send as PRIVMSG to channel (must keep track of current channel)
				if (strcmp(state->channel, "") != 0){
					int prefixlen = strlen(state->channel)+12;
					char *prefix = calloc(strlen(str)+ prefixlen, 1);
					snprintf(prefix, prefixlen, "PRIVMSG %s :",state->channel);
					strncat(prefix, str, strlen(str));
					// prefix[strlen(prefix)-1] = 13; // 13 = 0x0D = \r
					// prefix[strlen(prefix)] = 10; // 10 = =x0A = \n
					//sendall(socket, prefix, strlen(str) + prefixlen);
					sendall(socket, prefix, strlen(prefix));
				}
				else{
					sendall(socket, str, strlen(str));
				}
			}
			//free(str);
		}
		if (FD_ISSET(socket, &readfds)){
			// server response
			int bufLength = 1024;
			char *buf = recv_all(socket);
			if (strlen(buf) > 0){
				parse_response(socket, buf, state);
			}
			free(buf);
		}
	}
	return 0;
}

void print_usage(char *prog_name, int exit_code)
{
	fprintf(stderr, "\nUsage: %s <server> <port>\n\n", prog_name);
	exit(exit_code);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		print_usage(argv[0], 0);
	}

	int sockfd, numbytes;
	struct addrinfo hints, *servinfo, *p;
	int rv;

    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		print_usage(argv[0], 1);
	}

    p = servinfo;
	if ((sockfd = socket(p->ai_family, p->ai_socktype,
		p->ai_protocol)) == -1) {
		perror("client: socket");
	}

	if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
		perror("client: connect");
		close(sockfd);
		print_usage(argv[0], 1);
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		print_usage(argv[0], 2);
	}

	freeaddrinfo(servinfo); // all done with this structure

	chat(sockfd);

    close(sockfd);

	return 0;
}
