#include "minidb.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace minidb;

#ifdef _WIN32
using NativeSocket = SOCKET;
#else
using NativeSocket = int;
#endif

static NativeSocket native_socket(SocketHandle fd) {
    return static_cast<NativeSocket>(fd);
}

static void close_socket(SocketHandle fd) {
#ifdef _WIN32
    ::closesocket(native_socket(fd));
#else
    ::close(native_socket(fd));
#endif
}

int main(int argc, char** argv) {
#ifdef _WIN32
    WSADATA wsa {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "failed to initialize winsock\n";
        return 1;
    }
#endif
    const char* host = argc >= 2 ? argv[1] : "127.0.0.1";
    int port = argc >= 3 ? std::atoi(argv[2]) : 9090;

    SocketHandle fd = static_cast<SocketHandle>(::socket(AF_INET, SOCK_STREAM, 0));
#ifdef _WIN32
    if (static_cast<SOCKET>(fd) == INVALID_SOCKET) {
#else
    if (fd < 0) {
#endif
        std::cerr << "failed to create socket\n";
        return 1;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        std::cerr << "invalid server address\n";
        close_socket(fd);
        return 1;
    }
    if (::connect(native_socket(fd), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "failed to connect server\n";
        close_socket(fd);
        return 1;
    }

    std::cout << "Connected to mini relation DBMS. Type exit to quit.\n";
    std::string line;
    while (true) {
        std::cout << "mini-sql> ";
        if (!std::getline(std::cin, line)) break;
        line = trim(line);
        if (line.empty()) continue;
        if (!send_message(fd, line)) {
            std::cerr << "network send failed\n";
            break;
        }
        std::string payload;
        if (!recv_message(fd, payload)) {
            std::cerr << "network receive failed\n";
            break;
        }
        QueryResult result = decode_result(payload);
        std::cout << format_result_table(result);
        if (lower_copy(line) == "exit") break;
    }

    close_socket(fd);
    return 0;
}
