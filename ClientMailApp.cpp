#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int PORT = 8080;
const int BUF_SIZE = 8192;

SOCKET sock;
bool running = true;

// ─── Receive messages from server ────────────────────────────────────────────
void receive_loop() {
    char buffer[BUF_SIZE];
    while (running) {
        int bytes = recv(sock, buffer, BUF_SIZE - 1, 0);
        if (bytes <= 0) {
            cout << "\n[Connection lost]\n";
            running = false;
            break;
        }
        buffer[bytes] = '\0';
        // Print on a new line so it doesn't interrupt user input
        cout << "\r" << string(buffer, bytes) << "\n> " << flush;
    }
}

// ─── Send a file to the server ────────────────────────────────────────────────
void send_file(const string& local_path) {
    ifstream file(local_path, ios::binary | ios::ate);
    if (!file.is_open()) {
        cout << "Error: cannot open file " << local_path << "\n";
        return;
    }

    long long file_size = file.tellg();
    file.seekg(0);

    string filename = filesystem::path(local_path).filename().string();
    // Header: send_file <name> <size>\n
    string header = "send_file " + filename + " " + to_string(file_size) + "\n";
    send(sock, header.c_str(), (int)header.size(), 0);

    char buffer[BUF_SIZE];
    while (file) {
        file.read(buffer, BUF_SIZE);
        int bytes = (int)file.gcount();
        if (bytes > 0)
            send(sock, buffer, bytes, 0);
    }
    file.close();
    cout << "[File '" << filename << "' sent]\n";
}

// ─── Entry point ──────────────────────────────────────────────────────────────
int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Winsock initialization error!\n";
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cout << "Socket creation error: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    string server_ip;
    cout << "Enter server IP (Enter = 127.0.0.1): ";
    getline(cin, server_ip);
    if (server_ip.empty()) server_ip = "127.0.0.1";

    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        cout << "Error: invalid IP format!\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "Connecting to " << server_ip << ":" << PORT << " ...\n";

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        cout << "Connection error! Code: " << err << "\n";
        if (err == 10061) cout << "  Server is not running or port 8080 is closed\n";
        else if (err == 10060) cout << "  Timeout. Check firewall and IP address\n";
        else if (err == 10049) cout << "  Invalid address\n";
        closesocket(sock);
        WSACleanup();
        system("pause");
        return 1;
    }

    cout << "Connected successfully!\n";

    // Ask for username
    string username;
    cout << "Enter your username: ";
    getline(cin, username);
    if (username.empty()) username = "Anonymous";

    // Send username to server
    string name_cmd = "set_name " + username;
    send(sock, name_cmd.c_str(), (int)name_cmd.size(), 0);

    cout << "\n--- Chat started. Commands: ---\n";
    cout << "  <text>               - send a message\n";
    cout << "  send_file <path>     - send a file to the server\n";
    cout << "  count_spaces <file>  - count spaces in a server-side file\n";
    cout << "  get_file <file>      - download a file from the server\n";
    cout << "  exit                 - quit\n";
    cout << "-------------------------------\n\n";

    // Start receiving messages from server
    thread(receive_loop).detach();

    string line;
    while (running) {
        cout << "> " << flush;
        if (!getline(cin, line)) break;
        if (!running) break;
        if (line.empty()) continue;

        if (line == "exit") {
            break;
        }
        else if (line.substr(0, 10) == "send_file ") {
            send_file(line.substr(10));
        }
        else if (line.substr(0, 13) == "count_spaces " ||
            line.substr(0, 9) == "get_file ") {
            // Service commands — send directly
            send(sock, line.c_str(), (int)line.size(), 0);
        }
        else {
            // Regular message — add msg prefix
            string msg = "msg " + line;
            send(sock, msg.c_str(), (int)msg.size(), 0);
            // Local echo: show own message immediately
            cout << "\r[You]: " << line << "\n";
        }
    }

    running = false;
    closesocket(sock);
    WSACleanup();
    return 0;
}