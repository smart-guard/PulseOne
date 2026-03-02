/**
 * ExportGatewayWizard 남은 JSX 한글 추출 (non-console, non-t())
 */
const fs = require('fs');
const koreanRe = /[\uAC00-\uD7AF]/;
const fp = '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx';
const lines = fs.readFileSync(fp, 'utf-8').split('\n');
const found = [];
lines.forEach((l, i) => {
    const tr = l.trim();
    if (!koreanRe.test(l)) return;
    if (tr.startsWith('//') || tr.startsWith('*') || tr.startsWith('label:') || tr.startsWith('desc:')) return;
    if (l.includes("t('") || l.includes('t("')) return;
    if (l.includes('console.')) return;
    found.push((i + 1) + ': ' + tr.substring(0, 90));
});
found.slice(0, 40).forEach(l => console.log(l));
console.log('total:', found.length);
