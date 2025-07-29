// 🔥 최소한의 Application.h 테스트 (다른 헤더 include 제거)

#ifndef APPLICATION_H
#define APPLICATION_H

// 🔥 STL 헤더만 include (PulseOne 헤더 제거)
#include <string>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

namespace PulseOne {
namespace Core {

/**
 * @brief 최소한의 CollectorApplication 클래스 (테스트용)
 */
class CollectorApplication {
public:
    CollectorApplication() = default;
    ~CollectorApplication() = default;
    
    void Run();        // 🔥 선언만 (구현은 .cpp에서)
    void Stop();       // 🔥 선언만 (구현은 .cpp에서)

private:
    std::atomic<bool> running_{false};
    std::string version_ = "1.0.0";
};

} // namespace Core
} // namespace PulseOne

#endif // APPLICATION_H