-- ============================================================================
-- seed_hmi001_insite.sql
-- HMI-001 + Insite Export Gateway 확장 데이터
--
-- 용도: insite 연동 테스트 환경 복원용
--       seed.sql 적용 후 이 파일을 추가 실행
--
-- seed.sql 기준 ID:
--   edge_servers: 1 (Main Collector) → 이 파일에서 id=2 추가
--   export_profiles: 1 (샘플) → 이 파일에서 id=2 추가
--   export_targets: 1,2 (샘플 더미) → 이 파일에서 id=3,4 추가
--   export_profile_assignments: 1 → 이 파일에서 id=2 추가
-- ============================================================================

-- ──────────────────────────────────────────────
-- edge_server id=2: Insite Gateway
-- ──────────────────────────────────────────────
INSERT INTO edge_servers VALUES(
    2,1,'Insite Gateway','gateway',NULL,'PulseOne Demo Site',NULL,
    '127.0.0.1',NULL,NULL,8080,
    NULL,NULL,NULL,NULL,'active',NULL,NULL,NULL,0.0,0.0,0.0,0,
    '{"target_priorities":{"3":1,"4":3}}',NULL,
    1,1,datetime('now','localtime'),datetime('now','localtime'),
    0,1,100,1000,'selective',1
);

-- ──────────────────────────────────────────────
-- device_status: connected 상태로 업데이트 (실운영 마지막 값)
-- ──────────────────────────────────────────────
INSERT OR REPLACE INTO device_status VALUES(
    1,'connected','2026-02-06 04:32:41','2026-01-29 05:52:15',
    0,NULL,NULL,0,10,12,78,1.8,140997,140997,0,99.5,
    'V2.1.4','{"screen":"12inch","memory":"256MB"}','{"display":"active"}',
    18.7,45.2,'2026-02-06 04:32:41'
);

-- current_values: 마지막 수집값
INSERT OR REPLACE INTO current_values VALUES(4,'{"value":80.0}','{"value":80.0}',NULL,'double',1,'good','2026-02-06T04:32:41.125Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.125Z');
INSERT OR REPLACE INTO current_values VALUES(5,'{"value":360.0}','{"value":360.0}',NULL,'double',1,'good','2026-02-06T04:32:41.128Z',NULL,NULL,NULL,NULL,0,0,0,0,'normal',0,0,NULL,'2026-02-06T04:32:41.128Z');

-- ──────────────────────────────────────────────
-- payload_template id=1: Insite 포맷으로 교체
-- ──────────────────────────────────────────────
INSERT OR REPLACE INTO payload_templates (id,tenant_id,name,system_type,description,template_json,is_active,created_at,updated_at)
VALUES(1,1,'Insite 기본 템플릿','insite','Insite 빌딩 모니터링 시스템용 기본 템플릿',
    '[{"bd":"{{bd}}","ty":"{{ty}}","nm":"{{nm}}","vl":"{{vl}}","tm":"{{tm}}","st":"{{st}}","al":"{{al}}","des":"{{des}}","il":"{{il}}","xl":"{{xl}}","mi":"{{mi}}","mx":"{{mx}}"}]',
    1,datetime('now','localtime'),datetime('now','localtime')
);

-- ──────────────────────────────────────────────
-- export_profile id=2: HMI-001 → Insite 알람셋
-- ──────────────────────────────────────────────
INSERT INTO export_profiles (id,tenant_id,name,is_enabled,created_at,updated_at,created_by,point_count,data_points)
VALUES(2,1,'insite 알람셋',1,datetime('now','localtime'),datetime('now','localtime'),NULL,5,
    '[{"id":1,"name":"WLS.PV","device":"HMI-001","address":100,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.PV"},{"id":2,"name":"WLS.SRS","device":"HMI-001","address":101,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SRS"},{"id":3,"name":"WLS.SCS","device":"HMI-001","address":102,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SCS"},{"id":4,"name":"WLS.SSS","device":"HMI-001","address":200,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SSS"},{"id":5,"name":"WLS.SBV","device":"HMI-001","address":201,"site_id":280,"scale":1,"offset":0,"target_field_name":"ENVS_1.FIFR_1.VDSDC_1:WLS.SBV"}]'
);

