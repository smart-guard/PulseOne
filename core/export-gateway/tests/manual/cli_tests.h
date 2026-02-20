/**
 * @file cli_tests.h
 * @brief CLI 기반 수동 테스트 함수 선언
 *
 * 이 파일은 export-gateway --test-* 플래그로 실행되는
 * 수동 테스트 함수들의 인터페이스입니다.
 *
 * 프로덕션 코드(main.cpp)에서 테스트 로직을 분리하기 위해 만들어졌습니다.
 */

#pragma once

#include "Gateway/Service/ITargetRegistry.h"
#include "Gateway/Service/ITargetRunner.h"

/**
 * @brief 단일 테스트 알람을 모든 타겟에 전송
 */
void testSingleAlarm(PulseOne::Gateway::Service::ITargetRunner &runner);

/**
 * @brief DB에서 로드된 타겟 목록을 콘솔에 출력
 */
void testTargets(PulseOne::Gateway::Service::ITargetRegistry &registry);

/**
 * @brief 각 타겟에 연결 테스트 알람을 전송하고 결과를 출력
 */
void testConnection(PulseOne::Gateway::Service::ITargetRunner &runner,
                    PulseOne::Gateway::Service::ITargetRegistry &registry);

/**
 * @brief ScheduledExporter 상태 확인
 */
void testSchedule();

/**
 * @brief Export 통계 콘솔 출력
 */
void printStatistics(PulseOne::Gateway::Service::ITargetRunner &runner);

/**
 * @brief 대화형(REPL) 커맨드 루프 실행
 */
void runInteractiveMode(PulseOne::Gateway::Service::ITargetRunner &runner,
                        PulseOne::Gateway::Service::ITargetRegistry &registry);
