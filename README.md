### Quan Tran & Steven Wright
### May 15, 2018

# simpleIRC - an IRC client and server with nickname email registration

## 1. Program Structure and Design

#### Nickname registration
- required by all users at every client-server connection
- on connection, user will be asked for nickname
    - if already exists, must give password
        - on success, connect to server and use IRC normally
    - if doesn't exist, must register a nickname
        - user will supply nickname, password, email, and receive an email with
        a verification code in it
        - if the user enters the verification code correctly, they become a registered
        user and connect to the server to use IRC normally
- these client-server interactions will be done with a protocol of our invention
with fields for email, password, nickname, token, possible encryption of password
- our client should be inter-operable with existing servers but our server would
not be inter-operable with existing IRC clients

#### Client
- join channels
- send messages in channels
- send private messages
- have different user modes (operators, half-operators, etc.)
- send and receive our invented protocol messages for registration
- concurrent I/O, receive and send any messages to and from any other client or
server at any times

#### Server
- maintain channels
- maintain registered users and passwords
- maintain connections
- send registration emails
- send and receive our invented protocol messages for registration
- concurrent I/O, receive and send any messages to and from any other client or
server at any time

### a. Helper functions & structs

#### Client

- `struct global_state`:
    This struct just keeps track of some useful things for use by any function
    - `nickname`: keeps track of current nickname chosen by user
    - `nickname_registered`: flag set when nickname becomes registered  

- `sendall`: given socket, data buffer and length n, sends all n bytes of
 that buffer to the socket specified

- `strip_newline`: Strip trailing \r, \n and spaces from the given string.

- `recv_all`: receive 1 byte at a time from the given socket until a newline is
read, and return a string containing the data received with the newline.

- `to_lower`: convert the given string to lowercase in-place.


- `print_usage`: Print the usage for command line options and exit the program
with the given exit code.

#### Server

- `sendall`: given socket, data buffer and length n, sends all n bytes of
 that buffer to the socket specified

- `send_email`: send an email to the given email address giving the specified
token in order to complete the registration for the given nickname.

- `get_user_from_socket`: go through the list of users and get the user currently
on the given socket from the given list of users.

- `get_user_from_nick`: get the user with the given nickname from the given list
of users.

- `get_channel`: get the channel using the given channel name.

- `sendall`: given socket, data buffer and length n, sends all n bytes of
 that buffer to the socket specified

- `strip_newline`: Strip trailing \r, \n and spaces from the given string.

- `recv_all`: receive 1 byte at a time from the given socket until a newline is
read, and return a string containing the data received with the newline.


### b. Primary functions

#### Client

- `init`: send the initial NICK and USER command to initiate connection with
the server.

- `parse_registered`: handle the REGISTERED response from server:
    
    + Prompt the user to either enter the password for the registered account or
    choose a new nickname.

    + Send a LOGIN request to the server if the user enters the password, or 
    send a NICK command to the server to request another nickname.

- `parse_not_registered`: handle the NOT REGISTERED response from server - get
the password and email for the requested nickname from the user.

- `parse_token`: handle the TOKEN response from the server - get the token from
the user and send it to the server.

- `parse_wrong_password`: prompt the password from the user to send to the 
server in response to a WRONG PASSWORD command.

- `parse_user_logged_in`: notify the user that the requested nickname has been
registered and currently logged in - therefore not available to be used.

- `parse_response`: Parse the response from the server.

    + Try to match the response with the commands in our protocol, and use the
    proper function to parse.

    + If not our protocol's command then treat it as a regular IRC command and
    use regex to parse.

    + Change the UI according to responses from the server.

- `join_channel`: Check if the user is currently in a channel. If not then send 
a request to join the specified channel to the server (if the given channel name
is not empty).

- `leave_channel`: Check if the user is currently is a channel, if yes then send
a request to leave the current channel to the server.

- `quit_program`: Simply send a QUIT command to the server and exit the program.

- `send_names_command`: Request the list of users in the current channel to the
server.

- `send_nick_command`: Send a NICK command to the server to request a new 
nickname.

- `chat`: 

    + Prompt the user for a nickname and call `init()` to send the initial NICK
    and USER to the server.

    + Initiate a socket to the server and setup non-blocking I/O for STDIN and 
    the previous socket.

    + Handle input from the server and from the user.

#### Server

- `struct user`: a struct containing information for each user:

    + nickname
    
    + email address

    + password

    + token (for the registration process)

    + socket - the current socket the user is on, -1 if the user is not logged 
    in.

- `struct channel`: a struct containing information for each channel:

    + users: the list of users currently in the channel.

    + name: channel name.

