#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <fstream>
#include <cstdarg>
#include <algorithm>

using namespace std;

#define MAX_BUFFER 1024
#define TRACKER_PORT 5000
#define SA struct sockaddr

// ===================== DATA STRUCTURES =====================

struct File
{
    string filename;
    string file_hash;            // SHA1 of complete file
    vector<string> piece_hashes; // SHA1 of each 512KB piece
    int piece_count;
    long file_size;
};

struct Group
{
    string group_id;
    string owner_id;
    vector<string> members;            // List of member user IDs
    vector<string> pending_requests;   // Users requesting to join
    map<string, vector<string>> files; // Map: filename -> list of peers sharing it
};

struct User
{
    string user_id;
    string password;
    vector<string> groups;                  // Groups user belongs to
    map<string, vector<File>> shared_files; // Map: group_id -> files shared in that group
    string ip_address;
    int port;
    bool is_online;
};

struct Tracker
{
    int tracker_socket;
    struct sockaddr_in address;
    map<string, User> users;         // user_id -> User
    map<string, Group> groups;       // group_id -> Group
    map<string, File> file_registry; // filename -> File metadata
};

// ===================== GLOBAL TRACKER =====================
Tracker global_tracker;
pthread_mutex_t tracker_lock = PTHREAD_MUTEX_INITIALIZER;

// ===================== UTILITY FUNCTIONS =====================

void log_message(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("[TRACKER] ");
    vprintf(format, args);
    va_end(args);
}

void error_exit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

// Split string by delimiter
vector<string> split_string(string str, char delimiter)
{
    vector<string> tokens;
    string token;
    for (char c : str)
    {
        if (c == delimiter)
        {
            if (!token.empty())
            {
                tokens.push_back(token);
                token.clear();
            }
        }
        else
        {
            token += c;
        }
    }
    if (!token.empty())
    {
        tokens.push_back(token);
    }
    return tokens;
}

bool is_member(const Group &group, const string &user_id)
{
    return find(group.members.begin(), group.members.end(), user_id) != group.members.end();
}

void remove_user_from_file_lists(const string &user_id)
{
    for (auto &group_pair : global_tracker.groups)
    {
        for (auto file_it = group_pair.second.files.begin(); file_it != group_pair.second.files.end();)
        {
            vector<string> &peers = file_it->second;
            peers.erase(remove(peers.begin(), peers.end(), user_id), peers.end());
            if (peers.empty())
            {
                file_it = group_pair.second.files.erase(file_it);
            }
            else
            {
                ++file_it;
            }
        }
    }
}

// ===================== TRACKER OPERATIONS =====================

// 1. CREATE USER ACCOUNT
string handle_create_user(string user_id, string password)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.users.find(user_id) != global_tracker.users.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: User already exists";
    }

    User new_user;
    new_user.user_id = user_id;
    new_user.password = password;
    new_user.is_online = false;

    global_tracker.users[user_id] = new_user;
    log_message("User created: %s\n", user_id.c_str());

    pthread_mutex_unlock(&tracker_lock);
    return "SUCCESS: Account created for " + user_id;
}

// 2. LOGIN USER
string handle_login(string user_id, string password)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.users.find(user_id) == global_tracker.users.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: User not found";
    }

    User &user = global_tracker.users[user_id];
    if (user.password != password)
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Invalid password";
    }

    user.is_online = true;
    log_message("User logged in: %s\n", user_id.c_str());

    pthread_mutex_unlock(&tracker_lock);
    return "SUCCESS: Logged in successfully";
}

// 3. LOGOUT USER
string handle_logout(string user_id)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.users.find(user_id) == global_tracker.users.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: User not found";
    }

    User &user = global_tracker.users[user_id];
    user.is_online = false;

    remove_user_from_file_lists(user_id);
    for (auto &group_pair : user.shared_files)
    {
        user.shared_files[group_pair.first].clear();
    }

    log_message("User logged out: %s\n", user_id.c_str());

    pthread_mutex_unlock(&tracker_lock);
    return "SUCCESS: Logged out";
}

