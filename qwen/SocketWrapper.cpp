#include "SocketWrapper.h"
#include <iostream>

namespace net {

#ifdef _WIN32
bool SocketWrapper::winsockInitialized = false;
#endif

SocketWrapper::SocketWrapper() : socket_(INVALID_SOCKET) {
#ifdef _WIN32
    if (!winsockInitialized) {
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (res != 0) throw std::runtime_error("WSAStartup failed: " + std::to_string(res));
        winsockInitialized = true;
    }
#endif
}

SocketWrapper::~SocketWrapper() {
    close();
    stopReceiving();
}

SocketWrapper::SocketWrapper(SocketWrapper&& other) noexcept
    : socket_(other.socket_),
      recvThread_(std::move(other.recvThread_)),
      dataCallback_(std::move(other.dataCallback_)),
      running_(other.running_.load()),
      receiving_(other.receiving_.load()) {
    // Мьютексы создаются как новые — безопасно.
    other.socket_ = INVALID_SOCKET;
    other.running_ = false;
    other.receiving_ = false;
}

SocketWrapper& SocketWrapper::operator=(SocketWrapper&& other) noexcept {
    if (this != &other) {
        close();
        stopReceiving();

        socket_ = other.socket_;
        recvThread_ = std::move(other.recvThread_);
        dataCallback_ = std::move(other.dataCallback_);

        running_ = other.running_.load();
        receiving_ = other.receiving_.load();

        // sendMutex_, recvMutex_ остаются "своими" — не перемещаются

        other.socket_ = INVALID_SOCKET;
        other.running_ = false;
        other.receiving_ = false;
    }
    return *this;
}

void SocketWrapper::close() {
    if (socket_ != INVALID_SOCKET) {
#ifdef _WIN32
        shutdown(socket_, SD_BOTH);
#else
        shutdown(socket_, SHUT_RDWR);
#endif
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
}

bool SocketWrapper::bind(uint16_t port, const std::string& host) {
    if (isValid()) close();

    socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET) return false;

    int opt = 1;
#ifdef _WIN32
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));
#else
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (::bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close();
        return false;
    }
    return true;
}

bool SocketWrapper::listen(int backlog) {
    if (!isValid()) return false;
    return ::listen(socket_, backlog) == 0;
}

std::unique_ptr<SocketWrapper> SocketWrapper::accept() {
    if (!isValid()) return nullptr;

    struct sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    socket_t clientSock = ::accept(socket_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);

    if (clientSock == INVALID_SOCKET) return nullptr;

    auto client = std::make_unique<SocketWrapper>();
    client->socket_ = clientSock;
    return client;
}

bool SocketWrapper::connect(const std::string& host, uint16_t port) {
    if (isValid()) close();

    socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET) return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    int res = ::connect(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (res != 0) {
        close();
        return false;
    }
    return true;
}

bool SocketWrapper::send(const void* data, size_t len, int flags) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    if (!isValid()) return false;

    ssize_t sent = ::send(socket_, static_cast<const char*>(data), len, flags);
    if (sent < 0 || static_cast<size_t>(sent) != len) return false;
    return true;
}

void SocketWrapper::receiveLoop() {
    constexpr size_t BUFFER_SIZE = 4096;
    std::vector<uint8_t> buffer(BUFFER_SIZE);

    while (receiving_ && isValid()) {
        ssize_t received = ::recv(socket_, buffer.data(), buffer.size(), 0);
        if (received <= 0) break;

        buffer.resize(received);
        {
            std::lock_guard<std::mutex> lock(recvMutex_);
            if (dataCallback_) dataCallback_(buffer);
        }
        buffer.resize(BUFFER_SIZE);
    }

    receiving_ = false;
}

void SocketWrapper::startReceiving(DataCallback callback) {
    if (!isValid()) return;

    std::lock_guard<std::mutex> lock(recvMutex_);
    dataCallback_ = std::move(callback);
    receiving_ = true;
    running_ = true;

    recvThread_ = std::thread(&SocketWrapper::receiveLoop, this);
}

void SocketWrapper::stopReceiving() {
    receiving_ = false;
    if (recvThread_.joinable()) recvThread_.join();
}

bool SocketWrapper::isConnected() const {
    return isValid();
}

std::string SocketWrapper::ipToString(const struct sockaddr_in& addr) {
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, str, INET_ADDRSTRLEN);
    return std::string(str);
}

} // namespace net
