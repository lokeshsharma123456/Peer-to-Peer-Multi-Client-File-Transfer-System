# Running the Project

The project uses POSIX sockets, so build and run it in WSL (Ubuntu) or Linux.

## 1. Build

From the project directory:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -pthread tracker.cpp -o tracker
g++ -std=c++17 -Wall -Wextra -pedantic -pthread client.cpp -o client
```

## 2. Start the tracker

In terminal 1:

```bash
./tracker tracker_info.txt 0
```

The tracker listens on port `5000` for tracker number `0`.

## 3. Start a client

In terminal 2, use a different client port:

```bash
./client 127.0.0.1:18000 tracker_info.txt
```

Start another client with another port, such as `18001`, to test multiple users.

## 4. Basic commands

Run these commands in the client prompt:

```text
create_user alice password123
login alice password123
create_group team
upload_file /absolute/path/to/file.txt team
list_files team
get_peers team file.txt
stop_share team file.txt
logout
quit
```

A user must be logged in and must belong to a group before listing files, sharing files, or requesting peers.

## Configuration

`tracker_info.txt` contains the tracker host and port on separate lines:

```text
127.0.0.1
5000
```

## Current scope

The tracker and client currently support authentication, groups, file metadata registration, SHA1 file and 512 KiB piece hashes, file listing, peer lookup, and stop-sharing state cleanup.

Actual peer-to-peer piece transfer, parallel downloading, and synchronized dual trackers are not implemented yet. `download_file` therefore does not write a destination file.
