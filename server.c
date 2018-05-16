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
    char * token;
    int socket;
};
typedef struct user user_t;

struct channel{
    linked_list_t *users;
    char * name;
};
typedef struct channel channel_t;

struct server_state{
    linked_list_t *users;
    linked_list_t *pending_users;
    linked_list_t *channels;
};
typedef struct server_state server_state_t;

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

/*
    creates and sends an email to the specified email address through
    denison's mail server.
    cantains a randomly generated acccess token which is used to
    verify the email

    only returns nonzero on failure to connect to mail server
*/
int send_email(char *token, char *email, char * nick){
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
                   "Here is your cerification code for registering the nickname "
                   "\"%s\" on the simpleIRC server: %s.\r\n"
                   "Enter this where prompted in your terminal, then hit enter."
                   "\n.\n";

    sprintf(emailCmd,emailFmt,email, email, nick, token);
    sendall(sockfd, emailCmd, strlen(emailCmd));
    free(emailCmd);
    close(sockfd);

}

/*
    given a list of users and a socket descriptor,
    returns the node containing the user struct that is associated with
    that socket
*/
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

/*
    given a list of users and a nickname, returns the
    node containing the specified user
*/
ll_node_t* get_user_from_nick(linked_list_t *users, char *nick) {
    ll_node_t *node = users->head;
    while (node != NULL) {
        user_t *user = (user_t *) node->object;
        fprintf(stderr, "user->nick = %s, nick = %s, %lu %lu\n",user->nick, nick, strlen(user->nick), strlen(nick));
        fprintf(stderr, "nick = %s\n", nick);
        if (strcmp(user->nick, nick) == 0) {
            return node;
        }

        node = node->next;
    }

    return NULL;
}

/*
    given a list of channels and the name of a channel,
    returns the node containing the specified channel
    returns NULL if not found

*/
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

/*
    Strip trailing \r, \n or space from the given string.

*/
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

/*
    Receive 1 byte at a time from the given socket until a newline is found.
    Return a string containing the entire string including the newline, which
        have to be freed.
*/
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

/*
    Handle the NICK command from the user.
    - Doesn't allow the user to change nick if they're in a channel.
    - Otherwise, log the user out.
    - If the requested nick doesn't exist, send a message to the user saying
        the nick has to be registered.
    - Otherwise, if the account with the existing nick is logged in, notify the
    user that the account is logged in, and if that account isn't logged in then
    notify the user that the account is registered but not logged in - therefore
    available to be used.
*/
void handle_nick(server_state_t *state, int socket, char *nick)
{
    ll_node_t *sender_node = get_user_from_socket(state->users, socket);

    if (sender_node != NULL) {
        user_t *sender = (user_t *) sender_node->object;
        ll_node_t *channels = state->channels->head;
        int invalid_nick = 0;

        while (channels != NULL){
            channel_t *channel = (channel_t *)channels->object;
            ll_node_t *channel_user_node = channel->users->head;
            while (channel_user_node != NULL){
                user_t *channel_user = (user_t *) channel_user_node->object;
                if (strcmp(channel_user->nick,sender->nick)==0){
                    char * msg = "To change your nick you must first leave the "
                        "current channel.\r\n";
                    sendall(socket, msg, strlen(msg));
                    invalid_nick = 1;
                    break;
                }
                channel_user_node = channel_user_node->next;
            }
            channels = channels->next;
        }

        if (invalid_nick) {
            return;
        }

        user_t *user = (user_t *) sender_node->object;
        user->socket = -1;
    }

    ll_node_t *found_node = get_user_from_nick(state->users, nick);
    if (found_node != NULL) {
        user_t *user = (user_t *) found_node->object;
        if (user->socket == -1) {
            //nick taken but not logged in, expecting password
            char *msg = "REGISTERED\r\n";
            sendall(socket, msg, strlen(msg));
        } else {
            //nick taken and currently logged in, pick new name
            char *msg = "USER LOGGED IN\r\n";
            sendall(socket, msg, strlen(msg));
        }
    } else {
        //nick not taken, expecting registration
        char *msg = "NOT REGISTERED\r\n";
        sendall(socket, msg, strlen(msg));
    }
}

/*
    Handle the account registration request from the user.
        - If the requested nickname already exists, reject the registration request.
        - Otherwise, create a new user with the given details and put in a list
            of users pending registration completion.
        - Generate a random 7-digit token to the email the user gave.
        - Send a TOKEN request to the user requesting the above token.
 */
