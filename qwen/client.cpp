#include "SocketWrapper.h"
#include <iostream>

int main() {
    using namespace net;

    SocketWrapper client;
    if (!client.connect("127.0.0.1", 8080)) {
        std::cerr << "Connect failed\n";
        return 1;
    }

    client.startReceiving([](const std::vector<uint8_t>& data) {
        std::string msg(data.begin(), data.end());
        std::cout << "Received from server: " << msg << "\n";
    });

    // Отправка
    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        client.send("Hello, server! #" + std::to_string(i+1));
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}
