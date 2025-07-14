#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<bool> running(true);

void signalHandler(int signum) {
    running = false;
}

void collectLoop() {
    while (running) {
        // 실제 디바이스 데이터 수집 로직 (여기선 모의)
        std::cout << "[Collector] Data collected at " << std::time(nullptr) << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::cout << "[Collector] Service started." << std::endl;
    std::thread t(collectLoop);
    t.join();
    std::cout << "[Collector] Service stopped." << std::endl;
    return 0;
}