// 4. CREATE GROUP
string handle_create_group(string user_id, string group_id)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.users.find(user_id) == global_tracker.users.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: User not found";
    }

    if (global_tracker.groups.find(group_id) != global_tracker.groups.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Group already exists";
    }

    Group new_group;
    new_group.group_id = group_id;
    new_group.owner_id = user_id;
    new_group.members.push_back(user_id);

    global_tracker.groups[group_id] = new_group;
    global_tracker.users[user_id].groups.push_back(group_id);

    log_message("Group created: %s by %s\n", group_id.c_str(), user_id.c_str());

    pthread_mutex_unlock(&tracker_lock);
    return "SUCCESS: Group " + group_id + " created";
}

// 5. JOIN GROUP (Request)
string handle_join_group(string user_id, string group_id)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.users.find(user_id) == global_tracker.users.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: User not found";
    }

    if (global_tracker.groups.find(group_id) == global_tracker.groups.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Group not found";
    }

    Group &group = global_tracker.groups[group_id];

    // Check if already member
    for (const string &member : group.members)
    {
        if (member == user_id)
        {
            pthread_mutex_unlock(&tracker_lock);
            return "ERROR: Already member of group";
        }
    }

    if (find(group.pending_requests.begin(), group.pending_requests.end(), user_id) != group.pending_requests.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Join request already pending";
    }

    group.pending_requests.push_back(user_id);

    log_message("Join request from %s to group %s\n", user_id.c_str(), group_id.c_str());

    pthread_mutex_unlock(&tracker_lock);
    return "SUCCESS: Join request sent to group owner";
}

// 6. ACCEPT JOIN REQUEST (Owner only)
string handle_accept_request(string owner_id, string group_id, string user_id)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.groups.find(group_id) == global_tracker.groups.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Group not found";
    }

    Group &group = global_tracker.groups[group_id];
    if (group.owner_id != owner_id)
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Not group owner";
    }

    if (global_tracker.users.find(user_id) == global_tracker.users.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: User not found";
    }

    // Find and remove from pending
    auto it = find(group.pending_requests.begin(), group.pending_requests.end(), user_id);
    if (it == group.pending_requests.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: No pending request from user";
    }

    group.pending_requests.erase(it);
    group.members.push_back(user_id);
    global_tracker.users[user_id].groups.push_back(group_id);

    log_message("User %s accepted to group %s\n", user_id.c_str(), group_id.c_str());

    pthread_mutex_unlock(&tracker_lock);
    return "SUCCESS: User accepted to group";
}

// 7. LIST PENDING REQUESTS
string handle_list_requests(string owner_id, string group_id)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.groups.find(group_id) == global_tracker.groups.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Group not found";
    }

    Group &group = global_tracker.groups[group_id];
    if (group.owner_id != owner_id)
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Not group owner";
    }

    string result;
    for (const string &req : group.pending_requests)
    {
        result += req + " ";
    }

    pthread_mutex_unlock(&tracker_lock);
    return result.empty() ? "No pending requests" : "Pending requests for " + group_id + ": " + result;
}

// 8. LIST ALL GROUPS
string handle_list_groups()
{
    pthread_mutex_lock(&tracker_lock);

    string result;
    for (auto &group_pair : global_tracker.groups)
    {
        result += group_pair.first + " ";
    }

    pthread_mutex_unlock(&tracker_lock);
    return result.empty() ? "No groups available" : "Available groups: " + result;
}

// 9. LEAVE GROUP
string handle_leave_group(string user_id, string group_id)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.groups.find(group_id) == global_tracker.groups.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Group not found";
    }

    Group &group = global_tracker.groups[group_id];

    if (group.owner_id == user_id)
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Group owner cannot leave the group";
    }

    auto it = find(group.members.begin(), group.members.end(), user_id);
    if (it == group.members.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Not member of group";
    }

    group.members.erase(it);

    // Remove from user's groups
    auto uit = find(global_tracker.users[user_id].groups.begin(),
                    global_tracker.users[user_id].groups.end(), group_id);
    if (uit != global_tracker.users[user_id].groups.end())
    {
        global_tracker.users[user_id].groups.erase(uit);
    }

    log_message("User %s left group %s\n", user_id.c_str(), group_id.c_str());

    pthread_mutex_unlock(&tracker_lock);
    return "SUCCESS: Left group " + group_id;
}

