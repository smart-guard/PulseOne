-- ============================================================================
-- PulseOne Export System - HDCL-CSP 완전한 설정 (최종 완성본)
-- ============================================================================
-- 파일명: hdcl-export-system-complete.sql
-- 작성일: 2025-10-23
-- 버전: 2.0.0 FINAL
-- 
-- 📋 이 파일의 역할:
--   - 기존 export 관련 데이터 완전 삭제
--   - HDCL-CSP 운영 환경에 맞는 Export Targets 생성 (4개)
--   - 포인트 매핑 설정 (총 28개)
--   - 모든 Foreign Key 에러 해결
--
-- 🏗️ 데이터 흐름:
--   iCos Collector
--     ↓
--   1️⃣ S3 저장 (s3://hdcl-csp-prod/icos/) - 전체 백업
--   2️⃣ API Gateway → Lambda → EC2 WAS
--       - POST /alarm → icos_5_csp_alarm
--       - POST /control → icos_5_csp_control_request
--       - PUT /control → icos_5_csp_control_result
--
-- 📊 Export Targets (4개):
--   1. Alarm API (알람 전송) - 2개 포인트
--   2. S3 Data Lake (전체 백업) - 17개 포인트
--   3. Control Request API (제어 요청) - 6개 포인트
--   4. Control Result API (제어 결과) - 3개 포인트
--
-- 🚀 적용 방법:
--   sqlite3 /app/data/sql/pulseone.db < hdcl-export-system-complete.sql
--
-- ⚠️ 주의:
--   - 기존 export_targets, export_target_mappings 데이터 삭제됨
--   - AWS 키와 API Gateway 키는 YOUR_XXX로 설정됨 (나중에 업데이트)
-- ============================================================================

PRAGMA foreign_keys = ON;

-- ============================================================================
-- 1단계: 기존 데이터 완전 삭제
-- ============================================================================

BEGIN TRANSACTION;

-- export_schedules 삭제 (FK 제약 때문에 먼저)
DELETE FROM export_schedules;

-- export_logs 삭제
DELETE FROM export_logs;

-- export_target_mappings 삭제 (FK 제약 때문에 먼저)
DELETE FROM export_target_mappings;

-- export_targets 삭제
DELETE FROM export_targets;

-- export_profile_points 삭제
DELETE FROM export_profile_points;

-- protocol_mappings 삭제
DELETE FROM protocol_mappings;

-- protocol_services 삭제
DELETE FROM protocol_services;

COMMIT;

SELECT '✅ 1단계: 기존 Export 데이터 삭제 완료' as step1;

-- ============================================================================
-- 2단계: Export Profiles 확인/생성
-- ============================================================================

-- 기본 프로필이 없으면 생성
INSERT OR IGNORE INTO export_profiles (id, name, description, is_enabled)
VALUES (1, 'HDCL-CSP 운영 데이터', 'HDCL-CSP 실제 운영 환경 데이터', 1);

SELECT '✅ 2단계: Export Profile 확인 완료' as step2;

-- ============================================================================
-- 3단계: Export Targets 생성 (4개) - FK 에러 없는 버전
-- ============================================================================

-- ============================================================================
-- Target 1: Alarm API (긴급 알람 전송)
-- ============================================================================
-- 엔드포인트: POST https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/alarm
-- Lambda: icos_5_csp_alarm → EC2 WAS
-- 전송 조건: 알람 발생 시 즉시 (on_change)
-- 매핑: Emergency_Stop, Active_Alarms (2개)

INSERT INTO export_targets (
    id,
    name,
    target_type,
    description,
    config,
    is_enabled,
    export_mode,
    execution_order
) VALUES (
    1,
    'ICOS5 API - Alarm',
    'http',
    'API Gateway POST /alarm → Lambda (icos_5_csp_alarm) → EC2 WAS',
    '{
        "url": "https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/alarm",
        "method": "POST",
        "headers": {
            "Content-Type": "application/json",
            "x-api-key": "YOUR_API_GATEWAY_KEY"
        },
        "timeout": 10000,
        "retry": 3,
        "retryDelay": 2000,
        "lambdaFunction": "icos_5_csp_alarm"
    }',
    1,
    'on_change',
    1
);

