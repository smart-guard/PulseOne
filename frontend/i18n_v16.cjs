/**
 * 여러 파일 잔여 한글 처리 v16 - 배치 처리
 * DeviceRtuMonitorTab, DeviceDataPointsBulkModal, ExportGatewayWizard, DeviceTemplateWizard 일괄
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const TARGET_FILES = [
    '/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx',
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/components/modals/DeviceTemplateWizard.tsx',
    '/app/src/components/modals/DeviceDetailModal.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx',
];

// 공통 단어 매핑
const WORD_MAP = [
    ['새로고침', 'Refresh'],
    ['임시 ID', 'temp ID'],
    ['복사', 'copy'],
    ['삭제', 'delete'],
    ['수정', 'edit'],
    ['저장', 'save'],
    ['취소', 'cancel'],
    ['확인', 'confirm'],
    ['추가', 'add'],
    ['등록', 'register'],
    ['생성', 'create'],
    ['닫기', 'close'],
    ['적용', 'apply'],
    ['주소', 'address'],
    ['이름', 'name'],
    ['태그', 'tag'],
    ['이상 없음', 'no issues'],
    ['연결됨', 'connected'],
    ['연결 끊김', 'disconnected'],
    ['활성', 'active'],
    ['비활성', 'inactive'],
    ['정상', 'normal'],
    ['이상', 'abnormal'],
    ['완료', 'complete'],
    ['처리 중', 'processing'],
    ['로딩', 'loading'],
    ['오류', 'error'],
];

let total = 0;

for (const fp of TARGET_FILES) {
    if (!fs.existsSync(fp)) continue;
    let c = fs.readFileSync(fp, 'utf-8');
    const orig = c;
    const lines = c.split('\n');
    let lineChanged = false;

    for (let i = 0; i < lines.length; i++) {
        const l = lines[i];
        if (!koreanRe.test(l)) continue;
        if (l.trim().startsWith('//') || l.trim().startsWith('*')) continue;
        if (l.includes("t('") || l.includes('t("')) continue;

        let newLine = l;

        // JSX 텍스트노드 한글 치환: >한글< → >English<
        for (const [ko, en] of WORD_MAP) {
            newLine = newLine.split(`>${ko}<`).join(`>${en}<`);
            newLine = newLine.split(`>${ko}</`).join(`>${en}</`);
            newLine = newLine.split(`"${ko}"`).join(`"${en}"`);
        }

        if (newLine !== l) {
            lines[i] = newLine;
            lineChanged = true;
        }
    }

    if (lineChanged) {
        c = lines.join('\n');
        fs.writeFileSync(fp, c, 'utf-8');
        const remaining = (c.match(/[가-힣]/g) || []).length;
        const was = (orig.match(/[가-힣]/g) || []).length;
        console.log(`FIXED ${path.basename(fp)}: ${was} → ${remaining} chars`);
        total++;
    }
}

console.log('\nFiles fixed:', total);