- `struct server_state`: a struct for variables to be used by all functions in 
the server.

    + users: list of all registered users on the server.

    + pending_users: list of all users pending token verification.

    + channels: list of all channels created.

- `handle_nick`: Handle the NICK command from the user.

    - Doesn't allow the user to change nick if they're in a channel.

    - Otherwise, log the user out.

    - If the requested nick doesn't exist, send a message to the user saying
        the nick has to be registered.

    - Otherwise, if the account with the existing nick is logged in, notify the
    user that the account is logged in, and if that account isn't logged in then
    notify the user that the account is registered but not logged in - therefore
    available to be used.

- `handle_register`: Handle the account registration request from the user.

    - If the requested nickname already exists, reject the registration request.
    
    - Otherwise, create a new user with the given details and put in a list
        of users pending registration completion.

    - Generate a random 7-digit token to the email the user gave.

    - Send a TOKEN request to the user requesting the above token.

- `handle_token`: Compare the given token to the generated token for the 
specified user.

    + If they match then put the user in the list of registered users and log
    the user in.

    + Otherwise notify the client of the wrong token.

- `handle_login`: Compare the given password to the requested user's password.

    + If they match and the user is not logged in, log the user in.

    + If the user is logged in, notify the client that the user is not available
    to be used to log in.

    + If the password is wrong, notify the client of the wrong password.

- `handle_quit`: Handle the QUIT command from the client.

    + Remove the quitting user from all channels they are in.

    + Set the user's socket to -1 to log out.

- `handle_join`: Handle a JOIN request from the users.

    + Get the user from the socket value.

    + Find the channel and add the user to that channel.

    + If no channel found, create one and add the user to it.

- `handle_privmsg`: Handle private messages from the users through PRIVMSG.

    + Treat the recipient as a channel's name and look for the channel. if found
    then broadcast the message to everyone currently in the channel.

    + Otherwise check if the recipient is a user. If yes then send the message
    to that user.

    + If no user with such name found, send a NOT FOUND response to the client.

- `handle_part`: Handle the PART command from the client.

    + Find the channel with the given name, and find the user based on the 
    current socket.

    + If both are found, remove the user from the channel and notify everyone in
    the channel, including the leaving user (to let them know they leave the 
    channel successfully).

- `handle_names`: Handle the NAMES command from the client.

    + Find the channel with the given name. If found, send the client the names
    of all users currently in the channel.

- `handle_channels`: Handle the CHANNELS command the client by sending the names
of all created channels on the server to the client.

- `handle_data`: Main function that handles data coming in from the clients.

    + Extract the first part of the data and treat it as the command.

    + Use the appropriate function to handle the data based on the command, 
    otherwise if the command is not recognized we send a "Unknown command" 
    response to the client.


## 2. Implementation Challenges

- The biggest challenge we encounter was with receiving data. At first we just
received as much data as possible and try to parse them as a single command.

    This didn't cause any problem at first because we tokenized the command by 
newlines, which effectively discarded everything after the first command if 
there were than one command in the received data.

    Later we had to change how we tokenized the data, which led to us figuring 
out there had always been multiple commands in a single segment of data, and it
messed up our handling of data.

    We had to then end all commands with a newline, and when receiving data we 
had to receive 1 byte at a time and use newlines to delimit data.

- Also, we didn't refactor and document the code properly while developing, so
it took quite some time at the end to perform those tasks.

## 3. Testing

- We first developed the client and made it interoperable with regular IRC 
servers, so we used `chat.freenode.net` as our regular IRC server, and `weechat`
as a standard IRC client to compare our behaviors to its behaviors.

- We also captured traffic between `weechat` and `chat.freenode.net` to further
understand IRC in addition to the RFC's, and used that while developing our 
client.

- For the server, we also started by implementing regular IRC features by making
it recognize simple IRC commands like NICK or PRIVMSG. Because our client already
interoperated with regular IRC servers at that point, we can just make sure our
server worked well with our client.

- Finally we went on to implement the registration and user authentication phase.
We tested by trying running the clients on machines the server was not running
on, registering different users and logging into different users, and also by
using regular IRC features such as sending messages and joining/leaving channels
to see if they still worked.


## 4. Remaining Bugs

- Currently the server is designed to work specifically with the client we 
created - therefore the sequence of commands is preserved and many edge cases
can be eliminated. However, if a user tries to communicate with the server
using programs like `nc`, they can send commands in different sequence and end
up producing unexpected results.

    For example, it isn't possible to get to the registration phase without
sending a NICK command to the server first, which helps the server checks if 
the registering nickname exists or not. However, a malicious user can skip 
sending NICK and just send REGISTER to register an account with an existing 
nickname.