void handle_register(server_state_t *state, int socket, char *parameters)
{
    //command = strtok(line," ");
    char *nick = strtok(parameters, " ");
    char *email = strtok(NULL, " ");
    char *password = strtok(NULL, "");

    ll_node_t *user_node = get_user_from_nick(state->users, nick);
    if (user_node != NULL) {
        // if the registering nickname already exists, reject the registration
        // request
        char *msg = "Requested nickname already exists!\r\n";
        sendall(socket, msg, strlen(msg));
        return;
    }

    user_t *new_user = (user_t *) calloc(sizeof(user_t), 1);
    int n = 256;

    new_user->nick = calloc(sizeof(char), n);
    new_user->email = calloc(sizeof(char), n);
    new_user->password = calloc(sizeof(char), n);
    new_user->token = calloc(sizeof(char), 16);
    new_user->socket = socket;

    strcpy(new_user->nick, nick);
    strcpy(new_user->email, email);
    strcpy(new_user->password, password);

    int token = rand() % 9000000 + 1000000;
    char *token_str = calloc(16, sizeof(char));
    sprintf(token_str, "%d", token);
    strcpy(new_user->token, token_str);
    send_email(token_str,email,nick);

    ll_add(state->pending_users, new_user);

    char *tokenRequest = "TOKEN\r\n";
    sendall(socket, tokenRequest, strlen(tokenRequest));
}

/*
    Handle the token given by the user.
 */
void handle_token(server_state_t *state, int socket, char *parameters)
{
    char *nick = strtok(parameters, " ");
    char *token_str = strtok(NULL, "");

    ll_node_t *pending_user_node = get_user_from_nick(state->pending_users, nick);
    if (pending_user_node != NULL) {
        user_t *pending_user = (user_t *) pending_user_node->object;
        if (strcmp(pending_user->token, token_str) == 0) {
            // If the token matches the one we generate, add the user to the 
            // list of registered users and remove them from the list of
            // pending users.
            ll_remove(state->pending_users, pending_user_node);
            ll_add(state->users, pending_user);

            // Send a NICK command to the user letting them know the registration
            // process completed successfully.
            char *joinFmt = ":%s!%s NICK :%s\r\n";
            char *joinMsg = calloc(sizeof(char), strlen(joinFmt) + strlen(nick)*3 + 2);
            sprintf(joinMsg, joinFmt, nick, pending_user->email, nick);
            sendall(socket, joinMsg, strlen(joinMsg));
        } else {
            // If the tokens don't match, send a WRONG TOKEN response to the user.
            char *wrongToken = "WRONG TOKEN\r\n";
            sendall(socket, wrongToken, strlen(wrongToken));
        }
    }
}

/*
    Handle the login request from user.
 */
void handle_login(server_state_t *state, int socket, char *parameters)
{
    char *nick = strtok(parameters, " ");
    char *pass = strtok(NULL, "\r\n");

    // get the user with the given nick
    ll_node_t *user_node = get_user_from_nick(state->users, nick);

    if (user_node != NULL) {
        user_t *user = (user_t *) user_node->object;
        if (strcmp(user->password, pass) == 0 && user->socket == -1) {
            // Send a NICK command to the user if the password is correct,
            // and the requested account is not logged in.
            user->socket = socket;

            char *joinFmt = ":%s!%s NICK :%s\r\n";
            char *joinMsg = calloc(sizeof(char), strlen(joinFmt) + strlen(nick)*3 + 2);
            sprintf(joinMsg, joinFmt, nick, user->email, nick);
            sendall(socket, joinMsg, strlen(joinMsg));

            free(joinMsg);
        } else if (user->socket != -1) {
            // if the account is logged in then reject the request
            char *msg = "USER LOGGED IN\r\n";
            sendall(socket, msg, strlen(msg));
        } else {
            // if the password is incorrect then notify the user
            char *wrongPass = "WRONG PASSWORD\r\n";
            sendall(socket, wrongPass, strlen(wrongPass));
        }
    }
}

/*
    Handle the user's request to QUIT/disconnect from the server.
 */
void handle_quit(server_state_t *state, int socket)
{
    // remove QUITting user from all channels they are in and set their socket to -1
    ll_node_t *sender_node = get_user_from_socket(state->users, socket);
    ll_node_t *channel_node = state->channels->head;

    while (channel_node != NULL && sender_node != NULL){
        user_t *sender = (user_t *) sender_node->object;
        channel_t *channel = (channel_t *) channel_node->object;
        ll_node_t *channel_user_node = channel->users->head;

        // loop through all the channels
        while (channel_user_node != NULL){
            user_t *channel_user = (user_t *) channel_user_node->object;
            if (strcmp(channel_user->nick, sender->nick)==0){
                // If we find the user in the channel, remove them and notify
                // everyone in the channel
                ll_node_t *node_to_remove = get_user_from_nick(channel->users, sender->nick);

                char *partFmt = ":%s!%s PART %s\r\n";
                char *partCmd = calloc(1024, sizeof(char));
                ll_node_t* user_to_send_part = channel->users->head;

                // send PART message to channel
                while (user_to_send_part != NULL && channel_user_node != user_to_send_part) {
                    user_t *user = (user_t *) user_to_send_part->object;
                    sprintf(partCmd, partFmt, sender->nick, sender->email, channel->name);
                    sendall(user->socket, partCmd, strlen(partCmd));
                    user_to_send_part = user_to_send_part->next;
                }
                ll_remove(channel->users, node_to_remove);

                break;
            }
            channel_user_node = channel_user_node->next;
        }
        channel_node = channel_node->next;
    }

    // set the user's socket to -1
    if (sender_node != NULL){
        user_t *sender = (user_t *) sender_node->object;
        sender->socket = -1;
    }
}