-- ============================================================================
-- Target 2: S3 Data Lake (전체 데이터 백업)
-- ============================================================================
-- 엔드포인트: s3://hdcl-csp-prod/icos/
-- 전송 조건: 1분마다 배치 (batch)
-- 매핑: 전체 17개 포인트

INSERT INTO export_targets (
    id,
    name,
    target_type,
    description,
    config,
    is_enabled,
    export_mode,
    execution_order
) VALUES (
    2,
    'HDCL-CSP S3 Data Lake',
    's3',
    'S3 저장 (s3://hdcl-csp-prod/icos/) - 전체 데이터 백업',
    '{
        "bucket": "hdcl-csp-prod",
        "region": "ap-northeast-2",
        "prefix": "icos/",
        "accessKey": "YOUR_AWS_ACCESS_KEY",
        "secretKey": "YOUR_AWS_SECRET_KEY",
        "fileFormat": "json",
        "compression": "gzip",
        "partitionBy": "datetime",
        "fileNaming": "{timestamp}-{type}.json.gz"
    }',
    1,
    'batch',
    0
);

-- ============================================================================
-- Target 3: Control Request API (제어 명령 전송)
-- ============================================================================
-- 엔드포인트: POST https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/control
-- Lambda: icos_5_csp_control_request → EC2 WAS
-- 전송 조건: 제어 요청 시 즉시 (on_change)
-- 매핑: Line_Speed, Motor_Current, Robot 위치, Welding_Current (6개)

INSERT INTO export_targets (
    id,
    name,
    target_type,
    description,
    config,
    is_enabled,
    export_mode,
    execution_order
) VALUES (
    3,
    'ICOS5 API - Control Request',
    'http',
    'API Gateway POST /control → Lambda (icos_5_csp_control_request) → EC2 WAS',
    '{
        "url": "https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/control",
        "method": "POST",
        "headers": {
            "Content-Type": "application/json",
            "x-api-key": "YOUR_API_GATEWAY_KEY"
        },
        "timeout": 10000,
        "retry": 3,
        "retryDelay": 2000,
        "lambdaFunction": "icos_5_csp_control_request"
    }',
    1,
    'on_change',
    2
);

-- ============================================================================
-- Target 4: Control Result API (제어 결과 업데이트)
-- ============================================================================
-- 엔드포인트: PUT https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/control
-- Lambda: icos_5_csp_control_result → EC2 WAS
-- 전송 조건: 명시적 호출 시 (on_demand)
-- 매핑: Production_Count, Screen_Status, User_Level (3개)

INSERT INTO export_targets (
    id,
    name,
    target_type,
    description,
    config,
    is_enabled,
    export_mode,
    execution_order
) VALUES (
    4,
    'ICOS5 API - Control Result',
    'http',
    'API Gateway PUT /control → Lambda (icos_5_csp_control_result) → EC2 WAS',
    '{
        "url": "https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/control",
        "method": "PUT",
        "headers": {
            "Content-Type": "application/json",
            "x-api-key": "YOUR_API_GATEWAY_KEY"
        },
        "timeout": 10000,
        "retry": 2,
        "retryDelay": 1000,
        "lambdaFunction": "icos_5_csp_control_result"
    }',
    1,
    'on_demand',
    3
);

SELECT '✅ 3단계: Export Targets 생성 완료 (4개)' as step3;

-- ============================================================================
-- 4단계: Export Target Mappings (포인트 매핑) - 총 28개
-- ============================================================================

