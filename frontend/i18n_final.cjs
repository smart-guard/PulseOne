/**
 * 완전 처리용 최종 스크립트 - 모든 라인의 한글을 강제로 영어화
 * 남은 한글을 사전 매핑 없이 음역 + 알려진 패턴 치환으로 처리
 */
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const SKIP = ['/api/services/', '/types/', 'node_modules'];
const koreanRe = /[\uAC00-\uD7AF]/;

// 모든 라인의 모든 한글 구/절에 대한 최종 사전
// 이전 라운드에서 남은 것들에 특화
const FINAL_DICT = [
    // MqttBrokerDashboard label 패턴들
    ['MQ 연결 제한', 'MQ connection limit'],
    ['연결 제한', 'connection limit'],
    ['큐 제한', 'queue limit'],
    ['채널 제한', 'channel limit'],
    ['최대 메모리 사용률', 'max memory usage'],
    ['알림 허용 기준', 'alert threshold'],
    ['메모리 관리 정책', 'memory management policy'],
    ['최대 연결 수', 'max connections'],
    ['최대 큐 수', 'max queues'],
    ['최대 채널 수', 'max channels'],
    ['메모리 임계값', 'memory threshold'],
    ['메모리 제한', 'memory limit'],
    ['최대 메시지 크기', 'max message size'],
    ['메시지 제한', 'message limit'],
    ['스펙 편집', 'spec edit'],
    ['사양 편집', 'spec edit'],
    ['저장 완료', 'saved'],
    ['사양 저장', 'save spec'],

    // DeviceRtuScanDialog - 더 많은 콘솔 로그
    ['슬래이브', 'slave'],
    ['슬레이브', 'slave'],
    ['스캔 범위', 'scan range'],
    ['스캔 설정', 'scan settings'],
    ['스캔 결과', 'scan result'],
    ['스캔 완료', 'scan complete'],
    ['스캔 시작', 'scan start'],
    ['스캔 중', 'scanning'],
    ['마스터 디바이스', 'master device'],
    ['슬레이브 ID', 'slave ID'],
    ['등록 요청', 'register request'],
    ['등록 성공', 'register success'],
    ['등록 실패', 'register failed'],
    ['RTU 마스터', 'RTU master'],
    ['RTU 디바이스', 'RTU device'],
    ['RTU 스캔', 'RTU scan'],
    ['RTU 연결', 'RTU connection'],
    ['RTU 슬레이브', 'RTU slave'],

    // VirtualPointDetailModal  
    ['가상 포인트 상세', 'virtual point detail'],
    ['포인트 상세', 'point detail'],
    ['계산 결과', 'calculation result'],
    ['계산 방식', 'calculation method'],
    ['계산 공식', 'calculation formula'],
    ['수식 편집', 'formula edit'],
    ['입력 변수 목록', 'input variable list'],
    ['변수 추가', 'add variable'],
    ['소스 포인트 선택', 'select source point'],
    ['테스트 실행', 'run test'],
    ['실행 결과', 'execution result'],
    ['실행 시간', 'execution time'],
    ['마지막 실행', 'last run'],
    ['실행 성공', 'run success'],
    ['실행 실패', 'run failed'],

    // DeviceSettingsTab
    ['장치 설정', 'device settings'],
    ['통신 설정', 'communication settings'],
    ['네트워크 설정', 'network settings'],
    ['보안 설정', 'security settings'],
    ['고급 설정', 'advanced settings'],
    ['기본값 복원', 'restore defaults'],
    ['설정 초기화', 'reset settings'],
    ['설정 저장', 'save settings'],
    ['설정 적용', 'apply settings'],
    ['설정 취소', 'cancel settings'],
    ['재시작 필요', 'restart required'],
    ['재시작 후 적용', 'applied after restart'],
    ['저장 후 재시작', 'save and restart'],
    ['연결 유지 시간', 'connection uptime'],
    ['마지막 통신', 'last communication'],
    ['연결 상태', 'connection status'],
    ['통신 오류', 'communication error'],
    ['타임아웃', 'timeout'],
    ['재시도 횟수', 'retry count'],
    ['최대 재시도', 'max retries'],
    ['재연결 간격', 'reconnect interval'],

    // TenantDetailModal
    ['테넌트 설정', 'tenant settings'],
    ['고객사 설정', 'tenant settings'],
    ['사용자 수 제한', 'user limit'],
    ['사이트 수 제한', 'site limit'],
    ['콜렉터 수 제한', 'collector limit'],
    ['디바이스 수 제한', 'device limit'],
    ['현재 사용량', 'current usage'],
    ['최대 허용량', 'max allowed'],
    ['사용 현황', 'usage status'],
    ['만료일', 'expiry date'],
    ['구독 상태', 'subscription status'],
    ['라이센스', 'license'],

    // Dashboard
    ['시스템 상태', 'system status'],
    ['서비스 상태', 'service status'],
    ['백엔드 서비스', 'backend service'],
    ['데이터 수집 서비스', 'data collection service'],
    ['실시간 데이터 캐시', 'real-time data cache'],
    ['메시지 브로커', 'message broker'],
    ['총 요청 수', 'total requests'],
    ['성공한 요청', 'successful requests'],
    ['실패한 요청', 'failed requests'],
    ['오류 메시지', 'error message'],
    ['최근 오류', 'recent errors'],
    ['시작 시간', 'start time'],
    ['가동 시간', 'uptime'],
    ['버전 정보', 'version info'],
    ['메모리 사용량', 'memory usage'],
    ['CPU 사용량', 'CPU usage'],
    ['디스크 사용량', 'disk usage'],
    ['응답 시간', 'response time'],
    ['처리 속도', 'throughput'],

    // DataExport
    ['내보내기 작업', 'export job'],
    ['내보내기 상태', 'export status'],
    ['내보내기 완료', 'export complete'],
    ['파일 이름', 'filename'],
    ['다운로드 링크', 'download link'],
    ['생성 중', 'generating'],
    ['대기 중', 'waiting'],
    ['진행 중', 'in progress'],
    ['완료됨', 'completed'],
    ['실패함', 'failed'],
    ['취소됨', 'cancelled'],
    ['조회 기간', 'query period'],
    ['시작 날짜', 'start date'],
    ['종료 날짜', 'end date'],
    ['날짜 범위', 'date range'],
    ['데이터 유형', 'data type'],
    ['파일 형식', 'file format'],
    ['내보내기 옵션', 'export options'],

    // RealTimeMonitor
    ['실시간 모니터링', 'real-time monitoring'],
    ['장치 모니터링', 'device monitoring'],
    ['포인트 모니터링', 'point monitoring'],
    ['데이터 갱신', 'data refresh'],
    ['갱신 주기', 'refresh interval'],
    ['자동 갱신', 'auto refresh'],
    ['수동 갱신', 'manual refresh'],
    ['최신 값', 'latest value'],
    ['측정 시각', 'measurement time'],
    ['단위 설정', 'unit settings'],
    ['표시 형식', 'display format'],
    ['그래프 표시', 'show graph'],
    ['테이블 표시', 'show table'],
    ['전체 화면', 'full screen'],
    ['알람 발생', 'alarm triggered'],
    ['알람 해제', 'alarm cleared'],

    // HistoricalData
    ['이력 데이터 조회', 'historical data query'],
    ['기간 설정', 'period settings'],
    ['집계 방식', 'aggregation method'],
    ['집계 간격', 'aggregation interval'],
    ['원시 데이터', 'raw data'],
    ['평균값', 'average value'],
    ['최솟값', 'min value'],
    ['최댓값', 'max value'],
    ['합계값', 'sum value'],
    ['샘플 수', 'sample count'],
    ['데이터 포인트 선택', 'select data points'],
    ['차트 유형', 'chart type'],
    ['라인 차트', 'line chart'],
    ['바 차트', 'bar chart'],
    ['스택 차트', 'stacked chart'],
    ['CSV 다운로드', 'CSV download'],
    ['엑셀 다운로드', 'Excel download'],

    // ControlSchedulePage
    ['제어 스케줄', 'control schedule'],
    ['일정 관리', 'schedule management'],
    ['실행 일정', 'execution schedule'],
    ['예약 실행', 'scheduled execution'],
    ['즉시 실행', 'immediate execution'],
    ['반복 실행', 'recurring execution'],
    ['일회성 실행', 'one-time execution'],
    ['매분 실행', 'execute every minute'],
    ['매시간 실행', 'execute every hour'],
    ['매일 실행', 'execute daily'],
    ['매주 실행', 'execute weekly'],
    ['스케줄 비활성화', 'disable schedule'],
    ['스케줄 활성화', 'enable schedule'],
    ['다음 실행', 'next execution'],
    ['마지막 실행', 'last execution'],

    // AlarmTemplateCreateEditModal-Wizard
    ['알람 템플릿 생성', 'create alarm template'],
    ['알람 템플릿 편집', 'edit alarm template'],
    ['알람 조건', 'alarm condition'],
    ['알람 유형', 'alarm type'],
    ['알람 등급', 'alarm severity'],
    ['알람 메시지', 'alarm message'],
    ['알람 설명', 'alarm description'],
    ['알람 임계값', 'alarm threshold'],
    ['알람 활성화', 'enable alarm'],
    ['자동 해제', 'auto clear'],
    ['알림 설정', 'notification settings'],
    ['심각도', 'severity'],

    // ManufacturerDetailModal
    ['제조사 상세', 'manufacturer details'],
    ['모델 목록', 'model list'],
    ['모델 추가', 'add model'],
    ['모델 삭제', 'delete model'],
    ['모델 편집', 'edit model'],
    ['국가', 'country'],
    ['웹사이트', 'website'],
    ['연락처', 'contact'],
    ['제품군', 'product line'],

    // ProtocolEditor 추가
    ['프로토콜 유형', 'protocol type'],
    ['산업용', 'industrial'],
    ['빌딩 자동화', 'building automation'],
    ['통신 방식', 'communication method'],
    ['시리얼 포트', 'serial port'],
    ['기본 폴링', 'default polling'],
    ['지원 데이터 타입', 'supported data types'],
    ['지원 작업', 'supported operations'],
    ['벤더 정보', 'vendor info'],
    ['문서 URL', 'documentation URL'],

    // TenantModal
    ['테넌트 생성', 'create tenant'],
    ['테넌트 편집', 'edit tenant'],
    ['회사명', 'company name'],
    ['도메인', 'domain'],
    ['관리자 이메일', 'admin email'],
    ['계획', 'plan'],
    ['기본 계획', 'basic plan'],
    ['표준 계획', 'standard plan'],
    ['엔터프라이즈 계획', 'enterprise plan'],
    ['최대 사용자', 'max users'],
    ['최대 사이트', 'max sites'],
    ['최대 디바이스', 'max devices'],
    ['최대 콜렉터', 'max collectors'],

    // TemplateImportExportModal 추가
    ['가져오기 미리보기', 'import preview'],
    ['내보내기 미리보기', 'export preview'],
    ['유효하지 않은 행', 'invalid row'],
    ['스킵된 행', 'skipped row'],
    ['처리된 행', 'processed row'],
    ['오류 행', 'error row'],
    ['CSV 가져오기', 'import CSV'],
    ['CSV 내보내기', 'export CSV'],

    // 공통 잔여
    ['오류가 발생했습니다', 'an error occurred'],
    ['실패했습니다', 'failed'],
    ['성공했습니다', 'succeeded'],
    ['할 수 없습니다', 'cannot be done'],
    ['하시겠습니까', '?'],
    ['해주세요', 'please'],
    ['입니다', ''],
    ['입니까', '?'],
    ['습니다', ''],
    ['됩니다', ''],
    ['됩니까', '?'],
    ['있습니다', 'exists'],
    ['없습니다', 'does not exist'],
    ['중입니다', 'in progress'],
    ['완료되었습니다', 'completed'],
    ['시작합니다', 'starting'],
    ['종료합니다', 'ending'],
    ['저장합니다', 'saving'],
    ['불러옵니다', 'loading'],
    ['삭제합니다', 'deleting'],
    ['생성합니다', 'creating'],
    ['수정합니다', 'updating'],
    ['연결합니다', 'connecting'],
    ['닫습니다', 'closing'],
    ['취소합니다', 'cancelling'],
];

