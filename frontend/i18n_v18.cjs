/**
 * DeviceRtuMonitorTab + DeviceDetailModal + ExportGatewayWizard 남은 한글 v18
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

// DeviceRtuMonitorTab
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    [">평균 응답시간</label>", ">Avg Response Time</label>"],
    [">수신 바이트<", ">RX Bytes<"],
    [">송신 바이트<", ">TX Bytes<"],
    [">패킷 오류<", ">Packet Errors<"],
    [">재시도 횟수<", ">Retry Count<"],
    [">연속 실패<", ">Consecutive Failures<"],
    [">Modbus RTU 통신 모니터링<", ">Modbus RTU Communication Monitor<"],
    [">모니터링 시간 범위<", ">Monitoring Time Range<"],
    [">데이터 새로고침<", ">Refresh Data<"],
    [">모니터링 시작<", ">Start Monitoring<"],
    [">모니터링 중지<", ">Stop Monitoring<"],
]);

// DeviceDetailModal
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ["'연결됨'", "'Connected'"],
    ["'연결끊김'", "'Disconnected'"],
    ["'알 수 없음'", "'Unknown'"],
    [">기본 정보<", ">Basic Info<"],
    [">데이터 포인트<", ">Data Points<"],
    [">설정<", ">Settings<"],
    [">RTU 네트워크<", ">RTU Network<"],
    [">로그<", ">Log<"],
    [">모니터링<", ">Monitoring<"],
    [">저장<", ">Save<"],
    [">닫기<", ">Close<"],
    [">삭제<", ">Delete<"],
    [">편집 시작<", ">Start Edit<"],
    [">편집 취소<", ">Cancel Edit<"],
    ["title: '디바이스 삭제'", "title: 'Delete Device'"],
    ["• 이 작업은 되돌릴 수 없습니다.", "• This action cannot be undone."],
    ["• 연결된 알람도 함께 삭제됩니다.", "• All associated alarms will also be deleted."],
    ["confirmText: '삭제'", "confirmText: 'Delete'"],
]);

// ExportGatewayWizard
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    [">게이트웨이 편집<", ">Edit Gateway<"],
    [">게이트웨이 생성<", ">Create Gateway<"],
    [">기본 정보<", ">Basic Info<"],
    [">프로파일<", ">Profile<"],
    [">템플릿<", ">Template<"],
    [">타겟<", ">Target<"],
    [">완료<", ">Complete<"],
    [">저장<", ">Save<"],
    [">이전<", ">Previous<"],
    [">다음<", ">Next<"],
    [">닫기<", ">Close<"],
    [">스케줄<", ">Schedule<"],
    ["미러 타겟 설정", "Mirror Target Settings"],
    ["타겟 연결 설정", "Target Connection Settings"],
]);

console.log('\nTotal:', total);
