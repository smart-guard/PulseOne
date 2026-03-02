/**
 * 세 번째 라운드 - 구체적인 잔여 패턴 추가 처리
 * 이전 두 라운드에서 처리 못한 엣지 케이스들
 */
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const SKIP_PATTERNS = ['/api/services/', '/types/', 'node_modules'];
const koreanRe = /[\uAC00-\uD7AF]/;

// 추가 사전 - 이전 라운드에서 미처리된 패턴들
const EXTRA_DICT = [
    // 스크립트 엔진 / 가상 포인트 관련
    ['입력 변수', 'Input Variable'],
    ['출력', 'Output'],
    ['공식', 'Formula'],
    ['계산', 'Calculate'],
    ['결과', 'Result'],
    ['입력', 'Input'],
    ['처리', 'Process'],
    ['실행', 'Execute'],
    ['추가', 'Add'],
    ['제거', 'Remove'],
    ['선택된', 'Selected'],
    ['선택하세요', 'Select'],
    ['선택 안 됨', 'Not selected'],
    ['없을 경우', 'if none'],
    ['사용 가능한', 'Available'],
    ['사용 불가능한', 'Unavailable'],
    ['지원', 'Support'],
    ['지원됨', 'Supported'],
    ['미지원', 'Unsupported'],

    // VirtualPoint 관련
    ['가상 포인트 생성', 'Create Virtual Point'],
    ['가상 포인트 편집', 'Edit Virtual Point'],
    ['가상 포인트 상세', 'Virtual Point Detail'],
    ['가상 포인트 목록', 'Virtual Point List'],
    ['포인트 유형', 'Point Type'],
    ['포인트 선택', 'Select Point'],
    ['포인트 추가', 'Add Point'],
    ['포인트 삭제', 'Delete Point'],
    ['소스 선택', 'Select Source'],
    ['소스 유형', 'Source Type'],
    ['소스 포인트', 'Source Point'],
    ['데이터 포인트 선택', 'Select Data Point'],
    ['가상 포인트 선택', 'Select Virtual Point'],
    ['상수 값', 'Constant Value'],
    ['상수', 'Constant'],
    ['수식', 'Formula'],
    ['스크립트 모드', 'Script Mode'],
    ['공식 모드', 'Formula Mode'],
    ['고급 모드', 'Advanced Mode'],
    ['기본 모드', 'Basic Mode'],

    // RealTime Monitor 관련
    ['실시간 모니터', 'Real-time Monitor'],
    ['실시간 데이터', 'Real-time Data'],
    ['신호 없음', 'No Signal'],
    ['데이터 없음', 'No Data'],
    ['값 없음', 'No Value'],
    ['갱신됨', 'Updated'],
    ['갱신 중', 'Updating'],
    ['마지막 갱신', 'Last Update'],
    ['갱신 주기', 'Update Interval'],
    ['자동 갱신', 'Auto Refresh'],
    ['수동 갱신', 'Manual Refresh'],

    // MqttBrokerDashboard 관련
    ['연결된 클라이언트', 'Connected Clients'],
    ['클라이언트 목록', 'Client List'],
    ['클라이언트', 'Client'],
    ['세션', 'Session'],
    ['구독', 'Subscription'],
    ['발행', 'Publication'],
    ['큐', 'Queue'],
    ['토픽 구독', 'Topic Subscription'],
    ['메시지 수', 'Message Count'],
    ['처리 속도', 'Throughput'],
    ['메모리 사용', 'Memory Usage'],
    ['메모리', 'Memory'],
    ['CPU 사용', 'CPU Usage'],
    ['디스크 사용', 'Disk Usage'],
    ['업타임', 'Uptime'],
    ['가동 시간', 'Uptime'],

    // TemplateManagementTab
    ['템플릿 관리', 'Template Management'],
    ['템플릿 이름', 'Template Name'],
    ['템플릿 설명', 'Template Description'],
    ['템플릿 선택', 'Select Template'],
    ['새 템플릿', 'New Template'],
    ['기본 템플릿', 'Default Template'],
    ['기존 템플릿', 'Existing Template'],
    ['시스템 타입', 'System Type'],
    ['시스템 유형', 'System Type'],

    // ExportGatewayWizard 관련
    ['게이트웨이 이름', 'Gateway Name'],
    ['게이트웨이 설정', 'Gateway Settings'],
    ['게이트웨이 목록', 'Gateway List'],
    ['게이트웨이 상태', 'Gateway Status'],
    ['허용 IP', 'Allowed IP'],
    ['알람 구독 모드', 'Alarm Subscription Mode'],
    ['데이터 수신 범위', 'Data Reception Scope'],
    ['매핑 설정', 'Mapping Settings'],
    ['필드 매핑', 'Field Mapping'],
    ['SITE ID 일괄 적용', 'Bulk Apply SITE ID'],
    ['미러 타겟', 'Mirror Target'],
    ['HTTP 타겟', 'HTTP Target'],
    ['MQTT 타겟', 'MQTT Target'],
    ['S3 타겟', 'S3 Target'],
    ['전송 우선순위', 'Transmission Priority'],
    ['전송 프로토콜', 'Transmission Protocol'],
    ['전송 모드', 'Transmission Mode'],

    // ScheduleManagementTab
    ['스케줄 관리', 'Schedule Management'],
    ['스케줄 이름', 'Schedule Name'],
    ['스케줄 설정', 'Schedule Settings'],
    ['배치 전송', 'Batch Transmission'],
    ['전송 대상', 'Transmission Target'],
    ['전송 간격', 'Transmission Interval'],
    ['즉시 전송', 'Immediate Transmission'],

    // 나머지 일반 단어
    ['상위 사이트', 'Parent Site'],
    ['최상위', 'Top Level'],
    ['하위', 'Sub'],
    ['공유', 'Share'],
    ['분리', 'Separate'],
    ['독립', 'Independent'],
    ['의존', 'Dependent'],
    ['참조', 'Reference'],
    ['복제', 'Clone'],
    ['내보내기', 'Export'],
    ['가져오기', 'Import'],
    ['동기화', 'Sync'],
    ['재동기화', 'Re-sync'],
    ['동기화 중', 'Syncing'],
    ['동기화 완료', 'Synced'],
    ['연결 확인', 'Check Connection'],
    ['상태 확인', 'Check Status'],
    ['유효성 검사', 'Validation'],
    ['유효성', 'Validity'],
    ['유효하지 않은', 'Invalid'],
    ['유효한', 'Valid'],
    ['형식', 'Format'],
    ['형식 오류', 'Format Error'],
    ['파싱 오류', 'Parse Error'],
    ['파싱', 'Parse'],
    ['json 형식', 'JSON format'],
    ['json', 'JSON'],
    ['csv 형식', 'CSV format'],
    ['csv', 'CSV'],
    ['파일 크기', 'File Size'],
    ['파일 형식', 'File Format'],
    ['파일 선택', 'Select File'],
    ['파일 업로드', 'Upload File'],
    ['파일 다운로드', 'Download File'],
    ['업로드', 'Upload'],
    ['다운로드', 'Download'],
    ['미리보기', 'Preview'],
    ['JSON 미리보기', 'JSON Preview'],
    ['결과 미리보기', 'Result Preview'],
];

