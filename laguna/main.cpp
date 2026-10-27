// main.cpp
#include "SocketWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>

void runServer() {
    try {
        SocketWrapper server;
        server.createServer(8080);
        std::cout << "Server started on port 8080\n";

        server.startAccepting([](int clientSock) {
            char buffer[1024];
            ssize_t bytes = recv(clientSock, buffer, sizeof(buffer), 0);
            if (bytes > 0) {
                std::string msg = "Echo: " + std::string(buffer, bytes);
                ::send(clientSock, msg.c_str(), msg.size(), 0);  // Используем глобальную send
            }
            close(clientSock);
        });

        std::cout << "Server running...\n";
        std::this_thread::sleep_for(std::chrono::seconds(30));
        server.stop();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }
}

void runClient() {
    try {
        SocketWrapper client;
        client.connectToServer("127.0.0.1", 8080);
        std::cout << "Connected to server\n";

        client.startReceiving([](const char* data, size_t size) {
            std::cout << "Received: " << std::string(data, size) << "\n";
        });

        std::string msg = "Hello from client!";
        client.send(msg.c_str(), msg.size());
        std::cout << "Sent: " << msg << "\n";

        std::this_thread::sleep_for(std::chrono::seconds(5));
        client.stop();
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << "\n";
    }
}

int main() {
    // Run server in background
    std::thread serverThread(runServer);
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for server to start
    
    // Run client
    runClient();
    
    serverThread.join();
    return 0;
}