// 10. REGISTER FILE (Upload)
string handle_register_file(string user_id, string group_id, string filename,
                            string file_hash, vector<string> piece_hashes, long file_size)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.users.find(user_id) == global_tracker.users.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: User not found";
    }

    if (global_tracker.groups.find(group_id) == global_tracker.groups.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Group not found";
    }

    Group &group = global_tracker.groups[group_id];
    if (!is_member(group, user_id))
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Not member of group";
    }

    // Update file registry
    File file_info;
    file_info.filename = filename;
    file_info.file_hash = file_hash;
    file_info.piece_hashes = piece_hashes;
    file_info.piece_count = piece_hashes.size();
    file_info.file_size = file_size;

    global_tracker.file_registry[filename] = file_info;

    // Add to group's file list
    vector<string> &peers = group.files[filename];
    if (find(peers.begin(), peers.end(), user_id) == peers.end())
    {
        peers.push_back(user_id);
    }

    // Add to user's shared files
    global_tracker.users[user_id].shared_files[group_id].push_back(file_info);

    log_message("File registered: %s by %s in group %s\n", filename.c_str(), user_id.c_str(), group_id.c_str());

    pthread_mutex_unlock(&tracker_lock);
    return "SUCCESS: File registered";
}

string handle_stop_share(string user_id, string group_id, string filename)
{
    pthread_mutex_lock(&tracker_lock);

    auto group_it = global_tracker.groups.find(group_id);
    if (group_it == global_tracker.groups.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Group not found";
    }

    Group &group = group_it->second;
    if (!is_member(group, user_id))
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Not member of group";
    }

    auto file_it = group.files.find(filename);
    if (file_it == group.files.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: File not shared in group";
    }

    vector<string> &peers = file_it->second;
    peers.erase(remove(peers.begin(), peers.end(), user_id), peers.end());
    if (peers.empty())
    {
        group.files.erase(file_it);
    }

    auto &shared_files = global_tracker.users[user_id].shared_files[group_id];
    shared_files.erase(remove_if(shared_files.begin(), shared_files.end(),
                                 [&](const File &file) { return file.filename == filename; }),
                        shared_files.end());

    pthread_mutex_unlock(&tracker_lock);
    return "SUCCESS: Stopped sharing " + filename;
}

// 11. LIST FILES IN GROUP
string handle_list_files(string user_id, string group_id)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.groups.find(group_id) == global_tracker.groups.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Group not found";
    }

    Group &group = global_tracker.groups[group_id];
    if (!is_member(group, user_id))
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Not member of group";
    }

    string result;
    for (auto &file_pair : group.files)
    {
        result += file_pair.first + " ";
    }

    pthread_mutex_unlock(&tracker_lock);
    return result.empty() ? "No files in group" : "Files in " + group_id + ": " + result;
}

// 12. GET PEER LIST (For downloading)
string handle_get_peers(string user_id, string group_id, string filename)
{
    pthread_mutex_lock(&tracker_lock);

    if (global_tracker.groups.find(group_id) == global_tracker.groups.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Group not found";
    }

    Group &group = global_tracker.groups[group_id];

    if (!is_member(group, user_id))
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: Not member of group";
    }

    if (group.files.find(filename) == group.files.end())
    {
        pthread_mutex_unlock(&tracker_lock);
        return "ERROR: File not found in group";
    }

    string result;
    for (const string &peer : group.files[filename])
    {
        if (global_tracker.users.find(peer) != global_tracker.users.end() &&
            global_tracker.users[peer].is_online)
        {
            result += peer + " ";
        }
    }

    pthread_mutex_unlock(&tracker_lock);
    return result.empty() ? "No peers available" : "Peers for " + filename + ": " + result;
}

// ===================== COMMAND PARSER =====================

