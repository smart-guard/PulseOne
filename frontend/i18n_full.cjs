/**
 * 전체 한글 → 영어 일괄 처리 - 대사전 방식
 * 한글 포함하는 모든 UI 파일을 일괄 처리
 */
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// 1. 처리 대상 파일 목록 (API 타입 파일 제외)
const SKIP_PATTERNS = [
    '/api/services/',
    '/types/',
    '.test.',
    '.spec.',
    'node_modules',
];

// 2. 대규모 한글→영어 사전
const DICT = [
    // UI 공통
    ['게이트웨이', 'Gateway'],
    ['디바이스', 'Device'],
    ['포인트', 'Point'],
    ['프로토콜', 'Protocol'],
    ['인스턴스', 'Instance'],
    ['콜렉터', 'Collector'],
    ['텐넌트', 'Tenant'],
    ['고객사', 'Tenant'],
    ['사이트', 'Site'],
    ['알람', 'Alarm'],
    ['알람', 'Alarm'],
    ['스케줄', 'Schedule'],
    ['템플릿', 'Template'],
    ['프로파일', 'Profile'],
    ['타겟', 'Target'],
    ['미러', 'Mirror'],
    ['브로커', 'Broker'],

    // 버튼/액션
    ['저장', 'Save'],
    ['취소', 'Cancel'],
    ['확인', 'OK'],
    ['삭제', 'Delete'],
    ['수정', 'Edit'],
    ['편집', 'Edit'],
    ['추가', 'Add'],
    ['생성', 'Create'],
    ['등록', 'Register'],
    ['닫기', 'Close'],
    ['적용', 'Apply'],
    ['초기화', 'Reset'],
    ['새로고침', 'Refresh'],
    ['불러오기', 'Load'],
    ['내보내기', 'Export'],
    ['가져오기', 'Import'],
    ['복사', 'Copy'],
    ['붙여넣기', 'Paste'],
    ['검색', 'Search'],
    ['필터', 'Filter'],
    ['정렬', 'Sort'],
    ['선택', 'Select'],
    ['해제', 'Deselect'],
    ['이전', 'Previous'],
    ['다음', 'Next'],
    ['완료', 'Complete'],
    ['시작', 'Start'],
    ['중지', 'Stop'],
    ['일시정지', 'Pause'],
    ['재시작', 'Restart'],
    ['테스트', 'Test'],
    ['연결', 'Connect'],
    ['연결 해제', 'Disconnect'],
    ['로그아웃', 'Logout'],
    ['로그인', 'Login'],

    // 상태
    ['활성', 'Active'],
    ['비활성', 'Inactive'],
    ['정상', 'Normal'],
    ['이상', 'Abnormal'],
    ['오류', 'Error'],
    ['경고', 'Warning'],
    ['성공', 'Success'],
    ['실패', 'Failed'],
    ['로딩', 'Loading'],
    ['완료', 'Done'],
    ['대기', 'Waiting'],
    ['연결됨', 'Connected'],
    ['연결끊김', 'Disconnected'],
    ['연결중', 'Connecting'],
    ['알수없음', 'Unknown'],
    ['활성화', 'Enabled'],
    ['비활성화', 'Disabled'],
    ['사용 중', 'In Use'],
    ['미사용', 'Unused'],

    // 필드 레이블
    ['이름', 'Name'],
    ['이름 *', 'Name *'],
    ['설명', 'Description'],
    ['주소', 'Address'],
    ['주소 *', 'Address *'],
    ['포트', 'Port'],
    ['단위', 'Unit'],
    ['태그', 'Tags'],
    ['그룹', 'Group'],
    ['카테고리', 'Category'],
    ['타입', 'Type'],
    ['타입 *', 'Type *'],
    ['유형', 'Type'],
    ['버전', 'Version'],
    ['상태', 'Status'],
    ['날짜', 'Date'],
    ['시간', 'Time'],
    ['시작일', 'Start Date'],
    ['종료일', 'End Date'],
    ['생성일', 'Created'],
    ['수정일', 'Modified'],
    ['작성자', 'Author'],
    ['담당자', 'Manager'],
    ['이메일', 'Email'],
    ['전화번호', 'Phone'],
    ['주석', 'Notes'],
    ['메모', 'Notes'],
    ['비고', 'Notes'],
    ['위치', 'Location'],
    ['경로', 'Path'],
    ['URL', 'URL'],
    ['아이디', 'ID'],
    ['비밀번호', 'Password'],
    ['사용자명', 'Username'],
    ['사용자', 'User'],
    ['권한', 'Permission'],
    ['역할', 'Role'],
    ['접근 모드', 'Access Mode'],
    ['접근', 'Access'],
    ['프로토콜 타입', 'Protocol Type'],
    ['표시명', 'Display Name'],
    ['식별자', 'Identifier'],

    // 메시지
    ['오류가 발생했습니다', 'An error occurred'],
    ['알 수 없는 오류', 'Unknown error'],
    ['데이터가 없습니다', 'No data'],
    ['항목이 없습니다', 'No items'],
    ['불러오는 중', 'Loading'],
    ['저장 중', 'Saving'],
    ['처리 중', 'Processing'],
    ['로딩 중', 'Loading'],
    ['필수', 'Required'],
    ['선택 사항', 'Optional'],
    ['없음', 'None'],
    ['전체', 'All'],
    ['기타', 'Other'],

    // 탭/메뉴
    ['기본 정보', 'Basic Info'],
    ['상세 정보', 'Details'],
    ['설정', 'Settings'],
    ['통계', 'Statistics'],
    ['모니터링', 'Monitoring'],
    ['로그', 'Logs'],
    ['이력', 'History'],
    ['현황', 'Status'],
    ['대시보드', 'Dashboard'],
    ['관리', 'Management'],
    ['운영', 'Operations'],
    ['분석', 'Analytics'],
    ['보고서', 'Report'],
    ['도움말', 'Help'],

    // 숫자/단위
    ['개', ''],
    ['건', ''],
    ['초', 's'],
    ['분', 'min'],
    ['시간', 'h'],
    ['일', 'd'],
    ['주', 'wk'],

    // 기타
    ['새 ', 'New '],
    ['신규 ', 'New '],
    ['전체 ', 'All '],
    ['현재 ', 'Current '],
    ['기본 ', 'Default '],
    ['최근 ', 'Recent '],
    ['안전하게', 'safely'],
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
        }
    }
    return result;
}

