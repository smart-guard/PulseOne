/**
 * 상위 파일들의 렌더링 컨텍스트 한글만 추출
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/components/modals/DeviceDetailModal.tsx',
    '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx',
    '/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx',
    '/app/src/components/modals/DeviceTemplateWizard.tsx',
    '/app/src/components/modals/DeviceModal/DataPointModal.tsx',
    '/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx',
    '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx',
    '/app/src/components/protocols/ProtocolDashboard.tsx',
    '/app/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx',
    '/app/src/components/modals/ProtocolModal/ProtocolEditor.tsx',
    '/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx',
    '/app/src/components/modals/SiteModal/SiteDetailModal.tsx',
];

for (const fp of FILES) {
    if (!fs.existsSync(fp)) continue;
    const lines = fs.readFileSync(fp, 'utf-8').split('\n');
    const found = [];
    lines.forEach((l, i) => {
        const tr = l.trim();
        if (!koreanRe.test(l)) return;
        if (tr.startsWith('//') || tr.startsWith('*')) return;
        // 이미 t() 처리된 건 제외
        if (l.includes("t('") || l.includes('t("')) return;
        // 렌더링 관련만
        if (l.includes('>') || l.includes('"') || l.includes("'") || l.includes('title=') || l.includes('placeholder') || l.includes('message') || l.includes('alert') || l.includes('throw') || l.includes('Error(') || l.includes('setError')) {
            found.push((i + 1) + ': ' + tr.substring(0, 90));
        }
    });
    if (found.length > 0) {
        console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ') ===');
        found.slice(0, 10).forEach(l => console.log(l));
        if (found.length > 10) console.log('  ... +' + (found.length - 10) + ' more');
    }
}
