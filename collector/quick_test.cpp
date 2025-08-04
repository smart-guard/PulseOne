
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "=== PulseOne 기본 컴파일 테스트 ===" << std::endl;
    std::string test = "Hello PulseOne";
    std::vector<int> data = {1,2,3};
    std::cout << "✅ 기본 컴파일 성공: " << test << std::endl;
    std::cout << "✅ STL 컨테이너 동작: " << data.size() << " elements" << std::endl;
    return 0;
}

