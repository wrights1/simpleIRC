## SimpleIRC & Username email registration
#### Quan Tran & Steven Wright

### Nickname registration
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

### Client
- join channels
- send messages in channels
- send private messages
- have different user modes (operators, half-operators, etc.)
- multiple channels at once? how to have multiple screens/buffers?
- send and receive our invented protocol messages for registration
- concurrent I/O, receive and send any messages to and from any other client or
server at any time

### Server
- maintain channels
- maintain registered users and passwords
- maintain connections
- send registration emails
- send and receive our invented protocol messages for registration
- concurrent I/O, receive and send any messages to and from any other client or
server at any time
