/**
 * i18n_translate.js
 * 도커 안에서 실행: node /app/i18n_translate.js
 * 
 * 전략: JSX 텍스트 노드, 속성값, 확인창 문자열에서
 * 패턴 매칭으로 한국어 → t('key') 치환
 * 치환된 키는 ko/en JSON에 자동 추가
 */
const fs = require('fs');
const path = require('path');

const base = '/app/src';
const localesBase = '/app/public/locales';

// ===================================================================
// 공통 치환 맵 (파일 무관)
// ===================================================================
const COMMON_MAP = [
    // 버튼/액션
    ['닫기', "t('common:close')", 'close', 'Close'],
    ['저장', "t('common:save')", 'save', 'Save'],
    ['취소', "t('common:cancel')", 'cancel', 'Cancel'],
    ['확인', "t('common:confirm')", 'confirm', 'Confirm'],
    ['삭제', "t('common:delete')", 'delete', 'Delete'],
    ['수정', "t('common:edit')", 'edit', 'Edit'],
    ['등록', "t('common:register')", 'register', 'Register'],
    ['생성', "t('common:create')", 'create', 'Create'],
    ['추가', "t('common:add')", 'add', 'Add'],
    ['검색', "t('common:search')", 'search', 'Search'],
    ['새로고침', "t('common:refresh')", 'refresh', 'Refresh'],
    ['내보내기', "t('common:export')", 'export', 'Export'],
    ['불러오기', "t('common:import')", 'import', 'Import'],
    ['초기화', "t('common:reset')", 'reset', 'Reset'],
    ['적용', "t('common:apply')", 'apply', 'Apply'],
    ['선택', "t('common:select')", 'select', 'Select'],
    ['전체', "t('common:all')", 'all', 'All'],
    ['이전', "t('common:prev')", 'prev', 'Previous'],
    ['다음', "t('common:next')", 'next', 'Next'],
    ['완료', "t('common:done')", 'done', 'Done'],
    ['복원', "t('common:restore')", 'restore', 'Restore'],
    // 상태
    ['온라인', "t('common:online')", 'online', 'Online'],
    ['오프라인', "t('common:offline')", 'offline', 'Offline'],
    ['활성', "t('common:active')", 'active', 'Active'],
    ['비활성', "t('common:inactive')", 'inactive', 'Inactive'],
    ['사용중', "t('common:inUse')", 'inUse', 'In Use'],
    ['대기중', "t('common:pending')", 'pending', 'Pending'],
    ['성공', "t('common:success')", 'success', 'Success'],
    ['실패', "t('common:failure')", 'failure', 'Failure'],
    ['오류', "t('common:error')", 'error', 'Error'],
    ['경고', "t('common:warning')", 'warning', 'Warning'],
    ['정보', "t('common:info')", 'info', 'Info'],
    // 공통 메시지
    ['데이터를 불러오는 중', "t('common:loading')", 'loading', 'Loading...'],
    ['로딩 중', "t('common:loading')", 'loading', 'Loading...'],
    ['데이터가 없습니다', "t('common:noData')", 'noData', 'No data'],
    ['없습니다', "t('common:none')", 'none', 'None'],
];

