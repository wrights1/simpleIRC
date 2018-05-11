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
#include "linked_list.h"

#define PORT "6667"   // port we're listening on

struct user{
    char * nick;
    char * email;
    char * password;
    int socket;
    int token;
};
typedef struct user user_t;

struct channel{
    linked_list_t *users;
    char * name;
    linked_list_t* ops;
};
typedef struct channel channel_t;

struct server_state{
    linked_list_t *users;
    linked_list_t *pending_users;
    linked_list_t *channels;
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

ll_node_t* get_user_from_socket(linked_list_t* users, int socket){
    ll_node_t *node = users->head;
    while (node != NULL){
        user_t *user = (user_t *) node->object;
        if (user->socket == socket){
            return node;
        }

        node = node->next;
    }
    return NULL;
}

ll_node_t* get_user_from_nick(linked_list_t *users, char *nick) {
    ll_node_t *node = users->head;
    while (node != NULL) {
        user_t *user = (user_t *) node->object;
        if (strcmp(user->nick, nick) == 0) {
            return node;
        }

        node = node->next;
    }

    return NULL;
}

ll_node_t *get_channel(linked_list_t *channels, char *channel_name){
    ll_node_t *node = channels->head;
    while (node != NULL) {
        channel_t *channel = (channel_t *) node->object;
        if (strcmp(channel->name, channel_name) == 0) {
            return node;
        }
        node = node->next;
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
        ll_node_t *found_node = get_user_from_nick(state->users, parameter);
        if (found_node != NULL) {
            user_t *user = (user_t *) found_node->object;
            if (user->socket == -1) {
                sendall(socket, "REGISTERED", 11); //nick taken but not logged in, expecting password
            } else {
                sendall(socket, "USER LOGGED IN", 15); //nick taken and currently logged in, pick new name
            }
        } else {
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

        ll_add(state->pending_users, new_user);

        //fprintf(stderr, "token: %d\n", token);

        char *tokenRequest = "TOKEN";
        sendall(socket, tokenRequest, strlen(tokenRequest));
    } else if (strcmp(command, "TOKEN") == 0) {
        char *nick = strtok(parameter, " ");
        char *token_str = strtok(NULL, " ");
        int received_token = atoi(token_str);

        ll_node_t *pending_user_node = get_user_from_nick(state->pending_users, nick);
        if (pending_user_node != NULL) {
            user_t *pending_user = (user_t *) pending_user_node->object;
            if (pending_user->token == received_token) {
                ll_remove(state->pending_users, pending_user_node);
                ll_add(state->users, pending_user);
                char *tokenSuccess = "RIGHT TOKEN";
                sendall(socket, tokenSuccess, strlen(tokenSuccess));
            } else {
                char *wrongToken = "WRONG TOKEN";
                sendall(socket, wrongToken, strlen(wrongToken));
            }
        }
    } else if (strcmp(command, "LOGIN") == 0) {
        char *nick = strtok(parameter, " ");
        char *pass = strtok(NULL, " ");
        ll_node_t *user_node = get_user_from_nick(state->users, nick);

        if (user_node == NULL) {
            user_t *user = (user_t *) user_node->object;
            if (strcmp(user->password, pass) == 0) {
                char *rightPass = "RIGHT PASSWORD";
                user->socket = socket;
                sendall(socket, rightPass, strlen(rightPass));
            }
            else {
                char *wrongPass = "WRONG PASSWORD";
                sendall(socket, wrongPass, strlen(wrongPass));
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
        ll_node_t *sender_node = get_user_from_socket(state->users, socket);
        user_t *sender = NULL;
        if (sender_node != NULL) {
            sender = (user_t *) sender_node->object;
        } else {
            return;
        }

        char *channelName = strtok(parameter, "\r\n");
        //fprintf(stderr, "channelName = %s\n", channelName);
        char *joinFmt = ":%s!%s JOIN %s\r\n";
        char *joinCmd = calloc(1024, sizeof(char));
        sprintf(joinCmd, joinFmt, sender->nick, sender->email, channelName);

        ll_node_t *channel_node = get_channel(state->channels, channelName);
        if (channel_node != NULL) {
            channel_t *channel = (channel_t *) channel_node->object;
            ll_add(channel->users, sender);

            ll_node_t *channel_user_node = channel->users->head;
            while (channel_user_node != NULL) {
                user_t *channel_user = (user_t *) channel_user_node->object;
                sendall(channel_user->socket, joinCmd, strlen(joinCmd));
                channel_user_node = channel_user_node->next;
            }
        }
        // didn't find channel, create one and add user to it
        else {
            channel_t *channel = calloc(sizeof(channel_t),1);
            channel->users = calloc(sizeof(linked_list_t), 1);
            channel->name = calloc(strlen(channelName),1);
            strcpy(channel->name, channelName);

            ll_add(state->channels, channel);
            ll_add(channel->users, sender);
            fprintf(stderr, "channel->name = %s\n", channel->name );

            // send JOIN message to everyone in channel
            // while (new_channel_users != NULL){
            //     sendall(new_channel_users->socket, joinCmd, strlen(joinCmd));
            //     new_channel_users = new_channel_users->next;
            // }
        }
    }
    else if (strcmp(command,"PRIVMSG") == 0){
        char *channelName = strtok(parameter, " ");
        char *content = strtok(NULL, "");
        content += 1;
        fprintf(stderr, "channelName = %s\n", channelName);
        fprintf(stderr, "content = %s\n", content);

        char *privmsgFmt = ":%s!%s PRIVMSG %s :%s\r\n";
        char *privmsgCmd = calloc(1024, sizeof(char));

        ll_node_t *channel_node = get_channel(state->channels, channelName);
        if (channel_node != NULL) {
            channel_t *channel = (channel_t *) channel_node->object;
            ll_node_t *sender_node = get_user_from_socket(channel->users, socket);

            if (sender_node != NULL) {
                user_t *sender = (user_t *) sender_node->object;
                ll_node_t *receiver_node = channel->users->head;

                while (receiver_node != NULL) {
                    user_t *receiver = (user_t *) receiver_node->object;
                    if (receiver_node != sender_node) {
                        sprintf(privmsgCmd, privmsgFmt, sender->nick, sender->email, channelName, content);
                        sendall(receiver->socket, privmsgCmd, strlen(privmsgCmd));
                    }

                    receiver_node = receiver_node->next;
                }
            }
        } else {
            // no matching channels, checking nicknames
            ll_node_t *user_node = get_user_from_nick(state->users, channelName);
            ll_node_t *sender_node = get_user_from_socket(state->users, socket);
            if (user_node != NULL && sender_node != NULL) {
                user_t *user = (user_t *) user_node->object;
                user_t *sender = (user_t *)sender_node->object;

                if (user->socket != -1 ) {
                    sprintf(privmsgCmd, privmsgFmt, sender->nick, sender->email, channelName, content);
                    sendall(user->socket, privmsgCmd, strlen(privmsgCmd));
                }
            }
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
    state->users = calloc(sizeof(linked_list_t), 1);
    state->pending_users = calloc(sizeof(linked_list_t), 1);
    state->channels = calloc(sizeof(linked_list_t), 1);


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
                            ll_node_t *node = state->users->head;
                            while (node != NULL){
                                user_t *user = (user_t *) node->object;
                                if (user->socket == i) {
                                    user->socket = -1;
                                    break;
                                } else {
                                    node = node->next;
                                }
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
