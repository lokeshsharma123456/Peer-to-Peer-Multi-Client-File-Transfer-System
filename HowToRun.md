# Peer-to-Peer-Multi-Client-File-Transfer-System

# Peer-to-Peer Group Based File Sharing System

## Prerequisites

**Software Requirement**

1. G++ compiler

   - **To install G++ :** `sudo apt-get install g++`
2. OpenSSL library

   - **To install OpenSSL library :** `sudo apt-get install openssl`

**Platform:** Linux `<br/>`

## Installation

```
1. cd client
2. make
3. cd ../tracker
5. make
6. cd ..
```

## Usage

### Tracker

1. Run Tracker:

```
cd tracker
./tracker <TRACKER INFO FILE> <TRACKER NUMBER>
ex: ./tracker tracker_info.txt 1
```

`<TRACKER INFO FILE>` contains the IP, Port details of all the trackers.

```
Ex:
127.0.0.1
5000
127.0.0.1
6000
```

2. Close Tracker:

```
quit
```

### Client:

1. Run Client:

```
cd client
./client <IP>:<PORT> <TRACKER INFO FILE>
ex: ./client 127.0.0.1:18000 tracker_info.txt
```

2. Create user account:

```
create_user <user_id> <password>
```

3. Login:

```
login <user_id> <password>
```

4. Create Group:

```
create_group <group_id>
```

5. Join Group:

```
join_group <group_id>
```

6. Leave Group:

```
leave_group <group_id>
```

7. List pending requests:

```
list_requests <group_id>
```

8. Accept Group Joining Request:

```
accept_request <group_id> <user_id>
```

9. List All Group In Network:

```
list_groups
```

10. List All sharable Files In Group:

```
list_files <group_id>
```

11. Upload File:

```
upload_file <file_path> <group_id>
```

12. Download File:

```
download_file <group_id> <file_name> <destination_path>
```

13. Logout:

```
logout
```

14. Show_downloads:

```
show_downloads
```

15. Stop sharing:

```
stop_share <group_id> <file_name>
```

## Working

1. User should create an account and register with tracker.
2. Login using the user credentials.
3. Tracker maintains information of clients with their files(shared by client) to assist the clients for the communication between peers.
4. User can create Group and hence will become admin of that group.
5. User can fetch list of all Groups in server.
6. User can join/leave group.
7. Group admin can accept group join requests.
8. Share file across group: Shares the filename and SHA1 hash of the complete file as well as piecewise SHA1 with the tracker.
9. Fetch list of all sharable files in a Group.
10. Download:
    1. Retrieve peer information from tracker for the file.
    2. Download file from multiple peers (different pieces of file from different peers - piece selection algorithm) simultaneously and all the files which client downloads will be shareable to other users in the same group. File integrity is ensured using SHA1 comparison.
11. Piece selection algorithm used: Selects random piece and then downloads it from a random peer having that piece.
12. Show downloads.
13. Stop sharing file.
14. Logout - stops sharing all files.
15. Whenever client logins, all previously shared files before logout should automatically be on sharing mode.

## Assumptions

1. Only one tracker is implemented and that tracker should always be online.
2. The peer can login from different IP addresses, but the details of his downloads/uploads will not be persistent across sessions.
3. SHA1 integrity checking doesn't work correctly for binary files, even though in most likelihood the file would have downloaded correctly.
4. File paths should be absolute.

Advanced OS
Assignment #3 - Peer-to-Peer Group Based File Sharing System Hard - Deadline : 08/11/2021 (11:55 pm)
Guidelines:
Languages Allowed: C/C++ Submission format: `<rollno>`_a3.zip
ZERO tolerance towards any kind of code plagiarism. Plagiarism will fetch you a ZERO.
Pre-requisites:
Socket Programming, SHA1 hash, Multi-threading
Goal:
In this assignment, you need to build a group based file sharing system where users can share, download files from the group they belong to. Download should be parallel with multiple pieces from multiple peers.
Note:

