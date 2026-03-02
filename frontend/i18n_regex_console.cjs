/**
 * 한글 포함 console/alert/throw/setError 줄을 자동으로 영어 형태로 변환
 * 한글을 포함하는 메시지 문자열만 선택적으로 변환
 */
const fs = require('fs');
const path = require('path');

const koreanRe = /[\uAC00-\uD7AF]/;

// 한글 → 영어 직역 사전 (자주 쓰는 단어들)
const KO_EN = [
    [/불러오는데 실패/g, 'load failed'],
    [/불러오는 중/g, 'loading'],
    [/불러올 수 없/g, 'unable to load'],
    [/조회 실패/g, 'query failed'],
    [/조회 완료/g, 'query complete'],
    [/조회 중/g, 'querying'],
    [/저장 실패/g, 'save failed'],
    [/저장 완료/g, 'saved'],
    [/생성 실패/g, 'create failed'],
    [/생성 완료/g, 'created'],
    [/수정 실패/g, 'update failed'],
    [/수정 완료/g, 'updated'],
    [/삭제 실패/g, 'delete failed'],
    [/삭제 완료/g, 'deleted'],
    [/오류 발생/g, 'error occurred'],
    [/오류가 발생/g, 'error occurred'],
    [/오류:/g, 'Error:'],
    [/실패:/g, 'failed:'],
    [/실패$/g, 'failed'],
    [/성공/g, 'success'],
    [/로드 완료/g, 'load complete'],
    [/로드 실패/g, 'load failed'],
    [/연결됨/g, 'connected'],
    [/연결 해제/g, 'disconnected'],
    [/연결 실패/g, 'connection failed'],
    [/목록 조회/g, 'list query'],
    [/상세 조회/g, 'detail query'],
    [/데이터 없음/g, 'no data'],
    [/디바이스/g, 'device'],
    [/가상포인트/g, 'virtual point'],
    [/게이트웨이/g, 'gateway'],
    [/프로토콜/g, 'protocol'],
    [/알람/g, 'alarm'],
    [/사이트/g, 'site'],
    [/테넌트/g, 'tenant'],
    [/포인트/g, 'point'],
    [/네트워크/g, 'network'],
    [/타임아웃/g, 'timeout'],
    [/인증/g, 'auth'],
    [/권한/g, 'permission'],
    [/데이터/g, 'data'],
    [/서버/g, 'server'],
    [/데이터베이스/g, 'database'],
    [/설정/g, 'settings'],
    [/목록/g, 'list'],
    [/추가/g, 'add'],
    [/변경/g, 'change'],
    [/처리/g, 'process'],
    [/시작/g, 'start'],
    [/완료/g, 'complete'],
    [/중/g, ''],
    [/중\.$/g, '.'],
];

// 대상 파일들 (UI .tsx 파일)
const TARGET_FILES = [
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/components/modals/DeviceDetailModal.tsx',
    '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx',
    '/app/src/pages/export-gateway/tabs/GatewayListTab.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx',
    '/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx',
    '/app/src/components/modals/DeviceTemplateWizard.tsx',
    '/app/src/components/modals/DeviceModal/DataPointModal.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx',
    '/app/src/components/protocols/ProtocolDashboard.tsx',
    '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx',
    '/app/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx',
    '/app/src/components/modals/ProtocolModal/ProtocolEditor.tsx',
    '/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx',
    '/app/src/components/modals/SiteModal/SiteDetailModal.tsx',
];

let totalFixed = 0;

for (const fp of TARGET_FILES) {
    if (!fs.existsSync(fp)) continue;

    let c = fs.readFileSync(fp, 'utf-8');
    const lines = c.split('\n');
    let fileChanged = 0;

    for (let i = 0; i < lines.length; i++) {
        const l = lines[i];
        if (!koreanRe.test(l)) continue;

        // console.log/error/warn 라인만 (JSX 렌더링 아닌 JS 로직 라인)
        if (l.includes('console.log(') || l.includes('console.error(') || l.includes('console.warn(')) {
            let newLine = l;
            for (const [re, en] of KO_EN) {
                newLine = newLine.replace(re, en);
            }
            if (newLine !== l) {
                lines[i] = newLine;
                fileChanged++;
            }
        }
    }

    if (fileChanged > 0) {
        fs.writeFileSync(fp, lines.join('\n'), 'utf-8');
        totalFixed += fileChanged;
        console.log('FIXED', fileChanged, path.basename(fp));
    }
}

console.log('\nTotal:', totalFixed);
