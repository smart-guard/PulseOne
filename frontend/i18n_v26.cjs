/**
 * MainLayout + DataPointModal + DeviceBasicInfoTab + ProtocolDashboard + MqttBrokerDashboard 처리 v26
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

// ====== MainLayout.tsx ======
total += fix('/app/src/components/layout/MainLayout.tsx', [
    ["title: '대시보드'", "title: 'Dashboard'"],
    ["title: '디바이스 관리'", "title: 'Device Management'"],
    ["title: '디바이스 목록'", "title: 'Device List'"],
    ["title: '디바이스 추가'", "title: 'Add Device'"],
    ["title: '디바이스 그룹'", "title: 'Device Groups'"],
    ["title: '프로토콜 설정'", "title: 'Protocol Settings'"],
    ["title: '데이터 관리'", "title: 'Data Management'"],
    ["title: '데이터 익스플로러'", "title: 'Data Explorer'"],
    ["title: '실시간 데이터'", "title: 'Real-time Data'"],
    ["title: '이력 데이터'", "title: 'Historical Data'"],
]);

// ====== DataPointModal.tsx ======
total += fix('/app/src/components/modals/DeviceModal/DataPointModal.tsx', [
    ["delete newParams.bit_index; // 빈 값이면 삭제", "delete newParams.bit_index; // delete if empty"],
    ['placeholder="Register 비트 추출용 (예: 0)"', 'placeholder="For register bit extraction (e.g., 0)"'],
    ["<label>접근 모드</label>", "<label>Access Mode</label>"],
    ["<option value=\"write\">쓰기 전용</option>", "<option value=\"write\">Write Only</option>"],
    ["<option value=\"read_write\">읽기/쓰기</option>", "<option value=\"read_write\">Read/Write</option>"],
    ["<option value=\"control\">제어 전용</option>", "<option value=\"control\">Control Only</option>"],
    ["{dataPoint?.unit || '없음'}", "{dataPoint?.unit || 'N/A'}"],
    ['placeholder="예: °C, kWh, m/s"', 'placeholder="e.g., °C, kWh, m/s"'],
    ["{/* 스케일링 및 범위 */}", "{/* Scaling & Range */}"],
    ["<h3>📐 스케일링 및 범위</h3>", "<h3>📐 Scaling & Range</h3>"],
]);

// ====== DeviceBasicInfoTab.tsx ======
total += fix('/app/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx', [
    ["{/* 시스템 관리자용 테넌트 선택 (목록이 있을 때만 표시) */}", "{/* Tenant selection for system admin (only shown when list exists) */}"],
    ["<option value=\"ACTUATOR\">액추에이터</option>", "<option value=\"ACTUATOR\">Actuator</option>"],
    ["{/* 2. 제조사 정보 */}", "{/* 2. Manufacturer Info */}"],
    ["{/* 기존에 목록에 없는 수동 입력값 지원 */}", "{/* support manually entered values not in list */}"],
    ["(수동 입력됨)", "(manual input)"],
    ["{/* 3. 운영 설정 */}", "{/* 3. Operation Settings */}"],
    ["{/* 4. 네트워크 설정 (Moved here) */}", "{/* 4. Network Settings (moved here) */}"],
    ["{/* 🔥 NEW: 프로토콜 인스턴스 선택 (인스턴스가 존재할 때만 표시) */}", "{/* Protocol instance selection (only shown when instances exist) */}"],
    ["// 수동으로 바꾸면 자동 매칭 표시 제거", "// remove auto-match indicator when changed manually"],
    ["{/* 🔥 MQTT Base Topic (MQTT 일 때 엔드포인트 바로 아래 배치) */}", "{/* MQTT Base Topic (placed below endpoint for MQTT) */}"],
]);

// ====== ProtocolDashboard.tsx ======
total += fix('/app/src/components/protocols/ProtocolDashboard.tsx', [
    [">연결된 장치 목록 (", ">Connected Device List ("],
    [">장치 관리에서 보기</", ">View in Device Management</"],
    [">장치명</th>", ">Device Name</th>"],
    [">사이트</th>", ">Site</th>"],
    [">상태</th>", ">Status</th>"],
    ["changes.push(`시리얼 지원: ${protocol.uses_serial}", "changes.push(`Serial support: ${protocol.uses_serial}"],
]);

// ====== MqttBrokerDashboard.tsx ======
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    [">저장 완료</", ">Saved</"],
    ["> 사양 편집 모드</", "> Spec Edit Mode</"],
    ["> 상태 새로고침</i>", "> Refresh Status</i>"],
]);

console.log('\nTotal:', total);
