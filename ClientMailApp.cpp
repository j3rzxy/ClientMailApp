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

// Поток для приёма сообщений от сервера
void receive_loop() {
    char buffer[BUF_SIZE];
    while (true) {
        int bytes = recv(sock, buffer, BUF_SIZE, 0);
        if (bytes <= 0) {
            cout << "\n[Connection lost]" << endl;
            break;
        }
        cout << string(buffer, bytes) << flush;
    }
}

void send_file(const string& local_path) {
    ifstream file(local_path, ios::binary | ios::ate);
    if (!file.is_open()) {
        cout << "Error: can't open file " << local_path << endl;
        return;
    }

    long long file_size = file.tellg();
    file.seekg(0);

    string filename = filesystem::path(local_path).filename().string();
    string header = "send_file " + filename + " " + to_string(file_size) + "\n";

    send(sock, header.c_str(), header.size(), 0);

    char buffer[BUF_SIZE];
    while (file) {
        file.read(buffer, BUF_SIZE);
        int bytes = file.gcount();
        if (bytes > 0) {
            send(sock, buffer, bytes, 0);
        }
    }
    file.close();
    cout << "File " << filename << " was sent to server." << endl;
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Inicialization error Winsock!" << endl;
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cout << "Paket creation error socket!" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    string server_ip;
    cout << "Enter IP (example 127.0.0.1): ";
    getline(cin, server_ip);

    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    cout << "Connect to server " << server_ip << ":" << PORT << "..." << endl;

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cout << "Connection lost!" << endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "Connection complete!" << endl;
    cout << "Commands:" << endl;
    cout << "  msg <text>                  - send message all" << endl;
    cout << "  broadcast <text>            - send all" << endl;
    cout << "  send_file <file_path>     - send file to server" << endl;
    cout << "  get_file <file_name>         - download file from server" << endl;
    cout << "  count_spaces <file_name>     - count spaces" << endl;
    cout << "  list_files                   - file list" << endl;
    cout << "  history                      - show chat story" << endl;
    cout << "  exit                         - exit" << endl << endl;

    // Запускаем поток приёма сообщений
    thread(receive_loop).detach();

    string line;
    while (getline(cin, line)) {
        if (line.empty()) continue;

        if (line == "exit") {
            break;
        }

        // Специальная обработка отправки файла
        if (line.find("send_file ") == 0) {
            string path = line.substr(10);
            send_file(path);
            continue;
        }

        // Отправляем команду на сервер
        send(sock, line.c_str(), line.size(), 0);
    }

    cout << "Disconnection..." << endl;
    closesocket(sock);
    WSACleanup();
    return 0;
}