// ===================================================================
// 파일별 치환 맵 정의
// ===================================================================
const FILE_CONFIGS = [
    // ---- DeviceDetailModal ----
    {
        file: 'components/modals/DeviceDetailModal.tsx',
        ns: 'devices',
        koMap: [
            ['새 디바이스 추가', "t('modal.createTitle')", 'modal.createTitle', 'Add New Device'],
            ['디바이스 편집', "t('modal.editTitle')", 'modal.editTitle', 'Edit Device'],
            ['디바이스 상세', "t('modal.detailTitle')", 'modal.detailTitle', 'Device Detail'],
            ['기본정보', "t('tabs.basic')", 'tabs.basic', 'Basic Info'],
            ['통신 설정', "t('tabs.commSettings')", 'tabs.commSettings', 'Comm Settings'],
            ['데이터포인트', "t('tabs.dataPoints')", 'tabs.dataPoints', 'Data Points'],
            ['RTU 네트워크', "t('tabs.rtuNetwork')", 'tabs.rtuNetwork', 'RTU Network'],
            ['통신 모니터', "t('tabs.rtuMonitor')", 'tabs.rtuMonitor', 'RTU Monitor'],
            ['상태', "t('tabs.status')", 'tabs.status', 'Status'],
            ['로그', "t('tabs.logs')", 'tabs.logs', 'Logs'],
            ['1단계', "t('wizard.step1')", 'wizard.step1', 'Step 1'],
            ['2단계', "t('wizard.step2')", 'wizard.step2', 'Step 2'],
            ['3단계', "t('wizard.step3')", 'wizard.step3', 'Step 3'],
            ['등록 요약', "t('wizard.summary')", 'wizard.summary', 'Registration Summary'],
            ['장치 식별', "t('wizard.deviceId')", 'wizard.deviceId', 'Device Identity'],
            ['포인트 및 요약', "t('wizard.pointsSummary')", 'wizard.pointsSummary', 'Points & Summary'],
            ['연결됨', "t('status.connected')", 'status.connected', 'Connected'],
            ['연결끊김', "t('status.disconnected')", 'status.disconnected', 'Disconnected'],
            ['연결중', "t('status.connecting')", 'status.connecting', 'Connecting'],
            ['알수없음', "t('status.unknown')", 'status.unknown', 'Unknown'],
            ['디바이스 명', "t('field.name')", 'field.name', 'Device Name'],
            ['제조사/모델', "t('field.manufacturer')", 'field.manufacturer', 'Manufacturer/Model'],
            ['사이트 ID', "t('field.siteId')", 'field.siteId', 'Site ID'],
            ['프로토콜', "t('field.protocol')", 'field.protocol', 'Protocol'],
            ['엔드포인트', "t('field.endpoint')", 'field.endpoint', 'Endpoint'],
            ['디바이스 생성 완료', "t('msg.createSuccess')", 'msg.createSuccess', 'Device Created'],
            ['디바이스 수정 완료', "t('msg.editSuccess')", 'msg.editSuccess', 'Device Updated'],
            ['디바이스 삭제 완료', "t('msg.deleteSuccess')", 'msg.deleteSuccess', 'Device Deleted'],
            ['디바이스 삭제 확인', "t('msg.deleteConfirm')", 'msg.deleteConfirm', 'Confirm Delete'],
            ['저장 완료', "t('msg.savedSuccess')", 'msg.savedSuccess', 'Saved'],
            ['저장 실패', "t('msg.saveFailed')", 'msg.saveFailed', 'Save Failed'],
            ['삭제 실패', "t('msg.deleteFailed')", 'msg.deleteFailed', 'Delete Failed'],
            ['생성 중...', "t('action.creating')", 'action.creating', 'Creating...'],
            ['저장 중...', "t('action.saving')", 'action.saving', 'Saving...'],
        ]
    },

    // ---- DataExport ----
    {
        file: 'pages/DataExport.tsx',
        ns: 'dataExport',
        koMap: [
            ['데이터 내보내기', "t('title')", 'title', 'Data Export'],
            ['신규 작업', "t('tab.new')", 'tab.new', 'New Task'],
            ['내보내기 이력', "t('tab.history')", 'tab.history', 'Export History'],
            ['빠른 시작 템플릿', "t('template.quickStart')", 'template.quickStart', 'Quick Start Templates'],
            ['일일 생산 데이터', "t('template.dailyProd')", 'template.dailyProd', 'Daily Production Data'],
            ['주간 알람 리포트', "t('template.weeklyAlarm')", 'template.weeklyAlarm', 'Weekly Alarm Report'],
            ['월간 장치 통계', "t('template.monthlyStats')", 'template.monthlyStats', 'Monthly Device Stats'],
            ['시스템 운영 로그', "t('template.sysLog')", 'template.sysLog', 'System Operations Log'],
            ['유형 선택', "t('wizard.step1')", 'wizard.step1', 'Select Type'],
            ['대상 장치', "t('wizard.step2')", 'wizard.step2', 'Target Device'],
            ['추출 설정', "t('wizard.step3')", 'wizard.step3', 'Export Settings'],
            ['최종 검토', "t('wizard.step4')", 'wizard.step4', 'Review'],
            ['이력 데이터', "t('type.historical')", 'type.historical', 'Historical Data'],
            ['실시간 데이터', "t('type.realtime')", 'type.realtime', 'Real-time Data'],
            ['알람 이력', "t('type.alarms')", 'type.alarms', 'Alarm History'],
            ['운영 로그', "t('type.logs')", 'type.logs', 'Operation Logs'],
            ['선택된 유형 상세 정보', "t('typeDetail.title')", 'typeDetail.title', 'Selected Type Details'],
            ['장치 목록', "t('deviceList')", 'deviceList', 'Device List'],
            ['데이터 포인트', "t('pointList')", 'pointList', 'Data Points'],
            ['작업 이름', "t('field.taskName')", 'field.taskName', 'Task Name'],
            ['파일 형식', "t('field.format')", 'field.format', 'File Format'],
            ['기간 설정', "t('field.period')", 'field.period', 'Period'],
            ['추출 유형', "t('review.type')", 'review.type', 'Export Type'],
            ['대상 장치', "t('review.device')", 'review.device', 'Target Device'],
            ['포인트 개수', "t('review.points')", 'review.points', 'Point Count'],
            ['내보내기 이력이 없습니다', "t('history.empty')", 'history.empty', 'No export history'],
            ['내보내기 시작', "t('action.start')", 'action.start', 'Start Export'],
        ]
    },

    // ---- SiteModal ----
    {
        file: 'components/modals/SiteModal/SiteModal.tsx',
        ns: 'sites',
        koMap: [
            ['사이트 등록', "t('modal.createTitle')", 'modal.createTitle', 'Register Site'],
            ['사이트 수정', "t('modal.editTitle')", 'modal.editTitle', 'Edit Site'],
            ['사이트명', "t('field.name')", 'field.name', 'Site Name'],
            ['사이트 코드', "t('field.code')", 'field.code', 'Site Code'],
            ['위치', "t('field.location')", 'field.location', 'Location'],
            ['설명', "t('field.description')", 'field.description', 'Description'],
            ['테넌트', "t('field.tenant')", 'field.tenant', 'Tenant'],
            ['사이트를 선택하세요', "t('placeholder.select')", 'placeholder.select', 'Select a site'],
            ['저장 성공', "t('msg.saveSuccess')", 'msg.saveSuccess', 'Saved successfully'],
            ['저장 실패', "t('msg.saveFailed')", 'msg.saveFailed', 'Save failed'],
        ]
    },

    // ---- SiteDetailModal ----
    {
        file: 'components/modals/SiteModal/SiteDetailModal.tsx',
        ns: 'sites',
        koMap: [
            ['사이트 상세', "t('modal.title')", 'modal.title', 'Site Detail'],
            ['기본 정보', "t('section.basic')", 'section.basic', 'Basic Info'],
            ['사이트명', "t('field.name')", 'field.name', 'Site Name'],
            ['사이트 코드', "t('field.code')", 'field.code', 'Site Code'],
            ['위치', "t('field.location')", 'field.location', 'Location'],
            ['설명', "t('field.description')", 'field.description', 'Description'],
            ['테넌트', "t('field.tenant')", 'field.tenant', 'Tenant'],
            ['수집기 수', "t('field.collectors')", 'field.collectors', 'Collectors'],
            ['장치 수', "t('field.devices')", 'field.devices', 'Devices'],
            ['생성일', "t('field.createdAt')", 'field.createdAt', 'Created At'],
            ['수정일', "t('field.updatedAt')", 'field.updatedAt', 'Updated At'],
        ]
    },

    // ---- TenantModal ----
    {
        file: 'components/modals/TenantModal/TenantModal.tsx',
        ns: 'tenants',
        koMap: [
            ['테넌트 등록', "t('modal.createTitle')", 'modal.createTitle', 'Register Tenant'],
            ['테넌트 수정', "t('modal.editTitle')", 'modal.editTitle', 'Edit Tenant'],
            ['테넌트명', "t('field.name')", 'field.name', 'Tenant Name'],
            ['코드', "t('field.code')", 'field.code', 'Code'],
            ['설명', "t('field.description')", 'field.description', 'Description'],
            ['최대 사이트', "t('field.maxSites')", 'field.maxSites', 'Max Sites'],
            ['최대 엣지 서버', "t('field.maxEdgeServers')", 'field.maxEdgeServers', 'Max Edge Servers'],
            ['저장 성공', "t('msg.saveSuccess')", 'msg.saveSuccess', 'Saved successfully'],
            ['저장 실패', "t('msg.saveFailed')", 'msg.saveFailed', 'Save failed'],
        ]
    },

    // ---- TenantDetailModal ----
    {
        file: 'components/modals/TenantModal/TenantDetailModal.tsx',
        ns: 'tenants',
        koMap: [
            ['테넌트 상세', "t('modal.title')", 'modal.title', 'Tenant Detail'],
            ['기본 정보', "t('section.basic')", 'section.basic', 'Basic Info'],
            ['테넌트명', "t('field.name')", 'field.name', 'Tenant Name'],
            ['코드', "t('field.code')", 'field.code', 'Code'],
            ['설명', "t('field.description')", 'field.description', 'Description'],
            ['사이트 수', "t('field.sitesCount')", 'field.sitesCount', 'Sites'],
            ['수집기 수', "t('field.collectorsCount')", 'field.collectorsCount', 'Collectors'],
            ['생성일', "t('field.createdAt')", 'field.createdAt', 'Created At'],
        ]
    },

    // ---- UserModal ----
    {
        file: 'components/modals/UserModal/UserModal.tsx',
        ns: 'settings',
        koMap: [
            ['사용자 등록', "t('userModal.createTitle')", 'userModal.createTitle', 'Register User'],
            ['사용자 수정', "t('userModal.editTitle')", 'userModal.editTitle', 'Edit User'],
            ['아이디', "t('field.username')", 'field.username', 'Username'],
            ['이메일', "t('field.email')", 'field.email', 'Email'],
            ['이름', "t('field.displayName')", 'field.displayName', 'Display Name'],
            ['비밀번호', "t('field.password')", 'field.password', 'Password'],
            ['역할', "t('field.role')", 'field.role', 'Role'],
            ['사이트', "t('field.site')", 'field.site', 'Site'],
            ['저장 성공', "t('msg.saveSuccess')", 'msg.saveSuccess', 'Saved successfully'],
            ['저장 실패', "t('msg.saveFailed')", 'msg.saveFailed', 'Save failed'],
        ]
    },

    // ---- UserDetailModal ----
    {
        file: 'components/modals/UserModal/UserDetailModal.tsx',
        ns: 'settings',
        koMap: [
            ['사용자 상세', "t('userModal.detailTitle')", 'userModal.detailTitle', 'User Detail'],
            ['기본 정보', "t('section.basic')", 'section.basic', 'Basic Info'],
            ['아이디', "t('field.username')", 'field.username', 'Username'],
            ['이메일', "t('field.email')", 'field.email', 'Email'],
            ['이름', "t('field.displayName')", 'field.displayName', 'Display Name'],
            ['역할', "t('field.role')", 'field.role', 'Role'],
            ['사이트', "t('field.site')", 'field.site', 'Site'],
            ['마지막 로그인', "t('field.lastLogin')", 'field.lastLogin', 'Last Login'],
            ['생성일', "t('field.createdAt')", 'field.createdAt', 'Created At'],
        ]
    },

    // ---- RoleModal ----
    {
        file: 'components/modals/RoleModal/RoleModal.tsx',
        ns: 'permissions',
        koMap: [
            ['역할 등록', "t('roleModal.createTitle')", 'roleModal.createTitle', 'Register Role'],
            ['역할 수정', "t('roleModal.editTitle')", 'roleModal.editTitle', 'Edit Role'],
            ['역할명', "t('roleModal.name')", 'roleModal.name', 'Role Name'],
            ['설명', "t('roleModal.description')", 'roleModal.description', 'Description'],
            ['권한 설정', "t('roleModal.permissions')", 'roleModal.permissions', 'Permission Settings'],
            ['저장 성공', "t('msg.saveSuccess')", 'msg.saveSuccess', 'Saved successfully'],
        ]
    },

    // ---- RoleDetailModal ----
    {
        file: 'components/modals/RoleModal/RoleDetailModal.tsx',
        ns: 'permissions',
        koMap: [
            ['역할 상세', "t('roleModal.detailTitle')", 'roleModal.detailTitle', 'Role Detail'],
            ['역할명', "t('roleModal.name')", 'roleModal.name', 'Role Name'],
            ['설명', "t('roleModal.description')", 'roleModal.description', 'Description'],
            ['소속 사용자', "t('roleModal.members')", 'roleModal.members', 'Members'],
            ['권한 목록', "t('roleModal.permissions')", 'roleModal.permissions', 'Permissions'],
        ]
    },

    // ---- LogDetailModal ----
    {
        file: 'components/modals/LogDetailModal.tsx',
        ns: 'dataExport',
        koMap: [
            ['로그 상세', "t('logModal.title')", 'logModal.title', 'Log Detail'],
            ['발생 시간', "t('logModal.time')", 'logModal.time', 'Occurred At'],
            ['내용', "t('logModal.content')", 'logModal.content', 'Content'],
            ['레벨', "t('logModal.level')", 'logModal.level', 'Level'],
            ['소스', "t('logModal.source')", 'logModal.source', 'Source'],
        ]
    },

    // ---- MoveGroupModal ----
    {
        file: 'components/modals/MoveGroupModal.tsx',
        ns: 'devices',
        koMap: [
            ['그룹 이동', "t('moveGroup.title')", 'moveGroup.title', 'Move Group'],
            ['대상 그룹', "t('moveGroup.target')", 'moveGroup.target', 'Target Group'],
            ['선택', "t('moveGroup.select')", 'moveGroup.select', 'Select'],
            ['이동', "t('moveGroup.move')", 'moveGroup.move', 'Move'],
        ]
    },

    // ---- NetworkScanModal ----
    {
        file: 'components/modals/NetworkScanModal.tsx',
        ns: 'network',
        koMap: [
            ['네트워크 스캔', "t('scan.title')", 'scan.title', 'Network Scan'],
            ['스캔 범위', "t('scan.range')", 'scan.range', 'Scan Range'],
            ['스캔 시작', "t('scan.start')", 'scan.start', 'Start Scan'],
            ['스캔 중', "t('scan.scanning')", 'scan.scanning', 'Scanning...'],
            ['발견된 장치', "t('scan.found')", 'scan.found', 'Discovered Devices'],
            ['호스트', "t('scan.host')", 'scan.host', 'Host'],
            ['포트', "t('scan.port')", 'scan.port', 'Port'],
            ['상태', "t('scan.status')", 'scan.status', 'Status'],
            ['장치 추가', "t('scan.addDevice')", 'scan.addDevice', 'Add Device'],
        ]
    },

    // ---- ProtocolEditor ----
    {
        file: 'components/modals/ProtocolModal/ProtocolEditor.tsx',
        ns: 'protocols',
        koMap: [
            ['프로토콜 등록', "t('modal.createTitle')", 'modal.createTitle', 'Register Protocol'],
            ['프로토콜 수정', "t('modal.editTitle')", 'modal.editTitle', 'Edit Protocol'],
            ['프로토콜 유형', "t('field.type')", 'field.type', 'Protocol Type'],
            ['프로토콜명', "t('field.name')", 'field.name', 'Protocol Name'],
            ['호스트', "t('field.host')", 'field.host', 'Host'],
            ['포트', "t('field.port')", 'field.port', 'Port'],
            ['설명', "t('field.description')", 'field.description', 'Description'],
            ['연결 테스트', "t('action.test')", 'action.test', 'Test Connection'],
            ['저장 성공', "t('msg.saveSuccess')", 'msg.saveSuccess', 'Saved successfully'],
            ['저장 실패', "t('msg.saveFailed')", 'msg.saveFailed', 'Save failed'],
        ]
    },

    // ---- ProtocolDetailModal ----
    {
        file: 'components/modals/ProtocolModal/ProtocolDetailModal.tsx',
        ns: 'protocols',
        koMap: [
            ['프로토콜 상세', "t('modal.detailTitle')", 'modal.detailTitle', 'Protocol Detail'],
            ['기본 정보', "t('section.basic')", 'section.basic', 'Basic Info'],
            ['프로토콜명', "t('field.name')", 'field.name', 'Protocol Name'],
            ['유형', "t('field.type')", 'field.type', 'Type'],
            ['호스트', "t('field.host')", 'field.host', 'Host'],
            ['포트', "t('field.port')", 'field.port', 'Port'],
            ['연결 상태', "t('field.status')", 'field.status', 'Connection Status'],
            ['연결된 장치', "t('field.devices')", 'field.devices', 'Connected Devices'],
        ]
    },

    // ---- ProtocolDevicesModal ----
    {
        file: 'components/modals/ProtocolModal/ProtocolDevicesModal.tsx',
        ns: 'protocols',
        koMap: [
            ['연결된 장치 목록', "t('devicesModal.title')", 'devicesModal.title', 'Connected Devices'],
            ['장치명', "t('devicesModal.name')", 'devicesModal.name', 'Device Name'],
            ['상태', "t('devicesModal.status')", 'devicesModal.status', 'Status'],
            ['프로토콜', "t('devicesModal.protocol')", 'devicesModal.protocol', 'Protocol'],
        ]
    },

    // ---- ManufacturerModal ----
    {
        file: 'components/modals/ManufacturerModal/ManufacturerModal.tsx',
        ns: 'manufacturers',
        koMap: [
            ['제조사 등록', "t('modal.createTitle')", 'modal.createTitle', 'Register Manufacturer'],
            ['제조사 수정', "t('modal.editTitle')", 'modal.editTitle', 'Edit Manufacturer'],
            ['제조사명', "t('field.name')", 'field.name', 'Manufacturer Name'],
            ['코드', "t('field.code')", 'field.code', 'Code'],
            ['설명', "t('field.description')", 'field.description', 'Description'],
            ['저장 성공', "t('msg.saveSuccess')", 'msg.saveSuccess', 'Saved successfully'],
            ['저장 실패', "t('msg.saveFailed')", 'msg.saveFailed', 'Save failed'],
        ]
    },

    // ---- ManufacturerDetailModal ----
    {
        file: 'components/modals/ManufacturerModal/ManufacturerDetailModal.tsx',
        ns: 'manufacturers',
        koMap: [
            ['제조사 상세', "t('modal.detailTitle')", 'modal.detailTitle', 'Manufacturer Detail'],
            ['제조사명', "t('field.name')", 'field.name', 'Manufacturer Name'],
            ['코드', "t('field.code')", 'field.code', 'Code'],
            ['설명', "t('field.description')", 'field.description', 'Description'],
            ['장치 수', "t('field.devicesCount')", 'field.devicesCount', 'Devices'],
            ['생성일', "t('field.createdAt')", 'field.createdAt', 'Created At'],
        ]
    },

    // ---- ModelListModal ----
    {
        file: 'components/modals/ModelListModal.tsx',
        ns: 'deviceTemplates',
        koMap: [
            ['마스터 모델 목록', "t('modelList.title')", 'modelList.title', 'Master Model List'],
            ['모델명', "t('modelList.name')", 'modelList.name', 'Model Name'],
            ['제조사', "t('modelList.manufacturer')", 'modelList.manufacturer', 'Manufacturer'],
            ['프로토콜', "t('modelList.protocol')", 'modelList.protocol', 'Protocol'],
            ['데이터포인트 수', "t('modelList.points')", 'modelList.points', 'Data Points'],
            ['선택', "t('modelList.select')", 'modelList.select', 'Select'],
        ]
    },

    // ---- AlarmBulkUpdateModal ----
    {
        file: 'components/modals/AlarmBulkUpdateModal.tsx',
        ns: 'alarms',
        koMap: [
            ['일괄 처리', "t('bulkModal.title')", 'bulkModal.title', 'Bulk Action'],
            ['선택된 알람', "t('bulkModal.selected')", 'bulkModal.selected', 'Selected Alarms'],
            ['일괄 확인', "t('bulkModal.acknowledge')", 'bulkModal.acknowledge', 'Bulk Acknowledge'],
            ['일괄 해제', "t('bulkModal.clear')", 'bulkModal.clear', 'Bulk Clear'],
            ['처리 메모', "t('bulkModal.memo')", 'bulkModal.memo', 'Note'],
            ['처리 중', "t('bulkModal.processing')", 'bulkModal.processing', 'Processing...'],
        ]
    },

    // ---- AlarmDetailModal ----
    {
        file: 'components/modals/AlarmDetailModal.tsx',
        ns: 'alarms',
        koMap: [
            ['알람 상세', "t('detailModal.title')", 'detailModal.title', 'Alarm Detail'],
            ['알람 확인', "t('detailModal.acknowledge')", 'detailModal.acknowledge', 'Acknowledge'],
            ['알람 해제', "t('detailModal.clear')", 'detailModal.clear', 'Clear'],
            ['타겟', "t('detailModal.target')", 'detailModal.target', 'Target'],
            ['심각도', "t('detailModal.severity')", 'detailModal.severity', 'Severity'],
            ['발생 시간', "t('detailModal.triggeredAt')", 'detailModal.triggeredAt', 'Triggered At'],
            ['확인 시간', "t('detailModal.acknowledgedAt')", 'detailModal.acknowledgedAt', 'Acknowledged At'],
            ['해제 시간', "t('detailModal.clearedAt')", 'detailModal.clearedAt', 'Cleared At'],
            ['메시지', "t('detailModal.message')", 'detailModal.message', 'Message'],
        ]
    },

    // ---- AlarmHistoryDetailModal ----
    {
        file: 'components/modals/AlarmHistoryDetailModal.tsx',
        ns: 'alarms',
        koMap: [
            ['알람 이력 상세', "t('historyModal.title')", 'historyModal.title', 'Alarm History Detail'],
            ['발생 시간', "t('historyModal.triggeredAt')", 'historyModal.triggeredAt', 'Triggered At'],
            ['해제 시간', "t('historyModal.clearedAt')", 'historyModal.clearedAt', 'Cleared At'],
            ['지속 시간', "t('historyModal.duration')", 'historyModal.duration', 'Duration'],
            ['타겟', "t('historyModal.target')", 'historyModal.target', 'Target'],
            ['심각도', "t('historyModal.severity')", 'historyModal.severity', 'Severity'],
            ['조치 이력', "t('historyModal.actions')", 'historyModal.actions', 'Actions'],
        ]
    },

    // ---- AlarmSettingsModal ----
    {
        file: 'components/modals/AlarmSettingsModal.tsx',
        ns: 'alarms',
        koMap: [
            ['알람 설정', "t('settingsModal.title')", 'settingsModal.title', 'Alarm Settings'],
            ['알림 채널', "t('settingsModal.channels')", 'settingsModal.channels', 'Notification Channels'],
            ['이메일', "t('settingsModal.email')", 'settingsModal.email', 'Email'],
            ['슬랙', "t('settingsModal.slack')", 'settingsModal.slack', 'Slack'],
            ['웹훅', "t('settingsModal.webhook')", 'settingsModal.webhook', 'Webhook'],
            ['음소거', "t('settingsModal.mute')", 'settingsModal.mute', 'Mute'],
        ]
    },

    // ---- TemplateApplyModal ----
    {
        file: 'components/modals/TemplateApplyModal.tsx',
        ns: 'deviceTemplates',
        koMap: [
            ['템플릿 적용', "t('applyModal.title')", 'applyModal.title', 'Apply Template'],
            ['대상 장치', "t('applyModal.target')", 'applyModal.target', 'Target Device'],
            ['적용 방식', "t('applyModal.mode')", 'applyModal.mode', 'Apply Mode'],
            ['덮어쓰기', "t('applyModal.overwrite')", 'applyModal.overwrite', 'Overwrite'],
            ['병합', "t('applyModal.merge')", 'applyModal.merge', 'Merge'],
            ['적용 중', "t('applyModal.applying')", 'applyModal.applying', 'Applying...'],
        ]
    },

    // ---- TemplateCreateModal ----
    {
        file: 'components/modals/TemplateCreateModal.tsx',
        ns: 'deviceTemplates',
        koMap: [
            ['템플릿 생성', "t('createModal.title')", 'createModal.title', 'Create Template'],
            ['템플릿명', "t('createModal.name')", 'createModal.name', 'Template Name'],
            ['설명', "t('createModal.description')", 'createModal.description', 'Description'],
            ['카테고리', "t('createModal.category')", 'createModal.category', 'Category'],
            ['기반 장치', "t('createModal.baseDevice')", 'createModal.baseDevice', 'Base Device'],
        ]
    },

    // ---- TemplateImportExportModal ----
    {
        file: 'components/modals/TemplateImportExportModal.tsx',
        ns: 'deviceTemplates',
        koMap: [
            ['템플릿 가져오기/내보내기', "t('importExportModal.title')", 'importExportModal.title', 'Template Import/Export'],
            ['가져오기', "t('importExportModal.import')", 'importExportModal.import', 'Import'],
            ['내보내기', "t('importExportModal.export')", 'importExportModal.export', 'Export'],
            ['파일 선택', "t('importExportModal.selectFile')", 'importExportModal.selectFile', 'Select File'],
            ['형식', "t('importExportModal.format')", 'importExportModal.format', 'Format'],
        ]
    },

    // ---- TemplatePropagateModal ----
    {
        file: 'components/modals/TemplatePropagateModal.tsx',
        ns: 'deviceTemplates',
        koMap: [
            ['템플릿 적용 전파', "t('propagateModal.title')", 'propagateModal.title', 'Propagate Template'],
            ['적용 대상', "t('propagateModal.targets')", 'propagateModal.targets', 'Targets'],
            ['전파 중', "t('propagateModal.propagating')", 'propagateModal.propagating', 'Propagating...'],
            ['전파 완료', "t('propagateModal.done')", 'propagateModal.done', 'Propagation Complete'],
        ]
    },
];

