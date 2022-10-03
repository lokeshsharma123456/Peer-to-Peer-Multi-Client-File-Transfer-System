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
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cstdarg>
#include <cstdint>

using namespace std;

#define MAX_BUFFER 4096
#define MAX_COMMAND 512
#define SA struct sockaddr

// ===================== DATA STRUCTURES =====================

struct DownloadStatus
{
    string group_id;
    string filename;
    bool is_downloading;
    bool is_complete;
    int progress_percent;
};

struct SharedFile
{
    string filename;
    string group_id;
    bool is_sharing;
};

struct Client
{
    string client_id;
    string ip_address;
    int port;
    int tracker_socket;
    string current_user_id;
    bool is_authenticated;
    vector<string> joined_groups;
    map<string, DownloadStatus> downloads;
    vector<SharedFile> shared_files;
};

// ===================== GLOBAL CLIENT =====================
Client global_client;
string tracker_host = "127.0.0.1";
int tracker_port = 5000;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

const size_t PIECE_SIZE = 512 * 1024;

// ===================== UTILITY FUNCTIONS =====================

void log_message(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("[CLIENT] ");
    vprintf(format, args);
    va_end(args);
}

void error_exit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

// Connect to tracker
int connect_to_tracker()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        error_exit("Socket creation failed");
    }

    struct sockaddr_in tracker_addr;
    tracker_addr.sin_family = AF_INET;
    tracker_addr.sin_port = htons(tracker_port);
    tracker_addr.sin_addr.s_addr = inet_addr(tracker_host.c_str());

    if (connect(sock, (SA *)&tracker_addr, sizeof(tracker_addr)) < 0)
    {
        perror("Connection to tracker failed");
        return -1;
    }

    log_message("Connected to tracker at %s:%d\n", tracker_host, tracker_port);
    return sock;
}

// Send command to tracker and get response
string send_command_to_tracker(const string &command)
{
    if (global_client.tracker_socket < 0)
    {
        global_client.tracker_socket = connect_to_tracker();
        if (global_client.tracker_socket < 0)
        {
            return "ERROR: Cannot connect to tracker";
        }
    }

    send(global_client.tracker_socket, command.c_str(), command.length(), 0);

    char buffer[MAX_BUFFER];
    memset(buffer, 0, MAX_BUFFER);
    int bytes = read(global_client.tracker_socket, buffer, MAX_BUFFER - 1);

    if (bytes <= 0)
    {
        close(global_client.tracker_socket);
        global_client.tracker_socket = -1;
        return "ERROR: Tracker connection lost";
    }

    return string(buffer);
}

string sha1_hex(const vector<unsigned char> &data)
{
    vector<unsigned char> message = data;
    uint64_t bit_length = static_cast<uint64_t>(message.size()) * 8;
    message.push_back(0x80);
    while ((message.size() % 64) != 56)
    {
        message.push_back(0);
    }
    for (int shift = 56; shift >= 0; shift -= 8)
    {
        message.push_back(static_cast<unsigned char>(bit_length >> shift));
    }

    uint32_t hash[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    for (size_t block = 0; block < message.size(); block += 64)
    {
        uint32_t words[80] = {};
        for (int index = 0; index < 16; ++index)
        {
            size_t offset = block + index * 4;
            words[index] = (static_cast<uint32_t>(message[offset]) << 24) |
                           (static_cast<uint32_t>(message[offset + 1]) << 16) |
                           (static_cast<uint32_t>(message[offset + 2]) << 8) |
                           static_cast<uint32_t>(message[offset + 3]);
        }
        for (int index = 16; index < 80; ++index)
        {
            uint32_t value = words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16];
            words[index] = (value << 1) | (value >> 31);
        }

        uint32_t a = hash[0], b = hash[1], c = hash[2], d = hash[3], e = hash[4];
        for (int index = 0; index < 80; ++index)
        {
            uint32_t function = index < 20 ? ((b & c) | (~b & d)) :
                                index < 40 ? (b ^ c ^ d) :
                                index < 60 ? ((b & c) | (b & d) | (c & d)) : (b ^ c ^ d);
            uint32_t constant = index < 20 ? 0x5a827999 : index < 40 ? 0x6ed9eba1 :
                                index < 60 ? 0x8f1bbcdc : 0xca62c1d6;
            uint32_t rotated = (a << 5) | (a >> 27);
            uint32_t next = rotated + function + e + constant + words[index];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = next;
        }
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
    }

    static const char hex_digits[] = "0123456789abcdef";
    string result;
    result.reserve(40);
    for (uint32_t value : hash)
    {
        for (int shift = 28; shift >= 0; shift -= 4)
        {
            result += hex_digits[(value >> shift) & 0x0f];
        }
    }
    return result;
}

