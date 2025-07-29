// =============================================================================
// 🔥 최소한의 Application.cpp 테스트
// =============================================================================

#include "Core/Application.h"
#include <iostream>

namespace PulseOne {
namespace Core {

void CollectorApplication::Run() {
    std::cout << "CollectorApplication::Run() called" << std::endl;
}

void CollectorApplication::Stop() {
    std::cout << "CollectorApplication::Stop() called" << std::endl;
}

} // namespace Core
} // namespace PulseOne

// =============================================================================
// 🔥 main.cpp 테스트
// =============================================================================

#include "Core/Application.h"

int main() {
    PulseOne::Core::CollectorApplication app;
    app.Run();
    app.Stop();
    return 0;
}