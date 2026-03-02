/**
 * API 파일 실제 한글 샘플 추출
 */
const fs = require('fs');
const path = require('path');
const koreanRe = /[\uAC00-\uD7AF]/;

const API_FILES = [
    '/app/src/api/services/alarmApi.ts',
    '/app/src/api/services/deviceApi.ts',
    '/app/src/api/services/virtualPointsApi.ts',
];

for (const fp of API_FILES) {
    if (!fs.existsSync(fp)) continue;
    const lines = fs.readFileSync(fp, 'utf-8').split('\n');
    const found = [];
    lines.forEach((l, i) => {
        if (!koreanRe.test(l)) return;
        const tr = l.trim();
        if (tr.startsWith('//') || tr.startsWith('*')) return;
        found.push((i + 1) + ': ' + tr.substring(0, 80));
    });
    console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ') ===');
    found.slice(0, 10).forEach(l => console.log(l));
}