/*
    Handle a JOIN request from the users.
 */
void handle_join(server_state_t *state, int socket, char *parameters)
{
    // Get the user using the current socket
    ll_node_t *sender_node = get_user_from_socket(state->users, socket);
    user_t *sender = NULL;
    if (sender_node != NULL) {
        sender = (user_t *) sender_node->object;
    } else {
        return;
    }    

    char *channelName = strtok(parameters, "");
    fprintf(stderr, "channelName = %s\n", channelName);
    char *joinFmt = ":%s!%s JOIN %s\r\n";
    char *joinCmd = calloc(1024, sizeof(char));
    sprintf(joinCmd, joinFmt, sender->nick, sender->email, channelName);

    ll_node_t *channel_node = get_channel(state->channels, channelName);
    if (channel_node != NULL) {
        // if channel found, add the user to it and notify everyone in the 
        // channel, including the joining user.
        channel_t *channel = (channel_t *) channel_node->object;
        ll_add(channel->users, sender);

        ll_node_t *channel_user_node = channel->users->head;
        while (channel_user_node != NULL) {
            user_t *channel_user = (user_t *) channel_user_node->object;
            sendall(channel_user->socket, joinCmd, strlen(joinCmd));
            channel_user_node = channel_user_node->next;
        }
    } else {
        // didn't find channel, create one and add user to it
        channel_t *channel = calloc(sizeof(channel_t),1);
        channel->users = calloc(sizeof(linked_list_t), 1);
        channel->name = calloc(strlen(channelName),1);
        strcpy(channel->name, channelName);

        ll_add(state->channels, channel);
        ll_add(channel->users, sender);
        fprintf(stderr, "channel->name = %s\n", channel->name);
        sendall(sender->socket, joinCmd, strlen(joinCmd));
    }
}

/*
    Handle the PRIVMSG command from users.
*/
void handle_privmsg(server_state_t *state, int socket, char *parameters)
{
    char *channelName = strtok(parameters, " ");
    char *content = strtok(NULL, "");
    content += 1;
    fprintf(stderr, "channelName = %s\n", channelName);
    fprintf(stderr, "content = %s\n", content);

    char *privmsgFmt = ":%s!%s PRIVMSG %s :%s\r\n";
    char *privmsgCmd = calloc(1024, sizeof(char));

    // Treat the recipient as a channel's name and look for the channel
    ll_node_t *channel_node = get_channel(state->channels, channelName);
    if (channel_node != NULL) {
        // if a channel is found, broadcast the message to everyone in the channel
        // except for the sender
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
        } else {
            // no matching nick or channel, notify client
            if (sender_node != NULL) {
                user_t *sender = (user_t *)sender_node->object;
                sendall(sender->socket, "NOT FOUND\r\n", 12);
            }
        }
    }
}

/* 
    Handle users' request to leave a channel
*/
void handle_part(server_state_t *state, int socket, char *parameters)
{
    // find the channel
    char *channelName = strtok(parameters, "");
    ll_node_t *channel_node = get_channel(state->channels, channelName);

    if (channel_node != NULL) {
        //remove user from channel and send PART message to rest of channel
        channel_t *channel = (channel_t *) channel_node->object;
        ll_node_t *sender_node = get_user_from_socket(channel->users, socket);

        if (sender_node != NULL){
            user_t* sender = (user_t *) sender_node->object;

            char *partFmt = ":%s!%s PART %s\r\n";
            char *partCmd = calloc(1024, sizeof(char));
            ll_node_t* channel_user = channel->users->head;

            // send PART message to channel
            while (channel_user != NULL) {
                user_t *user = (user_t *) channel_user->object;
                sprintf(partCmd, partFmt, sender->nick, sender->email, channelName);
                sendall(user->socket, partCmd, strlen(partCmd));
                channel_user = channel_user->next;
            }

            // remove sender of PART from channel
            ll_remove(channel->users, sender_node); 
        }
    }
}

