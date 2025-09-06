#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
#endif

namespace PulseOne {
    using UUID = std::string;
    
    // 크로스 플랫폼 sleep 함수
    void sleep_seconds(int seconds) {
        #ifdef _WIN32
            Sleep(seconds * 1000);  // Windows: milliseconds
        #else
            sleep(seconds);         // Unix: seconds
        #endif
    }
    
    class Collector {
    private:
        bool running_;
        std::vector<std::string> protocols_;
        
    public:
        Collector() : running_(false) {
            protocols_ = {"Modbus TCP", "MQTT", "BACnet/IP", "Redis"};
        }
        
        void start() {
            running_ = true;
            std::cout << "========================================\n";
            std::cout << "PulseOne Collector v2.1.0 - Windows\n";
            std::cout << "========================================\n\n";
            
            std::cout << "Enabled Protocols:\n";
            for (size_t i = 0; i < protocols_.size(); ++i) {
                std::cout << "  [OK] " << protocols_[i] << "\n";
            }
            std::cout << "\nCollector started successfully!\n";
            std::cout << "Press ESC to stop...\n\n";
            
            run();
        }
        
        void run() {
            int counter = 0;
            while (running_) {
                // Windows Sleep 사용
                sleep_seconds(2);
                counter++;
                
                std::cout << "[" << counter << "] Working... Status: OK\n";
                std::cout.flush();
                
                #ifdef _WIN32
                if (_kbhit()) {
                    char ch = _getch();
                    if (ch == 27) {  // ESC key
                        stop();
                    }
                }
                #endif
                
                // 10번 반복 후 자동 종료
                if (counter >= 10) {
                    std::cout << "\nDemo complete after 10 iterations\n";
                    stop();
                }
            }
        }
        
        void stop() {
            running_ = false;
            std::cout << "\nShutting down...\n";
        }
    };
}

int main(int argc, char* argv[]) {
    std::cout << "Starting PulseOne Collector...\n\n";
    
    try {
        PulseOne::Collector collector;
        collector.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Collector terminated normally.\n";
    
    #ifdef _WIN32
    std::cout << "\nPress any key to exit...";
    _getch();
    #endif
    
    return 0;
}
