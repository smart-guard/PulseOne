/**
 * TemplateManagementTab 남은 한글 추출
 */
const fs = require('fs');
const koreanRe = /[\uAC00-\uD7AF]/;

const fp = '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx';
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
console.log('TemplateManagementTab remaining (' + found.length + '):');
found.slice(0, 25).forEach(l => console.log(l));
if (found.length > 25) console.log('... +' + (found.length - 25) + ' more');
