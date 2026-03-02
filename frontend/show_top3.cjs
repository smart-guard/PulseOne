/**
 * ExportGatewayWizard.tsx 렌더링 한글 추출 (별도 스크립트)
 */
const fs = require('fs');
const koreanRe = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/components/modals/DeviceDetailModal.tsx',
    '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx',
];

for (const fp of FILES) {
    if (!fs.existsSync(fp)) continue;
    const lines = fs.readFileSync(fp, 'utf-8').split('\n');
    const found = [];
    const name = fp.split('/').pop();

    lines.forEach((l, i) => {
        const tr = l.trim();
        if (!koreanRe.test(l)) return;
        if (tr.startsWith('//') || tr.startsWith('*')) return;
        if (l.includes("t('") || l.includes('t("')) return;
        if (l.includes('>') || l.includes('"') || l.includes("'") || l.includes('message') || l.includes('alert') || l.includes('title:') || l.includes('Error(') || l.includes('setError') || l.includes('label')) {
            found.push((i + 1) + ': ' + tr.substring(0, 85));
        }
    });

    console.log('\n=== ' + name + ' (' + found.length + ') ===');
    found.slice(0, 20).forEach(l => console.log(l));
    if (found.length > 20) console.log('  ... +' + (found.length - 20) + ' more');
}
