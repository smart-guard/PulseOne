/**
 * 남은 UI 파일 한글 추출용 스크립트 업데이트
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx',
    '/app/src/components/modals/DeviceModal/DataPointModal.tsx',
    '/app/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx',
    '/app/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx',
    '/app/src/components/protocols/ProtocolDashboard.tsx',
];

for (const fp of FILES) {
    if (!fs.existsSync(fp)) continue;
    const lines = fs.readFileSync(fp, 'utf-8').split('\n');
    const found = [];
    lines.forEach((l, i) => {
        const tr = l.trim();
        if (!koreanRe.test(l)) return;
        if (tr.startsWith('//') || tr.startsWith('*')) return;
        if (l.includes("t('") || l.includes('t("')) return;
        if (l.includes('console.')) return;
        found.push((i + 1) + ': ' + tr.substring(0, 90));
    });
    if (found.length > 0) {
        console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ') ===');
        found.slice(0, 12).forEach(l => console.log(l));
        if (found.length > 12) console.log('  ... +' + (found.length - 12) + ' more');
    }
}
