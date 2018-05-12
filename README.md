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

#### Server

- `struct user`:

- `struct channel`:

- `struct server_state`:

- `sendall`:

- `add_user`:

- `remove_user`:

### b. Primary functions

#### Client

- `init`:

- `parse_response`:

- `chat`:

- `main`:

#### Server

- `send_email`:

- `handle_data`:

- `main`:


## 2. Implementation Challenges

## 3. Testing

## 4. Remaining Bugs


## 5. Memory management

#### Client side:

- Command used to run `valgrind`:

```
sudo valgrind --leak-check=full --show-leak-kinds=all ./ctcp -c localhost:1234 -p 12312 --corrupt 5 --delay 5 --duplicate 5 --drop 5 -w 10 < reference
```

- `valgrind`'s output:


#### Server side:

- Command used to run `valgrind`:

```
sudo valgrind --leak-check=full --show-leak-kinds=all ./ctcp -s -p 1234 --corrupt 5 --delay 5 --duplicate 5 --drop 5 -w 10 > out
```

- `valgrind`'s output:
