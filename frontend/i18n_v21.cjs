/**
 * ProtocolEditor + MqttBrokerDashboard + InputVariableSourceSelector + DeviceRtuNetworkTab + DeviceDetailModal 처리 v21
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
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log('FIXED', n, path.basename(fp)); }
    return n;
}

let total = 0;

// ====== ProtocolEditor.tsx ======
total += fix('/app/src/components/modals/ProtocolModal/ProtocolEditor.tsx', [
    ["err.message : '저장 실패'", "err.message : 'Save failed'"],
    ["title: 'API 키 재발급'", "title: 'Regenerate API Key'"],
    ["message: '새로운 API 키를 발급하시겠습니까? 기존 키를 사용하는 장치들의 연결이 끊어질 수 있습니다.'", "message: 'Issue a new API key? Devices using the existing key may lose connection.'"],
    ["'프로토콜 설정 편집'", "'Protocol Settings Edit'"],
    ["'프로토콜 상세보기'", "'Protocol Detail View'"],
    ["{/* 모달 헤더 */}", "{/* Modal Header */}"],
    ["<div>프로토콜 정보를 불러오는 중...</div>", "<div>Loading protocol info...</div>"],
    ["{/* 기본 정보 */}", "{/* Basic Info */}"],
    ["<h3><i className=\"fas fa-info-circle\"></i> 기본 정보</h3>", "<h3><i className=\"fas fa-info-circle\"></i> Basic Info</h3>"],
    ["<label className=\"required\">프로토콜 타입</label>", "<label className=\"required\">Protocol Type</label>"],
    ["title=\"시스템 정의 값으로 변경할 수 없습니다.\"", "title=\"Cannot change system-defined value.\""],
    ["<label className=\"required\">표시명</label>", "<label className=\"required\">Display Name</label>"],
    ['placeholder="예: Modbus TCP"', 'placeholder="e.g., Modbus TCP"'],
    ['placeholder="프로토콜에 대한 내용을 입력하세요"', 'placeholder="Enter protocol description"'],
    ["><option value=\"industrial\">산업용</option>", "><option value=\"industrial\">Industrial</option>"],
    ["><option value=\"building\">건물관리</option>", "><option value=\"building\">Building Management</option>"],
    ["><option value=\"iot\">IoT</option>", "><option value=\"iot\">IoT</option>"],
    ["><option value=\"telecom\">통신\b</option>", "><option value=\"telecom\">Telecom</option>"],
]);

// ====== MqttBrokerDashboard.tsx ======
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    ["> 상태 새로고침</i>", "> Refresh Status</i>"],
    ["<span>메모리 안정성 임계치</span>", "<span>Memory Stability Threshold</span>"],
    ["본 지표는 PulseOne 내장 RabbitMQ 서버의 실시간 통계를 바탕으로 합니다.", "These metrics are based on real-time statistics from PulseOne's built-in RabbitMQ server."],
    ["<span style={{ color: 'var(--neutral-600)', fontSize: '13px', fontWeight: 600 }}>생성된 ", "<span style={{ color: 'var(--neutral-600)', fontSize: '13px', fontWeight: 600 }}>Created "],
    ["<span style={{ color: 'var(--neutral-600)', fontSize: '13px', fontWeight: 600 }}>활성 컨", "<span style={{ color: 'var(--neutral-600)', fontSize: '13px', fontWeight: 600 }}>Active Con"],
    [">클라이언트 엔드포인트</", ">Client Endpoint</"],
    [">계정 정보</th>", ">Account Info</th>"],
    [">세션 상태</th>", ">Session Status</th>"],
    [">프로토콜</th>", ">Protocol</th>"],
    [">연결 시점</th>", ">Connected At</th>"],
    [">접속된 클라이언트 센서가 없습니다.</span>", ">No connected client sensors.</span>"],
]);

// ====== InputVariableSourceSelector.tsx ======
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/InputVariableSourceSelector.tsx', [
    ["// 불필요한 호출 방지", "// prevent unnecessary calls"],
    ["// 함수 제거, 값만 의존", "// removed function, value-only dependency"],
    ["throw new Error(result.message || 'API 응답 구조 오류')", "throw new Error(result.message || 'Invalid API response structure')"],
    ["description: '평균 온도'", "description: 'Average Temperature'"],
    ["description: '에너지 효율'", "description: 'Energy Efficiency'"],
    ["'PLC-001 (보일러)'", "'PLC-001 (Boiler)'"],
    ["'WEATHER-001 (기상)'", "'WEATHER-001 (Weather)'"],
    ["'HVAC-001 (공조)'", "'HVAC-001 (HVAC)'"],
    ["// 디바이스 목록 먼저 로드", "// load device list first"],
    ["// sourceType만 의존", "// sourceType dependency only"],
    ["// 300ms로 줄여서 더 반응성 있게", "// reduced to 300ms for better responsiveness"],
    [">상수값은 직접 입력해주세요<", ">Enter constant value directly<"],
]);

// ====== DeviceRtuNetworkTab.tsx ======
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx', [
    ["// RTU 디바이스만 필터링", "// RTU devices only"],
    ["alert(`상태 변경 실패: ${error instanceof Error ? error.message : 'Unknown error'}`)", "alert(`Status change failed: ${error instanceof Error ? error.message : 'Unknown error'}`)"],
    ["`연결 성공! 응답시간: ${result.response_time_ms}ms`", "`Connection success! Response: ${result.response_time_ms}ms`"],
    ["`연결 실패: ${result.error_message}`", "`Connection failed: ${result.error_message}`"],
    ["alert(`테스트 실패: ${response.error}`)", "alert(`Test failed: ${response.error}`)"],
    ["alert(`연결 테스트 오류: ${error instanceof Error ? error.message : 'Unknown error'}`)", "alert(`Connection test error: ${error instanceof Error ? error.message : 'Unknown error'}`)"],
    ["text: '연결됨'", "text: 'Connected'"],
    ["text: '연결끊김'", "text: 'Disconnected'"],
    ["text: '연결중'", "text: 'Connecting'"],
    ["text: '오류'", "text: 'Error'"],
    ["text: '알수없음'", "text: 'Unknown'"],
    ["return '방금 전'", "return 'Just now'"],
    ["return `${diffMinutes}분 전`", "return `${diffMinutes}m ago`"],
    ["return `${Math.floor(diffMinutes / 60)}시간 전`", "return `${Math.floor(diffMinutes / 60)}h ago`"],
    ["return `${Math.floor(diffMinutes / 1440)}일 전`", "return `${Math.floor(diffMinutes / 1440)}d ago`"],
]);

// ====== DeviceDetailModal.tsx 잔여 ======
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ["> 2단계: 통신 및 운영 설정</", "> Step 2: Communication & Operation Settings</"],
    ["> 3단계: 장치 요약 및 포인트</", "> Step 3: Device Summary & Points</"],
    ["'(미입력)'", "'(not entered)'"],
    ["(기본값)", "(default)"],
]);

console.log('\nTotal:', total);
