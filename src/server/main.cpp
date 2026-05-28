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
    int port = argc >= 2 ? std::atoi(argv[1]) : 9090;
    SocketHandle server_fd = static_cast<SocketHandle>(::socket(AF_INET, SOCK_STREAM, 0));
#ifdef _WIN32
    if (static_cast<SOCKET>(server_fd) == INVALID_SOCKET) {
#else
    if (server_fd < 0) {
#endif
        std::cerr << "failed to create socket\n";
        return 1;
    }

    int opt = 1;
    ::setsockopt(native_socket(server_fd), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(native_socket(server_fd), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "failed to bind port " << port << "\n";
        close_socket(server_fd);
        return 1;
    }
    if (::listen(native_socket(server_fd), 8) < 0) {
        std::cerr << "failed to listen\n";
        close_socket(server_fd);
        return 1;
    }

    std::cout << "mini_db_server listening on port " << port << "\n";
    DatabaseEngine engine("data");
    while (true) {
        SocketHandle client_fd = static_cast<SocketHandle>(::accept(native_socket(server_fd), nullptr, nullptr));
#ifdef _WIN32
        if (static_cast<SOCKET>(client_fd) == INVALID_SOCKET) continue;
#else
        if (client_fd < 0) continue;
#endif
        std::cout << "client connected\n";
        std::string sql;
        while (recv_message(client_fd, sql)) {
            QueryResult result = engine.execute_sql(sql);
            send_message(client_fd, encode_result(result));
            if (trim(lower_copy(sql)) == "exit") break;
        }
        close_socket(client_fd);
        std::cout << "client disconnected\n";
    }
}
