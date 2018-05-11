/*
    ~~~~~ simpleIRC ~~~~~
    Steven Wright and Quan Tran
    May 15, 2018
    server.c
        IRC server for clients to connect to, register with,
        and chat in

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define PORT "6667"   // port we're listening on

struct user{
    char * nick;
    char * email;
    char * password;
    int socket;
    struct user *next;
    struct user *prev;
    int token;
};
typedef struct user user_t;

struct channel{
    user_t *users;
    char * name;
    user_t* ops;
    struct channel *next;
    struct channel *prev;
};
typedef struct channel channel_t;

struct server_state{
    user_t *users;
    user_t *pending_users;
    channel_t *channels;

};

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

user_t* remove_user(user_t *users, user_t *user_to_remove){
    user_t *head = users;
    if (users == user_to_remove) {
        user_to_remove->next->prev == NULL;
        head = user_to_remove->next;
    } else {
        if (user_to_remove->next == NULL) {
            user_to_remove->prev->next == NULL;
        } else {
            user_to_remove->prev->next = user_to_remove->next;
            user_to_remove->next->prev = user_to_remove->prev;
        }
    }

    return head;
}

user_t *add_user(user_t *users, user_t *user_to_add) {
    if (users == NULL) {
        return user_to_add;
    }

    users->prev = user_to_add;
    user_to_add->next = users;

    return user_to_add;
}

channel_t *add_channel(channel_t *channels,  channel_t *channel_to_add) {
    if (channels == NULL) {
        return channel_to_add;
    }

    channels->prev = channel_to_add;
    channel_to_add->next = channels;

    return channel_to_add;
}

int send_email(int token, char *email, char * nick){
    int sockfd, numbytes;
	char buf[100];
	struct addrinfo hints, *servinfo, *p;
	int rv;

    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo("mail.denison.edu", "25", &hints, &servinfo)) != 0) {
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

    char *emailCmd = calloc(1024, sizeof(char));
    char * emailFmt = "HELO cs375\nMAIL FROM: server@simpleIRC.com\n"
                   "RCPT TO: %s\nDATA\nFrom: server@simpleIRC.com\nTo: %s\n"
                   "Subject: simpleIRC Verification Code\n"
                   "Here is your cerification code for registering the nickname \"%s\" on the simpleIRC server: %d.\r\n"
                   "Enter this where prompted in your terminal, then hit enter."
                   "\n.\n";

    sprintf(emailCmd,emailFmt,email, email, nick, token);
    sendall(sockfd, emailCmd, strlen(emailCmd));
    free(emailCmd);
    close(sockfd);

}

user_t* get_user_from_socket(user_t* users, int socket){
    user_t* head = users;
    while (head != NULL){
        if (head->socket == socket){
            return head;
        }
        else
            head = head->next;
    }
    return NULL;
}

void handle_data(char * buf, int socket, struct server_state *state){
    char line[512];
    strcpy(line, buf);  // copy of str to be tokenized
    char * command = NULL;
    command = strtok(line," ");
    char * parameter = strtok(NULL,"\r\n");
    if (strcmp(command,"NICK") == 0){
        user_t *user = state->users;

        while ( user != NULL){
            if (strcmp(parameter,user->nick) == 0 && user->socket == -1){
                sendall(socket, "REGISTERED", 11); //nick taken but not logged in, expecting password
                break;
            }
            else if (strcmp(parameter,user->nick) == 0 && user->socket != -1) {
                sendall(socket, "USER LOGGED IN", 15); //nick taken and currently logged in, pick new name
                break;
            }
            else {
                user = user->next;
            }
        }

        if (user == NULL) {
            sendall(socket, "NOT REGISTERED", 15); //nick not taken, expecting registration
        }
    } else if (strcmp(command, "REGISTER") == 0) {
        char *nick = strtok(parameter, " ");
        char *email = strtok(NULL, " ");
        char *password = strtok(NULL, " ");

        user_t *new_user = (user_t *) calloc(sizeof(user_t), 1);
        int n = 1024;

        new_user->nick = (char *) calloc(sizeof(char), n);
        new_user->email = (char *) calloc(sizeof(char), n);
        new_user->password = (char *) calloc(sizeof(char), n);
        new_user->socket = socket;

        strncpy(new_user->nick, nick, n);
        strncpy(new_user->email, email, n);
        strncpy(new_user->password, password, n);

        int token = rand() % 9000000 + 1000000;
        token = 0;
        new_user->token = token;

        //send_email(token,email,nick);

        if (state->pending_users == NULL) {
            state->pending_users = new_user;
        } else {
            state->pending_users = add_user(state->pending_users, new_user);
        }

        //fprintf(stderr, "token: %d\n", token);

        char *tokenRequest = "TOKEN";
        sendall(socket, tokenRequest, strlen(tokenRequest));
    } else if (strcmp(command, "TOKEN") == 0) {
        char *nick = strtok(parameter, " ");
        char *token_str = strtok(NULL, " ");
        int received_token = atoi(token_str);

        user_t *pending_user = state->pending_users;
        while (pending_user != NULL) {
            if (strcmp(pending_user->nick, nick) == 0) {
                if (pending_user->token == received_token) {
                    state->pending_users = remove_user(state->pending_users, pending_user);
                    state->users = add_user(state->users, pending_user);
                    char *tokenSuccess = "RIGHT TOKEN";
                    sendall(socket, tokenSuccess, strlen(tokenSuccess));
                } else {
                    char *wrongToken = "WRONG TOKEN";
                    sendall(socket, wrongToken, strlen(wrongToken));
                }

                break;
            } else {
                pending_user = pending_user->next;
            }
        }
    } else if (strcmp(command, "LOGIN") == 0) {
        char *nick = strtok(parameter, " ");
        char *pass = strtok(NULL, " ");
        user_t *user = state->users;
        while (user != NULL) {
            if (strcmp(user->nick, nick) == 0) {
                if (strcmp(user->password, pass) == 0) {
                    char *rightPass = "RIGHT PASSWORD";
                    user->socket = socket;
                    sendall(socket, rightPass, strlen(rightPass));
                }
                else {
                    char *wrongPass = "WRONG PASSWORD";
                    sendall(socket, wrongPass, strlen(wrongPass));
                }
                break;
            }
            else {
                user = user->next;
            }
        }
    } else if (strcmp(command, "QUIT") == 0) {
        // user_t *user = state->users;
        //
        // while ( user != NULL){
        //     if (socket == user->socket){
        //         user->socket = -1;
        //         break;
        //     }
        //     else {
        //         user = user->next;
        //     }
        // }
        //
        // close()
    }
    else if (strcmp(command,"JOIN") == 0){
        user_t *user = state->users; // find user that sent JOIN command
        while (user != NULL) {
            if (user->socket == socket) {
                break;
            }
            else
                user = user->next;
        }
        char *channelName = strtok(parameter, "\r\n");
        //fprintf(stderr, "channelName = %s\n", channelName);
        char *joinFmt = ":%s!%s JOIN %s\r\n";
        char *joinCmd = calloc(1024, sizeof(char));
        sprintf(joinCmd, joinFmt, user->nick, user->email, channelName);

        channel_t *channel_head = state->channels; // check if channel exists already
        while (channel_head != NULL){
            if (strcmp(channel_head->name,channelName) == 0 ){
                user_t* new_channel_users = add_user(channel_head->users, user);
                channel_head->users = new_channel_users;
                // send JOIN message to everyone in channel
                while (new_channel_users != NULL){
                    sendall(new_channel_users->socket, joinCmd, strlen(joinCmd));
                    new_channel_users = new_channel_users->next;
                }
                break; // found existing channel, added user to it
            }
            else
                channel_head = channel_head->next;
        }
        // didn't find channel, create one and add user to it
        if (channel_head == NULL) {
            channel_t *channel = calloc(sizeof(channel_t),1);
            channel->name = calloc(strlen(channelName),1);
            strcpy(channel->name, channelName);
            channel->prev = NULL;
            state->channels = add_channel(state->channels, channel);
            fprintf(stderr, "channel->name = %s\n", channel->name );
            user_t* new_channel_users = add_user(channel->users, user);
            channel->users = new_channel_users;

            // send JOIN message to everyone in channel
            while (new_channel_users != NULL){
                sendall(new_channel_users->socket, joinCmd, strlen(joinCmd));
                new_channel_users = new_channel_users->next;
            }
        }
    }
    else if (strcmp(command,"PRIVMSG") == 0){
        char *channelName = strtok(parameter, " ");
        char *content = strtok(NULL, " :");
        fprintf(stderr, "channelName = %s\n", channelName);
        fprintf(stderr, "content = %s\n", content);

        char *privmsgFmt = ":%s!%s PRIVMSG %s :%s\r\n";
        char *privmsgCmd = calloc(1024, sizeof(char));

        channel_t *channel = state->channels;
        user_t *sender = get_user_from_socket(channel->users,socket);
        while (channel != NULL){
            fprintf(stderr, "channel->name = %s\n", channel->name);
            if (strcmp(channel->name, channelName) == 0){
                user_t *user = channel->users;
                while (user != NULL){
                    if (user != sender){
                        sprintf(privmsgCmd, privmsgFmt, sender->nick, sender->email, channelName, content);
                        sendall(user->socket, privmsgCmd, strlen(privmsgCmd));
                    }
                    user = user->next;
                }
                break;
            }
            else{
                channel = channel->next;
            }
        }
        user_t* user = state->users;
        // no matching channels, checking nicknames
        while (user != NULL){
            if (strcmp(user->nick,channelName)==0){
                if(user->socket != -1){
                    sprintf(privmsgCmd, privmsgFmt, sender->nick, sender->email, channelName, content);
                    sendall(user->socket, privmsgCmd, strlen(privmsgCmd));
                }
                break;
            }
            user = user->next;
        }
    }
    else if (strcmp(command,"PART") == 0){
        // char *channelName = strtok(parameter, " ");
        //
        // channel_t *channel = state->channels;
        // user_t *sender = get_user_from_socket(channel->users,socket);
        // while (channel != NULL){
        //     if (strcmp(channel->name, channelName) == 0){
        //         channel->users = remove_user(channel->users, )
        //     }
        //     else{
        //         channel = channel->next;
        //     }
    }
}

int main(void)
{
    srand(time(NULL));
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number


    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[1024];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    struct server_state *state = calloc(sizeof(struct server_state),1);


    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    while(1) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        fprintf(stderr, "selectserver: new connection from %s \n", ".");
                    }
                } else {
                    // handle data from a client
                    memset(buf, 0, sizeof(buf));
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed, find user that was using that socket and make it -1 (avail)
                            printf("selectserver: socket %d hung up\n", i);
                            user_t *head = state->users;
                            while (head != NULL){
                                if (head->socket == i)
                                    head->socket = -1;
                                else
                                    head = head->next;
                            }
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        // we got some data from a client
                        handle_data(buf, i, state);
                        // for(j = 0; j <= fdmax; j++) {
                        //     // send to everyone!
                        //     if (FD_ISSET(j, &master)) {
                        //         // except the listener and ourselves
                        //         if (j != listener && j != i) {
                        //             if (send(j, buf, nbytes, 0) == -1) {
                        //                 perror("send");
                        //             }
                        //         }
                        //     }
                        // }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!

    return 0;
}