- You have to divide the file into logical “pieces” , wherein the size of each piece should be 512KB
- SHA1: Suppose the file size is 1024KB, then divide it into two pieces of 512KB each
  and take SHA1 hash of each part, assume that the hashes are HASH1 & HASH2 then the corresponding hash string would be H1H2 , where H1 & H2 are starting 20 characters of HASH1 & HASH2 respectively and hence H1H2 is 40 characters
- Authentication for login needs to be done
- Error handling to be done(else marks will be deducted)
- You can implement your own version of torrent architecture i.e., you need not implement exact same protocols mentioned in the bit torrent paper. You can design your own algorithm of your own architecture.
- You need to design your own Piece Selection Algorithm.
- Zip file should contain only 2 files(tracker.cpp and client.cpp).
  Architecture Overview:
  The Following entities will be present in the network :

1. Synchronized trackers(2 tracker system) :
   a. Maintain information of clients with their files(shared by client) to assist the clients for the communication between peers
   b. Trackers should be synchronized i.e all the trackers if online should be in sync with each other
2. Clients:
   a. User should create an account and register with tracker
   b. Login using the user credentials
   c. Create Group and hence will become owner of that group
   d. Fetch list of all Groups in server
   e. Request to Join Group
   f. Leave Group
   g. Accept Group join requests (if owner)
   h. Share file across group: Share the filename and SHA1 hash of the complete file as well as piecewise SHA1 with the tracker
   i. Fetch list of all sharable files in a Group
   j. Download file
   i. Retrieve peer information from tracker for the file
   ii. Core Part: Download file from multiple peers (different pieces of file from different peers - piece selection algorithm) simultaneously and all the files which client downloads will be shareable to other users in the same group. Ensure file integrity from SHA1 comparison
   k. Show downloads
   l. Stop sharing file
   m. Stop sharing all files(Logout)
   n. Whenever client logins, all previously shared files before logout should automatically be on sharing mode
   Working:
3. At Least one tracker will always be online.
4. Client needs to create an account (userid and password) in order to be part of the network.
5. Client can create any number of groups(groupid should be different) and hence will be owner of those groups
6. Client needs to be part of the group from which it wants to download the file
7. Client will send join request to join a group
8. Owner Client Will Accept/Reject the request
9. After joining group ,client can see list of all the shareable files in the group
10. Client can share file in any group (note: file will not get uploaded to tracker but only the `<ip>`:`<port>` of the client for that file)
11. Client can send the download command to tracker with the group name and filename and tracker will send the details of the group members which are currently sharing that particular file
12. After fetching the peer info from the tracker, client will communicate with peers about the portions of the file they contain and hence accordingly decide which part of data to take from which peer (You need to design your own Piece Selection Algorithm)
13. As soon as a piece of file gets downloaded it should be available for sharing
14. After logout, the client should temporarily stop sharing the currently shared files till the next login
15. All trackers need to be in sync with each other
    Commands:
16. Tracker:
    a. Run Tracker: ./tracker tracker_info.txt tracker_no tracker_info.txt - Contains ip, port details of all the trackers
    b. Close Tracker: quit
17. Client:
    a. Run Client: ./client `<IP>`:`<PORT>` tracker_info.txt
    tracker_info.txt - Contains ip, port details of all the trackers
    b. Create User Account: create_user <user_id> `<passwd>`
    c. Login: login <user_id> `<passwd>`
    d. Create Group: create_group <group_id>
    e. Join Group: join_group <group_id>
    f. Leave Group: leave_group <group_id>
    g. List pending join: requests list_requests <group_id>
    h. Accept Group Joining Request: accept_request <group_id> <user_id>
    i. List All Group In Network: list_groups
    j. List All sharable Files In Group: list_files <group_id>
    k. Upload File: upload_file <file_path> <group_id>
    l. Download File: download_file <group_id> <file_name> <destination_path>
    m. Logout: logout
    n. Show_downloads: show_downloads
    Output format:
    [D] [grp_id] filename
    [C] [grp_id] filename
    D(Downloading), C(Complete)
    o. Stop sharing: stop_share <group_id> <file_name>
