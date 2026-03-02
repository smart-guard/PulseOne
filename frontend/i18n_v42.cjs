/**
 * v42 - 8개 파일 잔여 한글 처리
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

// DeviceDetailModal
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ['// 데이터Point 로드', '// load data points'],
    ['// 데이터Point Management 함수들', '// data point management functions'],
    ['// 🎨 예쁜 Save 함수 (브라우저 Default 팝업 대신 커스텀 모달)', '// 🎨 custom save function (instead of browser default popup)'],
]);

// DeviceDataPointsBulkModal
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ['// 값 파싱 및 검증', '// parse and validate values'],
    ['// 빈 행은 에러 처리하지 않음 (Save 시 Filter링)', '// skip error for empty rows (filtered on save)'],
    ['// DB 내 중복 체크 (비트 Point는 같은 Address를 여러 Point가 공유하므로 너뜀)', '// skip duplicate check for bit points (same address shared across multiple points)'],
]);

// ActiveAlarms
total += fix('/app/src/pages/ActiveAlarms.tsx', [
    ['// 전역 뱃지 재조회 (AlarmContext 미OK 수 동기화)', '// re-fetch global badge (sync unacknowledged count in AlarmContext)'],
    ["alert(`${actionType === 'acknowledge' ? 'OK' : 'Deselect'} Processing Error가 발생했.`)", "alert(`${actionType === 'acknowledge' ? 'Acknowledge' : 'Deselect'} processing error occurred.`)"],
    ['>정지</>', '>Paused</>'],
    ["title={autoRefresh ? '자동 Refresh Stop' : '자동 Refresh Start'}", "title={autoRefresh ? 'Stop Auto Refresh' : 'Start Auto Refresh'}"],
]);

// InputVariableEditor
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/InputVariableEditor.tsx', [
    ['`Input Variable \\"${variable.input_name}\\"을(를) 정말 Delete??`', '`Really delete Input Variable \\"${variable.input_name}\\"?`'],
    ["title: '탭 이동 안내'", "title: 'Move to Next Tab'"],
    ["message: 'Variable가 Add되었. Formula 작성 단계로 이동할까요?'", "message: 'Variable added. Move to the formula writing step?'"],
    ["confirmText: '이동하기'", "confirmText: 'Move'"],
]);

// ManufacturerDetailModal - 국가명 키 오브젝트 설명 주석만
// (국가명 키 자체가 한국어인 경우 타입 매핑 목적상 유지가 맞음)

// ProtocolInstanceManager
total += fix('/app/src/components/protocols/ProtocolInstanceManager.tsx', [
    ['// 간단한 JWT 파싱 유틸리티', '// simple JWT parsing utility'],
    ['// 🔥 발 환경 더미 토큰 처리', '// 🔥 handle dev environment dummy token'],
    ['// role이 없을 경우 대비, 혹은 대소문자 이슈 처리', '// handle missing role or case sensitivity issues'],
    ["// message.error('Instance 목록을 불러오는데 Failed했.'); // Removed Ant Design specific message", "// message.error('Failed to load instance list.'); // Removed Ant Design specific message"],
]);

// Dashboard
total += fix('/app/src/pages/Dashboard.tsx', [
    ["confirmText: 'Start하기'", "confirmText: 'Start'"],
    ["title: '서비스 Stop'", "title: 'Stop Service'"],
    ['`${displayName}를 Stop??\\n\\nStop하면 관련된 모든 기능이 d시적으로 Usecannot be done.`', '`Stop ${displayName}?\\n\\nAll related functions will be temporarily unavailable.`'],
    ["confirmText: 'Stop하기'", "confirmText: 'Stop'"],
]);

// RealTimeMonitor
total += fix('/app/src/pages/RealTimeMonitor.tsx', [
    ['// Filter/Search 조이 실제로 바뀔 때만 1페이지로 이동 (allData 갱신 시엔 페이지 유지)', '// go to page 1 only when filter/search conditions actually change (preserve page on allData update)'],
    ['// ISO 타임스탬프 감지 → 오늘이면 Time만, 다른 날이면 Date+Time', '// detect ISO timestamp → show time only if today, date+time otherwise'],
    ['// ✍️ Point 제어 함수 (모달에서 호출)', '// ✍️ point control function (called from modal)'],
    ['setTimeout(() => setControlModal(null), 1500); // 성공 후 모달 닫기', 'setTimeout(() => setControlModal(null), 1500); // close modal after success'],
]);

console.log('\nTotal:', total);