-- ============================================================================
-- Target 1 (Alarm API): 알람 관련 포인트만 (2개)
-- ============================================================================
-- Emergency_Stop (id=5), Active_Alarms (id=7)

INSERT INTO export_target_mappings (
    target_id, point_id, target_field_name, target_description, is_enabled
) VALUES
    (1, 5, 'emergency_stop', 'Emergency Stop - 긴급정지', 1),
    (1, 7, 'active_alarms', 'Active Alarms - 활성 알람', 1);

-- ============================================================================
-- Target 2 (S3): 전체 데이터 백업 (17개)
-- ============================================================================
-- 모든 포인트를 S3에 백업

INSERT INTO export_target_mappings (target_id, point_id, target_field_name, is_enabled)
SELECT 2, id, name, 1 FROM data_points WHERE id <= 17;

-- ============================================================================
-- Target 3 (Control Request): 제어 가능 포인트 (6개)
-- ============================================================================
-- Line_Speed, Motor_Current, Robot 위치(X,Y,Z), Welding_Current

INSERT INTO export_target_mappings (
    target_id, point_id, target_field_name, target_description, is_enabled
) VALUES
    (3, 2, 'line_speed', 'Line Speed - 라인 속도 제어', 1),
    (3, 3, 'motor_current', 'Motor Current - 모터 전류', 1),
    (3, 9, 'robot_x_position', 'Robot X Position', 1),
    (3, 10, 'robot_y_position', 'Robot Y Position', 1),
    (3, 11, 'robot_z_position', 'Robot Z Position', 1),
    (3, 12, 'welding_current', 'Welding Current - 용접 전류', 1);

-- ============================================================================
-- Target 4 (Control Result): 제어 결과 포인트 (3개)
-- ============================================================================
-- Production_Count, Screen_Status, User_Level

INSERT INTO export_target_mappings (
    target_id, point_id, target_field_name, target_description, is_enabled
) VALUES
    (4, 1, 'production_count', 'Production Count - 생산 수량', 1),
    (4, 6, 'screen_status', 'Screen Status - 화면 상태', 1),
    (4, 8, 'user_level', 'User Level - 사용자 레벨', 1);

SELECT '✅ 4단계: Export Target Mappings 생성 완료 (28개)' as step4;

-- ============================================================================
-- 5단계: 검증 및 결과 출력
-- ============================================================================

SELECT '' as blank;
SELECT '━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━' as separator;
SELECT '🎉 HDCL-CSP Export System 설정 완료!' as message;
SELECT '━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━' as separator;
SELECT '' as blank2;

-- 데이터 흐름 설명
SELECT '🏗️  데이터 흐름:' as architecture;
SELECT '   iCos Collector' as flow1;
SELECT '     ↓' as arrow1;
SELECT '   1️⃣  S3 저장 (전체 백업 - 1분마다)' as flow2;
SELECT '   2️⃣  API Gateway → Lambda → EC2 WAS' as flow3;
SELECT '       • POST /alarm → icos_5_csp_alarm' as flow4;
SELECT '       • POST /control → icos_5_csp_control_request' as flow5;
SELECT '       • PUT /control → icos_5_csp_control_result' as flow6;
SELECT '' as blank3;

-- Export Targets 요약
SELECT '📊 Export Targets (4개):' as targets_title;
SELECT '' as blank4;

SELECT 
    '   ' || id || '. ' || name as target,
    '      Type: ' || target_type || ' | Mode: ' || export_mode || ' | ' || 
    CASE WHEN is_enabled = 1 THEN '✅ 활성' ELSE '❌ 비활성' END as status
FROM export_targets
WHERE id IN (1, 2, 3, 4)
ORDER BY id;

SELECT '' as blank5;

-- 엔드포인트 정보
SELECT '🌐 엔드포인트:' as endpoint_title;
SELECT '   Target 1: POST /alarm → icos_5_csp_alarm' as ep1;
SELECT '   Target 2: s3://hdcl-csp-prod/icos/' as ep2;
SELECT '   Target 3: POST /control → icos_5_csp_control_request' as ep3;
SELECT '   Target 4: PUT /control → icos_5_csp_control_result' as ep4;
SELECT '' as blank6;

