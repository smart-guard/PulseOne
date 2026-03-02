/**
 * 남은 UI 파일 추가 정밀 처리 v13
 * - DeviceRtuMonitorTab 남은 한글
 * - DeviceDataPointsTab 남은 한글
 * - DataPointModal 남은 한글  
 * - DeviceTemplateWizard 남은 한글
 * - ProtocolDashboard 남은 한글
 * - ProtocolEditor 남은 한글
 * - DeviceBasicInfoTab 남은 한글
 * - DeviceDetailModal 추가
 */
const fs = require('fs');
const path = require('path');

function fix(fp, repls) {
    if (!fs.existsSync(fp)) return 0;
    let c = fs.readFileSync(fp, 'utf-8');
    let n = 0;
    for (const [a, b] of repls) {
        if (typeof a === 'string') {
            if (c.includes(a)) { c = c.split(a).join(b); n++; }
        } else {
            const prev = c;
            c = c.replace(a, b);
            if (c !== prev) n++;
        }
    }
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log('FIXED', n, path.basename(fp)); }
    return n;
}

let total = 0;

// ============ DeviceRtuMonitorTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    [/>통신 테스트</g, ">Communication Test<"],
    [">최근 통신 로그<", ">Recent Comm Logs<"],
    [">성공률<", ">Success Rate<"],
    [">총 시도<", ">Total Attempts<"],
    [">총 성공<", ">Total Success<"],
    [">총 실패<", ">Total Failed<"],
    [">평균 응답시간<", ">Avg Response Time<"],
    [">연결 정보<", ">Connection Info<"],
    [">드라이버 상태<", ">Driver Status<"],
    [">드라이버 버전<", ">Driver Version<"],
    [">폴링 간격<", ">Polling Interval<"],
    [">마지막 통신<", ">Last Communication<"],
    [">초/분/시간 단위 통계<", ">Per second/min/hour stats<"],
    [/ms\s*\)/g, "ms)"],
]);

// ============ DeviceDataPointsTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    ["><label>이름 *</label>", "><label>Name *</label>"],
    ["><label>접근 모드</label>", "><label>Access Mode</label>"],
    [">포인트 활성화</", ">Enable Point</"],
    ["setFormData({ ...dp }); // 복사", "setFormData({ ...dp }); // copy"],
    ["id: Date.now() + Math.random(), // 임시 ID", "id: Date.now() + Math.random(), // temp ID"],
]);

// ============ DataPointModal.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DataPointModal.tsx', [
    ["><label>주소 *</label>", "><label>Address *</label>"],
    ['placeholder="데이터포인트 이름"', 'placeholder="Data point name"'],
    ["포인트명", "Point Name"],
    [">없음<", ">N/A<"],
    [">읽기 테스트<", ">Read Test<"],
    [">쓰기 테스트<", ">Write Test<"],
    [">스케일링<", ">Scaling<"],
    [">오프셋<", ">Offset<"],
    [">최소값<", ">Min<"],
    [">최대값<", ">Max<"],
    [">단위<", ">Unit<"],
    [">비고<", ">Notes<"],
]);

// ============ DeviceTemplateWizard.tsx ============
total += fix('/app/src/components/modals/DeviceTemplateWizard.tsx', [
    [">기본 정보<", ">Basic Info<"],
    [">접속 설정<", ">Connection Settings<"],
    [">데이터 포인트<", ">Data Points<"],
    [">고급 설정<", ">Advanced Settings<"],
    [">검증 설정<", ">Validation Settings<"],
    [">이상치 탐지<", ">Outlier Detection<"],
    [">데드밴드<", ">Deadband<"],
    [">템플릿 선택<", ">Select Template<"],
    [">마스터 모델 사용<", ">Use Master Model<"],
    ["title: '저장 완료'", "title: 'Saved'"],
    ["title: '생성 완료'", "title: 'Created'"],
    ["title: '수정 완료'", "title: 'Updated'"],
]);

// ============ ProtocolDashboard.tsx ============
total += fix('/app/src/components/protocols/ProtocolDashboard.tsx', [
    [">연결 상태<", ">Connection Status<"],
    [">연결된 인스턴스<", ">Connected Instances<"],
    [">연결된 장치<", ">Connected Devices<"],
    [">오류 로그<", ">Error Logs<"],
    [">편집 모드<", ">Edit Mode<"],
    [">기본 설정<", ">Basic Settings<"],
    [">프로토콜 이름<", ">Protocol Name<"],
    [">기본 포트<", ">Default Port<"],
    [">카테고리<", ">Category<"],
    [">데이터 타입<", ">Data Types<"],
    [">작업 방식<", ">Operations<"],
    ["title: '변경사항 저장'", "title: 'Save Changes'"],
    ["confirmText: '저장'", "confirmText: 'Save'"],
]);

// ============ ProtocolEditor.tsx ============
total += fix('/app/src/components/modals/ProtocolModal/ProtocolEditor.tsx', [
    [">프로토콜 명칭<", ">Protocol Name<"],
    [">기본 포트<", ">Default Port<"],
    [">시리얼 지원<", ">Serial Support<"],
    [">브로커 필수<", ">Broker Required<"],
    [">기본 폴링 간격<", ">Default Polling Interval<"],
    [">카테고리<", ">Category<"],
    [">설명<", ">Description<"],
    [">지원 데이터 타입<", ">Supported Data Types<"],
    [">지원 작업<", ">Supported Operations<"],
    [">저장<", ">Save<"],
    [">취소<", ">Cancel<"],
]);

// ============ DeviceBasicInfoTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx', [
    [">디바이스 명*<", ">Device Name*<"],
    [">디바이스 명<", ">Device Name<"],
    [">사이트<", ">Site<"],
    [">콜렉터<", ">Collector<"],
    [">프로토콜 인스턴스<", ">Protocol Instance<"],
    [">제조사<", ">Manufacturer<"],
    [">마스터 모델<", ">Master Model<"],
    [">시리얼 번호<", ">Serial Number<"],
    [">태그<", ">Tags<"],
    [">Slave ID<", ">Slave ID<"],
    ["title: '연결 테스트'", "title: 'Connection Test'"],
    ["confirmText: '테스트 실행'", "confirmText: 'Run Test'"],
]);

console.log('\nTotal:', total);
