#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>
#include <memory>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socket_t = SOCKET;
    constexpr socket_t INVALID_SOCKET = INVALID_SOCKET_VALUE;
    #define closesocket ::closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <cstring>
    using socket_t = int;
    constexpr socket_t INVALID_SOCKET = -1;
    #define closesocket ::close
#endif

namespace net {

class SocketWrapper {
public:
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;

    explicit SocketWrapper();
    ~SocketWrapper();

    // Запрещаем копирование, разрешаем перемещение (без мьютексов)
    SocketWrapper(const SocketWrapper&) = delete;
    SocketWrapper& operator=(const SocketWrapper&) = delete;
    SocketWrapper(SocketWrapper&& other) noexcept;
    SocketWrapper& operator=(SocketWrapper&& other) noexcept;

    // Сервер: привязка и ожидание подключений
    bool bind(uint16_t port, const std::string& host = "0.0.0.0");
    bool listen(int backlog = 10);
    std::unique_ptr<SocketWrapper> accept();

    // Клиент: подключение к серверу
    bool connect(const std::string& host, uint16_t port);

    // Отправка данных (потокобезопасна)
    bool send(const void* data, size_t len, int flags = 0);
    bool send(const std::vector<uint8_t>& data, int flags = 0) {
        return send(data.data(), data.size(), flags);
    }
    // Удобные перегрузки для C++17
    bool send(const char* str, size_t len, int flags = 0) {
        return send(static_cast<const void*>(str), len, flags);
    }
    bool send(const std::string& s, int flags = 0) {
        return send(s.c_str(), s.size(), flags);
    }

    // Начать асинхронный приём (запуск потока)
    void startReceiving(DataCallback callback);
    void stopReceiving(); // остановить приём и поток

    // Общие методы
    bool isValid() const { return socket_ != INVALID_SOCKET; }
    void close();
    bool isConnected() const;

    // Утилиты
    static std::string ipToString(const struct sockaddr_in& addr);

private:
    socket_t socket_;
    std::thread recvThread_;
    DataCallback dataCallback_;

    mutable std::mutex sendMutex_;   // mutable — можно использовать в const-методах
    std::mutex recvMutex_;           // для dataCallback_
    std::atomic<bool> running_{false};
    std::atomic<bool> receiving_{false};

    void receiveLoop();

#ifdef _WIN32
    static bool winsockInitialized;
#endif
};

} // namespace net
