
-- devices: endpoint=host:port, config={"slave_id":N} 형태 (백업 DB 기준 포맷)

-- Table: schema_versions
-- Table: tenants
-- Table: users
-- Table: sites
-- Table: edge_servers
-- Table: system_settings
-- Table: protocols
-- Table: manufacturers
-- Table: device_models
-- Table: template_devices
-- Table: template_device_settings
-- Table: template_data_points
-- Table: protocol_instances
-- Table: devices
-- Table: device_settings
-- Table: device_status
-- Table: data_points
-- Table: current_values
-- Table: alarm_rules
-- Table: alarm_occurrences
-- Table: javascript_functions
-- Table: virtual_points
-- Table: system_logs
-- Table: export_profiles
-- Table: export_profile_points
-- Table: export_targets
-- Table: export_target_mappings
-- Table: export_profile_assignments
-- Table: permissions
-- Table: roles
-- Table: role_permissions
-- Table: backups
-- Table: sqlite_sequence
-- Table: schema_versions
-- Table: system_settings
-- Table: tenants
-- Table: roles
-- Table: permissions
-- Table: role_permissions
-- Table: users
-- Table: sites
-- Table: edge_servers
-- Table: manufacturers
-- Table: device_models
-- Table: protocols
-- Table: protocol_instances
-- Table: template_devices
-- Table: template_device_settings
-- Table: template_data_points
-- Table: devices
-- Table: device_settings
-- Table: device_status
-- Table: data_points
-- Table: current_values
-- Table: javascript_functions
-- Table: virtual_points
-- Table: alarm_rules
-- Table: alarm_occurrences
-- Table: payload_templates
-- Table: export_profiles
-- Table: export_profile_points
-- Table: export_targets
-- Table: export_target_mappings
-- Table: export_profile_assignments
-- Table: export_logs
-- Table: system_logs
-- Table: backups
-- Table: sqlite_sequence
-- Table: schema_versions
INSERT INTO schema_versions VALUES(1,'2.1.0','2026-01-29 07:52:15','Complete PulseOne v2.1.0 schema - C++ SQLQueries.h compatible');
INSERT INTO schema_versions VALUES(2,'2.1.0','2026-01-29 07:55:48','Complete PulseOne v2.1.0 schema - C++ SQLQueries.h compatible');
INSERT INTO schema_versions VALUES(3,'2.1.0','2026-01-29 09:38:43','Complete PulseOne v2.1.0 schema - C++ SQLQueries.h compatible');
-- Table: system_settings
INSERT INTO system_settings VALUES(1,'backup.auto_enabled','false',NULL,'general','string',0,0,NULL,NULL,NULL,NULL,1,1770072789924);
INSERT INTO system_settings VALUES(2,'backup.schedule_time','02:00',NULL,'general','string',0,0,NULL,NULL,NULL,NULL,1,1770072789931);
INSERT INTO system_settings VALUES(3,'backup.retention_days','3130',NULL,'general','string',0,0,NULL,NULL,NULL,NULL,1,1770072789931);
INSERT INTO system_settings VALUES(4,'backup.include_logs','true',NULL,'general','string',0,0,NULL,NULL,NULL,NULL,1,1770072789931);
-- Table: tenants
INSERT INTO tenants VALUES(1,'Smart Factory Korea','SFK001','smartfactory.pulseone.io','Factory Manager','manager@smartfactory.co.kr','+82-2-1234-5678','professional','active',10,10000,50,'monthly',NULL,NULL,NULL,1,'UTC','USD','en','2026-01-29 07:52:15','2026-01-29 07:52:15',0);
INSERT INTO tenants VALUES(2,'Global Manufacturing Inc','GMI002','global-mfg.pulseone.io','Operations Director','ops@globalmfg.com','+1-555-0123','enterprise','active',50,100000,200,'monthly',NULL,NULL,NULL,1,'UTC','USD','en','2026-01-29 07:52:15','2026-01-29 07:52:15',0);
INSERT INTO tenants VALUES(3,'Demo Corporation','DEMO','demo.pulseone.io','Demo Manager','demo@pulseone.com','+82-10-0000-0000','starter','trial',3,1000,10,'monthly',NULL,NULL,NULL,1,'UTC','USD','en','2026-01-29 07:52:15','2026-01-29 07:52:15',0);
INSERT INTO tenants VALUES(4,'Test Factory Ltd','TEST','test.pulseone.io','Test Engineer','test@testfactory.com','+82-31-9999-8888','professional','active',5,5000,25,'monthly',NULL,NULL,NULL,1,'UTC','USD','en','2026-01-29 07:52:15','2026-01-29 07:52:15',0);
-- Table: roles
INSERT INTO roles VALUES('system_admin','시스템 관리자','시스템 전체 리소스 및 모든 테넌트에 대한 전체 제어 권한',1,'2026-02-02 12:53:10','2026-02-02 12:53:10');
INSERT INTO roles VALUES('company_admin','고객사 관리자','해당 테넌트(고객사) 내의 모든 리소스 및 사용자 관리 권한',1,'2026-02-02 12:53:10','2026-02-02 12:53:10');
INSERT INTO roles VALUES('manager','운영 관리자','현장 운영 및 사용자 관리를 포함한 일반적인 관리 권한',1,'2026-02-02 12:53:10','2026-02-02 12:53:10');
INSERT INTO roles VALUES('engineer','엔지니어','디바이스 설정 및 알람 규칙 관리 등 기술적인 운영 권한',1,'2026-02-02 12:53:10','2026-02-02 12:53:10');
INSERT INTO roles VALUES('operator','운전원','실시간 모니터링 및 디바이스 상태 확인 권한',1,'2026-02-02 12:53:10','2026-02-02 12:53:10');
INSERT INTO roles VALUES('viewer','조회자','데이터 및 보고서 조회만 가능한 제한된 권한',1,'2026-02-02 12:53:10','2026-02-02 12:53:10');
-- Table: permissions
INSERT INTO permissions VALUES('view_dashboard','대시보드 조회','시스템 현황 대시보드를 조회할 수 있는 권한','조회','dashboard','["read"]',1,'2026-02-02 12:53:10');
INSERT INTO permissions VALUES('manage_devices','디바이스 관리','디바이스 등록, 수정, 삭제 및 제어 권한','관리','devices','["read", "write", "delete"]',1,'2026-02-02 12:53:10');
INSERT INTO permissions VALUES('manage_alarms','알람 관리','알람 규칙 설정 및 알람 이력 관리 권한','관리','alarms','["read", "write", "delete"]',1,'2026-02-02 12:53:10');
INSERT INTO permissions VALUES('manage_users','사용자 관리','사용자 계정 생성, 수정, 삭제 및 권한 할당 권한','관리','users','["read", "write", "delete"]',1,'2026-02-02 12:53:10');
INSERT INTO permissions VALUES('view_reports','보고서 조회','수집 데이터 보고서 및 통계 자료 조회 권한','조회','reports','["read"]',1,'2026-02-02 12:53:10');
INSERT INTO permissions VALUES('export_data','데이터 내보내기','수집 데이터를 외부 파일(CSV, Excel 등)로 추출하는 권한','데이터','data','["read", "execute"]',1,'2026-02-02 12:53:10');
INSERT INTO permissions VALUES('system_settings','시스템 설정','시스템 환경 설정 및 네트워크 설정 변경 권한','시스템','settings','["read", "write"]',1,'2026-02-02 12:53:10');
INSERT INTO permissions VALUES('backup_restore','백업/복원','데이터베이스 백업 생성 및 시스템 복원 권한','시스템','backup','["read", "execute"]',1,'2026-02-02 12:53:10');
INSERT INTO permissions VALUES('manage_tenants','고객사 관리','테넌트(고객사) 정보 및 라이선스 관리 권한','시스템','tenants','["read", "write", "delete"]',1,'2026-02-02 12:53:10');
INSERT INTO permissions VALUES('manage_roles','권한 관리','시스템 역할(Role) 및 세부 권한 정의 권한','시스템','permissions','["read", "write", "delete"]',1,'2026-02-02 12:53:10');
-- Table: role_permissions
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
-- Table: users
INSERT INTO users VALUES(1,NULL,'admin','admin@pulseone.com','$2a$10$IjD584sBOhIlAH76kcc68eq6x9S5qEsVDHqrr1Z.q5AC93HkT6bvO','Administrator',NULL,NULL,NULL,NULL,NULL,NULL,'system_admin',NULL,NULL,NULL,NULL,0,NULL,NULL,NULL,NULL,0,1,0,0,NULL,NULL,0,'UTC','ko','light',NULL,NULL,'2026-01-29 23:05:10','2026-01-29 23:05:10',NULL,0);
INSERT INTO users VALUES(2,1,'sfk_admin','admin@smartfactory.co.kr','$2a$10$IjD584sBOhIlAH76kcc68eq6x9S5qEsVDHqrr1Z.q5AC93HkT6bvO','강지훈',NULL,NULL,NULL,'관리팀','팀장',NULL,'company_admin',NULL,NULL,NULL,NULL,0,NULL,NULL,NULL,NULL,0,1,0,0,NULL,NULL,0,'UTC','en','light',NULL,NULL,'2026-02-02 12:51:20','2026-02-02 12:51:20',NULL,0);
INSERT INTO users VALUES(3,1,'sfk_engineer','engineer1@smartfactory.co.kr','$2a$10$IjD584sBOhIlAH76kcc68eq6x9S5qEsVDHqrr1Z.q5AC93HkT6bvO','이동욱',NULL,NULL,NULL,'기술지원팀','대리',NULL,'engineer',NULL,NULL,NULL,NULL,0,NULL,NULL,NULL,NULL,0,1,0,0,NULL,NULL,0,'UTC','en','light',NULL,NULL,'2026-02-02 12:51:20','2026-02-02 12:51:20',NULL,0);
INSERT INTO users VALUES(4,1,'sfk_operator','operator1@smartfactory.co.kr','$2a$10$IjD584sBOhIlAH76kcc68eq6x9S5qEsVDHqrr1Z.q5AC93HkT6bvO','김철수',NULL,NULL,NULL,'생산1팀','사원',NULL,'operator',NULL,NULL,NULL,NULL,0,NULL,NULL,NULL,NULL,0,1,0,0,NULL,NULL,0,'UTC','en','light',NULL,NULL,'2026-02-02 12:51:20','2026-02-02 12:51:20',NULL,0);
INSERT INTO users VALUES(5,1,'sfk_viewer','viewer1@smartfactory.co.kr','$2a$10$IjD584sBOhIlAH76kcc68eq6x9S5qEsVDHqrr1Z.q5AC93HkT6bvO','박영희',NULL,NULL,NULL,'기능관리팀','사원',NULL,'viewer',NULL,NULL,NULL,NULL,0,NULL,NULL,NULL,NULL,0,1,0,0,NULL,NULL,0,'UTC','en','light',NULL,NULL,'2026-02-02 12:51:20','2026-02-02 12:51:20',NULL,0);
INSERT INTO users VALUES(7,2,'gmi_engineer','eng@globalmfg.com','$2b$10$EpjXWzO2yzcaEUPPP5PRLOJHWLgH6Cq7.V/vV5V6V7V8V9V0V1V2V','Jane Doe',NULL,NULL,NULL,'Maintenance','Senior Engineer',NULL,'engineer',NULL,NULL,NULL,NULL,0,NULL,NULL,NULL,NULL,0,1,0,0,NULL,NULL,0,'UTC','en','light',NULL,NULL,'2026-02-02 12:51:20','2026-02-02 12:51:20',NULL,0);
-- Table: sites
INSERT INTO sites VALUES(1,1,NULL,'Seoul Main Factory','SMF001','factory','Main manufacturing facility','Seoul Industrial Complex',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO sites VALUES(2,1,NULL,'Busan Secondary Plant','BSP002','factory','Secondary production facility','Busan Industrial Park',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO sites VALUES(3,2,NULL,'New York Plant','NYP003','factory','East Coast Manufacturing Plant','New York Industrial Zone',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO sites VALUES(4,2,NULL,'Detroit Automotive Plant','DAP004','factory','Automotive Manufacturing Plant','Detroit, MI',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO sites VALUES(5,3,NULL,'Demo Factory','DEMO005','factory','Demonstration facility','Demo Location',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO sites VALUES(6,4,NULL,'Test Facility','TEST006','factory','Testing and R&D facility','Test Location',NULL,NULL,NULL,NULL,NULL,NULL,'UTC','USD','en',NULL,NULL,NULL,NULL,NULL,NULL,'MON-FRI',NULL,NULL,NULL,NULL,0,NULL,0,1,0,1,1,NULL,NULL,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
-- Table: edge_servers
INSERT INTO edge_servers VALUES(1,1,'Main Collector','collector',NULL,'Seoul Main Factory',NULL,'collector',NULL,NULL,8501,NULL,'docker-collector-1:70a33d',NULL,NULL,'active','2026-02-06 04:32:31','2026-02-06 04:32:31','2.1.0',0.0,0.0,0.0,0,NULL,NULL,1,1,'2026-01-29 07:55:48','2026-01-29 07:55:48',0,1,100,1000,'all',1);
INSERT INTO edge_servers VALUES(2,2,'NY Collector','collector',NULL,'New York Plant',NULL,'10.0.1.5',NULL,NULL,8502,NULL,NULL,NULL,NULL,'active',NULL,NULL,'2.1.0',0.0,0.0,0.0,0,NULL,NULL,1,1,'2026-01-29 07:55:48','2026-01-29 07:55:48',0,3,100,1000,'all',1);
INSERT INTO edge_servers VALUES(3,3,'Demo Collector','collector',NULL,'Demo Factory',NULL,'192.168.100.20',NULL,NULL,8503,NULL,NULL,NULL,NULL,'active',NULL,NULL,'2.1.0',0.0,0.0,0.0,0,NULL,NULL,1,1,'2026-01-29 07:55:48','2026-01-29 07:55:48',0,5,100,1000,'all',1);
INSERT INTO edge_servers VALUES(4,4,'Test Collector','collector',NULL,'Test Facility',NULL,'192.168.200.20',NULL,NULL,8504,NULL,NULL,NULL,NULL,'active',NULL,NULL,'2.1.0',0.0,0.0,0.0,0,NULL,NULL,1,1,'2026-01-29 07:55:48','2026-01-29 07:55:48',0,6,100,1000,'all',1);
INSERT INTO edge_servers VALUES(6,1,'Insite Gateway','gateway','',NULL,NULL,'127.0.0.1',NULL,NULL,8080,NULL,NULL,NULL,NULL,'active',NULL,NULL,NULL,0.0,0.0,0.0,0,'{"target_priorities":{"18":3,"19":1}}',NULL,1,1,'2026-02-06 04:01:25','2026-02-06 04:07:55',0,2,100,1000,'selective',1);
INSERT INTO edge_servers VALUES(7,1,'VerificationGateway','gateway',NULL,NULL,NULL,'127.0.0.1',NULL,NULL,8080,NULL,NULL,NULL,NULL,'pending',NULL,NULL,NULL,0.0,0.0,0.0,0,'{}',NULL,1,1,'2026-01-31 03:03:09','2026-01-31 03:03:13',1,NULL,100,1000,'all',1);
INSERT INTO edge_servers VALUES(12,1,'insite 운영 게이트웨이','gateway','',NULL,NULL,'127.0.0.1',NULL,NULL,8080,NULL,NULL,NULL,NULL,'active','2026-02-06 04:32:33',NULL,NULL,0.0,0.0,0.0,0,'{"target_priorities":{"10":3,"21":1}}',NULL,1,1,'2026-02-06 04:01:07','2026-02-06 04:08:26',0,2,100,1000,'selective',1);
-- Table: manufacturers
INSERT INTO manufacturers VALUES(1,'LS Electric','Global total solution provider in electric power and automation','South Korea','https://www.lselectric.co.kr',NULL,1,0,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO manufacturers VALUES(2,'Siemens','German multinational technology conglomerate','Germany','https://www.siemens.com',NULL,1,0,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO manufacturers VALUES(3,'Schneider Electric','French multinational company specializing in digital automation and energy management','France','https://www.se.com',NULL,1,0,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO manufacturers VALUES(4,'ABB','Swedish-Swiss multinational corporation operating mainly in robotics, power, heavy electrical equipment, and automation technology','Switzerland','https://new.abb.com',NULL,1,0,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO manufacturers VALUES(5,'Delta Electronics','Taiwanese electronics manufacturing company','Taiwan','https://www.deltaww.com',NULL,1,0,'2026-01-29 07:55:48','2026-01-29 07:55:48');
-- Table: device_models
INSERT INTO device_models VALUES(1,1,'XGT Series','XGK-CPUS','PLC','LS Electric XGT Series Programmable Logic Controller',NULL,NULL,NULL,1,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO device_models VALUES(2,1,'iS7 Inverter','SV-iS7','INVERTER','LS Electric iS7 Series High Performance Inverter',NULL,NULL,NULL,1,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO device_models VALUES(3,2,'S7-1200','CPU 1214C','PLC','Siemens SIMATIC S7-1200 Compact Controller',NULL,NULL,NULL,1,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO device_models VALUES(4,3,'PowerLogic PM8000','METSEPM8000','METER','Schneider Electric PowerLogic PM8000 series power meter',NULL,NULL,NULL,1,'2026-01-29 07:55:48','2026-01-29 07:55:48');
-- Table: protocols
INSERT INTO protocols VALUES(1,'MODBUS_TCP','Modbus TCP/IP','Industrial protocol over Ethernet',502,0,0,'["read_coils", "read_discrete_inputs", "read_holding_registers", "read_input_registers", "write_single_coil", "write_single_register", "write_multiple_coils", "write_multiple_registers"]','["boolean", "int16", "uint16", "int32", "uint32", "float32"]','{"slave_id": {"type": "integer", "default": 1, "min": 1, "max": 247}, "timeout_ms": {"type": "integer", "default": 3000}, "byte_order": {"type": "string", "default": "big_endian"}}','{}',1000,3000,10,1,0,'1.0','industrial','Modbus Organization','Modbus Application Protocol V1.1b3','2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO protocols VALUES(2,'BACNET_IP','BACnet/IP','Building automation and control networks (BACnet/IP)',47808,0,0,'["read_property", "write_property", "read_property_multiple", "who_is", "i_am", "subscribe_cov"]','["boolean", "int32", "uint32", "float32", "string", "enumerated", "bitstring"]','{"device_id": {"type": "integer", "default": 1001}, "network": {"type": "integer", "default": 1}, "max_apdu": {"type": "integer", "default": 1476}}','{}',5000,10000,5,1,0,'1.0','building_automation','ASHRAE','ANSI/ASHRAE 135-2020','2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO protocols VALUES(3,'MQTT','MQTT v3.1.1','Lightweight messaging protocol for IoT',1883,0,1,'["publish", "subscribe", "unsubscribe", "ping", "connect", "disconnect"]','["boolean", "int32", "float32", "string", "json", "binary"]','{"client_id":{"type":"string","default":"pulseone_client"},"keep_alive":{"type":"integer","default":60}}','{}',0,5000,100,1,0,'3.1.1','iot','MQTT.org','MQTT v3.1.1 OASIS Standard','2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO protocols VALUES(4,'ETHERNET_IP','EtherNet/IP','Industrial Ethernet communication protocol',44818,0,0,'["explicit_messaging", "implicit_messaging", "forward_open", "forward_close", "get_attribute_single", "set_attribute_single"]','["boolean", "int8", "int16", "int32", "uint8", "uint16", "uint32", "float32", "string"]','{"connection_type": {"type": "string", "default": "explicit"}, "assembly_instance": {"type": "integer", "default": 100}}','{}',200,1000,20,1,0,'1.0','industrial','ODVA','EtherNet/IP Specification Volume 1 & 2','2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO protocols VALUES(5,'MODBUS_RTU','Modbus RTU','Modbus over serial communication',NULL,1,0,'["read_coils", "read_discrete_inputs", "read_holding_registers", "read_input_registers", "write_single_coil", "write_single_register", "write_multiple_coils", "write_multiple_registers"]','["boolean", "int16", "uint16", "int32", "uint32", "float32"]','{"slave_id": {"type": "integer", "default": 1}, "baud_rate": {"type": "integer", "default": 9600}, "parity": {"type": "string", "default": "none"}, "data_bits": {"type": "integer", "default": 8}, "stop_bits": {"type": "integer", "default": 1}}','{}',1000,3000,1,1,0,'1.0','industrial','Modbus Organization','Modbus over Serial Line V1.02','2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO protocols VALUES(6,'OPC_UA','OPC UA','OPC Unified Architecture industrial machine-to-machine communication',4840,0,0,'["read", "write", "browse", "subscribe", "call"]','["boolean", "int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64", "float", "double", "string", "datetime"]','{"security_mode": {"type": "string", "default": "None"}, "timeout_ms": {"type": "integer", "default": 5000}, "publish_interval_ms": {"type": "integer", "default": 500}}','{}',1000,5000,100,1,0,'1.04','industrial','OPC Foundation','OPC Unified Architecture Specification','2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO protocols VALUES(7,'ROS_BRIDGE','ROS Bridge','Robot Operating System via rosbridge WebSocket',9090,0,0,'["subscribe", "publish", "call_service"]','["boolean", "int32", "float32", "float64", "string", "json"]','{"rosbridge_port": {"type": "integer", "default": 9090}}','{}',1000,5000,50,1,0,'2.0','industrial','Open Robotics','ROS Bridge Protocol','2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO protocols VALUES(8,'HTTP_REST','HTTP/REST','HTTP REST API data collection',80,0,0,'["GET", "POST", "PUT", "DELETE"]','["boolean", "int32", "float32", "float64", "string", "json"]','{"method": {"type": "string", "default": "GET"}, "headers": {"type": "object", "default": {}}, "poll_interval_ms": {"type": "integer", "default": 5000}}','{}',0,10000,20,1,0,'1.1','iot','IETF','HTTP/1.1 RFC 7230','2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO protocols VALUES(13,'BLE_BEACON','BLE Beacon','Bluetooth Low Energy Beaconing',NULL,0,0,'["read"]','["BOOL", "INT32", "FLOAT32", "STRING"]','{"uuid": {"type": "string"}}','{}',1000,3000,100,1,0,'4.0','iot','Bluetooth SIG','Bluetooth Core Specification 4.0','2026-01-29 07:55:48','2026-01-29 07:55:48');
-- Table: protocol_instances
INSERT INTO protocol_instances VALUES(9,3,'A','','/A','344f78f665d271c8497bcc15414168b8','2026-02-02T04:23:42.594Z','{}',1,'STOPPED','2026-02-02 04:23:42','2026-02-02 04:42:41',NULL,'INTERNAL');
-- Table: template_devices
INSERT INTO template_devices VALUES(1,1,'XGT Standard Modbus TCP','Standard template for LS XGT PLC using Modbus TCP',1,'{"slave_id": 1, "byte_order": "big_endian"}',1000,3000,1,NULL,'2026-01-29 07:55:48','2026-01-29 07:55:48',0);
INSERT INTO template_devices VALUES(2,2,'iS7 Inverter Basic','Basic monitoring and control for iS7 Inverter',1,'{"slave_id": 1, "byte_order": "big_endian"}',1000,3000,1,NULL,'2026-01-29 07:55:48','2026-01-29 07:55:48',0);
INSERT INTO template_devices VALUES(3,3,'S7-1200 Standard','Standard monitoring for Siemens S7-1200',1,'{"slave_id": 1, "byte_order": "big_endian"}',1000,3000,1,NULL,'2026-01-29 07:55:48','2026-01-29 07:55:48',0);
INSERT INTO template_devices VALUES(21,3,'S7-1200 Native S7','Monitoring Siemens S7-1200 via S7 Protocol',5,'{"rack": 0, "slot": 1}',1000,3000,1,NULL,'2026-01-29 09:38:43','2026-01-29 09:38:43',0);
-- Table: template_device_settings
INSERT INTO template_device_settings VALUES(1,1000,10000,5000,5000,3);
INSERT INTO template_device_settings VALUES(2,1000,10000,5000,5000,3);
INSERT INTO template_device_settings VALUES(3,1000,10000,5000,5000,3);
INSERT INTO template_device_settings VALUES(21,1000,10000,5000,5000,3);
-- Table: template_data_points
INSERT INTO template_data_points VALUES(1,1,'CPU_Status',NULL,0,NULL,'UINT16','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(2,1,'Run_Stop',NULL,1,NULL,'BOOL','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(3,1,'Error_Code',NULL,2,NULL,'UINT16','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(4,1,'Input_W0',NULL,100,NULL,'UINT16','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(5,1,'Output_W0',NULL,200,NULL,'UINT16','read_write','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(6,2,'Frequency',NULL,1,NULL,'FLOAT32','read','Hz',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(7,2,'Output_Current',NULL,2,NULL,'FLOAT32','read','A',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(8,2,'Output_Voltage',NULL,3,NULL,'FLOAT32','read','V',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(9,2,'DC_Link_Voltage',NULL,4,NULL,'FLOAT32','read','V',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(10,2,'Status_Word',NULL,10,NULL,'UINT16','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(11,2,'Control_Word',NULL,11,NULL,'UINT16','read_write','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(12,3,'Status',NULL,0,NULL,'UINT16','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(13,3,'Run_Mode',NULL,1,NULL,'BOOL','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(14,3,'Process_Value',NULL,10,NULL,'FLOAT32','read','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(15,3,'Setpoint',NULL,12,NULL,'FLOAT32','read_write','',1.0,0,1,0,NULL,0.0,NULL,NULL);
INSERT INTO template_data_points VALUES(64,1,'MQTT Test Point',NULL,0,'sensors/temp','FLOAT32','read',NULL,1.0,0,1,0,'{"unit":"C"}',0.0,'{"qos":1,"retained":true}','val');
-- Table: devices
-- Docker 전용 디바이스는 is_active=0으로 비활성화 (Windows 베어메탈에서 연결 불가)
INSERT INTO devices VALUES(2,1,1,NULL,1,'HMI-001',NULL,'HMI','PulseOne',NULL,NULL,NULL,1,'simulator-modbus:50502','{"slave_id":1}',1000,3000,3,NULL,NULL,NULL,NULL,NULL,0,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-29 09:55:07','2026-01-29 09:55:07',NULL);
-- E2E-Device(BACnet): endpoint 포트 502(Modbus용) → 47808(BACnet 표준)
INSERT INTO devices VALUES(200,1,1,NULL,1,'E2E-Device',NULL,'PLC',NULL,NULL,NULL,NULL,2,'127.0.0.1:47808','{}',1000,3000,3,NULL,NULL,NULL,NULL,NULL,1,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-01-31 12:38:16','2026-01-31 12:38:16',NULL);
-- MQTT, insite 운영 테스트: Docker 내부 호스트 사용으로 비활성화
INSERT INTO devices VALUES(300,1,1,NULL,1,'MQTT-Test-Device',NULL,'SENSOR',NULL,NULL,NULL,NULL,3,'tcp://rabbitmq:1883','{"topic":"vfd/#"}',1000,3000,3,NULL,NULL,NULL,NULL,NULL,0,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-02-04 03:30:01','2026-02-04 03:30:01',9);
INSERT INTO devices VALUES(12346,1,2,NULL,1,'insite 운영 테스트',NULL,'PLC','LS Electric',NULL,NULL,NULL,1,'172.18.0.9:50502','{"slave_id":1}',1000,3000,3,NULL,NULL,NULL,NULL,NULL,0,0,0,100,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2026-02-06 04:03:15','2026-02-06 04:03:15',NULL);
-- Table: device_settings
INSERT INTO device_settings VALUES(2,1000,NULL,1,10000,3000,5000,10,3,5000,1.5,60000,300000,1,30,10,1,0,1,0,1,0,0,0,1024,1024,100,'2026-01-29 09:50:46','2026-01-29 09:50:46',NULL);
INSERT INTO device_settings VALUES(300,1000,NULL,1,10000,5000,5000,10,3,5000,1.5,60000,300000,1,30,10,1,0,1,0,1,0,0,1,1024,1024,100,'2026-02-04 04:11:44','2026-02-04 04:11:44',NULL);
-- Table: device_status
INSERT INTO device_status VALUES(2,'connected','2026-02-06 04:32:41','2026-01-29 05:52:15',0,NULL,NULL,0,10,12,78,1.800000000000000044,140997,140997,0,99.5,'V2.1.4','{"screen": "12inch", "memory": "256MB"}','{"display": "active"}',18.69999999999999929,45.20000000000000285,'2026-02-06 04:32:41');
INSERT INTO device_status VALUES(200,'connected','2026-01-31 12:26:55',NULL,0,NULL,NULL,0,6,NULL,NULL,0.0,4426,4426,0,0.0,NULL,NULL,NULL,NULL,NULL,'2026-01-31 12:26:55');
INSERT INTO device_status VALUES(300,'connected','2026-02-05 07:37:19',NULL,0,NULL,NULL,0,0,NULL,NULL,0.0,10,10,0,0.0,NULL,NULL,NULL,NULL,NULL,'2026-02-05 07:37:19');
INSERT INTO device_status VALUES(12346,'connected','2026-02-06 04:32:40',NULL,0,NULL,NULL,0,4,NULL,NULL,0.0,546,546,0,0.0,NULL,NULL,NULL,NULL,NULL,'2026-02-06 04:32:40');
-- Table: data_points
INSERT INTO data_points VALUES(1,2,'WLS.PV',NULL,100,'CO:100','value','BOOL','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-29 09:55:07','2026-01-29 09:55:07',0,'medium');
INSERT INTO data_points VALUES(2,2,'WLS.SRS',NULL,101,'CO:101','data','BOOL','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-29 09:55:07','2026-01-29 09:55:07',0,'medium');
INSERT INTO data_points VALUES(3,2,'WLS.SCS',NULL,102,'CO:102',NULL,'BOOL','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-29 09:55:07','2026-01-29 09:55:07',0,'medium');
INSERT INTO data_points VALUES(4,2,'WLS.SSS',NULL,200,'HR:200',NULL,'UINT16','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-29 09:55:07','2026-01-29 09:55:07',0,'medium');
INSERT INTO data_points VALUES(5,2,'WLS.SBV',NULL,201,'HR:201',NULL,'UINT16','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-29 09:55:07','2026-01-29 09:55:07',0,'medium');
INSERT INTO data_points VALUES(2001,200,'E2E-Point',NULL,1,NULL,NULL,'FLOAT32','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,0,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-31 12:38:16','2026-01-31 12:38:16',0,'medium');
INSERT INTO data_points VALUES(2008,12346,'fire',NULL,100,NULL,NULL,'FLOAT32','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,0,1,1,0.0,NULL,NULL,NULL,NULL,0,NULL,NULL,0.0,'standard',1,'2026-02-06 04:03:18','2026-02-06 04:03:18',0,'medium');
-- Table: current_values
INSERT INTO current_values VALUES(1,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'good','2026-02-06T04:32:41.118Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.119Z');
INSERT INTO current_values VALUES(2,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'good','2026-02-06T04:32:41.121Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.121Z');
INSERT INTO current_values VALUES(3,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'good','2026-02-06T04:32:41.123Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.123Z');
INSERT INTO current_values VALUES(4,'{"value":80.0}','{"value":80.0}',NULL,'double',1,'good','2026-02-06T04:32:41.125Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.125Z');
INSERT INTO current_values VALUES(5,'{"value":360.0}','{"value":360.0}',NULL,'double',1,'good','2026-02-06T04:32:41.128Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.128Z');
INSERT INTO current_values VALUES(2008,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'good','2026-02-06T04:32:40.500Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:40.501Z');
-- Table: javascript_functions
INSERT INTO javascript_functions VALUES(1,1,'average',NULL,'Calculate average of values','function average(...values) { return values.reduce((a, b) => a + b, 0) / values.length; }','math','[{"name": "values", "type": "number[]", "required": true}]','number',0,NULL,NULL,NULL,1,0,'2026-01-29 07:52:15','2026-01-29 07:52:15',NULL);
INSERT INTO javascript_functions VALUES(2,1,'oeeCalculation',NULL,'Calculate Overall Equipment Effectiveness','function oeeCalculation(availability, performance, quality) { return (availability / 100) * (performance / 100) * (quality / 100) * 100; }','engineering','[{"name": "availability", "type": "number"}, {"name": "performance", "type": "number"}, {"name": "quality", "type": "number"}]','number',0,NULL,NULL,NULL,1,0,'2026-01-29 07:52:15','2026-01-29 07:52:15',NULL);
INSERT INTO javascript_functions VALUES(3,1,'powerFactorCorrection',NULL,'Calculate power factor correction','function powerFactorCorrection(activePower, reactivePower) { return activePower / Math.sqrt(activePower * activePower + reactivePower * reactivePower); }','electrical','[{"name": "activePower", "type": "number"}, {"name": "reactivePower", "type": "number"}]','number',0,NULL,NULL,NULL,1,0,'2026-01-29 07:52:15','2026-01-29 07:52:15',NULL);
INSERT INTO javascript_functions VALUES(4,1,'productionEfficiency',NULL,'Calculate production efficiency for automotive line','function productionEfficiency(actual, target, hours) { return (actual / target) * 100; }','custom','[{"name": "actual", "type": "number"}, {"name": "target", "type": "number"}, {"name": "hours", "type": "number"}]','number',0,NULL,NULL,NULL,1,0,'2026-01-29 07:52:15','2026-01-29 07:52:15',NULL);
INSERT INTO javascript_functions VALUES(5,1,'energyIntensity',NULL,'Calculate energy intensity per unit','function energyIntensity(totalEnergy, productionCount) { return productionCount > 0 ? totalEnergy / productionCount : 0; }','custom','[{"name": "totalEnergy", "type": "number"}, {"name": "productionCount", "type": "number"}]','number',0,NULL,NULL,NULL,1,0,'2026-01-29 07:52:15','2026-01-29 07:52:15',NULL);
-- Table: virtual_points
INSERT INTO virtual_points VALUES(1,1,'site',1,NULL,'Production_Efficiency','Overall production efficiency calculation','const production = getValue("Production_Count"); return (production / 20000) * 100;','float','%',5000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO virtual_points VALUES(2,1,'site',1,NULL,'Energy_Per_Unit','Energy consumption per unit','const power = 847.5; const production = getValue("Production_Count"); return power / production;','float','kW/unit',10000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO virtual_points VALUES(3,1,'site',1,NULL,'Overall_Equipment_Effectiveness','OEE calculation for production line','const availability = 95; const performance = getValue("Line_Speed") / 50 * 100; const quality = 98; return (availability * performance * quality) / 10000;','float','%',15000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO virtual_points VALUES(4,2,'site',3,NULL,'Assembly_Throughput','Assembly line throughput','const cycleTime = 45; return 3600 / cycleTime;','float','units/hour',5000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO virtual_points VALUES(5,3,'site',5,NULL,'Demo_Performance','Demo performance index','return Math.sin(Date.now() / 10000) * 50 + 75;','float','%',2000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO virtual_points VALUES(6,4,'site',6,NULL,'Test_Metric','Test calculation metric','return Math.random() * 100;','float','%',3000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:52:15','2026-01-29 07:52:15');
INSERT INTO virtual_points VALUES(7,1,'site',1,NULL,'Production_Efficiency','Overall production efficiency calculation','const production = getValue("Production_Count"); return (production / 20000) * 100;','float','%',5000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO virtual_points VALUES(8,1,'site',1,NULL,'Energy_Per_Unit','Energy consumption per unit','const power = 847.5; const production = getValue("Production_Count"); return power / production;','float','kW/unit',10000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO virtual_points VALUES(9,1,'site',1,NULL,'Overall_Equipment_Effectiveness','OEE calculation for production line','const availability = 95; const performance = getValue("Line_Speed") / 50 * 100; const quality = 98; return (availability * performance * quality) / 10000;','float','%',15000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO virtual_points VALUES(10,2,'site',3,NULL,'Assembly_Throughput','Assembly line throughput','const cycleTime = 45; return 3600 / cycleTime;','float','units/hour',5000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO virtual_points VALUES(11,3,'site',5,NULL,'Demo_Performance','Demo performance index','return Math.sin(Date.now() / 10000) * 50 + 75;','float','%',2000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO virtual_points VALUES(12,4,'site',6,NULL,'Test_Metric','Test calculation metric','return Math.random() * 100;','float','%',3000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 07:55:48','2026-01-29 07:55:48');
INSERT INTO virtual_points VALUES(13,1,'site',1,NULL,'Production_Efficiency','Overall production efficiency calculation','const production = getValue("Production_Count"); return (production / 20000) * 100;','float','%',5000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 09:38:43','2026-01-29 09:38:43');
INSERT INTO virtual_points VALUES(14,1,'site',1,NULL,'Energy_Per_Unit','Energy consumption per unit','const power = 847.5; const production = getValue("Production_Count"); return power / production;','float','kW/unit',10000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 09:38:43','2026-01-29 09:38:43');
INSERT INTO virtual_points VALUES(15,1,'site',1,NULL,'Overall_Equipment_Effectiveness','OEE calculation for production line','const availability = 95; const performance = getValue("Line_Speed") / 50 * 100; const quality = 98; return (availability * performance * quality) / 10000;','float','%',15000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 09:38:43','2026-01-29 09:38:43');
INSERT INTO virtual_points VALUES(16,2,'site',3,NULL,'Assembly_Throughput','Assembly line throughput','const cycleTime = 45; return 3600 / cycleTime;','float','units/hour',5000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 09:38:43','2026-01-29 09:38:43');
INSERT INTO virtual_points VALUES(17,3,'site',5,NULL,'Demo_Performance','Demo performance index','return Math.sin(Date.now() / 10000) * 50 + 75;','float','%',2000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 09:38:43','2026-01-29 09:38:43');
INSERT INTO virtual_points VALUES(18,4,'site',6,NULL,'Test_Metric','Test calculation metric','return Math.random() * 100;','float','%',3000,'timer',1,NULL,NULL,'javascript',NULL,0,'return_null',NULL,0,0.0,NULL,NULL,5000,10000,3,1,NULL,0,1,5000,0,1,0,NULL,NULL,0.0,0,NULL,'2026-01-29 09:38:43','2026-01-29 09:38:43');
-- Table: alarm_rules
INSERT INTO alarm_rules VALUES(23,1,'WLS.SRS State Change',NULL,'data_point',2,NULL,'digital',NULL,NULL,NULL,NULL,0.0,0.0,'on_change',NULL,NULL,NULL,NULL,'medium',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-29 10:38:36','2026-01-29 10:38:36',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT INTO alarm_rules VALUES(24,1,'WLS.SCS State Change',NULL,'data_point',3,NULL,'digital',NULL,NULL,NULL,NULL,0.0,0.0,'on_change',NULL,NULL,NULL,NULL,'medium',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-29 10:38:36','2026-01-29 10:38:36',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT INTO alarm_rules VALUES(25,1,'WLS.SSS Out of Range',NULL,'data_point',4,NULL,'analog',NULL,100.0,0.0,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-29 10:38:36','2026-01-29 10:38:36',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT INTO alarm_rules VALUES(26,1,'WLS.SBV Out of Range',NULL,'data_point',5,NULL,'analog',NULL,15.0,10.0,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-29 10:38:36','2026-01-29 10:38:36',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT INTO alarm_rules VALUES(200,1,'E2E_ALARM_RULE',NULL,'data_point',200,NULL,'digital',NULL,NULL,NULL,NULL,0.0,0.0,'on_true',NULL,NULL,NULL,NULL,'critical',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-31 11:06:22','2026-01-31 11:06:22',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT INTO alarm_rules VALUES(201,1,'E2E-High-Alarm',NULL,'data_point',2001,NULL,'analog',NULL,50.0,NULL,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'critical',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-31 12:38:16','2026-01-31 12:38:16',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT INTO alarm_rules VALUES(3001,1,'MQTT-Temp-High',NULL,'POINT',3001,NULL,'LIMIT_HIGH',NULL,150.0,NULL,NULL,0.0,0.0,NULL,NULL,NULL,NULL,'Temp High {{value}}','critical',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-02-04 03:31:55','2026-02-04 03:31:55',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
-- Table: alarm_occurrences
INSERT INTO alarm_occurrences VALUES(208,26,1,'2026-02-06 13:23:31','360.0','HIGH','WLS.SBV Out of Range: HIGH (Value: 360.00)','high','active',NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,0,NULL,'{}','','','2026-02-06 04:23:31','2026-02-06 04:23:31',2,5,NULL,NULL);
-- Table: payload_templates
INSERT INTO payload_templates (id, tenant_id, name, system_type, description, template_json, is_active, created_at, updated_at) VALUES(1,1,'Insite 기본 템플릿','insite','Insite 빌딩 모니터링 시스템용 기본 템플릿','[{"bd":"{{bd}}","ty":"{{ty}}","nm":"{{nm}}","vl":"{{vl}}","tm":"{{tm}}","st":"{{st}}","al":"{{al}}","des":"{{des}}","il":"{{il}}","xl":"{{xl}}","mi":"{{mi}}","mx":"{{mx}}"}]',1,'2026-01-29 07:52:15','2026-01-29 10:50:27');
-- Table: export_profiles
INSERT INTO export_profiles (id, tenant_id, name, is_enabled, created_at, updated_at, created_by, point_count, data_points) VALUES(3,1,'insite 알람셋',1,'2026-02-06 04:00:04','2026-02-06 04:00:04',NULL,0,'[{"id":1,"name":"WLS.PV","device":"HMI-001","address":100,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.PV"},{"id":2,"name":"WLS.SRS","device":"HMI-001","address":101,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SRS"},{"id":3,"name":"WLS.SCS","device":"HMI-001","address":102,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SCS"},{"id":4,"name":"WLS.SSS","device":"HMI-001","address":200,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SSS"},{"id":5,"name":"WLS.SBV","device":"HMI-001","address":201,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SBV"}]');
INSERT INTO export_profiles (id, tenant_id, name, is_enabled, created_at, updated_at, created_by, point_count, data_points) VALUES(103,1,'insite 운영 프로파일',1,'2026-02-06 03:59:17','2026-02-06 03:59:17',NULL,0,'[{"id":2008,"name":"fire","device_name":"insite 운영 테스트","site_id":20,"site_name":"System","data_type":"FLOAT32","unit":"","address":1001,"target_field_name":"FIFR_1.F_1:OP.SST","scale":1,"offset":0}]');
-- Table: export_targets
INSERT INTO export_targets (id, tenant_id, profile_id, name, target_type, is_enabled, config, template_id, export_mode, export_interval, batch_size, execution_delay_ms, created_at, updated_at) VALUES(18,3,3,'insite API Gateway(stg)','HTTP',1,'[{"url":"https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/alarm","method":"POST","headers":{"Content-Type":"application/json","x-api-key":"${INSITE_API_GATEWAY_STG_APIKEY_4732}","Authorization":"${INSITE_API_GATEWAY_STG_AUTH_5850}"},"timeout":10000,"retry":3,"retryDelay":2000,"execution_order":3}]',NULL,'on_change',0,100,0,'2026-02-06 04:03:25','2026-02-06 04:07:55');
INSERT INTO export_targets (id, tenant_id, profile_id, name, target_type, is_enabled, config, template_id, export_mode, export_interval, batch_size, execution_delay_ms, created_at, updated_at) VALUES(19,3,3,'insite S3(stg)','S3',1,'[{"bucket_name":"hdcl-csp-stg","AccessKeyID":"${INSITE_S3_STG_S3_ACCESS_8503}","SecretAccessKey":"${INSITE_S3_STG_S3_SECRET_3105}","S3ServiceUrl":"https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/alarm","region":"ap-northeast-2","Folder":"e2e-s3","execution_order":1}]',NULL,'on_change',0,100,0,'2026-02-06 04:03:32','2026-02-06 04:07:55');
-- Table: export_profile_assignments
INSERT INTO export_profile_assignments (id, profile_id, gateway_id, is_active, assigned_at) VALUES(12,3,6,1,'2026-02-06 04:04:21');
INSERT INTO export_profile_assignments (id, profile_id, gateway_id, is_active, assigned_at) VALUES(13,103,12,1,'2026-02-06 04:04:21');
-- Table: export_logs
INSERT INTO export_logs (id, log_type, tenant_id, service_id, target_id, mapping_id, point_id, source_value, converted_value, status, http_status_code, error_message, error_code, response_data, processing_time_ms, timestamp, client_info, gateway_id) VALUES(275,'alarm_export',NULL,NULL,NULL,NULL,'[{"al":1,"bd":1,"des":"⚠️ [높음] WLS.SBV Out of Range: HIGH (Value: 360.00) [위치: Location for Pt 5, 시간: 2026-02-06 13:23:31] → 신속 점검 요망","il":"","mi":[],"mx":[],"nm":"WLS.SBV","st":0,"tm":"2026-02-06 13:23:31","ty":"num","vl":360.0,"xl":""}]','','failure',403,'HTTP 403: {"message":"The request signature we calculated does not match the signature you provided. Check you...','','',51,'2026-02-06 13:23:31','',NULL,NULL);
INSERT INTO export_logs (id, log_type, tenant_id, service_id, target_id, mapping_id, point_id, source_value, converted_value, status, http_status_code, error_message, error_code, response_data, processing_time_ms, timestamp, client_info, gateway_id) VALUES(276,'alarm_export',NULL,NULL,NULL,NULL,'[{"al":1,"bd":1,"des":"⚠️ [높음] WLS.SBV Out of Range: HIGH (Value: 360.00) [위치: Location for Pt 5, 시간: 2026-02-06 13:23:31] → 신속 점검 요망","il":"","mi":[],"mx":[],"nm":"WLS.SBV","st":0,"tm":"2026-02-06 13:23:31","ty":"num","vl":360.0,"xl":""}]','','failure',403,'HTTP 403: {"message":"Forbidden"}','','',43,'2026-02-06 13:23:31','',NULL,NULL);
-- Table: system_logs
INSERT INTO system_logs VALUES(1,NULL,NULL,'INFO','database',NULL,'Complete initial data loading with C++ schema compatibility',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"tables_populated": 12, "protocols": 5, "devices": 11, "data_points": 17, "current_values": 17, "device_settings": 6, "device_status": 6, "virtual_points": 6, "alarm_rules": 7, "js_functions": 5, "schema_version": "2.1.0"}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 07:52:15');
INSERT INTO system_logs VALUES(2,NULL,NULL,'INFO','protocols',NULL,'Protocol support initialized',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"modbus_tcp": 1, "bacnet": 2, "mqtt": 3, "ethernet_ip": 4, "modbus_rtu": 5}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 07:52:15');
INSERT INTO system_logs VALUES(3,NULL,NULL,'INFO','devices',NULL,'Sample devices created with protocol_id foreign keys and complete settings',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"smart_factory": 6, "global_manufacturing": 2, "demo": 2, "test": 1, "total": 11, "all_configured": true}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 07:52:15');
INSERT INTO system_logs VALUES(4,NULL,NULL,'INFO','datapoints',NULL,'Data points created with C++ SQLQueries.h compatibility',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"total_points": 17, "log_enabled": 17, "data_types": ["BOOL", "UINT16", "UINT32", "FLOAT32"], "schema_compatible": true}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 07:52:15');
INSERT INTO system_logs VALUES(5,NULL,NULL,'INFO','database',NULL,'Complete initial data loading with C++ schema compatibility',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"tables_populated": 12, "protocols": 5, "devices": 11, "data_points": 17, "current_values": 17, "device_settings": 6, "device_status": 6, "virtual_points": 6, "alarm_rules": 7, "js_functions": 5, "schema_version": "2.1.0"}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 07:55:48');
INSERT INTO system_logs VALUES(6,NULL,NULL,'INFO','protocols',NULL,'Protocol support initialized',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"modbus_tcp": 1, "bacnet": 2, "mqtt": 3, "ethernet_ip": 4, "modbus_rtu": 5}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 07:55:48');
INSERT INTO system_logs VALUES(7,NULL,NULL,'INFO','devices',NULL,'Sample devices created with protocol_id foreign keys and complete settings',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"smart_factory": 6, "global_manufacturing": 2, "demo": 2, "test": 1, "total": 11, "all_configured": true}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 07:55:48');
INSERT INTO system_logs VALUES(8,NULL,NULL,'INFO','datapoints',NULL,'Data points created with C++ SQLQueries.h compatibility',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"total_points": 17, "log_enabled": 17, "data_types": ["BOOL", "UINT16", "UINT32", "FLOAT32"], "schema_compatible": true}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 07:55:48');
INSERT INTO system_logs VALUES(9,NULL,NULL,'INFO','database',NULL,'Complete initial data loading with C++ schema compatibility',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"tables_populated": 12, "protocols": 5, "devices": 11, "data_points": 17, "current_values": 17, "device_settings": 6, "device_status": 6, "virtual_points": 6, "alarm_rules": 7, "js_functions": 5, "schema_version": "2.1.0"}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 09:38:43');
INSERT INTO system_logs VALUES(10,NULL,NULL,'INFO','protocols',NULL,'Protocol support initialized',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"modbus_tcp": 1, "bacnet": 2, "mqtt": 3, "ethernet_ip": 4, "modbus_rtu": 5}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 09:38:43');
INSERT INTO system_logs VALUES(11,NULL,NULL,'INFO','devices',NULL,'Sample devices created with protocol_id foreign keys and complete settings',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"smart_factory": 6, "global_manufacturing": 2, "demo": 2, "test": 1, "total": 11, "all_configured": true}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 09:38:43');
INSERT INTO system_logs VALUES(12,NULL,NULL,'INFO','datapoints',NULL,'Data points created with C++ SQLQueries.h compatibility',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'{"total_points": 17, "log_enabled": 17, "data_types": ["BOOL", "UINT16", "UINT32", "FLOAT32"], "schema_compatible": true}',NULL,NULL,NULL,NULL,NULL,'2026-01-29 09:38:43');
-- Table: backups
INSERT INTO backups VALUES(1,'수동 백업 (21:47)','pulseone_20260202_214750.db','full','completed',1400000,'/app/data/backup','2026-02-02 21:47:50',NULL,'작업 전 수동 백업',NULL,0);
INSERT INTO backups VALUES(2,'자동 백업 (01-29)','pulseone_backup_2026-01-29T13-04-28-597Z.db','full','completed',1500000,'/app/data/backup','2026-01-29 22:04:28',NULL,'시스템 자동 백업',NULL,0);
INSERT INTO backups VALUES(3,'자동 백업 (02-02)','pulseone_backup_2026-02-02T09-39-32-227Z.db','full','completed',4000,'/app/data/backup','2026-02-02 09:39:32',NULL,'시스템 자동 백업',NULL,0);
INSERT INTO backups VALUES(4,'백업 (01-31)','pulseone_backup_20260131_1852.db','full','completed',1400000,'/app/data/backup','2026-01-31 18:52:00',NULL,'주간 백업',NULL,0);
INSERT INTO backups VALUES(5,'백업 (02-02)','pulseone_backup_20260202_142543.db','full','completed',1400000,'/app/data/backup','2026-02-02 14:22:00',NULL,'일일 백업',NULL,0);
INSERT INTO backups VALUES(6,'도커 베이스라인 (01-31)','pulseone_backup_docker_20260131.db','full','completed',1400000,'/app/data/backup','2026-01-31 18:55:00',NULL,'초기 도커 환경 베이스라인',NULL,0);
INSERT INTO backups VALUES(7,'RBAC 베이스라인 (02-02)','pulseone_baseline_rbac_20260202_215550.db','full','completed',1500000,'/app/data/backup','2026-02-02 21:55:50',NULL,'RBAC 도입 완료 공식 베이스라인',NULL,0);
-- Table: sqlite_sequence
INSERT INTO sqlite_sequence VALUES('payload_templates',5);
INSERT INTO sqlite_sequence VALUES('schema_versions',3);
INSERT INTO sqlite_sequence VALUES('tenants',4);
INSERT INTO sqlite_sequence VALUES('sites',6);
INSERT INTO sqlite_sequence VALUES('protocols',13);
INSERT INTO sqlite_sequence VALUES('virtual_points',18);
INSERT INTO sqlite_sequence VALUES('alarm_rules',3001);
INSERT INTO sqlite_sequence VALUES('javascript_functions',15);
INSERT INTO sqlite_sequence VALUES('system_logs',12);
INSERT INTO sqlite_sequence VALUES('edge_servers',12);
INSERT INTO sqlite_sequence VALUES('manufacturers',5);
INSERT INTO sqlite_sequence VALUES('device_models',4);
INSERT INTO sqlite_sequence VALUES('template_devices',23);
INSERT INTO sqlite_sequence VALUES('template_data_points',65);
INSERT INTO sqlite_sequence VALUES('export_profiles',103);
INSERT INTO sqlite_sequence VALUES('export_profile_points',7);
INSERT INTO sqlite_sequence VALUES('export_targets',1002);
INSERT INTO sqlite_sequence VALUES('export_profile_assignments',13);
INSERT INTO sqlite_sequence VALUES('devices',12346);
INSERT INTO sqlite_sequence VALUES('data_points',2008);
INSERT INTO sqlite_sequence VALUES('export_target_mappings',172);
INSERT INTO sqlite_sequence VALUES('alarm_occurrences',208);
INSERT INTO sqlite_sequence VALUES('export_schedules',4);
INSERT INTO sqlite_sequence VALUES('export_logs',277);
INSERT INTO sqlite_sequence VALUES('users',13);
INSERT INTO sqlite_sequence VALUES('protocol_instances',9);
INSERT INTO sqlite_sequence VALUES('backups',7);
INSERT INTO sqlite_sequence VALUES('system_settings',4);