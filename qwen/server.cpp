#include "SocketWrapper.h"
#include <iostream>
#include <thread>
#include <memory>
#include <csignal>

std::atomic<bool> shouldStop{false};

void signalHandler(int) {
    shouldStop = true;
}

void handleClient(std::shared_ptr<net::SocketWrapper> client) {
    if (!client || !client->isValid()) return;

    std::cout << "[Server] Client connected!\n";

    // Лямбда копируется — поэтому используем shared_ptr
    client->startReceiving([client](const std::vector<uint8_t>& data) {
        std::string msg(data.begin(), data.end());
        std::cout << "[Server] Received from client: " << msg << "\n";
        if (client && client->isValid()) {
            client->send("Echo: " + msg + "\n");
        }
    });

    // Ждём, пока клиент отключится или пришёл сигнал остановки
    while (!shouldStop && client->isConnected()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "[Server] Client disconnected.\n";
}

int main() {
    std::signal(SIGINT, signalHandler);

    using namespace net;

    SocketWrapper server;
    if (!server.bind(8080)) {
        std::cerr << "[Server] Failed to bind on port 8080\n";
        return 1;
    }
    if (!server.listen()) {
        std::cerr << "[Server] Listen failed\n";
        return 1;
    }

    std::cout << "[Server] Listening on 0.0.0.0:8080...\n";

    while (!shouldStop) {
        auto rawClient = server.accept();
        if (!rawClient || shouldStop) break;

        // Преобразуем unique_ptr → shared_ptr
        auto clientPtr = std::shared_ptr<SocketWrapper>(std::move(rawClient));

        std::thread t(handleClient, clientPtr);
        t.detach();

        std::cout << "[Server] New client accepted.\n";
    }

    return 0;
}