bool calculate_file_hashes(const string &file_path, string &file_hash,
                           vector<string> &piece_hashes, long &file_size)
{
    ifstream input(file_path, ios::binary);
    if (!input)
    {
        return false;
    }

    vector<unsigned char> file_data((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
    file_size = static_cast<long>(file_data.size());
    file_hash = sha1_hex(file_data);
    if (file_hash.empty())
    {
        return false;
    }

    for (size_t offset = 0; offset < file_data.size(); offset += PIECE_SIZE)
    {
        size_t length = min(PIECE_SIZE, file_data.size() - offset);
        vector<unsigned char> piece(file_data.begin() + offset, file_data.begin() + offset + length);
        string piece_hash = sha1_hex(piece);
        if (piece_hash.empty())
        {
            return false;
        }
        piece_hashes.push_back(piece_hash);
    }
    return true;
}

string file_name_from_path(const string &file_path)
{
    size_t separator = file_path.find_last_of("/\\");
    return separator == string::npos ? file_path : file_path.substr(separator + 1);
}

// ===================== COMMAND HANDLERS =====================

// 1. CREATE USER
void handle_create_user(const string &user_id, const string &password)
{
    string command = "create_user " + user_id + " " + password;
    string response = send_command_to_tracker(command);
    cout << response << endl;
}

// 2. LOGIN
void handle_login(const string &user_id, const string &password)
{
    string command = "login " + user_id + " " + password;
    string response = send_command_to_tracker(command);

    if (response.find("SUCCESS") != string::npos)
    {
        global_client.current_user_id = user_id;
        global_client.is_authenticated = true;
        log_message("User %s authenticated\n", user_id.c_str());
    }

    cout << response << endl;
}

// 3. LOGOUT
void handle_logout()
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Not logged in" << endl;
        return;
    }

    string command = "logout";
    string response = send_command_to_tracker(command);

    global_client.is_authenticated = false;
    global_client.current_user_id = "";
    global_client.joined_groups.clear();
    global_client.shared_files.clear();

    cout << response << endl;
}

// 4. CREATE GROUP
void handle_create_group(const string &group_id)
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Must login first" << endl;
        return;
    }

    string command = "create_group " + group_id;
    string response = send_command_to_tracker(command);

    if (response.find("SUCCESS") != string::npos)
    {
        global_client.joined_groups.push_back(group_id);
    }

    cout << response << endl;
}

// 5. JOIN GROUP
void handle_join_group(const string &group_id)
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Must login first" << endl;
        return;
    }

    string command = "join_group " + group_id;
    string response = send_command_to_tracker(command);

    cout << response << endl;
}

// 6. LEAVE GROUP
void handle_leave_group(const string &group_id)
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Must login first" << endl;
        return;
    }

    string command = "leave_group " + group_id;
    string response = send_command_to_tracker(command);

    if (response.find("SUCCESS") != string::npos)
    {
        auto it = find(global_client.joined_groups.begin(),
                       global_client.joined_groups.end(), group_id);
        if (it != global_client.joined_groups.end())
        {
            global_client.joined_groups.erase(it);
        }
    }

    cout << response << endl;
}

// 7. LIST PENDING REQUESTS
void handle_list_requests(const string &group_id)
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Must login first" << endl;
        return;
    }

    string command = "list_requests " + group_id;
    string response = send_command_to_tracker(command);

    cout << response << endl;
}

// 8. ACCEPT JOIN REQUEST
void handle_accept_request(const string &group_id, const string &user_id)
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Must login first" << endl;
        return;
    }

    string command = "accept_request " + group_id + " " + user_id;
    string response = send_command_to_tracker(command);

    cout << response << endl;
}

// 9. LIST ALL GROUPS
void handle_list_groups()
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Must login first" << endl;
        return;
    }

    string command = "list_groups";
    string response = send_command_to_tracker(command);

    cout << response << endl;
}