// ===================================================================
// 핵심 함수: JSX 텍스트 노드만 치환
// ===================================================================
function replaceJsxText(content, koText, replacement) {
    let changed = false;

    // Pattern 1: JSX children text node: >word< or > word < (with potential whitespace)
    // Only replace when it's a pure text content (no JSX expressions mixed)
    const escaped = koText.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

    // Pattern: >someText< where someText IS the Korean string
    const jsxTextPattern = new RegExp(`(>\\s*)${escaped}(\\s*<)`, 'g');
    const newContent1 = content.replace(jsxTextPattern, `$1{${replacement}}$2`);
    if (newContent1 !== content) { content = newContent1; changed = true; }

    // Pattern 2: string literal in JSX attribute: ="korean" or ={'korean'} or ={"korean"}
    const attrDoubleQ = new RegExp(`(\\w+|placeholder|title|label|description|alt|aria-label)="${escaped}"`, 'g');
    const newContent2 = content.replace(attrDoubleQ, `$1={${replacement}}`);
    if (newContent2 !== content) { content = newContent2; changed = true; }

    const attrSingleQ = new RegExp(`(\\w+|placeholder|title|label|description|alt|aria-label)='${escaped}'`, 'g');
    const newContent3 = content.replace(attrSingleQ, `$1={${replacement}}`);
    if (newContent3 !== content) { content = newContent3; changed = true; }

    // Pattern 3: template literal or plain string in JSX context
    // e.g. message: '한국어' or title: '한국어'
    const msgPattern = new RegExp(`(message|title|confirmText|cancelText|label):\\s*'${escaped}'`, 'g');
    const newContent4 = content.replace(msgPattern, `$1: ${replacement}`);
    if (newContent4 !== content) { content = newContent4; changed = true; }

    const msgPatternDQ = new RegExp(`(message|title|confirmText|cancelText|label):\\s*"${escaped}"`, 'g');
    const newContent5 = content.replace(msgPatternDQ, `$1: ${replacement}`);
    if (newContent5 !== content) { content = newContent5; changed = true; }

    return { content, changed };
}