function processLine(line) {
    if (!koreanRe.test(line)) return line;

    // JSX 텍스트 노드: >한글< → >English<
    let result = line.replace(/>([^<>{}]+)</g, (m, inner) => {
        if (!koreanRe.test(inner)) return m;
        const translated = translateStr(inner.trim());
        // 번역 후에도 한글이 남아있으면 원본 유지하지 않고 번역된 것 사용
        return '>' + translated + '<';
    });

    // 문자열 리터럴: '한글' or "한글"
    result = result.replace(/'([^']*[가-힣]+[^']*)'/g, (m, inner) => {
        const translated = translateStr(inner);
        return "'" + translated + "'";
    });
    result = result.replace(/"([^"]*[가-힣]+[^"]*)"/g, (m, inner) => {
        const translated = translateStr(inner);
        return '"' + translated + '"';
    });

    // 백틱 내부 한글
    result = result.replace(/`([^`]*[가-힣]+[^`]*)`/g, (m, inner) => {
        const translated = translateStr(inner);
        return '`' + translated + '`';
    });

    // placeholder=
    result = result.replace(/placeholder="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => {
        return 'placeholder="' + translateStr(inner) + '"';
    });
    result = result.replace(/placeholder='([^']*[가-힣]+[^']*)'/g, (m, inner) => {
        return "placeholder='" + translateStr(inner) + "'";
    });

    // title=
    result = result.replace(/title="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => {
        return 'title="' + translateStr(inner) + '"';
    });

    // label:
    result = result.replace(/label:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => {
        return "label: '" + translateStr(inner) + "'";
    });
    result = result.replace(/label:\s*"([^"]*[가-힣]+[^"]*)"/g, (m, inner) => {
        return 'label: "' + translateStr(inner) + '"';
    });

    // desc:
    result = result.replace(/desc:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => {
        return "desc: '" + translateStr(inner) + "'";
    });
    result = result.replace(/description:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => {
        return "description: '" + translateStr(inner) + "'";
    });

    return result;
}

// 모든 tsx/ts 파일 찾기
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

    const newLines = lines.map((line, i) => {
        const tr = line.trim();
        // 주석 라인은 처리하되 주석 내 한글 번역
        if (tr.startsWith('//')) {
            // 주석 내 한글 번역
            const translated = line.replace(/\/\/(.*)/, (m, comment) => {
                return '//' + translateStr(comment);
            });
            if (translated !== line) { changed = true; return translated; }
            return line;
        }

        const newLine = processLine(line);
        if (newLine !== line) { changed = true; return newLine; }
        return line;
    });

    if (changed) {
        const newContent = newLines.join('\n');
        // 한글이 실제로 줄었는지 확인
        const origKo = (orig.match(/[가-힣]/g) || []).length;
        const newKo = (newContent.match(/[가-힣]/g) || []).length;
        if (newKo < origKo) {
            fs.writeFileSync(fp, newContent, 'utf-8');
            totalFixed += (origKo - newKo);
            totalFiles++;
            console.log(`FIXED ${path.basename(fp)}: ${origKo} → ${newKo} (reduced ${origKo - newKo})`);
        }
    }
}

console.log(`\nTotal: ${totalFixed} Korean chars removed from ${totalFiles} files`);