/* 
    Handle NAMES commands from the users.
*/
void handle_names(server_state_t *state, int socket, char *parameters)
{
    if (parameters == NULL) {
        return;
    }

    // get the requested channel
    char *channelName = strtok(parameters, "");
    ll_node_t *channel_node = get_channel(state->channels, channelName);

    if (channel_node != NULL){
        channel_t *channel = (channel_t *) channel_node->object;

        if (channel != NULL){
            // get the names from all the users in the channel
            int channel_users_length = ll_length(channel->users);

            char * namesPrefix = "Users in the channel:\n";
            char * names = calloc(channel_users_length*257 + strlen(namesPrefix), 1);
            strcpy(names, namesPrefix);
            int currentBase = strlen(namesPrefix);

            ll_node_t *channel_names = channel->users->head;
            while (channel_names != NULL){
                user_t *user = (user_t *) channel_names->object;
                char *name = user->nick;
                char *nameCmd = calloc(258, sizeof(char));
                sprintf(nameCmd, "\t%s\n",name);
                strcpy(names+currentBase,nameCmd);
                currentBase = currentBase + strlen(nameCmd);
                channel_names = channel_names->next;
            }

            // send the list of users to the client
            ll_node_t *sender_node = get_user_from_socket(state->users, socket);
            if (sender_node != NULL){
                user_t *sender = (user_t *) sender_node->object;
                sendall(sender->socket, names, strlen(names));
            }
        }
    }
}

/*
    Handle CHANNELS commands from users.
 */
void handle_channels(server_state_t *state, int socket)
{
    // Get the list of channels and send their names to the user
    ll_node_t *channels = state->channels->head;

    if (channels != NULL) {
        int channels_length = ll_length(state->channels);

        char * channelsPrefix = "Channels on the server:\n";
        char * channel_list = calloc(channels_length*512 + strlen(channelsPrefix), 1);
        strcpy(channel_list, channelsPrefix);
        int currentBase = strlen(channelsPrefix);

        // loop through the channels and get their names
        while (channels != NULL){
            channel_t *channel = (channel_t *) channels->object;
            char *name = channel->name;
            char *nameCmd = calloc(514, sizeof(char));
            sprintf(nameCmd, "\t%s\n",name);
            strcpy(channel_list + currentBase, nameCmd);
            currentBase = currentBase + strlen(nameCmd);
            channels = channels->next;

        }

        // identify the sender using the socket value and send the list of 
        // channel names to that user
        ll_node_t *sender_node = get_user_from_socket(state->users, socket);
        if (sender_node != NULL){
            user_t *sender = (user_t *) sender_node->object;
            sendall(sender->socket, channel_list, strlen(channel_list));
        }
    }
}

/*
    parses client messages and respondes accordingly to each one.

    if client sends message that is not accounted for here, no response
    will be generated
    sends unknown command if command received is not valid
*/
void handle_data(char * buf, int socket, struct server_state *state){
    strip_newline(buf);
    if (strlen(buf) == 0) {
        return;
    }

    char *line = calloc(strlen(buf)+1,1);
    strcpy(line, buf);  // copy of str to be tokenized
    char * command = NULL;
    command = strtok(line," ");

    char * parameter = strtok(NULL, "");

    if (strcmp(command,"NICK") == 0 || strcmp(command,"nick") == 0){
        handle_nick(state, socket, parameter);
    } else if (strcmp(command, "REGISTER") == 0) {
        handle_register(state, socket, parameter);
    } else if (strcmp(command, "TOKEN") == 0) {
        handle_token(state, socket, parameter);
    } else if (strcmp(command, "LOGIN") == 0) {
        handle_login(state, socket, parameter);
    } else if (strcmp(command, "QUIT") == 0 || strcmp(command, "quit") == 0) {
        handle_quit(state, socket);
    } else if (strcmp(command,"JOIN") == 0 || strcmp(command,"join") == 0){
        handle_join(state, socket, parameter);
    } else if (strcmp(command,"PRIVMSG") == 0 || strcmp(command,"privmsg") == 0) {
        handle_privmsg(state, socket, parameter);
    } else if (strcmp(command,"PART") == 0){
        handle_part(state, socket, parameter);
    } else if (strcmp(command,"NAMES") == 0 || strcmp(command,"names") == 0){
        handle_names(state, socket, parameter);
    } else if (strcmp(command,"CHANNELS") == 0 || strcmp(command,"channels") == 0){
        handle_channels(state, socket);
    } else {
        char * uk = "Unknown command.\r\n";
        sendall(socket, uk, strlen(uk));
    }
}

int main(void)
{
    srand(time(NULL));
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number
    //char buf[1024];

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;
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
                    //char *buf = calloc(1024, sizeof(char));
                    //memset(buf, 0, sizeof(buf));
                    char *buf = recv_all(i);

                    if (strlen(buf) == 0) {
                    //if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        //if (nbytes == 0) {
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
                        //} else {
                        //    perror("recv");
                        //}
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        // we got some data from a client
                        handle_data(buf, i, state);
                    }

                    free(buf);
                } // END handle data from client

                //free(buf);
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!

    return 0;
}
