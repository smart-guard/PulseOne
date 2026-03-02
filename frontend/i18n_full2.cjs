/**
 * 두 번째 라운드 - 남은 한글 더 공격적으로 처리
 * i18n_full.cjs의 사전을 확장
 */
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const SKIP_PATTERNS = [
    '/api/services/',
    '/types/',
    'node_modules',
];

// 확장 사전 (더 많은 구/절 추가)
const DICT = [
    // 명시적 JSX 텍스트 패턴들
    ['페이로드 템플릿', 'Payload Template'],
    ['페이로드', 'Payload'],
    ['데이터 탐색기', 'Data Explorer'],
    ['추천 항목', 'Recommended'],
    ['매핑 명칭', 'Mapping Name'],
    ['데이터 값', 'Data Value'],
    ['고급 스마트 로직', 'Advanced Smart Logic'],
    ['스마트 로직', 'Smart Logic'],
    ['스케일링', 'Scaling'],
    ['오프셋', 'Offset'],
    ['데드밴드', 'Deadband'],
    ['임계값', 'Threshold'],
    ['임계치', 'Threshold'],
    ['임계', 'Threshold'],
    ['상한값', 'High Limit'],
    ['하한값', 'Low Limit'],
    ['최솟값', 'Min Value'],
    ['최댓값', 'Max Value'],
    ['최소값', 'Min Value'],
    ['최대값', 'Max Value'],
    ['측정값', 'Measured Value'],
    ['측정', 'Measurement'],
    ['배율', 'Multiplier'],
    ['배열', 'Array'],
    ['원본 포인트', 'Original Point'],
    ['원본', 'Original'],
    ['내부 포인트명', 'Internal Point Name'],
    ['포인트명', 'Point Name'],
    ['포인트 활성화', 'Enable Point'],
    ['알람 사용', 'Enable Alarm'],
    ['데이터 로깅 활성화', 'Enable Data Logging'],
    ['시리얼 지원', 'Serial Support'],
    ['브로커 필수', 'Broker Required'],
    ['기본 폴링 간격', 'Default Polling Interval'],
    ['폴링 간격', 'Polling Interval'],
    ['폴링', 'Polling'],
    ['수집기', 'Collector'],
    ['콜렉터', 'Collector'],
    ['엣지 서버', 'Edge Server'],
    ['엣지', 'Edge'],
    ['제조사', 'Manufacturer'],
    ['모델', 'Model'],
    ['마스터 모델', 'Master Model'],
    ['마스터', 'Master'],
    ['데이터포인트', 'Data Point'],
    ['데이터 포인트', 'Data Point'],
    ['기본값', 'Default'],
    ['초기값', 'Initial Value'],
    ['현재값', 'Current Value'],
    ['설정값', 'Set Value'],
    ['측정값', 'Measured Value'],
    ['범위', 'Range'],
    ['구간', 'Interval'],
    ['연속', 'Continuous'],
    ['반복', 'Repeat'],
    ['배치', 'Batch'],
    ['전송', 'Transmit'],
    ['수신', 'Receive'],
    ['송신', 'TX'],
    ['수신', 'RX'],
    ['응답시간', 'Response Time'],
    ['평균', 'Average'],
    ['최솟값', 'Min'],
    ['최댓값', 'Max'],
    ['합계', 'Total'],
    ['집계', 'Aggregate'],
    ['통계', 'Statistics'],
    ['비율', 'Rate'],
    ['성공률', 'Success Rate'],
    ['오류율', 'Error Rate'],
    ['가동률', 'Availability'],
    ['처리량', 'Throughput'],
    ['바이트', 'Bytes'],
    ['패킷', 'Packet'],
    ['프레임', 'Frame'],
    ['슬레이브', 'Slave'],
    ['마스터', 'Master'],
    ['레지스터', 'Register'],
    ['코일', 'Coil'],
    ['비트', 'Bit'],
    ['워드', 'Word'],
    ['더블워드', 'DWord'],
    ['실수', 'Float'],
    ['정수', 'Integer'],
    ['부호', 'Sign'],
    ['문자열', 'String'],
    ['부울', 'Boolean'],
    ['불린', 'Boolean'],

    // 페이지 제목
    ['대시보드', 'Dashboard'],
    ['디바이스 목록', 'Device List'],
    ['디바이스 관리', 'Device Management'],
    ['프로토콜 설정', 'Protocol Settings'],
    ['프로토콜 관리', 'Protocol Management'],
    ['프로토콜 정보', 'Protocol Info'],
    ['데이터 관리', 'Data Management'],
    ['데이터 익스플로러', 'Data Explorer'],
    ['실시간 데이터', 'Real-time Data'],
    ['이력 데이터', 'Historical Data'],
    ['알람 이력', 'Alarm History'],
    ['활성 알람', 'Active Alarms'],
    ['알람 설정', 'Alarm Settings'],
    ['알람 규칙', 'Alarm Rules'],
    ['알람 템플릿', 'Alarm Templates'],
    ['시스템 설정', 'System Settings'],
    ['시스템 상태', 'System Status'],
    ['백업 복원', 'Backup & Restore'],
    ['사용자 관리', 'User Management'],
    ['권한 관리', 'Permission Management'],
    ['역할 관리', 'Role Management'],
    ['테넌트 관리', 'Tenant Management'],
    ['사이트 관리', 'Site Management'],
    ['제조사 관리', 'Manufacturer Management'],
    ['디바이스 템플릿', 'Device Templates'],
    ['내보내기 게이트웨이', 'Export Gateway'],
    ['내보내기 이력', 'Export History'],
    ['데이터 내보내기', 'Data Export'],
    ['가상 포인트', 'Virtual Points'],
    ['제어 스케줄', 'Control Schedule'],
    ['네트워크 설정', 'Network Settings'],
    ['오류 모니터링', 'Error Monitoring'],
    ['감사 로그', 'Audit Log'],
    ['접속 이력', 'Login History'],

    // 모달 제목
    ['게이트웨이 생성', 'Create Gateway'],
    ['게이트웨이 편집', 'Edit Gateway'],
    ['디바이스 추가', 'Add Device'],
    ['디바이스 편집', 'Edit Device'],
    ['디바이스 상세', 'Device Detail'],
    ['프로토콜 편집', 'Edit Protocol'],
    ['프로토콜 상세', 'Protocol Detail'],
    ['사이트 추가', 'Add Site'],
    ['사이트 편집', 'Edit Site'],
    ['사이트 상세', 'Site Detail'],
    ['템플릿 생성', 'Create Template'],
    ['템플릿 편집', 'Edit Template'],
    ['템플릿 삭제', 'Delete Template'],
    ['스케줄 생성', 'Create Schedule'],
    ['스케줄 편집', 'Edit Schedule'],
    ['스케줄 삭제', 'Delete Schedule'],
    ['사용자 추가', 'Add User'],
    ['사용자 편집', 'Edit User'],
    ['역할 추가', 'Add Role'],
    ['그룹 추가', 'Add Group'],
    ['그룹 편집', 'Edit Group'],
    ['그룹 삭제', 'Delete Group'],
    ['삭제 확인', 'Confirm Delete'],
    ['저장 확인', 'Confirm Save'],
    ['변경 확인', 'Confirm Changes'],
    ['초기화 확인', 'Confirm Reset'],
    ['닫기 확인', 'Confirm Close'],

    // 안내 메시지
    ['이 작업은 되돌릴 수 없습니다', 'This action cannot be undone'],
    ['저장하지 않은 변경사항이 있습니다', 'You have unsaved changes'],
    ['변경사항을 저장하시겠습니까', 'Save changes?'],
    ['정말 삭제하시겠습니까', 'Are you sure you want to delete?'],
    ['필수 항목입니다', 'This field is required'],
    ['올바른 형식이 아닙니다', 'Invalid format'],
    ['데이터를 불러오는 중', 'Loading data'],
    ['저장 중입니다', 'Saving'],
    ['처리 중입니다', 'Processing'],
    ['잠시만 기다려주세요', 'Please wait'],
    ['변경사항 없음', 'No changes'],
    ['변경사항이 없습니다', 'No changes to save'],
    ['데이터가 없습니다', 'No data available'],
    ['결과가 없습니다', 'No results found'],
    ['항목이 없습니다', 'No items'],
    ['등록된 항목이 없습니다', 'No items registered'],
    ['로드 실패', 'Load failed'],
    ['저장 완료', 'Saved'],
    ['저장 성공', 'Save successful'],
    ['삭제 완료', 'Deleted'],
    ['적용 완료', 'Applied'],
    ['설정 완료', 'Configured'],
    ['처리 완료', 'Done'],
    ['업데이트 완료', 'Updated'],
    ['생성 완료', 'Created'],
    ['등록 완료', 'Registered'],
    ['연결 성공', 'Connected'],
    ['연결 실패', 'Connection failed'],
    ['테스트 성공', 'Test successful'],
    ['테스트 실패', 'Test failed'],
    ['저장 실패', 'Save failed'],
    ['불러오기 실패', 'Load failed'],
    ['삭제 실패', 'Delete failed'],
    ['동기화 실패', 'Sync failed'],

    // 시간 표현
    ['방금 전', 'Just now'],
    ['분 전', 'm ago'],
    ['시간 전', 'h ago'],
    ['일 전', 'd ago'],
    ['주 전', 'wk ago'],
    ['달 전', 'mo ago'],

    // 한글 단어 (남은 것들)
    ['게이트웨이', 'Gateway'],
    ['디바이스', 'Device'],
    ['포인트', 'Point'],
    ['프로토콜', 'Protocol'],
    ['인스턴스', 'Instance'],
    ['템플릿', 'Template'],
    ['프로파일', 'Profile'],
    ['타겟', 'Target'],
    ['스케줄', 'Schedule'],
    ['알람', 'Alarm'],
    ['사이트', 'Site'],
    ['테넌트', 'Tenant'],
    ['콜렉터', 'Collector'],
    ['브로커', 'Broker'],
    ['토픽', 'Topic'],
    ['페이로드', 'Payload'],
    ['엔드포인트', 'Endpoint'],
    ['스크립트', 'Script'],
    ['함수', 'Function'],
    ['변수', 'Variable'],
    ['파라미터', 'Parameter'],
    ['설정', 'Settings'],
    ['관리', 'Management'],
    ['모니터링', 'Monitoring'],
    ['통계', 'Statistics'],
    ['이력', 'History'],
    ['로그', 'Logs'],
    ['상태', 'Status'],
    ['연결', 'Connection'],
    ['네트워크', 'Network'],
    ['보안', 'Security'],
    ['인증', 'Authentication'],
    ['계정', 'Account'],
    ['사용자', 'User'],
    ['역할', 'Role'],
    ['권한', 'Permission'],
    ['등록', 'Register'],
    ['수정', 'Edit'],
    ['삭제', 'Delete'],
    ['저장', 'Save'],
    ['취소', 'Cancel'],
    ['확인', 'OK'],
    ['닫기', 'Close'],
    ['추가', 'Add'],
    ['생성', 'Create'],
    ['편집', 'Edit'],
    ['복사', 'Copy'],
    ['붙여넣기', 'Paste'],
    ['검색', 'Search'],
    ['필터', 'Filter'],
    ['정렬', 'Sort'],
    ['선택', 'Select'],
    ['이전', 'Previous'],
    ['다음', 'Next'],
    ['완료', 'Complete'],
    ['시작', 'Start'],
    ['중지', 'Stop'],
    ['새로고침', 'Refresh'],
    ['내보내기', 'Export'],
    ['가져오기', 'Import'],
    ['테스트', 'Test'],
    ['적용', 'Apply'],
    ['초기화', 'Reset'],

    // 탭/메뉴
    ['기본 정보', 'Basic Info'],
    ['상세 정보', 'Details'],
    ['연결 정보', 'Connection Info'],
    ['운영 설정', 'Operation Settings'],
    ['통신 설정', 'Communication Settings'],
    ['접속 설정', 'Access Settings'],
    ['네트워크 설정', 'Network Settings'],
    ['보안 설정', 'Security Settings'],
    ['고급 설정', 'Advanced Settings'],
    ['기본 설정', 'Default Settings'],
    ['전체 설정', 'All Settings'],
    ['데이터포인트', 'Data Points'],
    ['가상 포인트', 'Virtual Points'],
    ['실시간 모니터', 'Real-time Monitor'],
    ['통신 테스트', 'Communication Test'],
    ['연결 테스트', 'Connection Test'],

    // 상태값
    ['활성', 'Active'],
    ['비활성', 'Inactive'],
    ['정상', 'Normal'],
    ['이상', 'Abnormal'],
    ['오류', 'Error'],
    ['경고', 'Warning'],
    ['위험', 'Critical'],
    ['장애', 'Fault'],
    ['안전', 'Safe'],
    ['성공', 'Success'],
    ['실패', 'Failed'],
    ['연결됨', 'Connected'],
    ['끊김', 'Disconnected'],
    ['대기', 'Waiting'],
    ['처리중', 'Processing'],
    ['완료됨', 'Completed'],
    ['취소됨', 'Cancelled'],
    ['알수없음', 'Unknown'],
    ['없음', 'N/A'],
    ['미확인', 'Unconfirmed'],
    ['확인됨', 'Confirmed'],
    ['활성화', 'Enabled'],
    ['비활성화', 'Disabled'],
    ['사용', 'Use'],
    ['미사용', 'Unused'],
];