-- ──────────────────────────────────────────────
-- export_targets id=3,4: 실 insite 엔드포인트
-- ⚠️ 이 파일은 git에 올리지 않거나 별도 시크릿 관리 권장
-- ──────────────────────────────────────────────
INSERT INTO export_targets (id,tenant_id,name,target_type,is_enabled,config,export_mode,export_interval,batch_size,execution_delay_ms,created_at,updated_at)
VALUES(3,1,'insite API Gateway(stg)','HTTP',1,
    '[{"url":"https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/alarm","method":"POST","headers":{"Content-Type":"application/json","x-api-key":"${INSITE_API_GATEWAY_STG_APIKEY_4732}","Authorization":"${INSITE_API_GATEWAY_STG_AUTH_5850}"},"timeout":10000,"retry":3,"retryDelay":2000,"execution_order":1}]',
    'on_change',0,100,0,datetime('now','localtime'),datetime('now','localtime')
);
INSERT INTO export_targets (id,tenant_id,name,target_type,is_enabled,config,export_mode,export_interval,batch_size,execution_delay_ms,created_at,updated_at)
VALUES(4,1,'insite S3(stg)','S3',1,
    '[{"bucket_name":"hdcl-csp-stg","AccessKeyID":"${INSITE_S3_STG_S3_ACCESS_8503}","SecretAccessKey":"${INSITE_S3_STG_S3_SECRET_3105}","S3ServiceUrl":"https://rhnkxbwgb9.execute-api.ap-northeast-2.amazonaws.com/icos5Api/alarm","region":"ap-northeast-2","Folder":"e2e-s3","execution_order":2}]',
    'on_change',0,100,0,datetime('now','localtime'),datetime('now','localtime')
);

-- ──────────────────────────────────────────────
-- export_profile_assignments id=2: profile_id=2 → gateway_id=2
-- ──────────────────────────────────────────────
INSERT INTO export_profile_assignments (id,profile_id,gateway_id,is_active,assigned_at)
VALUES(2,2,2,1,datetime('now','localtime'));

-- ──────────────────────────────────────────────
-- sqlite_sequence 업데이트
-- ──────────────────────────────────────────────
DELETE FROM sqlite_sequence;
INSERT INTO sqlite_sequence VALUES('schema_versions',1);
INSERT INTO sqlite_sequence VALUES('system_settings',8);
INSERT INTO sqlite_sequence VALUES('tenants',1);
INSERT INTO sqlite_sequence VALUES('users',3);
INSERT INTO sqlite_sequence VALUES('sites',1);
INSERT INTO sqlite_sequence VALUES('edge_servers',2);
INSERT INTO sqlite_sequence VALUES('manufacturers',5);
INSERT INTO sqlite_sequence VALUES('device_models',4);
INSERT INTO sqlite_sequence VALUES('protocols',9);
INSERT INTO sqlite_sequence VALUES('template_devices',2);
INSERT INTO sqlite_sequence VALUES('template_data_points',16);
INSERT INTO sqlite_sequence VALUES('devices',1);
INSERT INTO sqlite_sequence VALUES('data_points',5);
INSERT INTO sqlite_sequence VALUES('alarm_rules',4);
INSERT INTO sqlite_sequence VALUES('javascript_functions',2);
INSERT INTO sqlite_sequence VALUES('virtual_points',2);
INSERT INTO sqlite_sequence VALUES('payload_templates',1);
INSERT INTO sqlite_sequence VALUES('export_profiles',2);
INSERT INTO sqlite_sequence VALUES('export_targets',4);
INSERT INTO sqlite_sequence VALUES('export_profile_assignments',2);
