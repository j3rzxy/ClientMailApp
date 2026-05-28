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

// ─── Поток приёма сообщений от сервера ───────────────────────────────────────
void receive_loop() {
    char buffer[BUF_SIZE];
    while (running) {
        int bytes = recv(sock, buffer, BUF_SIZE - 1, 0);
        if (bytes <= 0) {
            cout << "\n[Соединение разорвано]\n";
            running = false;
            break;
        }
        buffer[bytes] = '\0';
        // Печатаем сообщение с новой строки, чтобы не сбивать ввод
        cout << "\r" << string(buffer, bytes) << "\n> " << flush;
    }
}

// ─── Отправка файла ───────────────────────────────────────────────────────────
void send_file(const string& local_path) {
    ifstream file(local_path, ios::binary | ios::ate);
    if (!file.is_open()) {
        cout << "Ошибка: не удаётся открыть файл " << local_path << "\n";
        return;
    }

    long long file_size = file.tellg();
    file.seekg(0);

    string filename = filesystem::path(local_path).filename().string();
    // Заголовок: send_file <имя> <размер>\n
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
    cout << "[Файл '" << filename << "' отправлен]\n";
}

// ─── Главная функция ──────────────────────────────────────────────────────────
int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Ошибка инициализации Winsock!\n";
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cout << "Ошибка создания сокета: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    string server_ip;
    cout << "Введите IP сервера (Enter = 127.0.0.1): ";
    getline(cin, server_ip);
    if (server_ip.empty()) server_ip = "127.0.0.1";

    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        cout << "Ошибка: неверный формат IP!\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "Подключение к " << server_ip << ":" << PORT << " ...\n";

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        cout << "Ошибка подключения! Код: " << err << "\n";
        if (err == 10061) cout << "  Сервер не запущен или порт 8080 закрыт\n";
        else if (err == 10060) cout << "  Таймаут. Проверьте firewall и IP\n";
        else if (err == 10049) cout << "  Неверный адрес\n";
        closesocket(sock);
        WSACleanup();
        system("pause");
        return 1;
    }

    cout << "✅ Успешное подключение!\n";

    // Запрос имени пользователя
    string username;
    cout << "Введите ваш ник: ";
    getline(cin, username);
    if (username.empty()) username = "Аноним";

    // Сообщаем серверу своё имя
    string name_cmd = "set_name " + username;
    send(sock, name_cmd.c_str(), (int)name_cmd.size(), 0);

    cout << "\n--- Чат запущен. Команды: ---\n";
    cout << "  <текст>              — отправить сообщение\n";
    cout << "  send_file <путь>     — отправить файл\n";
    cout << "  count_spaces <файл>  — посчитать пробелы в файле на сервере\n";
    cout << "  get_file <файл>      — скачать файл с сервера\n";
    cout << "  exit                 — выйти\n";
    cout << "-----------------------------\n\n";

    // Запускаем поток чтения сообщений от сервера
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
            // Служебные команды — отправляем напрямую
            send(sock, line.c_str(), (int)line.size(), 0);
        }
        else {
            // Обычное сообщение — добавляем префикс msg
            string msg = "msg " + line;
            send(sock, msg.c_str(), (int)msg.size(), 0);
            // Локальное эхо: показываем своё сообщение сразу
            cout << "\r[Вы]: " << line << "\n";
        }
    }

    running = false;
    closesocket(sock);
    WSACleanup();
    return 0;
}