function translateStr(str, dict) {
    let result = str;
    for (const [ko, en] of dict) {
        if (koreanRe.test(result)) {
            result = result.split(ko).join(en);
        } else break;
    }
    return result;
}

function processLine(line, dict) {
    if (!koreanRe.test(line)) return line;
    let result = line;

    result = result.replace(/>([^<>{}]+)</g, (m, inner) => {
        if (!koreanRe.test(inner)) return m;
        return '>' + translateStr(inner, dict) + '<';
    });
    result = result.replace(/'([^']*[가-힣]+[^']*)'/g, (m, inner) => "'" + translateStr(inner, dict) + "'");
    result = result.replace(/"([^"]*[가-힣]+[^"]*)"/g, (m, inner) => '"' + translateStr(inner, dict) + '"');
    result = result.replace(/`([^`]*[가-힣]+[^`]*)`/g, (m, inner) => '`' + translateStr(inner, dict) + '`');
    result = result.replace(/placeholder="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'placeholder="' + translateStr(inner, dict) + '"');
    result = result.replace(/placeholder='([^']*[가-힣]+[^']*)'/g, (m, inner) => "placeholder='" + translateStr(inner, dict) + "'");
    result = result.replace(/title="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'title="' + translateStr(inner, dict) + '"');
    result = result.replace(/label:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "label: '" + translateStr(inner, dict) + "'");
    result = result.replace(/label:\s*"([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'label: "' + translateStr(inner, dict) + '"');
    result = result.replace(/description:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "description: '" + translateStr(inner, dict) + "'");
    result = result.replace(/message:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "message: '" + translateStr(inner, dict) + "'");
    result = result.replace(/tooltip="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'tooltip="' + translateStr(inner, dict) + '"');
    result = result.replace(/name:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "name: '" + translateStr(inner, dict) + "'");
    result = result.replace(/desc:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "desc: '" + translateStr(inner, dict) + "'");

    return result;
}

const output = execSync("find /app/src -name '*.tsx' -o -name '*.ts'").toString().trim().split('\n');

let totalFixed = 0;
let totalFiles = 0;

for (const fp of output) {
    if (!fp || SKIP_PATTERNS.some(p => fp.includes(p))) continue;
    if (!fs.existsSync(fp)) continue;

    const orig = fs.readFileSync(fp, 'utf-8');
    if (!koreanRe.test(orig)) continue;

    const lines = orig.split('\n');
    let changed = false;

    const newLines = lines.map(line => {
        const newLine = processLine(line, EXTRA_DICT);
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
            if (origKo - newKo >= 5) {
                console.log(`FIXED ${path.basename(fp)}: ${origKo} → ${newKo}`);
            }
        }
    }
}

console.log(`\nTotal: ${totalFixed} Korean chars removed from ${totalFiles} files`);
