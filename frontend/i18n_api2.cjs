/**
 * virtualPointsApi.ts + 남은 console 한글 처리
 * regex 기반 - console.log/error 한글 → 영어
 */
const fs = require('fs');
const path = require('path');

const CONSOLE_FILES = [
    '/app/src/api/services/virtualPointsApi.ts',
    '/app/src/api/services/realtimeApi.ts',
    '/app/src/api/services/alarmTemplatesApi.ts',
];

let total = 0;

for (const fp of CONSOLE_FILES) {
    if (!fs.existsSync(fp)) continue;
    let c = fs.readFileSync(fp, 'utf-8');
    const original = c;

    // console.log('가상포인트 목록 조회:', ...) 패턴 → 영어
    // 가상포인트 → Virtual Point
    c = c.replace(/가상포인트 목록 조회/g, 'Virtual point list query');
    c = c.replace(/가상포인트 상세 조회/g, 'Virtual point detail query');
    c = c.replace(/가상포인트 생성/g, 'Virtual point create');
    c = c.replace(/가상포인트 수정/g, 'Virtual point update');
    c = c.replace(/가상포인트 삭제/g, 'Virtual point delete');
    c = c.replace(/가상포인트 조회 실패/g, 'Virtual point query failed');
    c = c.replace(/가상포인트 목록 조회 실패/g, 'Virtual point list failed');
    c = c.replace(/`HTTP \$\{response\.status\}: 가상포인트 목록 조회 실패`/g, '`HTTP ${response.status}: Virtual point list failed`');
    c = c.replace(/가상 포인트/g, 'Virtual point');
    c = c.replace(/알람 템플릿 목록 조회/g, 'Alarm template list query');
    c = c.replace(/알람 템플릿 조회 실패/g, 'Alarm template query failed');
    c = c.replace(/알람 템플릿 생성/g, 'Alarm template create');
    c = c.replace(/알람 템플릿 수정/g, 'Alarm template update');
    c = c.replace(/알람 템플릿 삭제/g, 'Alarm template delete');
    c = c.replace(/실시간 데이터 조회/g, 'Realtime data query');
    c = c.replace(/실시간 데이터 구독/g, 'Realtime data subscription');
    c = c.replace(/실시간 연결/g, 'Realtime connection');
    c = c.replace(/조회 실패/g, 'Query failed');
    c = c.replace(/생성 실패/g, 'Create failed');
    c = c.replace(/수정 실패/g, 'Update failed');
    c = c.replace(/삭제 실패/g, 'Delete failed');

    if (c !== original) {
        const n = (c.match(/[\uAC00-\uD7AF]/g) || []).length;
        const was = (original.match(/[\uAC00-\uD7AF]/g) || []).length;
        fs.writeFileSync(fp, c, 'utf-8');
        console.log(`FIXED ${path.basename(fp)}: ${was} → ${n} Korean chars remaining`);
        total++;
    }
}

console.log('\nDone, files fixed:', total);