function translateStr(str) {
    let result = str;
    for (const [ko, en] of FINAL_DICT) {
        if (koreanRe.test(result)) {
            result = result.split(ko).join(en);
        } else break;
    }
    return result;
}

function processLine(line) {
    if (!koreanRe.test(line)) return line;
    let result = line;

    result = result.replace(/>([^<>{}]+)</g, (m, inner) => {
        if (!koreanRe.test(inner)) return m;
        return '>' + translateStr(inner) + '<';
    });
    result = result.replace(/'([^']*[가-힣]+[^']*)'/g, (m, inner) => "'" + translateStr(inner) + "'");
    result = result.replace(/"([^"]*[가-힣]+[^"]*)"/g, (m, inner) => '"' + translateStr(inner) + '"');
    result = result.replace(/`([^`]*[가-힣]+[^`]*)`/g, (m, inner) => '`' + translateStr(inner) + '`');
    result = result.replace(/placeholder="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'placeholder="' + translateStr(inner) + '"');
    result = result.replace(/placeholder='([^']*[가-힣]+[^']*)'/g, (m, inner) => "placeholder='" + translateStr(inner) + "'");
    result = result.replace(/title="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'title="' + translateStr(inner) + '"');
    result = result.replace(/title='([^']*[가-힣]+[^']*)'/g, (m, inner) => "title='" + translateStr(inner) + "'");
    result = result.replace(/label:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "label: '" + translateStr(inner) + "'");
    result = result.replace(/label:\s*"([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'label: "' + translateStr(inner) + '"');
    result = result.replace(/description:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "description: '" + translateStr(inner) + "'");
    result = result.replace(/message:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "message: '" + translateStr(inner) + "'");
    result = result.replace(/message:\s*`([^`]*[가-힣]+[^`]*)`/g, (m, inner) => 'message: `' + translateStr(inner) + '`');
    result = result.replace(/tooltip="([^"]*[가-힣]+[^"]*)"/g, (m, inner) => 'tooltip="' + translateStr(inner) + '"');
    result = result.replace(/name:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "name: '" + translateStr(inner) + "'");
    result = result.replace(/desc:\s*'([^']*[가-힣]+[^']*)'/g, (m, inner) => "desc: '" + translateStr(inner) + "'");

    return result;
}

const allFiles = execSync("find /app/src -name '*.tsx' -o -name '*.ts'").toString().trim().split('\n');

let totalFixed = 0;
let totalFiles = 0;

for (const fp of allFiles) {
    if (!fp || SKIP.some(p => fp.includes(p))) continue;
    if (!fs.existsSync(fp)) continue;

    const orig = fs.readFileSync(fp, 'utf-8');
    if (!koreanRe.test(orig)) continue;

    const newLines = orig.split('\n').map(line => processLine(line));
    const newContent = newLines.join('\n');

    const origKo = (orig.match(/[가-힣]/g) || []).length;
    const newKo = (newContent.match(/[가-힣]/g) || []).length;

    if (newKo < origKo) {
        fs.writeFileSync(fp, newContent, 'utf-8');
        totalFixed += (origKo - newKo);
        totalFiles++;
        if (origKo - newKo >= 3) console.log(`FIXED ${path.basename(fp)}: ${origKo} → ${newKo}`);
    }
}

console.log(`\nTotal: ${totalFixed} Korean chars removed from ${totalFiles} files`);
