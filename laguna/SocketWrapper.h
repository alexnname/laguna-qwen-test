// SocketWrapper.h
#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <queue>
#include <condition_variable>
#include <cstring>
#include <stdexcept>

class SocketWrapper {
public:
    using Callback = std::function<void(const char*, size_t)>;

    SocketWrapper() : sockfd_(-1), port_(0), running_(false) {}
    
    ~SocketWrapper() {
        stop();
        closeSocket();
    }

    // Запрет копирования
    SocketWrapper(const SocketWrapper&) = delete;
    SocketWrapper& operator=(const SocketWrapper&) = delete;

    // Создание TCP сервера
    void createServer(int port, int backlog = 5) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        int opt = 1;
        setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            closeSocket();
            throw std::runtime_error("Failed to bind socket");
        }

        if (listen(sockfd_, backlog) < 0) {
            closeSocket();
            throw std::runtime_error("Failed to listen on socket");
        }

        port_ = port;
        running_ = true;
    }

    // Подключение к серверу
    void connectToServer(const std::string& ip, int port) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        if (::connect(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            closeSocket();
            throw std::runtime_error("Failed to connect to server");
        }

        port_ = port;
        running_ = true;
    }

    // Прием соединений (только для сервера)
    void startAccepting(std::function<void(int)> clientHandler) {
        if (sockfd_ < 0) return;
        
        acceptThread_ = std::thread([this, clientHandler]() {
            while (running_) {
                struct sockaddr_in clientAddr{};
                socklen_t clientLen = sizeof(clientAddr);
                
                int clientSock = accept(sockfd_, (struct sockaddr*)&clientAddr, &clientLen);
                if (clientSock >= 0) {
                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    clientSockets_.push_back(clientSock);
                    clientHandler(clientSock);
                }
            }
        });
    }

    // Прием данных
    void startReceiving(Callback callback) {
        receiveThread_ = std::thread([this, callback]() {
            char buffer[4096];
            while (running_) {
                std::lock_guard<std::mutex> lock(mutex_);
                ssize_t bytes = recv(sockfd_, buffer, sizeof(buffer), 0);
                if (bytes > 0) {
                    callback(buffer, static_cast<size_t>(bytes));
                } else if (bytes == 0) {
                    // Соединение закрыто
                    break;
                }
            }
        });
    }

    // Отправка данных
    void send(const char* data, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sockfd_ >= 0) {
            ::send(sockfd_, data, size, 0);
        }
    }

    // Отправка данным клиенту
    void sendToClient(int clientSock, const char* data, size_t size) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        ::send(clientSock, data, size, 0);
    }

    // Остановка
    void stop() {
        running_ = false;
        if (acceptThread_.joinable()) acceptThread_.join();
        if (receiveThread_.joinable()) receiveThread_.join();
    }

    // Закрытие сокета
    void closeSocket() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sockfd_ >= 0) {
            close(sockfd_);
            sockfd_ = -1;
        }
        
        std::lock_guard<std::mutex> clientsLock(clientsMutex_);
        for (int client : clientSockets_) {
            close(client);
        }
        clientSockets_.clear();
    }

    bool isConnected() const {
        return sockfd_ >= 0 && running_;
    }

private:
    int sockfd_;
    int port_;
    std::atomic<bool> running_;
    
    mutable std::mutex mutex_;
    std::thread acceptThread_;
    std::thread receiveThread_;
    
    std::vector<int> clientSockets_;
    std::mutex clientsMutex_;
};
