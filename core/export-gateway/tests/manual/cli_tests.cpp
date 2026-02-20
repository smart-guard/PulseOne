/**
 * @file cli_tests.cpp
 * @brief CLI 기반 수동 테스트 함수 구현
 *
 * export-gateway --test-* 플래그들이 실행하는 테스트 로직입니다.
 * 프로덕션 코드인 main.cpp에서 분리되었습니다.
 *
 * 빌드: Makefile의 ALL_OBJECTS에 포함하지 않음 (테스트 전용 빌드에서만 사용)
 * 실행: export-gateway --test-alarm / --test-targets / --interactive 등
 */

#include "cli_tests.h"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "Gateway/Model/AlarmMessage.h"
#include "Logging/LogManager.h"
#include "Schedule/ScheduledExporter.h"

extern std::atomic<bool> g_shutdown_requested;

// =============================================================================
// 단일 알람 테스트
// =============================================================================

void testSingleAlarm(PulseOne::Gateway::Service::ITargetRunner &runner) {
  std::cout << "\n=== 단일 알람 테스트 ===\n";

  try {
    PulseOne::Gateway::Model::AlarmMessage alarm;
    alarm.site_id = 1001;
    alarm.point_name = "TEST_POINT_001";
    alarm.measured_value = 85.5;
    alarm.alarm_level = 1;
    alarm.status_code = 1;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    alarm.timestamp = ss.str();

    LogManager::getInstance().Info("테스트 알람 전송: " + alarm.point_name);

    auto results = runner.sendAlarm(alarm);

    std::cout << "전송 결과:\n";
    std::cout << "  총 타겟 수: " << results.size() << "\n";

    int success_count = 0, failure_count = 0;
    for (const auto &result : results) {
      if (result.success) {
        success_count++;
        std::cout << "  ✅ " << result.target_name << " (" << result.target_type
                  << ") - 성공\n";
      } else {
        failure_count++;
        std::cout << "  ❌ " << result.target_name << " (" << result.target_type
                  << ") - 실패: " << result.error_message << "\n";
      }
    }

    std::cout << "\n성공: " << success_count << " / 실패: " << failure_count
              << "\n\n";

  } catch (const std::exception &e) {
    std::cerr << "테스트 실패: " << e.what() << "\n";
    LogManager::getInstance().Error("Test alarm failed: " +
                                    std::string(e.what()));
  }
}

// =============================================================================
// 타겟 목록 출력
// =============================================================================

void testTargets(PulseOne::Gateway::Service::ITargetRegistry &registry) {
  std::cout << "\n=== 타겟 목록 ===\n";

  try {
    auto targets = registry.getAllTargets();
    std::cout << "총 타겟 수: " << targets.size() << "\n\n";

    for (size_t i = 0; i < targets.size(); ++i) {
      const auto &target = targets[i];
      std::cout << (i + 1) << ". " << target.name;
      std::cout << " (" << target.type << ")";
      std::cout << " - " << (target.enabled ? "활성화" : "비활성화");
      std::cout << "\n";
    }
    std::cout << "\n";

  } catch (const std::exception &e) {
    std::cerr << "타겟 목록 조회 실패: " << e.what() << "\n";
  }
}

// =============================================================================
// 연결 테스트
// =============================================================================

void testConnection(PulseOne::Gateway::Service::ITargetRunner &runner,
                    PulseOne::Gateway::Service::ITargetRegistry &registry) {
  std::cout << "\n=== 연결 테스트 ===\n";

  try {
    auto targets = registry.getAllTargets();
    std::cout << "총 타겟 수: " << targets.size() << "\n";

    PulseOne::Gateway::Model::AlarmMessage test_alarm;
    test_alarm.site_id = 1001;
    test_alarm.point_name = "CONNECTION_TEST";
    test_alarm.measured_value = 1.0;
    test_alarm.alarm_level = 0;
    test_alarm.status_code = 0;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    test_alarm.timestamp = ss.str();

    std::cout << "개별 타겟 연결 테스트:\n";
    auto results = runner.sendAlarm(test_alarm);

    int success_count = 0;
    for (const auto &result : results) {
      std::cout << (result.success ? "✅" : "❌") << " " << result.target_name
                << " (" << result.target_type << ")";
      if (!result.success) {
        std::cout << " (" << result.error_message << ")";
      }
      std::cout << "\n";
      if (result.success)
        success_count++;
    }

    std::cout << "\n연결 성공: " << success_count << " / " << results.size()
              << "\n\n";

  } catch (const std::exception &e) {
    std::cerr << "연결 테스트 실패: " << e.what() << "\n";
  }
}