-- 매핑 통계
SELECT '🔗 Target별 매핑 포인트:' as mapping_title;
SELECT '' as blank7;

SELECT 
    '   Target ' || et.id || ': ' || et.name as target,
    '      매핑: ' || COUNT(tm.id) || '개 포인트' as count
FROM export_targets et
LEFT JOIN export_target_mappings tm ON et.id = tm.target_id
WHERE et.id IN (1, 2, 3, 4)
GROUP BY et.id
ORDER BY et.id;

SELECT '' as blank8;

-- 상세 매핑 정보
SELECT '📋 매핑 상세:' as detail_title;
SELECT '   Target 1 (Alarm):        Emergency_Stop, Active_Alarms' as detail1;
SELECT '   Target 2 (S3):           전체 17개 포인트' as detail2;
SELECT '   Target 3 (Control Req):  Line_Speed, Motor, Robot(X,Y,Z), Welding' as detail3;
SELECT '   Target 4 (Control Res):  Production_Count, Screen_Status, User_Level' as detail4;
SELECT '' as blank9;

-- 설정 필요 항목
SELECT '⚠️  설정 필요 항목:' as warning_title;
SELECT '   1. AWS Access Key / Secret Key (S3용)' as item1;
SELECT '   2. API Gateway x-api-key (HTTP API 3개 공통)' as item2;
SELECT '' as blank10;

-- 키 업데이트 방법
SELECT '🔑 인증 키 업데이트 SQL:' as update_title;
SELECT '' as blank11;
SELECT '   -- API Gateway 키 업데이트' as update1;
SELECT '   UPDATE export_targets' as update2;
SELECT '   SET config = json_set(config, ''$.headers."x-api-key"'', ''실제-API-키'')' as update3;
SELECT '   WHERE id IN (1, 3, 4);' as update4;
SELECT '' as blank12;
SELECT '   -- S3 키 업데이트' as update5;
SELECT '   UPDATE export_targets' as update6;
SELECT '   SET config = json_set(config,' as update7;
SELECT '       ''$.accessKey'', ''AKIA실제키'',' as update8;
SELECT '       ''$.secretKey'', ''실제시크릿키'')' as update9;
SELECT '   WHERE id = 2;' as update10;
SELECT '' as blank13;

-- 확인 쿼리
SELECT '🔍 확인 쿼리:' as check_title;
SELECT '' as blank14;
SELECT '   -- 타겟 목록' as check1;
SELECT '   SELECT id, name, target_type, is_enabled FROM export_targets;' as check2;
SELECT '' as blank15;
SELECT '   -- 매핑 통계' as check3;
SELECT '   SELECT et.id, et.name, COUNT(tm.id) as points' as check4;
SELECT '   FROM export_targets et' as check5;
SELECT '   LEFT JOIN export_target_mappings tm ON et.id = tm.target_id' as check6;
SELECT '   GROUP BY et.id ORDER BY et.id;' as check7;
SELECT '' as blank16;
SELECT '   -- 매핑 상세' as check8;
SELECT '   SELECT et.name, dp.name, tm.target_field_name' as check9;
SELECT '   FROM export_target_mappings tm' as check10;
SELECT '   JOIN export_targets et ON tm.target_id = et.id' as check11;
SELECT '   JOIN data_points dp ON tm.point_id = dp.id' as check12;
SELECT '   ORDER BY et.id, dp.id;' as check13;
SELECT '' as blank17;

SELECT '━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━' as separator2;
SELECT '✅ 설정 완료! 인증 키 업데이트 후 C++ Collector 시작!' as final_message;
SELECT '━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━' as separator3;

-- ============================================================================
-- 끝
-- ============================================================================