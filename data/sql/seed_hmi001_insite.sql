-- ============================================================================
-- seed_hmi001_insite.sql
-- HMI-001 + Insite Export Gateway 데이터 보관 파일
--
-- 용도: insite 연동 테스트 환경 복원용
--       seed.sql 적용 후 이 파일을 추가 실행하면 HMI-001 + insite 구성 복원
--
-- 마지막 추출: 2026-03-05 (PulseOne v3.x)
-- ============================================================================

-- ============================================================
-- 전제 조건: seed.sql이 이미 적용된 상태에서 실행
--   tenant_id=1, site_id=1, edge_server_id=6 이 존재해야 함
-- ============================================================

-- ──────────────────────────────────────────────
-- 디바이스: HMI-001 (Modbus TCP, simulator)
-- ──────────────────────────────────────────────
INSERT OR IGNORE INTO devices VALUES(
    2,1,1,NULL,1,'HMI-001',NULL,'HMI','PulseOne',NULL,NULL,NULL,
    1,'simulator-modbus:50502','{"slave_id":1}',
    1000,3000,3,NULL,NULL,NULL,NULL,NULL,0,0,0,100,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    '2026-01-29 09:55:07','2026-01-29 09:55:07',NULL
);

INSERT OR IGNORE INTO device_settings VALUES(
    2,1000,NULL,1,10000,3000,5000,10,3,5000,1.5,60000,300000,
    1,30,10,1,0,1,0,1,0,0,0,1024,1024,100,
    '2026-01-29 09:50:46','2026-01-29 09:50:46',NULL
);

INSERT OR IGNORE INTO device_status VALUES(
    2,'connected','2026-02-06 04:32:41','2026-01-29 05:52:15',
    0,NULL,NULL,0,10,12,78,1.8,140997,140997,0,99.5,
    'V2.1.4','{"screen":"12inch","memory":"256MB"}','{"display":"active"}',
    18.7,45.2,'2026-02-06 04:32:41'
);

-- ──────────────────────────────────────────────
-- 데이터 포인트 (HMI-001: WLS 시리즈)
-- ──────────────────────────────────────────────
INSERT OR IGNORE INTO data_points VALUES(1,2,'WLS.PV',NULL,100,'CO:100','value','BOOL','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-29 09:55:07','2026-01-29 09:55:07',0,'medium');
INSERT OR IGNORE INTO data_points VALUES(2,2,'WLS.SRS',NULL,101,'CO:101','data','BOOL','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-29 09:55:07','2026-01-29 09:55:07',0,'medium');
INSERT OR IGNORE INTO data_points VALUES(3,2,'WLS.SCS',NULL,102,'CO:102',NULL,'BOOL','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-29 09:55:07','2026-01-29 09:55:07',0,'medium');
INSERT OR IGNORE INTO data_points VALUES(4,2,'WLS.SSS',NULL,200,'HR:200',NULL,'UINT16','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-29 09:55:07','2026-01-29 09:55:07',0,'medium');
INSERT OR IGNORE INTO data_points VALUES(5,2,'WLS.SBV',NULL,201,'HR:201',NULL,'UINT16','read',1,0,NULL,1.0,0.0,0.0,0.0,1,0,0.0,1000,1,1,0.0,NULL,NULL,NULL,NULL,1,NULL,NULL,0.0,'standard',1,'2026-01-29 09:55:07','2026-01-29 09:55:07',0,'medium');

-- ──────────────────────────────────────────────
-- 현재값
-- ──────────────────────────────────────────────
INSERT OR IGNORE INTO current_values VALUES(1,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'good','2026-02-06T04:32:41.118Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.119Z');
INSERT OR IGNORE INTO current_values VALUES(2,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'good','2026-02-06T04:32:41.121Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.121Z');
INSERT OR IGNORE INTO current_values VALUES(3,'{"value":0.0}','{"value":0.0}',NULL,'double',1,'good','2026-02-06T04:32:41.123Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.123Z');
INSERT OR IGNORE INTO current_values VALUES(4,'{"value":80.0}','{"value":80.0}',NULL,'double',1,'good','2026-02-06T04:32:41.125Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.125Z');
INSERT OR IGNORE INTO current_values VALUES(5,'{"value":360.0}','{"value":360.0}',NULL,'double',1,'good','2026-02-06T04:32:41.128Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.128Z');