// =============================================================================
// 스케줄 테스트
// =============================================================================

void testSchedule() {
  std::cout << "\n========================================\n";
  std::cout << "스케줄 Export 테스트\n";
  std::cout << "========================================\n";

  try {
    auto &scheduled_exporter =
        PulseOne::Schedule::ScheduledExporter::getInstance();

    std::cout << "ScheduledExporter 상태: "
              << (scheduled_exporter.isRunning() ? "실행 중" : "중지됨")
              << "\n";

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "스케줄 실행 대기 중...\n";

  } catch (const std::exception &e) {
    std::cout << "스케줄 테스트 실패: " << e.what() << "\n";
  }
}

// =============================================================================
// 통계 출력
// =============================================================================

void printStatistics(PulseOne::Gateway::Service::ITargetRunner &runner) {
  std::cout << "\n========================================\n";
  std::cout << "Export 통계 (Gateway V2)\n";
  std::cout << "========================================\n";

  try {
    auto stats = runner.getStats();

    std::cout << "전체 Export: " << stats.total_exports << "\n";
    std::cout << "성공: " << stats.successful_exports << "\n";
    std::cout << "실패: " << stats.failed_exports << "\n";

    if (stats.total_exports > 0) {
      double success_rate =
          (double)stats.successful_exports / stats.total_exports * 100.0;
      std::cout << "성공률: " << std::fixed << std::setprecision(2)
                << success_rate << "%\n";
    }

    std::cout << "\n알람 Export: " << stats.alarm_exports << "\n";
    std::cout << "스케줄 실행: " << stats.schedule_executions << "\n";
    std::cout << "스케줄 Export: " << stats.schedule_exports << "\n";
    std::cout << "\n평균 처리 시간: " << std::fixed << std::setprecision(2)
              << stats.avg_processing_time_ms << "ms\n";

    auto now = std::chrono::system_clock::now();
    auto uptime =
        std::chrono::duration_cast<std::chrono::seconds>(now - stats.start_time)
            .count();
    std::cout << "Uptime: " << uptime << " seconds\n";
    std::cout << "========================================\n";

  } catch (const std::exception &e) {
    std::cout << "통계 조회 실패: " << e.what() << "\n";
  }
}

// =============================================================================
// 대화형(Interactive) 모드
// =============================================================================

void runInteractiveMode(PulseOne::Gateway::Service::ITargetRunner &runner,
                        PulseOne::Gateway::Service::ITargetRegistry &registry) {
  LogManager::getInstance().Info("대화형 모드 시작");
  std::cout << "대화형 모드로 실행 중...\n";
  std::cout << "명령어: status, test, targets, schedule, connection, quit\n\n";

  std::string command;
  while (!g_shutdown_requested.load()) {
    std::cout << "> ";
    std::getline(std::cin, command);

    if (command == "quit" || command == "exit") {
      break;
    } else if (command == "status") {
      printStatistics(runner);
    } else if (command == "test") {
      testSingleAlarm(runner);
    } else if (command == "targets") {
      testTargets(registry);
    } else if (command == "schedule") {
      testSchedule();
    } else if (command == "connection") {
      testConnection(runner, registry);
    } else if (command == "help") {
      std::cout << "명령어:\n";
      std::cout << "  status      - 통계 출력\n";
      std::cout << "  test        - 테스트 알람 전송\n";
      std::cout << "  targets     - 타겟 목록\n";
      std::cout << "  schedule    - 스케줄 실행\n";
      std::cout << "  connection  - 연결 테스트\n";
      std::cout << "  quit/exit   - 종료\n";
    } else {
      std::cout << "알 수 없는 명령어. 'help' 입력\n";
    }
  }

  LogManager::getInstance().Info("대화형 모드 종료");
}