// 10. LIST FILES IN GROUP
void handle_list_files(const string &group_id)
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Must login first" << endl;
        return;
    }

    string command = "list_files " + group_id;
    string response = send_command_to_tracker(command);

    cout << response << endl;
}

// 11. UPLOAD FILE (Register with tracker)
void handle_upload_file(const string &file_path, const string &group_id)
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Must login first" << endl;
        return;
    }

    cout << "[INFO] Uploading file: " << file_path << " to group: " << group_id << endl;

    string filename = file_name_from_path(file_path);
    string file_hash;
    vector<string> piece_hashes;
    long file_size = 0;
    if (filename.empty() || !calculate_file_hashes(file_path, file_hash, piece_hashes, file_size))
    {
        cout << "ERROR: Cannot read file or calculate SHA1" << endl;
        return;
    }

    string command = "register_file " + group_id + " " + filename + " " + file_hash + " " +
                     to_string(file_size) + " " + to_string(piece_hashes.size());
    for (const string &piece_hash : piece_hashes)
    {
        command += " " + piece_hash;
    }

    string response = send_command_to_tracker(command);
    cout << response << endl;
    if (response.find("SUCCESS") == string::npos)
    {
        return;
    }

    SharedFile sf;
    sf.filename = filename;
    sf.group_id = group_id;
    sf.is_sharing = true;
    global_client.shared_files.push_back(sf);
}

// 12. DOWNLOAD FILE
void handle_download_file(const string &group_id, const string &filename, const string &dest_path)
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Must login first" << endl;
        return;
    }

    string command = "get_peers " + group_id + " " + filename;
    string response = send_command_to_tracker(command);
    cout << response << endl;
    if (response.find("Peers for ") == 0)
    {
        cout << "ERROR: Peer transfer is not implemented yet; no file was written to "
             << dest_path << endl;
    }
}

// 13. SHOW DOWNLOADS
void handle_show_downloads()
{
    if (global_client.downloads.empty())
    {
        cout << "No downloads in progress" << endl;
        return;
    }

    for (auto &dl : global_client.downloads)
    {
        if (dl.second.is_complete)
        {
            cout << "[C] [" << dl.second.group_id << "] " << dl.second.filename << endl;
        }
        else
        {
            cout << "[D] [" << dl.second.group_id << "] " << dl.second.filename
                 << " (" << dl.second.progress_percent << "%)" << endl;
        }
    }
}

// 14. STOP SHARING FILE
void handle_stop_share(const string &group_id, const string &filename)
{
    if (!global_client.is_authenticated)
    {
        cout << "ERROR: Must login first" << endl;
        return;
    }

    auto it = find_if(global_client.shared_files.begin(),
                      global_client.shared_files.end(),
                      [&](const SharedFile &sf)
                      {
                          return sf.filename == filename && sf.group_id == group_id;
                      });

    if (it != global_client.shared_files.end())
    {
        string response = send_command_to_tracker("stop_share " + group_id + " " + filename);
        cout << response << endl;
        if (response.find("SUCCESS") == 0)
        {
            global_client.shared_files.erase(it);
        }
    }
    else
    {
        cout << "ERROR: File not found in shared files" << endl;
    }
}

// ===================== COMMAND PARSER =====================