string parse_command(string user_id, string command)
{
    vector<string> tokens = split_string(command, ' ');

    if (tokens.empty())
    {
        return "ERROR: Empty command";
    }

    string cmd = tokens[0];

    // Commands don't require user login
    if (cmd == "create_user" && tokens.size() == 3)
    {
        return handle_create_user(tokens[1], tokens[2]);
    }
    if (cmd == "login" && tokens.size() == 3)
    {
        return handle_login(tokens[1], tokens[2]);
    }

    if (user_id.empty() || global_tracker.users.find(user_id) == global_tracker.users.end() ||
        !global_tracker.users[user_id].is_online)
    {
        return "ERROR: Must login first";
    }

    // Commands require user ID
    if (cmd == "logout" && tokens.size() == 1)
    {
        return handle_logout(user_id);
    }
    if (cmd == "create_group" && tokens.size() == 2)
    {
        return handle_create_group(user_id, tokens[1]);
    }
    if (cmd == "join_group" && tokens.size() == 2)
    {
        return handle_join_group(user_id, tokens[1]);
    }
    if (cmd == "accept_request" && tokens.size() == 3)
    {
        return handle_accept_request(user_id, tokens[1], tokens[2]);
    }
    if (cmd == "list_requests" && tokens.size() == 2)
    {
        return handle_list_requests(user_id, tokens[1]);
    }
    if (cmd == "list_groups" && tokens.size() == 1)
    {
        return handle_list_groups();
    }
    if (cmd == "leave_group" && tokens.size() == 2)
    {
        return handle_leave_group(user_id, tokens[1]);
    }
    if (cmd == "list_files" && tokens.size() == 2)
    {
        return handle_list_files(user_id, tokens[1]);
    }
    if (cmd == "stop_share" && tokens.size() == 3)
    {
        return handle_stop_share(user_id, tokens[1], tokens[2]);
    }
    if (cmd == "get_peers" && tokens.size() == 3)
    {
        return handle_get_peers(user_id, tokens[1], tokens[2]);
    }

    if (cmd == "register_file" && tokens.size() >= 6)
    {
        vector<string> piece_hashes(tokens.begin() + 6, tokens.end());
        int piece_count = atoi(tokens[5].c_str());
        if (piece_count < 0 || static_cast<size_t>(piece_count) != piece_hashes.size())
        {
            return "ERROR: Invalid piece metadata";
        }
        return handle_register_file(user_id, tokens[1], tokens[2], tokens[3], piece_hashes,
                                    atol(tokens[4].c_str()));
    }

    return "ERROR: Unknown command";
}

// ===================== SOCKET HANDLERS =====================

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[MAX_BUFFER];
    string user_id = "";

    log_message("Client connected\n");

    while (1)
    {
        memset(buffer, 0, MAX_BUFFER);
        int bytes = read(client_socket, buffer, MAX_BUFFER - 1);

        if (bytes <= 0)
        {
            log_message("Client disconnected\n");
            break;
        }

        string command(buffer);
        log_message("Received: %s\n", command.c_str());

        // Extract user_id if login command
        vector<string> tokens = split_string(command, ' ');
        if (!tokens.empty() && tokens[0] == "login" && tokens.size() >= 2)
        {
            user_id = tokens[1];
        }

        string response = parse_command(user_id, command);
        send(client_socket, response.c_str(), response.length(), 0);

        if (!tokens.empty() && tokens[0] == "logout" && response.find("SUCCESS") == 0)
        {
            user_id.clear();
        }
    }

    close(client_socket);
    return NULL;
}

// ===================== SOCKET SERVER =====================

void start_tracker_server(int tracker_no)
{
    global_tracker.tracker_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (global_tracker.tracker_socket == -1)
    {
        error_exit("Socket creation failed");
    }

    global_tracker.address.sin_family = AF_INET;
    global_tracker.address.sin_addr.s_addr = htonl(INADDR_ANY);
    global_tracker.address.sin_port = htons(TRACKER_PORT + tracker_no);

    if (bind(global_tracker.tracker_socket, (SA *)&global_tracker.address, sizeof(global_tracker.address)) < 0)
    {
        error_exit("Bind failed");
    }

    if (listen(global_tracker.tracker_socket, 10) < 0)
    {
        error_exit("Listen failed");
    }

    log_message("Tracker %d listening on port %d\n", tracker_no, TRACKER_PORT + tracker_no);
}

// ===================== MAIN =====================

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <tracker_info_file> <tracker_number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int tracker_no = atoi(argv[2]);

    start_tracker_server(tracker_no);

    log_message("Tracker initialized. Waiting for connections...\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_socket = accept(global_tracker.tracker_socket,
                                   (SA *)&client_addr,
                                   &client_addr_len);

        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }

        pthread_t thread_id;
        int *sock_ptr = (int *)malloc(sizeof(int));
        *sock_ptr = client_socket;

        if (pthread_create(&thread_id, NULL, handle_client, sock_ptr) != 0)
        {
            perror("pthread_create failed");
            free(sock_ptr);
            close(client_socket);
        }
    }

    close(global_tracker.tracker_socket);
    return 0;
}