const koreanRe = /[\uAC00-\uD7AF]/;

function shouldSkip(fp) {
    return SKIP_PATTERNS.some(p => fp.includes(p));
}

function translateStr(str) {
    let result = str;
    for (const [ko, en] of DICT) {
        if (koreanRe.test(result)) {
            result = result.split(ko).join(en);
        } else break;
    }
    return result;
}

function processLine(line) {
    if (!koreanRe.test(line)) return line;

    let result = line;

    // JSX 텍스트 노드
    result = result.replace(/>([^<>{}]+)</g, (m, inner) => {
        if (!koreanRe.test(inner)) return m;
        return '>' + translateStr(inner) + '<';
    });

    // 문자열 리터럴
    result = result.replace(/'([^']*[가-힣]+[^']*)'/g, (m, inner) => {
        return "'" + translateStr(inner) + "'";
    });
    result = result.replace(/"([^"]*[가-힣]+[^"]*)"/g, (m, inner) => {
        return '"' + translateStr(inner) + '"';
    });

    // 백틱
    result = result.replace(/`([^`]*[가-힣]+[^`]*)`/g, (m, inner) => {
        return '`' + translateStr(inner) + '`';
    });

    // placeholder/title/label
    result = result.replace(/placeholder="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'placeholder="' + translateStr(inner) + '"');
    result = result.replace(/placeholder='([^']*[가-힣]+[^']*)'/g, (m, inner) => "placeholder='" + translateStr(inner) + "'");
    result = result.replace(/title="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'title="' + translateStr(inner) + '"');
    result = result.replace(/title='([^']*[가-힣]+[^']*)'/g, (m, inner) => "title='" + translateStr(inner) + "'");
    result = result.replace(/label:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "label: '" + translateStr(inner) + "'");
    result = result.replace(/label:\s*"([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'label: "' + translateStr(inner) + '"');
    result = result.replace(/description:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "description: '" + translateStr(inner) + "'");
    result = result.replace(/desc:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "desc: '" + translateStr(inner) + "'");
    result = result.replace(/name:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "name: '" + translateStr(inner) + "'");
    result = result.replace(/message:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "message: '" + translateStr(inner) + "'");
    result = result.replace(/tooltip="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'tooltip="' + translateStr(inner) + '"');

    return result;
}

const output = execSync("find /app/src -name '*.tsx' -o -name '*.ts'").toString().trim().split('\n');

let totalFixed = 0;
let totalFiles = 0;

for (const fp of output) {
    if (!fp || shouldSkip(fp)) continue;
    if (!fs.existsSync(fp)) continue;

    const orig = fs.readFileSync(fp, 'utf-8');
    if (!koreanRe.test(orig)) continue;

    const lines = orig.split('\n');
    let changed = false;

    const newLines = lines.map(line => {
        const newLine = processLine(line);
        if (newLine !== line) { changed = true; return newLine; }
        return line;
    });

    if (changed) {
        const newContent = newLines.join('\n');
        const origKo = (orig.match(/[가-힣]/g) || []).length;
        const newKo = (newContent.match(/[가-힣]/g) || []).length;
        if (newKo < origKo) {
            fs.writeFileSync(fp, newContent, 'utf-8');
            totalFixed += (origKo - newKo);
            totalFiles++;
            if (origKo - newKo > 10) { // 10개 이상 차이나는 것만 출력
                console.log(`FIXED ${path.basename(fp)}: ${origKo} → ${newKo}`);
            }
        }
    }
}

console.log(`\nTotal: ${totalFixed} Korean chars removed from ${totalFiles} files`);
