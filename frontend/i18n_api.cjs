/**
 * API 서비스 파일 + scriptEngine.ts 한글 → 영어
 * 이 파일들은 console.error, throw Error, 주석만 한글
 */
const fs = require('fs');
const path = require('path');

function fixFile(fp, replacements) {
    if (!fs.existsSync(fp)) { console.log('SKIP:', path.basename(fp)); return 0; }
    let c = fs.readFileSync(fp, 'utf-8');
    let n = 0;
    for (const [a, b] of replacements) {
        if (c.includes(a)) { c = c.split(a).join(b); n++; }
    }
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log('FIXED', n, path.basename(fp)); }
    return n;
}

// API 서비스들: throw/console 한글을 영어로
const apiFiles = [
    '/app/src/api/services/alarmApi.ts',
    '/app/src/api/services/deviceApi.ts',
    '/app/src/api/services/virtualPointsApi.ts',
    '/app/src/api/services/realtimeApi.ts',
    '/app/src/api/services/alarmTemplatesApi.ts',
    '/app/src/api/services/auditLogApi.ts',
    '/app/src/api/services/exportGatewayApi.ts',
];

// 공통 한글 에러 메시지 → 영어 (API 파일 공통)
const commonApiReplacements = [
    ["'알 수 없는 오류가 발생했습니다'", "'An unknown error occurred'"],
    ["'알 수 없는 오류'", "'Unknown error'"],
    ["'서버 오류가 발생했습니다'", "'Server error occurred'"],
    ["'서버와 통신할 수 없습니다'", "'Cannot communicate with server'"],
    ["'데이터를 불러오는데 실패했습니다'", "'Failed to load data'"],
    ["'저장에 실패했습니다'", "'Save failed'"],
    ["'삭제에 실패했습니다'", "'Delete failed'"],
    ["'생성에 실패했습니다'", "'Create failed'"],
    ["'수정에 실패했습니다'", "'Update failed'"],
    ["'인증이 필요합니다'", "'Authentication required'"],
    ["'権限がありません'", "'No permission'"],
    ["'권한이 없습니다'", "'No permission'"],
    ["'네트워크 오류'", "'Network error'"],
    ["'요청 시간 초과'", "'Request timeout'"],
    ["'잘못된 요청'", "'Invalid request'"],
];

let total = 0;
for (const fp of apiFiles) {
    total += fixFile(fp, commonApiReplacements);
}

// alarmApi.ts 특수
total += fixFile('/app/src/api/services/alarmApi.ts', [
    ["'알람 목록 조회 실패'", "'Failed to fetch alarm list'"],
    ["'알람 상세 조회 실패'", "'Failed to fetch alarm detail'"],
    ["'알람 상태 업데이트 실패'", "'Failed to update alarm status'"],
    ["'알람 통계 조회 실패'", "'Failed to fetch alarm statistics'"],
    ["'알람 규칙 조회 실패'", "'Failed to fetch alarm rules'"],
    ["'알람 생성 실패'", "'Failed to create alarm'"],
    ["'알람 삭제 실패'", "'Failed to delete alarm'"],
]);

// deviceApi.ts 특수
total += fixFile('/app/src/api/services/deviceApi.ts', [
    ["'디바이스 목록 조회 실패'", "'Failed to fetch device list'"],
    ["'디바이스 상세 조회 실패'", "'Failed to fetch device detail'"],
    ["'디바이스 생성 실패'", "'Failed to create device'"],
    ["'디바이스 수정 실패'", "'Failed to update device'"],
    ["'디바이스 삭제 실패'", "'Failed to delete device'"],
    ["'디바이스 통계 조회 실패'", "'Failed to fetch device statistics'"],
    ["'프로토콜 목록 조회 실패'", "'Failed to fetch protocol list'"],
    ["'사이트 목록 조회 실패'", "'Failed to fetch site list'"],
    ["'제조사 목록 조회 실패'", "'Failed to fetch manufacturer list'"],
]);

// GatewayListTab.tsx 남은 한글 추가 처리
total += fixFile('/app/src/pages/export-gateway/tabs/GatewayListTab.tsx', [
    ["> 설정 수정</", "> Edit Settings</"],
    [">설정 수정<", ">Edit Settings<"],
]);

console.log('\nTotal API fixes:', total);
