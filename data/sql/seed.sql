-- ============================================================================
-- seed.sql — PulseOne 초기 데모 데이터
--
-- 구성 원칙:
--   - 단일 테넌트 / 단일 사이트 / HMI-001 디바이스 유지
--   - 테스트·E2E 더미 데이터 제거
--   - export-gateway 타겟은 샘플 URL/키 사용 (실제 운영 키 ❌)
--   - HMI-001 + insite 연동 복원이 필요하면 seed_hmi001_insite.sql 추가 실행
-- ============================================================================

-- ──────────────────────────────────────────────
-- schema_versions
-- ──────────────────────────────────────────────
INSERT INTO schema_versions VALUES(1,'3.6.0',datetime('now','localtime'),'PulseOne v3.6.x baseline schema');

-- ──────────────────────────────────────────────
-- system_settings
-- ──────────────────────────────────────────────
INSERT INTO system_settings VALUES(1,'backup.auto_enabled','false',NULL,'general','string',0,0,NULL,NULL,NULL,NULL,1,1770072789924);
INSERT INTO system_settings VALUES(2,'backup.schedule_time','02:00',NULL,'general','string',0,0,NULL,NULL,NULL,NULL,1,1770072789931);
INSERT INTO system_settings VALUES(3,'backup.retention_days','30',NULL,'general','string',0,0,NULL,NULL,NULL,NULL,1,1770072789931);
INSERT INTO system_settings VALUES(4,'backup.include_logs','true',NULL,'general','string',0,0,NULL,NULL,NULL,NULL,1,1770072789931);
INSERT INTO system_settings VALUES(5,'data_collection.influxdb_storage_interval','0',NULL,'collection','integer',0,0,NULL,NULL,NULL,NULL,1,1770072789931);
INSERT INTO system_settings VALUES(6,'data_collection.rdb_sync_interval','60',NULL,'collection','integer',0,0,NULL,NULL,NULL,NULL,1,1770072789932);
INSERT INTO system_settings VALUES(7,'collection.max_concurrent_connections','20',NULL,'collection','integer',0,0,NULL,NULL,NULL,NULL,1,1770072789931);
INSERT INTO system_settings VALUES(8,'collection.device_response_timeout','3000',NULL,'collection','integer',0,0,NULL,NULL,NULL,NULL,1,1770072789931);

-- ──────────────────────────────────────────────
-- tenants (1개)
-- ──────────────────────────────────────────────
INSERT INTO tenants VALUES(
    1,'PulseOne Demo','DEMO001','demo.pulseone.io',
    'Demo Manager','demo@pulseone.io','+82-2-0000-0000',
    'professional','active',10,10000,50,'monthly',
    NULL,NULL,NULL,1,'Asia/Seoul','KRW','ko',
    datetime('now','localtime'),datetime('now','localtime'),0
);