-- ──────────────────────────────────────────────
-- 알람 규칙 (WLS 4개)
-- ──────────────────────────────────────────────
INSERT OR IGNORE INTO alarm_rules VALUES(23,1,'WLS.SRS State Change',NULL,'data_point',2,NULL,'digital',NULL,NULL,NULL,NULL,0.0,0.0,'on_change',NULL,NULL,NULL,NULL,'medium',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-29 10:38:36','2026-01-29 10:38:36',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT OR IGNORE INTO alarm_rules VALUES(24,1,'WLS.SCS State Change',NULL,'data_point',3,NULL,'digital',NULL,NULL,NULL,NULL,0.0,0.0,'on_change',NULL,NULL,NULL,NULL,'medium',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-29 10:38:36','2026-01-29 10:38:36',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT OR IGNORE INTO alarm_rules VALUES(25,1,'WLS.SSS Out of Range',NULL,'data_point',4,NULL,'analog',NULL,100.0,0.0,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-29 10:38:36','2026-01-29 10:38:36',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);
INSERT OR IGNORE INTO alarm_rules VALUES(26,1,'WLS.SBV Out of Range',NULL,'data_point',5,NULL,'analog',NULL,15.0,10.0,NULL,0.0,0.0,NULL,NULL,NULL,NULL,NULL,'high',100,0,0,1,NULL,1,0,0,NULL,NULL,1,0,'2026-01-29 10:38:36','2026-01-29 10:38:36',NULL,NULL,NULL,0,NULL,0,3,NULL,NULL,NULL,0);

-- ──────────────────────────────────────────────
-- Export Gateway (Insite용)
-- ──────────────────────────────────────────────
INSERT OR IGNORE INTO edge_servers VALUES(
    6,1,'Insite Gateway','gateway','',NULL,NULL,'127.0.0.1',NULL,NULL,8080,
    NULL,NULL,NULL,NULL,'active',NULL,NULL,NULL,0.0,0.0,0.0,0,
    '{"target_priorities":{"18":3,"19":1}}',NULL,1,1,
    '2026-02-06 04:01:25','2026-02-06 04:07:55',0,2,100,1000,'selective',1
);

-- Payload 템플릿 (Insite 포맷)
INSERT OR IGNORE INTO payload_templates (id,tenant_id,name,system_type,description,template_json,is_active,created_at,updated_at)
VALUES(1,1,'Insite 기본 템플릿','insite','Insite 빌딩 모니터링 시스템용 기본 템플릿',
    '[{"bd":"{{bd}}","ty":"{{ty}}","nm":"{{nm}}","vl":"{{vl}}","tm":"{{tm}}","st":"{{st}}","al":"{{al}}","des":"{{des}}","il":"{{il}}","xl":"{{xl}}","mi":"{{mi}}","mx":"{{mx}}"}]',
    1,'2026-01-29 07:52:15','2026-01-29 10:50:27'
);

-- Export Profile (HMI-001 → Insite 알람셋)
INSERT OR IGNORE INTO export_profiles (id,tenant_id,name,is_enabled,created_at,updated_at,created_by,point_count,data_points)
VALUES(3,1,'insite 알람셋',1,'2026-02-06 04:00:04','2026-02-06 04:00:04',NULL,0,
    '[{"id":1,"name":"WLS.PV","device":"HMI-001","address":100,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.PV"},{"id":2,"name":"WLS.SRS","device":"HMI-001","address":101,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SRS"},{"id":3,"name":"WLS.SCS","device":"HMI-001","address":102,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SCS"},{"id":4,"name":"WLS.SSS","device":"HMI-001","address":200,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SSS"},{"id":5,"name":"WLS.SBV","device":"HMI-001","address":201,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SBV"}]'
);

-- Export Targets (실제 운영 키 포함 — 이 파일은 .gitignore 등으로 관리 권장)
INSERT OR IGNORE INTO export_targets (id,tenant_id,name,target_type,is_enabled,config,export_mode,export_interval,batch_size,execution_delay_ms,created_at,updated_at)
VALUES(18,3,'insite API Gateway(stg)','HTTP',1,
    '[{"url":"https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/alarm","method":"POST","headers":{"Content-Type":"application/json","x-api-key":"${INSITE_API_GATEWAY_STG_APIKEY_4732}","Authorization":"${INSITE_API_GATEWAY_STG_AUTH_5850}"},"timeout":10000,"retry":3,"retryDelay":2000,"execution_order":3}]',
    'on_change',0,100,0,'2026-02-06 04:03:25','2026-02-06 04:07:55'
);
INSERT OR IGNORE INTO export_targets (id,tenant_id,name,target_type,is_enabled,config,export_mode,export_interval,batch_size,execution_delay_ms,created_at,updated_at)
VALUES(19,3,'insite S3(stg)','S3',1,
    '[{"bucket_name":"hdcl-csp-stg","AccessKeyID":"${INSITE_S3_STG_S3_ACCESS_8503}","SecretAccessKey":"${INSITE_S3_STG_S3_SECRET_3105}","S3ServiceUrl":"https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/alarm","region":"ap-northeast-2","Folder":"e2e-s3","execution_order":1}]',
    'on_change',0,100,0,'2026-02-06 04:03:32','2026-02-06 04:07:55'
);

-- Profile → Gateway 연결
INSERT OR IGNORE INTO export_profile_assignments (id,profile_id,gateway_id,is_active,assigned_at) VALUES(12,3,6,1,'2026-02-06 04:04:21');
