/**
 * v34 - MqttBrokerDashboard + 남은 UI 파일들 정밀 처리
 */
const fs = require('fs');
const path = require('path');

function fix(fp, repls) {
    if (!fs.existsSync(fp)) return 0;
    let c = fs.readFileSync(fp, 'utf-8');
    let n = 0;
    for (const [a, b] of repls) {
        if (c.includes(a)) { c = c.split(a).join(b); n++; }
    }
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log(`FIXED ${n} ${path.basename(fp)}`); }
    return n;
}

let total = 0;

// MqttBrokerDashboard - 실제 라인 기반 직접 치환
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    [">{protocol.display_name} 관리 센터</h2>", ">{protocol.display_name} Management Center</h2>"],
    ["'MQTT Broker 서비스 Operations 및 Security Settings'", "'MQTT Broker Service Operations and Security Settings'"],
    ["> 하드웨어 리소스 지표\n", "> Hardware Resource Metrics\n"],
    ["> 메시징 큐 통계\n", "> Messaging Queue Statistics\n"],
    [">Created 전용 Queue (Queues)</span>", ">Dedicated Queues</span>"],
    [">Active Con슈머 (Consumers)</span>", ">Active Consumers</span>"],
    ["> 기본 정보\n", "> Basic Info\n"],
    [">표시 명칭 (Display Name)</label>", ">Display Name</label>"],
    [">Protocol 상세 Description</label>", ">Protocol Description</label>"],
    [">Formula 벤더</label>", ">Vendor</label>"],
    [">표준 Port</label>", ">Standard Port</label>"],
    [">기술 Category</label>", ">Tech Category</label>"],
    [">최소 Support Version</label>", ">Min Supported Version</label>"],
    ["> 통신 파라미터\n", "> Communication Parameters\n"],
    [">응답 Waiting 제한 Time (ms)</label>", ">Response Timeout (ms)</label>"],
    [">최대 동시 Session 수</label>", ">Max Concurrent Sessions</label>"],
    [">{protocol.max_concurrent_connections} 세션</div>", ">{protocol.max_concurrent_connections} sessions</div>"],
    [">Support 기능 (Capabilities)</h4>", ">Supported Capabilities</h4>"],
    [">MQTT Broker 서버 Active화</h3>", ">Enable MQTT Broker Server</h3>"],
    [">외부 장치 및 센서의 실Time 데이터 접속을 허용합니다.</p>", ">Allow real-time data access from external devices and sensors.</p>"],
    ["'동작 중 (RUNNING)'", "'Running (RUNNING)'"],
    ["'비Active (STOPPED)'", "'Inactive (STOPPED)'"],
    ["> 브로커 접속 및 인증 관리\n", "> Broker Access & Auth Management\n"],
    [">접속 URL (Endpoint)</label>", ">Connection URL (Endpoint)</label>"],
    [">vhost (기본: /)</label>", ">vhost (default: /)</label>"],
    [">센서 전용 Authentication 토큰 (API Key)</h4>", ">Sensor-only Auth Token (API Key)</h4>"],
]);

// TemplateManagementTab - style 속성 내 한글 라벨들
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    [">태그 삽입<", ">Insert Tag<"],
    [">JSON 배열 강제 (|array)<", ">Force JSON Array (|array)<"],
]);

// ExportGatewayWizard - 남은 것
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    [">전송 모드 가이드<", ">Transmission Mode Guide<"],
]);

// AlarmTemplateCreateEditModal-Wizard
total += fix('/app/src/components/modals/AlarmTemplateCreateEditModal-Wizard.tsx', [
    ['심각도', 'Severity'],
    ['알람 조건', 'Alarm Condition'],
    ['알람 메시지', 'Alarm Message'],
    ['알람 설명', 'Alarm Description'],
    ['통지 활성화', 'Enable Notification'],
    ['자동 해제', 'Auto Clear'],
]);

// DeviceDetailModal
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ['{/* 스타일 */}', '{/* Styles */}'],
    ['{/* 모달 헤더 */}', '{/* Modal Header */}'],
    ['{/* 탭 컨텐츠 */}', '{/* Tab Content */}'],
]);

// DeviceDataPointsBulkModal
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ['// 벌크 데이터포인트 관리', '// bulk datapoint management'],
    ['// 포인트 선택 상태', '// point selection state'],
    ['// 전체 선택 토글', '// toggle select all'],
    ['// 선택된 포인트에 적용', '// apply to selected points'],
    ['// 벌크 업데이트 실행', '// execute bulk update'],
    ['// 결과 처리', '// process result'],
]);

// DeviceSettingsTab
total += fix('/app/src/components/modals/DeviceModal/DeviceSettingsTab.tsx', [
    ['// 파라미터 업데이트', '// parameter update'],
    ['// 렌더링 헬퍼', '// rendering helper'],
    ['// 메인 렌더링', '// main rendering'],
]);

console.log('\nTotal:', total);
