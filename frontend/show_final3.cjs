/**
 * GatewayListTab + BasicInfoForm + InputVariableSourceSelector 추출
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/pages/export-gateway/tabs/GatewayListTab.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/InputVariableSourceSelector.tsx',
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx',
];

for (const fp of FILES) {
    if (!fs.existsSync(fp)) continue;
    const lines = fs.readFileSync(fp, 'utf-8').split('\n');
    const found = [];
    lines.forEach((l, i) => {
        if (!koreanRe.test(l)) return;
        found.push((i + 1) + ': ' + l.trim().substring(0, 88));
    });
    if (found.length > 0) {
        console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ') ===');
        found.slice(0, 10).forEach(l => console.log(l));
        if (found.length > 10) console.log('  ... +' + (found.length - 10));
    }
}
