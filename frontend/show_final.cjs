/**
 * 잔존 한글 정밀 추출 - 상위 UI 파일들
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const UI_FILES = [
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx',
    '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/InputVariableSourceSelector.tsx',
    '/app/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx',
    '/app/src/pages/DataExport.tsx',
    '/app/src/pages/RealTimeMonitor.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx',
    '/app/src/components/modals/DeviceTemplateWizard.tsx',
    '/app/src/components/modals/DeviceDetailModal.tsx',
];

for (const fp of UI_FILES) {
    if (!fs.existsSync(fp)) continue;
    const lines = fs.readFileSync(fp, 'utf-8').split('\n');
    const found = [];
    lines.forEach((l, i) => {
        if (!koreanRe.test(l)) return;
        const tr = l.trim();
        if (tr.startsWith('//') || tr.startsWith('*') || tr.startsWith('/*')) return;
        if (l.includes("t('") || l.includes('t("')) return;
        found.push((i + 1) + ': ' + tr.substring(0, 80));
    });
    if (found.length > 0) {
        console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ') ===');
        found.slice(0, 8).forEach(l => console.log(l));
        if (found.length > 8) console.log('  ... +' + (found.length - 8));
    }
}
