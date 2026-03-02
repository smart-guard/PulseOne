/**
 * DeviceDataPointsTab + DataPointModal + DeviceBasicInfoTab + DeviceRtuScanDialog + ProtocolDashboard 처리 v20
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

// ====== DeviceDataPointsTab.tsx ======
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    ["<label>이름 *</label>", "<label>Name *</label>"],
    ["{/* Modbus 전용: Function Code 선택 */}", "{/* Modbus only: Function Code select */}"],
    ["{/* Modbus 전용: Bit Index (비트맵 레지스터) */}", "{/* Modbus only: Bit Index (bitmap register) */}"],
    ["<span className=\"hint\">한 레지스터를 비트 단위로 쪼갤 때 사용 (0=LSB)</span>", "<span className=\"hint\">Used to split one register into bits (0=LSB)</span>"],
    ["<label>접근 모드</label>", "<label>Access Mode</label>"],
    ["<label>그룹명 (Group)</label>", "<label>Group Name</label>"],
    ["<label>태그 (Tags, 콤마 구분)</label>", "<label>Tags (comma separated)</label>"],
    [">포인트 활성화\n", ">Enable Point\n"],
    ["<label>메타데이터 (JSON)</label>", "<label>Metadata (JSON)</label>"],
    ["<label>단위 (Unit)</label>", "<label>Unit</label>"],
    ["<label>스케일링 팩터 (Scale)</label>", "<label>Scaling Factor (Scale)</label>"],
    ["<label>오프셋 (Offset)</label>", "<label>Offset</label>"],
    ["<label>최솟값 (Min)</label>", "<label>Min Value</label>"],
    ["<label>최댓값 (Max)</label>", "<label>Max Value</label>"],
    ["<label>임계값 (Threshold)</label>", "<label>Threshold</label>"],
    ["<label>설명 (Description)</label>", "<label>Description</label>"],
]);

// ====== DataPointModal.tsx ======
total += fix('/app/src/components/modals/DeviceModal/DataPointModal.tsx', [
    ["{/* 헤더 */}", "{/* Header */}"],
    ["{/* 내용 */}", "{/* Content */}"],
    ["{/* 기본 정보 */}", "{/* Basic Info */}"],
    ["<h3>📊 기본 정보</h3>", "<h3>📊 Basic Info</h3>"],
    ["<label>주소 *</label>", "<label>Address *</label>"],
    ['placeholder="예: 40001, 1001, 0x1000"', 'placeholder="e.g., 40001, 1001, 0x1000"'],
    ["<label>주소 문자열</label>", "<label>Address String</label>"],
    ["{dataPoint?.address_string || '없음'}", "{dataPoint?.address_string || 'N/A'}"],
    ['placeholder="예: 4:40001, MB:1001"', 'placeholder="e.g., 4:40001, MB:1001"'],
    ["<label>데이터 타입 *</label>", "<label>Data Type *</label>"],
    ["<label>비트 인덱스 (선택, 0~15)</label>", "<label>Bit Index (optional, 0~15)</label>"],
    ["{dataPoint?.protocol_params?.bit_index ?? '없음'}", "{dataPoint?.protocol_params?.bit_index ?? 'N/A'}"],
]);

// ====== DeviceBasicInfoTab.tsx ======
total += fix('/app/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx', [
    ["// API 실패 시 빈 배열 (사용자 요청: 서버 데이터만 표시)", "// empty array on API fail (server data only by user request)"],
    ["// 충분히 크게", "// large enough"],
    ["alert(`❌ 테스트 실패: ${response.error}`)", "alert(`❌ Test failed: ${response.error}`)"],
    ["title: '연결 테스트 오류'", "title: 'Connection Test Error'"],
    ["message: `❌ 네트워크 또는 서버 통신에 문제가 발생했습니다.\\n${errMsg}`", "message: `❌ Network or server communication issue.\\n${errMsg}`"],
    ["alert(`❌ 연결 테스트 오류: ${errMsg}`)", "alert(`❌ Connection test error: ${errMsg}`)"],
    ["// 권한 있는 경우만 로드됨", "// only loaded if has permission"],
    ["// id가 바뀔 때(다른 기기 선택 시) 재초기화", "// re-init when id changes (different device selected)"],
    ["return '/dev/ttyUSB0 또는 COM1';", "return '/dev/ttyUSB0 or COM1';"],
    ["return '연결 주소를 입력하세요';", "return 'Enter connection address';"],
    ["{/* 4단 그리드 레이아웃 */}", "{/* 4-column grid layout */}"],
    ["{/* 1. 기본 정보 */}", "{/* 1. Basic Info */}"],
]);

// ====== DeviceRtuScanDialog.tsx ======
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx', [
    ["setError('RTU 디바이스가 아닙니다')", "setError('Not an RTU device')"],
    ["throw new Error(`스캔 요청 실패: ${response.statusText}`)", "throw new Error(`Scan request failed: ${response.statusText}`)"],
    [": '스캔 중 오류가 발생했습니다'", ": 'Error during scan'"],
    [": '디바이스 등록 중 오류가 발생했습니다'", ": 'Error during device registration'"],
    ["// protocol_id 사용", "// use protocol_id"],
    ["// 호환성을 위해 protocol_type도 포함", "// include protocol_type for compatibility"],
    ["{/* 헤더 */}", "{/* Header */}"],
    ["RTU 슬래이브 스캔\n", "RTU Slave Scan\n"],
    ["{/* 내용 */}", "{/* Content */}"],
    ["<h3>마스터 디바이스</h3>", "<h3>Master Device</h3>"],
    ["<h3>스캔 설정</h3>", "<h3>Scan Settings</h3>"],
]);

// ====== ProtocolDashboard.tsx ======
total += fix('/app/src/components/protocols/ProtocolDashboard.tsx', [
    ["changes.push(`시리얼 지원: ${protocol.uses_serial}", "changes.push(`Serial support: ${protocol.uses_serial}"],
    ["{/* 1. 상단 요약 영역 (공통) */}", "{/* 1. Top Summary Section (common) */}"],
    ["{/* 2. 메인 탭 영역 */}", "{/* 2. Main Tab Section */}"],
    ["// 🔥 하단 보더라인 및 그림자 공간 충분히 확보", "// ensure enough space for border and shadow"],
    ["// 🔥 그림자가 잘리지 않도록 허용", "// allow shadow to not be clipped"],
    ["{/* 탭 헤더 */}", "{/* Tab Headers */}"],
    ["{ id: 'info', label: '프로토콜 정보'", "{ id: 'info', label: 'Protocol Info'"],
    ["{ id: 'monitoring', label: '실시간 상태'", "{ id: 'monitoring', label: 'Live Status'"],
]);

console.log('\nTotal:', total);
