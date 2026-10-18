#pragma once

#include <stdint.h>

#include <atomic>
#include <string>
#include <thread>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <signal.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <unistd.h>
#endif

#ifdef _WIN32
typedef SOCKET http_server_socket;
#  define HTTP_SERVER_INVALID_SOCKET INVALID_SOCKET
#  define HTTP_SERVER_SHUT_WR        SD_SEND
#else
typedef int http_server_socket;
#  define HTTP_SERVER_INVALID_SOCKET (-1)
#  define HTTP_SERVER_SHUT_WR        SHUT_WR
#endif

static inline void http_server_close(http_server_socket sfd) {
#ifdef _WIN32
    closesocket(sfd);
#else
    close(sfd);
#endif
}

static inline struct sockaddr_in http_server_loopback(uint16_t port) {
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    return addr;
}

static inline std::string http_server_url(uint16_t port, const char *path) {
    return "http://127.0.0.1:" + std::to_string(port) + path;
}

// Build an HTTP response with the given status line, headers, and body
static inline std::string http_response(const char *status, const std::string &headers, const std::string &body) {
    return std::string("HTTP/1.0 ") + status + "\r\n" + headers + "\r\n" + body;
}

// Get a url on a local port that refuses connections
static inline std::string http_refused_url(const char *path) {
    struct sockaddr_in addr = http_server_loopback(0);
    socklen_t addr_len = sizeof(addr);
    http_server_socket sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bind(sfd, (struct sockaddr *)&addr, addr_len);
    getsockname(sfd, (struct sockaddr *)&addr, &addr_len);
    http_server_close(sfd);
    return http_server_url(ntohs(addr.sin_port), path);
}

// Local HTTP server that answers every request with the same response
class http_server {
   public:
    explicit http_server(const std::string &response) : response(response) {
#ifndef _WIN32
        // Writing to a connection the client already closed must not end the process
        signal(SIGPIPE, SIG_IGN);
#endif
        struct sockaddr_in addr = http_server_loopback(0);
        socklen_t addr_len = sizeof(addr);
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (bind(listener, (struct sockaddr *)&addr, addr_len) != 0 || listen(listener, 8) != 0 ||
            getsockname(listener, (struct sockaddr *)&addr, &addr_len) != 0)
            return;
        port = ntohs(addr.sin_port);
        thread = std::thread([this] { serve(); });
    }

    ~http_server() {
        if (thread.joinable()) {
            // Wake the accept call with a connection of our own
            stopping = true;
            struct sockaddr_in addr = http_server_loopback(port);
            http_server_socket sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            connect(sfd, (struct sockaddr *)&addr, sizeof(addr));
            http_server_close(sfd);
            thread.join();
        }
        http_server_close(listener);
    }

    std::string url(const char *path = "/wpad.dat") const {
        return http_server_url(port, path);
    }

   private:
    void serve() {
        while (true) {
            http_server_socket client = accept(listener, NULL, NULL);
            if (client == HTTP_SERVER_INVALID_SOCKET)
                return;
            if (stopping) {
                http_server_close(client);
                return;
            }

            // Give up on clients that stop sending
#ifdef _WIN32
            DWORD timeout = 5000;
#else
            struct timeval timeout = {5, 0};
#endif
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));

            // Read the request headers before answering
            std::string request;
            char buffer[1024];
            while (request.find("\r\n\r\n") == std::string::npos) {
                int count = (int)recv(client, buffer, sizeof(buffer), 0);
                if (count <= 0)
                    break;
                request.append(buffer, count);
            }
            send(client, response.c_str(), (int)response.size(), 0);

            // Drain before closing so unread data does not reset the connection
            shutdown(client, HTTP_SERVER_SHUT_WR);
            while (recv(client, buffer, sizeof(buffer), 0) > 0) {
            }
            http_server_close(client);
        }
    }

    std::string response;
    http_server_socket listener = HTTP_SERVER_INVALID_SOCKET;
    uint16_t port = 0;
    std::atomic<bool> stopping{false};
    std::thread thread;
};