-- ──────────────────────────────────────────────
-- roles
-- ──────────────────────────────────────────────
INSERT INTO roles VALUES('system_admin','시스템 관리자','시스템 전체 리소스 및 모든 테넌트에 대한 전체 제어 권한',1,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO roles VALUES('company_admin','고객사 관리자','해당 테넌트(고객사) 내의 모든 리소스 및 사용자 관리 권한',1,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO roles VALUES('manager','운영 관리자','현장 운영 및 사용자 관리를 포함한 일반적인 관리 권한',1,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO roles VALUES('engineer','엔지니어','디바이스 설정 및 알람 규칙 관리 등 기술적인 운영 권한',1,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO roles VALUES('operator','운전원','실시간 모니터링 및 디바이스 상태 확인 권한',1,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO roles VALUES('viewer','조회자','데이터 및 보고서 조회만 가능한 제한된 권한',1,datetime('now','localtime'),datetime('now','localtime'));

-- ──────────────────────────────────────────────
-- permissions
-- ──────────────────────────────────────────────
INSERT INTO permissions VALUES('view_dashboard','대시보드 조회','시스템 현황 대시보드를 조회할 수 있는 권한','조회','dashboard','["read"]',1,datetime('now','localtime'));
INSERT INTO permissions VALUES('manage_devices','디바이스 관리','디바이스 등록, 수정, 삭제 및 제어 권한','관리','devices','["read", "write", "delete"]',1,datetime('now','localtime'));
INSERT INTO permissions VALUES('manage_alarms','알람 관리','알람 규칙 설정 및 알람 이력 관리 권한','관리','alarms','["read", "write", "delete"]',1,datetime('now','localtime'));
INSERT INTO permissions VALUES('manage_users','사용자 관리','사용자 계정 생성, 수정, 삭제 및 권한 할당 권한','관리','users','["read", "write", "delete"]',1,datetime('now','localtime'));
INSERT INTO permissions VALUES('view_reports','보고서 조회','수집 데이터 보고서 및 통계 자료 조회 권한','조회','reports','["read"]',1,datetime('now','localtime'));
INSERT INTO permissions VALUES('export_data','데이터 내보내기','수집 데이터를 외부 파일(CSV, Excel 등)로 추출하는 권한','데이터','data','["read", "execute"]',1,datetime('now','localtime'));
INSERT INTO permissions VALUES('system_settings','시스템 설정','시스템 환경 설정 및 네트워크 설정 변경 권한','시스템','settings','["read", "write"]',1,datetime('now','localtime'));
INSERT INTO permissions VALUES('backup_restore','백업/복원','데이터베이스 백업 생성 및 시스템 복원 권한','시스템','backup','["read", "execute"]',1,datetime('now','localtime'));
INSERT INTO permissions VALUES('manage_tenants','고객사 관리','테넌트(고객사) 정보 및 라이선스 관리 권한','시스템','tenants','["read", "write", "delete"]',1,datetime('now','localtime'));
INSERT INTO permissions VALUES('manage_roles','권한 관리','시스템 역할(Role) 및 세부 권한 정의 권한','시스템','permissions','["read", "write", "delete"]',1,datetime('now','localtime'));

-- ──────────────────────────────────────────────
-- role_permissions
-- ──────────────────────────────────────────────
INSERT INTO role_permissions VALUES('system_admin','backup_restore');
INSERT INTO role_permissions VALUES('system_admin','export_data');
INSERT INTO role_permissions VALUES('system_admin','manage_alarms');
INSERT INTO role_permissions VALUES('system_admin','manage_devices');
INSERT INTO role_permissions VALUES('system_admin','manage_roles');
INSERT INTO role_permissions VALUES('system_admin','manage_tenants');
INSERT INTO role_permissions VALUES('system_admin','manage_users');
INSERT INTO role_permissions VALUES('system_admin','system_settings');
INSERT INTO role_permissions VALUES('system_admin','view_dashboard');
INSERT INTO role_permissions VALUES('system_admin','view_reports');
INSERT INTO role_permissions VALUES('company_admin','backup_restore');
INSERT INTO role_permissions VALUES('company_admin','export_data');
INSERT INTO role_permissions VALUES('company_admin','manage_alarms');
INSERT INTO role_permissions VALUES('company_admin','manage_devices');
INSERT INTO role_permissions VALUES('company_admin','manage_roles');
INSERT INTO role_permissions VALUES('company_admin','manage_users');
INSERT INTO role_permissions VALUES('company_admin','system_settings');
INSERT INTO role_permissions VALUES('company_admin','view_dashboard');
INSERT INTO role_permissions VALUES('company_admin','view_reports');
INSERT INTO role_permissions VALUES('manager','view_dashboard');
INSERT INTO role_permissions VALUES('manager','manage_devices');
INSERT INTO role_permissions VALUES('manager','manage_alarms');
INSERT INTO role_permissions VALUES('manager','manage_users');
INSERT INTO role_permissions VALUES('manager','view_reports');
INSERT INTO role_permissions VALUES('manager','export_data');
INSERT INTO role_permissions VALUES('engineer','view_dashboard');
INSERT INTO role_permissions VALUES('engineer','manage_devices');
INSERT INTO role_permissions VALUES('engineer','manage_alarms');
INSERT INTO role_permissions VALUES('engineer','view_reports');
INSERT INTO role_permissions VALUES('operator','view_dashboard');
INSERT INTO role_permissions VALUES('operator','manage_devices');
INSERT INTO role_permissions VALUES('viewer','view_dashboard');
INSERT INTO role_permissions VALUES('viewer','view_reports');

-- ──────────────────────────────────────────────
-- users
-- admin 초기 비밀번호: admin123 (bcrypt)
-- ──────────────────────────────────────────────
INSERT INTO users VALUES(
    1,NULL,'admin','admin@pulseone.io',
    '$2a$10$IjD584sBOhIlAH76kcc68eq6x9S5qEsVDHqrr1Z.q5AC93HkT6bvO',
    'Administrator',NULL,NULL,NULL,NULL,NULL,NULL,
    'system_admin',NULL,NULL,NULL,NULL,0,NULL,NULL,NULL,NULL,
    0,1,0,0,NULL,NULL,0,'Asia/Seoul','ko','light',NULL,NULL,
    datetime('now','localtime'),datetime('now','localtime'),NULL,0
);
INSERT INTO users VALUES(
    2,1,'engineer01','engineer@pulseone.io',
    '$2a$10$IjD584sBOhIlAH76kcc68eq6x9S5qEsVDHqrr1Z.q5AC93HkT6bvO',
    '데모 엔지니어',NULL,NULL,NULL,'기술팀','엔지니어',NULL,
    'engineer',NULL,NULL,NULL,NULL,0,NULL,NULL,NULL,NULL,
    0,1,0,0,NULL,NULL,0,'Asia/Seoul','ko','light',NULL,NULL,
    datetime('now','localtime'),datetime('now','localtime'),NULL,0
);
INSERT INTO users VALUES(
    3,1,'operator01','operator@pulseone.io',
    '$2a$10$IjD584sBOhIlAH76kcc68eq6x9S5qEsVDHqrr1Z.q5AC93HkT6bvO',
    '데모 운전원',NULL,NULL,NULL,'운영팀','운전원',NULL,
    'operator',NULL,NULL,NULL,NULL,0,NULL,NULL,NULL,NULL,
    0,1,0,0,NULL,NULL,0,'Asia/Seoul','ko','light',NULL,NULL,
    datetime('now','localtime'),datetime('now','localtime'),NULL,0
);

-- ──────────────────────────────────────────────
-- sites (1개)
-- ──────────────────────────────────────────────
INSERT INTO sites VALUES(
    1,1,NULL,'PulseOne Demo Site','DEMO-SITE-001','factory',
    'PulseOne 데모 현장 — 다양한 프로토콜(Modbus/MQTT/OPC UA/ROS) 통합 수집 환경',
    '서울특별시 강남구',
    NULL,NULL,NULL,NULL,NULL,NULL,
    'Asia/Seoul','KRW','ko',
    NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,
    0,NULL,0,1,0,1,1,NULL,NULL,NULL,
    datetime('now','localtime'),datetime('now','localtime')
);

-- ──────────────────────────────────────────────
-- edge_servers
-- ──────────────────────────────────────────────
INSERT INTO edge_servers VALUES(
    1,1,'Main Collector','collector',NULL,'PulseOne Demo Site',NULL,
    '127.0.0.1',NULL,NULL,8501,NULL,NULL,NULL,NULL,
    'active',NULL,NULL,'3.6.0',0.0,0.0,0.0,0,NULL,NULL,
    1,1,datetime('now','localtime'),datetime('now','localtime'),
    0,1,100,1000,'all',1
);
INSERT INTO edge_servers VALUES(
    2,1,'Export Gateway','gateway','',NULL,NULL,
    '127.0.0.1',NULL,NULL,8080,NULL,NULL,NULL,NULL,
    'active',NULL,NULL,NULL,0.0,0.0,0.0,0,
    '{"target_priorities":{"1":1,"2":2}}',NULL,
    1,1,datetime('now','localtime'),datetime('now','localtime'),
    0,1,100,1000,'selective',1
);

-- ──────────────────────────────────────────────
-- manufacturers (레퍼런스 DB — 변경하지 않음)
-- ──────────────────────────────────────────────
INSERT INTO manufacturers VALUES(1,'LS Electric','국내 대표 전력/자동화 솔루션 제조사','South Korea','https://www.lselectric.co.kr',NULL,1,0,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO manufacturers VALUES(2,'Siemens','독일 대표 산업 자동화·디지털화 기업','Germany','https://www.siemens.com',NULL,1,0,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO manufacturers VALUES(3,'Schneider Electric','에너지 관리 및 자동화 전문 프랑스 기업','France','https://www.se.com',NULL,1,0,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO manufacturers VALUES(4,'ABB','로봇·전력·자동화 분야 스위스-스웨덴 기업','Switzerland','https://new.abb.com',NULL,1,0,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO manufacturers VALUES(5,'Delta Electronics','전력전자·자동화 대만 글로벌 기업','Taiwan','https://www.deltaww.com',NULL,1,0,datetime('now','localtime'),datetime('now','localtime'));

-- ──────────────────────────────────────────────
-- device_models (레퍼런스 DB)
-- ──────────────────────────────────────────────
INSERT INTO device_models VALUES(1,1,'XGT Series','XGK-CPUS','PLC','LS Electric XGT Series PLC — Modbus TCP/RTU 지원',NULL,NULL,NULL,1,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO device_models VALUES(2,1,'iS7 Inverter','SV-iS7','INVERTER','LS Electric iS7 고성능 인버터 — Modbus TCP/RTU 지원',NULL,NULL,NULL,1,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO device_models VALUES(3,2,'S7-1200','CPU 1214C','PLC','Siemens SIMATIC S7-1200 컴팩트 컨트롤러 — Modbus TCP/OPC UA 지원',NULL,NULL,NULL,1,datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO device_models VALUES(4,3,'PowerLogic PM8000','METSEPM8000','METER','Schneider PowerLogic PM8000 전력 계측기 — Modbus TCP 지원',NULL,NULL,NULL,1,datetime('now','localtime'),datetime('now','localtime'));

-- ──────────────────────────────────────────────
-- protocols (레퍼런스 DB — 변경하지 않음)
-- ──────────────────────────────────────────────
INSERT INTO protocols VALUES(1,'MODBUS_TCP','Modbus TCP/IP','산업 이더넷 기반 Modbus 프로토콜',502,0,0,'["read_coils","read_discrete_inputs","read_holding_registers","read_input_registers","write_single_coil","write_single_register","write_multiple_coils","write_multiple_registers"]','["boolean","int16","uint16","int32","uint32","float32"]','{"slave_id":{"type":"integer","default":1,"min":1,"max":247},"timeout_ms":{"type":"integer","default":3000},"byte_order":{"type":"string","default":"big_endian"}}','{}',1000,3000,10,1,0,'1.0','industrial','Modbus Organization','Modbus Application Protocol V1.1b3',datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO protocols VALUES(2,'BACNET_IP','BACnet/IP','건물 자동화 및 제어 네트워크 BACnet/IP',47808,0,0,'["read_property","write_property","read_property_multiple","who_is","i_am","subscribe_cov"]','["boolean","int32","uint32","float32","string","enumerated","bitstring"]','{"device_id":{"type":"integer","default":1001},"network":{"type":"integer","default":1},"max_apdu":{"type":"integer","default":1476}}','{}',5000,10000,5,1,0,'1.0','building_automation','ASHRAE','ANSI/ASHRAE 135-2020',datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO protocols VALUES(3,'MQTT','MQTT v3.1.1','IoT 경량 메시지 프로토콜',1883,0,1,'["publish","subscribe","unsubscribe","ping","connect","disconnect"]','["boolean","int32","float32","string","json","binary"]','{"client_id":{"type":"string","default":"pulseone_client"},"keep_alive":{"type":"integer","default":60}}','{}',0,5000,100,1,0,'3.1.1','iot','MQTT.org','MQTT v3.1.1 OASIS Standard',datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO protocols VALUES(4,'ETHERNET_IP','EtherNet/IP','ODVA 산업 이더넷 통신 프로토콜',44818,0,0,'["explicit_messaging","implicit_messaging","forward_open","forward_close","get_attribute_single","set_attribute_single"]','["boolean","int8","int16","int32","uint8","uint16","uint32","float32","string"]','{"connection_type":{"type":"string","default":"explicit"},"assembly_instance":{"type":"integer","default":100}}','{}',200,1000,20,1,0,'1.0','industrial','ODVA','EtherNet/IP Specification Volume 1 & 2',datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO protocols VALUES(5,'MODBUS_RTU','Modbus RTU','시리얼 통신 기반 Modbus 프로토콜',NULL,1,0,'["read_coils","read_discrete_inputs","read_holding_registers","read_input_registers","write_single_coil","write_single_register","write_multiple_coils","write_multiple_registers"]','["boolean","int16","uint16","int32","uint32","float32"]','{"slave_id":{"type":"integer","default":1},"baud_rate":{"type":"integer","default":9600},"parity":{"type":"string","default":"none"},"data_bits":{"type":"integer","default":8},"stop_bits":{"type":"integer","default":1}}','{}',1000,3000,1,1,0,'1.0','industrial','Modbus Organization','Modbus over Serial Line V1.02',datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO protocols VALUES(6,'OPC_UA','OPC UA','산업 M2M 통신 표준 — OPC 통합 아키텍처',4840,0,0,'["read","write","browse","subscribe","call"]','["boolean","int8","int16","int32","int64","uint8","uint16","uint32","uint64","float","double","string","datetime"]','{"security_mode":{"type":"string","default":"None"},"timeout_ms":{"type":"integer","default":5000},"publish_interval_ms":{"type":"integer","default":500}}','{}',1000,5000,100,1,0,'1.04','industrial','OPC Foundation','OPC Unified Architecture Specification',datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO protocols VALUES(7,'ROS_BRIDGE','ROS Bridge','Robot Operating System rosbridge WebSocket',9090,0,0,'["subscribe","publish","call_service"]','["boolean","int32","float32","float64","string","json"]','{"rosbridge_port":{"type":"integer","default":9090}}','{}',1000,5000,50,1,0,'2.0','industrial','Open Robotics','ROS Bridge Protocol',datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO protocols VALUES(8,'HTTP_REST','HTTP/REST','HTTP REST API 데이터 수집',80,0,0,'["GET","POST","PUT","DELETE"]','["boolean","int32","float32","float64","string","json"]','{"method":{"type":"string","default":"GET"},"headers":{"type":"object","default":{}},"poll_interval_ms":{"type":"integer","default":5000}}','{}',0,10000,20,1,0,'1.1','iot','IETF','HTTP/1.1 RFC 7230',datetime('now','localtime'),datetime('now','localtime'));
INSERT INTO protocols VALUES(13,'BLE_BEACON','BLE Beacon','Bluetooth Low Energy 비콘',NULL,0,0,'["read"]','["BOOL","INT32","FLOAT32","STRING"]','{"uuid":{"type":"string"}}','{}',1000,3000,100,1,0,'4.0','iot','Bluetooth SIG','Bluetooth Core Specification 4.0',datetime('now','localtime'),datetime('now','localtime'));

-- ──────────────────────────────────────────────
-- template_devices (실용 샘플 2개)
-- ──────────────────────────────────────────────
-- 1. LS Electric XGT PLC (Modbus TCP)
INSERT INTO template_devices VALUES(
    1,1,'LS XGT PLC — Modbus TCP 표준',
    'LS Electric XGT Series PLC Modbus TCP 기본 템플릿. 상태/속도/전류/전압/입출력 레지스터 포함.',
    1,'{"slave_id":1,"byte_order":"big_endian"}',
    1000,3000,1,NULL,
    datetime('now','localtime'),datetime('now','localtime'),0
);
INSERT INTO template_device_settings VALUES(1,1000,10000,5000,5000,3);

-- 데이터 포인트 (XGT PLC)
INSERT INTO template_data_points VALUES(1,1,'CPU_Status','CPU 상태 레지스터',0,NULL,'UINT16','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(2,1,'Run_Stop','운전/정지 비트',1,NULL,'BOOL','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(3,1,'Error_Code','에러 코드',2,NULL,'UINT16','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(4,1,'Motor_RPM','모터 회전수',10,NULL,'FLOAT32','read','rpm',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(5,1,'Motor_Current','모터 전류',12,NULL,'FLOAT32','read','A',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(6,1,'Motor_Voltage','모터 전압',14,NULL,'FLOAT32','read','V',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(7,1,'Input_Register','디지털 입력 레지스터',100,NULL,'UINT16','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(8,1,'Output_Register','디지털 출력 레지스터',200,NULL,'UINT16','read_write','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(9,1,'Speed_Setpoint','속도 설정값',201,NULL,'FLOAT32','read_write','rpm',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(10,1,'Alarm_Code','알람 코드',300,NULL,'UINT16','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);

-- 2. LS Electric iS7 인버터 (Modbus TCP)
INSERT INTO template_devices VALUES(
    2,2,'LS iS7 인버터 — Modbus TCP 기본',
    'LS Electric SV-iS7 인버터 Modbus TCP 모니터링/제어 템플릿. 주파수/전류/전압/DC링크/상태/제어어 포함.',
    1,'{"slave_id":1,"byte_order":"big_endian"}',
    1000,3000,1,NULL,
    datetime('now','localtime'),datetime('now','localtime'),0
);
INSERT INTO template_device_settings VALUES(2,1000,10000,5000,5000,3);

-- 데이터 포인트 (iS7 인버터)
INSERT INTO template_data_points VALUES(11,2,'Frequency','출력 주파수',1,NULL,'FLOAT32','read','Hz',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(12,2,'Output_Current','출력 전류',2,NULL,'FLOAT32','read','A',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(13,2,'Output_Voltage','출력 전압',3,NULL,'FLOAT32','read','V',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(14,2,'DC_Link_Voltage','DC 링크 전압',4,NULL,'FLOAT32','read','V',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(15,2,'Run_Status','운전 상태 워드',10,NULL,'UINT16','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(16,2,'Control_Word','제어 워드',11,NULL,'UINT16','read_write','',1.0,0,1,0,NULL,0.0,NULL,NULL);

-- ──────────────────────────────────────────────
-- devices (HMI-001 1개만)
-- ──────────────────────────────────────────────
INSERT INTO devices VALUES(
    2,1,1,NULL,1,'HMI-001',NULL,'HMI','PulseOne',NULL,NULL,NULL,
    1,'simulator-modbus:50502','{"slave_id":1}',
    1000,3000,3,NULL,NULL,NULL,NULL,NULL,0,0,0,100,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    datetime('now','localtime'),datetime('now','localtime'),NULL
);
INSERT INTO device_settings VALUES(
    2,1000,NULL,1,10000,3000,5000,10,3,5000,1.5,60000,300000,
    1,30,10,1,0,1,0,1,0,0,0,1024,1024,100,
    datetime('now','localtime'),datetime('now','localtime'),NULL
);
INSERT INTO device_status VALUES(
    2,'disconnected',NULL,NULL,0,NULL,NULL,0,0,NULL,NULL,0.0,0,0,0,0.0,
    NULL,NULL,NULL,NULL,NULL,datetime('now','localtime')
);

-- ──────────────────────────────────────────────
-- data_points (HMI-001 5개)
-- ──────────────────────────────────────────────
INSERT INTO data_points VALUES(1,2,'WLS.PV','화재감지 현재값',100,'CO:100','value','BOOL','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,datetime('now','localtime'),datetime('now','localtime'),0,'medium');
INSERT INTO data_points VALUES(2,2,'WLS.SRS','화재감지 수신반 상태',101,'CO:101','data','BOOL','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,datetime('now','localtime'),datetime('now','localtime'),0,'medium');
INSERT INTO data_points VALUES(3,2,'WLS.SCS','화재감지 수신반 제어',102,'CO:102',NULL,'BOOL','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,datetime('now','localtime'),datetime('now','localtime'),0,'medium');
INSERT INTO data_points VALUES(4,2,'WLS.SSS','화재감지 연기 수준',200,'HR:200',NULL,'UINT16','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,datetime('now','localtime'),datetime('now','localtime'),0,'medium');
INSERT INTO data_points VALUES(5,2,'WLS.SBV','화재감지 배터리 전압',201,'HR:201',NULL,'UINT16','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,datetime('now','localtime'),datetime('now','localtime'),0,'medium');

-- ──────────────────────────────────────────────
-- current_values (HMI-001 초기값)
-- ──────────────────────────────────────────────
INSERT INTO current_values VALUES(1,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'unknown',NULL,NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,datetime('now','localtime'));
INSERT INTO current_values VALUES(2,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'unknown',NULL,NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,datetime('now','localtime'));
INSERT INTO current_values VALUES(3,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'unknown',NULL,NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,datetime('now','localtime'));
INSERT INTO current_values VALUES(4,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'unknown',NULL,NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,datetime('now','localtime'));
INSERT INTO current_values VALUES(5,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'unknown',NULL,NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,datetime('now','localtime'));

-- ──────────────────────────────────────────────
-- javascript_functions (유용한 2개)
-- ──────────────────────────────────────────────
INSERT INTO javascript_functions VALUES(1,1,'average',NULL,'다수 값의 평균 계산','function average(...values) { return values.reduce((a,b)=>a+b,0)/values.length; }','math','[{"name":"values","type":"number[]","required":true}]','number',0,NULL,NULL,NULL,1,0,datetime('now','localtime'),datetime('now','localtime'),NULL);
INSERT INTO javascript_functions VALUES(2,1,'oeeCalculation',NULL,'OEE(종합설비효율) 계산','function oeeCalculation(availability, performance, quality) { return (availability/100)*(performance/100)*(quality/100)*100; }','engineering','[{"name":"availability","type":"number"},{"name":"performance","type":"number"},{"name":"quality","type":"number"}]','number',0,NULL,NULL,NULL,1,0,datetime('now','localtime'),datetime('now','localtime'),NULL);

-- ──────────────────────────────────────────────
-- virtual_points (사이트 레벨 2개)
-- ──────────────────────────────────────────────
INSERT INTO virtual_points VALUES(
    1,1,'site',1,NULL,'Production_Efficiency','생산 효율 계산 (샘플)',
    'const production = getValue("Production_Count"); return production > 0 ? (production / 20000) * 100 : 0;',
    'float','%',5000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,
    0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,
    datetime('now','localtime'),datetime('now','localtime')
);
INSERT INTO virtual_points VALUES(
    2,1,'site',1,NULL,'Overall_Equipment_Effectiveness','OEE 종합설비효율 계산 (샘플)',
    'const availability = 95; const performance = 90; const quality = 98; return (availability * performance * quality) / 10000;',
    'float','%',15000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,
    0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,
    datetime('now','localtime'),datetime('now','localtime')
);

-- ──────────────────────────────────────────────
-- alarm_rules (HMI-001 WLS 4개)
-- ──────────────────────────────────────────────
INSERT INTO alarm_rules VALUES(1,1,'WLS.SRS 상태 변화',NULL,'data_point',2,NULL,'digital',NULL,NULL,NULL,NULL,0.0,0.0,'on_change',NULL,NULL,NULL,NULL,'medium',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,datetime('now','localtime'),datetime('now','localtime'),NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT INTO alarm_rules VALUES(2,1,'WLS.SCS 상태 변화',NULL,'data_point',3,NULL,'digital',NULL,NULL,NULL,NULL,0.0,0.0,'on_change',NULL,NULL,NULL,NULL,'medium',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,datetime('now','localtime'),datetime('now','localtime'),NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT INTO alarm_rules VALUES(3,1,'WLS.SSS 범위 초과',NULL,'data_point',4,NULL,'analog',NULL,100.0,0.0,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,datetime('now','localtime'),datetime('now','localtime'),NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT INTO alarm_rules VALUES(4,1,'WLS.SBV 배터리 저전압',NULL,'data_point',5,NULL,'analog',NULL,15.0,10.0,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,datetime('now','localtime'),datetime('now','localtime'),NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);

-- ──────────────────────────────────────────────
-- payload_templates (샘플 1개)
-- ──────────────────────────────────────────────
INSERT INTO payload_templates (id,tenant_id,name,system_type,description,template_json,is_active,created_at,updated_at)
VALUES(1,1,'기본 HTTP 템플릿','generic','범용 HTTP JSON 페이로드 샘플',
    '[{"point_name":"{{nm}}","value":"{{vl}}","timestamp":"{{tm}}","status":"{{st}}","alarm":"{{al}}"}]',
    1,datetime('now','localtime'),datetime('now','localtime')
);

-- ──────────────────────────────────────────────
-- export_profiles
-- ──────────────────────────────────────────────
INSERT INTO export_profiles (id,tenant_id,name,is_enabled,created_at,updated_at,created_by,point_count,data_points)
VALUES(1,1,'HMI-001 샘플 프로파일',1,datetime('now','localtime'),datetime('now','localtime'),NULL,5,
    '[{"id":1,"name":"WLS.PV","device":"HMI-001","address":100,"scale":1,"offset":0,"target_field_name":"DEVICE_1.POINT:WLS.PV"},{"id":2,"name":"WLS.SRS","device":"HMI-001","address":101,"scale":1,"offset":0,"target_field_name":"DEVICE_1.POINT:WLS.SRS"},{"id":3,"name":"WLS.SCS","device":"HMI-001","address":102,"scale":1,"offset":0,"target_field_name":"DEVICE_1.POINT:WLS.SCS"},{"id":4,"name":"WLS.SSS","device":"HMI-001","address":200,"scale":1,"offset":0,"target_field_name":"DEVICE_1.POINT:WLS.SSS"},{"id":5,"name":"WLS.SBV","device":"HMI-001","address":201,"scale":1,"offset":0,"target_field_name":"DEVICE_1.POINT:WLS.SBV"}]'
);

-- ──────────────────────────────────────────────
-- export_targets (샘플 더미 URL/키 — 실제 운영 키 없음)
-- ──────────────────────────────────────────────
INSERT INTO export_targets (id,tenant_id,name,target_type,is_enabled,config,export_mode,export_interval,batch_size,execution_delay_ms,created_at,updated_at)
VALUES(1,1,'샘플 HTTP API','HTTP',0,
    '[{"url":"https://api.example.com/v1/data","method":"POST","headers":{"Content-Type":"application/json","x-api-key":"YOUR_API_KEY_HERE"},"timeout":10000,"retry":3,"retryDelay":2000,"execution_order":1}]',
    'on_change',0,100,0,datetime('now','localtime'),datetime('now','localtime')
);
INSERT INTO export_targets (id,tenant_id,name,target_type,is_enabled,config,export_mode,export_interval,batch_size,execution_delay_ms,created_at,updated_at)
VALUES(2,1,'샘플 S3 버킷','S3',0,
    '[{"bucket_name":"your-bucket-name","AccessKeyID":"YOUR_ACCESS_KEY_ID","SecretAccessKey":"YOUR_SECRET_ACCESS_KEY","region":"ap-northeast-2","Folder":"pulseone/data","execution_order":2}]',
    'on_change',0,100,0,datetime('now','localtime'),datetime('now','localtime')
);

-- ──────────────────────────────────────────────
-- export_profile_assignments
-- ──────────────────────────────────────────────
INSERT INTO export_profile_assignments (id,profile_id,gateway_id,is_active,assigned_at) VALUES(1,1,2,1,datetime('now','localtime'));

-- ──────────────────────────────────────────────
-- sqlite_sequence: 자동 삽입된 시퀀스를 정리 후 명시적 값으로 설정
-- ──────────────────────────────────────────────
DELETE FROM sqlite_sequence;
INSERT INTO sqlite_sequence VALUES('schema_versions',1);
INSERT INTO sqlite_sequence VALUES('tenants',1);
INSERT INTO sqlite_sequence VALUES('sites',1);
INSERT INTO sqlite_sequence VALUES('edge_servers',2);
INSERT INTO sqlite_sequence VALUES('manufacturers',5);
INSERT INTO sqlite_sequence VALUES('device_models',4);
INSERT INTO sqlite_sequence VALUES('template_devices',2);
INSERT INTO sqlite_sequence VALUES('template_data_points',16);
INSERT INTO sqlite_sequence VALUES('protocols',13);
INSERT INTO sqlite_sequence VALUES('devices',2);
INSERT INTO sqlite_sequence VALUES('data_points',5);
INSERT INTO sqlite_sequence VALUES('alarm_rules',4);
INSERT INTO sqlite_sequence VALUES('virtual_points',2);
INSERT INTO sqlite_sequence VALUES('javascript_functions',2);
INSERT INTO sqlite_sequence VALUES('payload_templates',1);
INSERT INTO sqlite_sequence VALUES('export_profiles',1);
INSERT INTO sqlite_sequence VALUES('export_targets',2);
INSERT INTO sqlite_sequence VALUES('export_profile_assignments',1);
INSERT INTO sqlite_sequence VALUES('users',3);
INSERT INTO sqlite_sequence VALUES('system_settings',8);