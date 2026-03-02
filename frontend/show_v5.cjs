/**
 * ProtocolEditor + MqttBrokerDashboard + InputVariableSourceSelector + DeviceRtuNetworkTab 추출
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/components/modals/ProtocolModal/ProtocolEditor.tsx',
    '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/InputVariableSourceSelector.tsx',
    '/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx',
    '/app/src/components/modals/DeviceDetailModal.tsx',
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
        found.push((i + 1) + ': ' + tr.substring(0, 85));
    });
    if (found.length > 0) {
        console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ') ===');
        found.slice(0, 15).forEach(l => console.log(l));
        if (found.length > 15) console.log('  ... +' + (found.length - 15) + ' more');
    }
}
