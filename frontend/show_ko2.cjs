/**
 * 남은 한글 파일 목록 추출 (상위 20개)
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/components/modals/DeviceDetailModal.tsx',
    '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx',
    '/app/src/components/modals/DeviceTemplateWizard.tsx',
    '/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx',
    '/app/src/components/modals/DeviceModal/DataPointModal.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx',
    '/app/src/components/modals/SiteModal/SiteDetailModal.tsx',
];

for (const fp of FILES) {
    if (!fs.existsSync(fp)) continue;
    const lines = fs.readFileSync(fp, 'utf-8').split('\n');
    const found = [];

    lines.forEach((l, i) => {
        const trimmed = l.trim();
        if (!koreanRe.test(l)) return;
        if (trimmed.startsWith('//') || trimmed.startsWith('*')) return;
        // 렌더링 관련 줄만
        if (l.includes('>') || l.includes('"') || l.includes("'") || l.includes('placeholder') || l.includes('title=') || l.includes('label') || l.includes('alert(') || l.includes('Error(')) {
            // t()로 이미 처리된 건 제외
            if (!l.includes("t('") && !l.includes('t("')) {
                found.push((i + 1) + ': ' + trimmed.substring(0, 100));
            }
        }
    });

    if (found.length > 0) {
        console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ' lines) ===');
        found.slice(0, 15).forEach(l => console.log(l));
        if (found.length > 15) console.log('  ... and ' + (found.length - 15) + ' more');
    }
}