void parse_and_execute_command(string command)
{
    // Remove trailing newline
    if (!command.empty() && command.back() == '\n')
    {
        command.pop_back();
    }

    // Skip empty commands
    if (command.empty())
    {
        return;
    }

    istringstream iss(command);
    vector<string> tokens;
    string token;

    while (iss >> token)
    {
        tokens.push_back(token);
    }

    if (tokens.empty())
    {
        return;
    }

    string cmd = tokens[0];

    if (cmd == "create_user" && tokens.size() == 3)
    {
        handle_create_user(tokens[1], tokens[2]);
    }
    else if (cmd == "login" && tokens.size() == 3)
    {
        handle_login(tokens[1], tokens[2]);
    }
    else if (cmd == "logout" && tokens.size() == 1)
    {
        handle_logout();
    }
    else if (cmd == "create_group" && tokens.size() == 2)
    {
        handle_create_group(tokens[1]);
    }
    else if (cmd == "join_group" && tokens.size() == 2)
    {
        handle_join_group(tokens[1]);
    }
    else if (cmd == "leave_group" && tokens.size() == 2)
    {
        handle_leave_group(tokens[1]);
    }
    else if (cmd == "list_requests" && tokens.size() == 2)
    {
        handle_list_requests(tokens[1]);
    }
    else if (cmd == "accept_request" && tokens.size() == 3)
    {
        handle_accept_request(tokens[1], tokens[2]);
    }
    else if (cmd == "list_groups" && tokens.size() == 1)
    {
        handle_list_groups();
    }
    else if (cmd == "list_files" && tokens.size() == 2)
    {
        handle_list_files(tokens[1]);
    }
    else if (cmd == "upload_file" && tokens.size() == 3)
    {
        handle_upload_file(tokens[1], tokens[2]);
    }
    else if (cmd == "download_file" && tokens.size() == 4)
    {
        handle_download_file(tokens[1], tokens[2], tokens[3]);
    }
    else if (cmd == "show_downloads" && tokens.size() == 1)
    {
        handle_show_downloads();
    }
    else if (cmd == "stop_share" && tokens.size() == 3)
    {
        handle_stop_share(tokens[1], tokens[2]);
    }
    else if (cmd == "help" && tokens.size() == 1)
    {
        cout << "\n========== AVAILABLE COMMANDS ==========\n"
             << "1. create_user <user_id> <password>\n"
             << "2. login <user_id> <password>\n"
             << "3. logout\n"
             << "4. create_group <group_id>\n"
             << "5. join_group <group_id>\n"
             << "6. leave_group <group_id>\n"
             << "7. list_requests <group_id>\n"
             << "8. accept_request <group_id> <user_id>\n"
             << "9. list_groups\n"
             << "10. list_files <group_id>\n"
             << "11. upload_file <file_path> <group_id>\n"
             << "12. download_file <group_id> <filename> <destination>\n"
             << "13. show_downloads\n"
             << "14. stop_share <group_id> <filename>\n"
             << "15. help\n"
             << "16. quit\n"
             << "========================================\n\n";
    }
    else if (cmd == "quit" || cmd == "exit")
    {
        if (global_client.is_authenticated)
        {
            handle_logout();
        }
        cout << "Goodbye!" << endl;
        exit(0);
    }
    else
    {
        cout << "ERROR: Unknown command '" << cmd << "'. Type 'help' for available commands." << endl;
    }
}

// ===================== INITIALIZATION =====================

void initialize_client(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: %s <IP>:<PORT> <tracker_info_file>\n", argv[0]);
        printf("Example: %s 127.0.0.1:18000 tracker_info.txt\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse IP:PORT
    string ip_port = argv[1];
    size_t colon_pos = ip_port.find(':');

    if (colon_pos == string::npos)
    {
        printf("ERROR: Invalid format. Use IP:PORT\n");
        exit(EXIT_FAILURE);
    }

    global_client.ip_address = ip_port.substr(0, colon_pos);
    global_client.port = atoi(ip_port.substr(colon_pos + 1).c_str());
    global_client.client_id = global_client.ip_address + ":" + to_string(global_client.port);

    // Read tracker info file
    ifstream tracker_file(argv[2]);
    if (!tracker_file.is_open())
    {
        printf("ERROR: Cannot open tracker_info.txt\n");
        exit(EXIT_FAILURE);
    }

    getline(tracker_file, tracker_host);
    tracker_file >> tracker_port;
    tracker_file.close();

    log_message("Client ID: %s\n", global_client.client_id.c_str());
    log_message("Tracker: %s:%d\n", tracker_host.c_str(), tracker_port);

    global_client.tracker_socket = -1;
    global_client.is_authenticated = false;
}

// ===================== MAIN =====================

int main(int argc, char *argv[])
{
    initialize_client(argc, argv);

    cout << "\n========== Peer-to-Peer File Sharing System - CLIENT ==========\n"
         << "Type 'help' for available commands\n"
         << "Type 'quit' to exit\n"
         << "==========================================================\n\n";

    string command;
    while (1)
    {
        if (global_client.is_authenticated)
        {
            cout << "[" << global_client.current_user_id << "] > ";
        }
        else
        {
            cout << "> ";
        }

        if (!getline(cin, command))
        {
            break;
        }

        parse_and_execute_command(command);
    }

    if (global_client.tracker_socket >= 0)
    {
        close(global_client.tracker_socket);
    }

    return 0;
}
