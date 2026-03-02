/**
 * DeviceDetailModal/DeviceDataPointsBulkModal/DeviceRtuMonitorTab 남은 렌더링 한글 처리 v15
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

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

// 남은 한글 추출 및 주요 패턴 교체
let total = 0;

// DeviceDetailModal - 남은 것 확인
const ddm = fs.readFileSync('/app/src/components/modals/DeviceDetailModal.tsx', 'utf-8');
const ddmKoLines = ddm.split('\n').filter((l, i) => {
    return koreanRe.test(l) && !l.trim().startsWith('//') && !l.includes("t('") && !l.includes('console.');
}).slice(0, 15);
console.log('DeviceDetailModal remaining:');
ddmKoLines.forEach(l => console.log(' ', l.trim().substring(0, 90)));

// DeviceDataPointsBulkModal 알림 패턴 재처리
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    // 이전에 비슷한 패턴을 처리했으나 미처리 부분
    ["title: '알림'", "title: 'Notice'"],
    ["message: '템플릿 정보를 불러오는 데 실패했습니다.'", "message: 'Failed to load template information.'"],
    ["message: '이 템플릿에는 정의된 포인트가 없습니다.'", "message: 'This template has no defined points.'"],
    ["message: '템플릿 적용 중 오류가 발생했습니다.'", "message: 'Error applying template.'"],
    // 탭 레이블
    [">JSON/CSV 직접 입력<", ">JSON/CSV Direct Input<"],
    [">템플릿 적용<", ">Apply Template<"],
    [">포인트 목록<", ">Point List<"],
    [">행 번호<", ">Row<"],
    [">오류 내용<", ">Error<"],
    [">유효성 결과<", ">Validation Result<"],
    [">포인트명<", ">Point Name<"],
    [">데이터 타입<", ">Data Type<"],
    [">주소<", ">Address<"],
    [">접근 모드<", ">Access Mode<"],
    [">제어<", ">Control<"],
    [">삭제<", ">Delete<"],
    [">추가<", ">Add<"],
    // 오류 메시지
    ["errors.push('유효하지 않은 타입')", "errors.push('Invalid type')"],
    ["errors.push('유효하지 않은 권한')", "errors.push('Invalid access mode')"],
    ["errors.push('이미 존재하는 주소(DB)')", "errors.push('Address already exists in DB')"],
    // placeholder
    ['placeholder="포인트 수 기준"', 'placeholder="Based on point count"'],
]);

// DeviceRtuMonitorTab 남은 한글 - 스탯 레이블
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    // 남은 JSX 한글
    [">통신 상태<", ">Communication Status<"],
    [">드라이버 연결<", ">Driver Connection<"],
    [">마지막 폴링<", ">Last Poll<"],
    [">통신 간격<", ">Comm Interval<"],
    [">정상<", ">Normal<"],
    [">이상<", ">Abnormal<"],
    [">연결 정보<", ">Connection Info<"],
    ['// 95% 성공률', '// 95% success rate'],
]);

// DeviceTemplateWizard 남은 것들
total += fix('/app/src/components/modals/DeviceTemplateWizard.tsx', [
    ["title: '디바이스 등록 완료'", "title: 'Device Registered'"],
    ["title: '디바이스 생성 완료'", "title: 'Device Created'"],
    ["message: '아래 내용으로 디바이스를 등록합니다.'", "message: 'Register device with the following details.'"],
    [">디바이스 등록<", ">Register Device<"],
    [">수정하기<", ">Edit<"],
    [">등록하기<", ">Register<"],
    [">설정 확인<", ">Review Settings<"],
    [">이전<", ">Previous<"],
    [">다음<", ">Next<"],
    [">완료<", ">Complete<"],
]);

// DeviceDataPointsTab 남은 것들
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    ["'(디바이스 전체", "'(Save all device"],
    ["서버에 최종 저장됩니다.", "will be saved to server."],
    [">새 포인트 추가<", ">Add New Point<"],
    [">읽기 테스트<", ">Read Test<"],
    [">쓰기 테스트<", ">Write Test<"],
    [">일괄 추가<", ">Bulk Add<"],
    [">선택 삭제<", ">Delete Selected<"],
]);

console.log('\nTotal:', total);
