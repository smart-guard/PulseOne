/**
 * ExportGatewayWizard 남은 렌더링 한글 추출
 */
const fs = require('fs');
const koreanRe = /[\uAC00-\uD7AF]/;

const fp = '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx';
const lines = fs.readFileSync(fp, 'utf-8').split('\n');
const found = [];

lines.forEach((l, i) => {
    const tr = l.trim();
    if (!koreanRe.test(l)) return;
    if (tr.startsWith('//') || tr.startsWith('*')) return;
    if (l.includes("t('") || l.includes('t("')) return;
    // 렌더링 관련 + console 제외
    if (!l.includes('console.')) {
        found.push((i + 1) + ': ' + tr.substring(0, 90));
    }
});

console.log('ExportGatewayWizard non-console Korean (' + found.length + '):');
found.slice(0, 30).forEach(l => console.log(l));
if (found.length > 30) console.log('... +' + (found.length - 30) + ' more');
