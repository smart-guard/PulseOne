const fs = require('fs');
const fp = '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx';
let c = fs.readFileSync(fp, 'utf-8');

// templates.map(t => 를 templates.map(tmpl => 로 rename
// 그 블록 안의 t. 를 tmpl. 로 변경 (t(' 는 유지)
const lines = c.split('\n');
let inBlock = false;
let parenDepth = 0;

for (let i = 500; i < 560 && i < lines.length; i++) {
    if (!lines[i]) continue;

    if (lines[i].includes('templates.map(t =>')) {
        inBlock = true;
        lines[i] = lines[i].replace('templates.map(t =>', 'templates.map(tmpl =>');
        parenDepth = 1;
    }

    if (inBlock && i > 503) {
        // t. 를 tmpl. 로 (단 t(' 는 유지)
        lines[i] = lines[i].replace(/\bt\.(id|name|description|type|config|created_at|updated_at|is_active|target)\b/g, 'tmpl.$1');

        // 블록 끝 감지
        for (const ch of lines[i]) {
            if (ch === '(') parenDepth++;
            if (ch === ')') parenDepth--;
        }
        if (parenDepth <= 0) { inBlock = false; break; }
    }
}

c = lines.join('\n');
fs.writeFileSync(fp, c, 'utf-8');
console.log('FIXED: TemplateManagementTab variable rename');

// 검증
const checkLines = c.split('\n');
const line548 = checkLines[547] || '';
console.log('Line 548:', line548.trim().substring(0, 80));