// ===================================================================
// JSON 번역 파일 업데이트
// ===================================================================
function setNestedKey(obj, keyPath, value) {
    const keys = keyPath.split('.');
    let cur = obj;
    for (let i = 0; i < keys.length - 1; i++) {
        if (!cur[keys[i]]) cur[keys[i]] = {};
        cur = cur[keys[i]];
    }
    cur[keys[keys.length - 1]] = value;
}

function updateLocaleFile(lang, ns, keyPath, value) {
    const filePath = path.join(localesBase, lang, ns + '.json');
    let obj = {};
    if (fs.existsSync(filePath)) {
        obj = JSON.parse(fs.readFileSync(filePath, 'utf-8'));
    }
    setNestedKey(obj, keyPath, value);
    fs.writeFileSync(filePath, JSON.stringify(obj, null, 4), 'utf-8');
}

// ===================================================================
// 메인 실행
// ===================================================================
let totalFiles = 0;
let totalChanges = 0;

for (const config of FILE_CONFIGS) {
    const filePath = path.join(base, config.file);
    if (!fs.existsSync(filePath)) {
        console.log(`NOT FOUND: ${config.file}`);
        continue;
    }

    let content = fs.readFileSync(filePath, 'utf-8');
    let fileChanged = false;

    // Apply common map first
    for (const [ko, replacement, enKey, enVal] of COMMON_MAP) {
        const tKey = replacement.match(/t\('([^']+)'\)/)?.[1];
        if (!tKey) continue;
        const ns = tKey.includes(':') ? tKey.split(':')[0] : config.ns;
        const key = tKey.includes(':') ? tKey.split(':')[1] : tKey;
        const result = replaceJsxText(content, ko, replacement);
        if (result.changed) {
            content = result.content;
            fileChanged = true;
            // Update locale files for common: namespace
            if (tKey.startsWith('common:')) {
                updateLocaleFile('ko', 'common', key, ko);
                updateLocaleFile('en', 'common', key, enVal);
            }
        }
    }

    // Apply file-specific map
    for (const [ko, replacement, enKey, enVal] of config.koMap) {
        const result = replaceJsxText(content, ko, replacement);
        if (result.changed) {
            content = result.content;
            fileChanged = true;
            updateLocaleFile('ko', config.ns, enKey, ko);
            updateLocaleFile('en', config.ns, enKey, enVal);
            totalChanges++;
        }
    }

    if (fileChanged) {
        fs.writeFileSync(filePath, content, 'utf-8');
        totalFiles++;
        console.log(`DONE: ${config.file}`);
    } else {
        console.log(`NO CHANGE: ${config.file}`);
    }
}

console.log(`\nCompleted: ${totalFiles} files updated, ${totalChanges} translations applied`);
