/**
 * 렌더링 컨텍스트의 한글만 추출 (JSX 텍스트, 속성값)
 * - 타입 이름, 변수명은 제외
 * - > 텍스트 < 패턴, placeholder="", title="", label 내용 등
 */
const fs = require('fs');
const path = require('path');
const koreanRegex = /[\uAC00-\uD7AF]/;

const FILES = [
    '/app/src/pages/NetworkSettings.tsx',
    '/app/src/pages/SystemStatus.tsx',
    '/app/src/pages/BackupRestore.tsx',
    '/app/src/pages/HistoricalData.tsx',
    '/app/src/pages/AuditLogPage.tsx',
    '/app/src/pages/PermissionManagement.tsx',
];

for (const fp of FILES) {
    if (!fs.existsSync(fp)) continue;
    const lines = fs.readFileSync(fp, 'utf-8').split('\n');
    const found = [];

    lines.forEach((l, i) => {
        const trimmed = l.trim();
        // 주석, import, 타입선언 제외
        if (trimmed.startsWith('//') || trimmed.startsWith('*') || trimmed.startsWith('import ')) return;
        if (!koreanRegex.test(l)) return;
        // JSX 텍스트 노드, placeholder, title, label, alert, console 제외
        if (l.includes('console.log') || l.includes('console.error')) return;
        // 실제 렌더링 관련 줄만
        if (
            l.includes('>') || l.includes('"') || l.includes("'") ||
            l.includes('placeholder') || l.includes('title=') ||
            l.includes('label') || l.includes('message') ||
            l.includes('throw') || l.includes('alert(') || l.includes('Error(')
        ) {
            found.push((i + 1) + ':\t' + trimmed.substring(0, 90));
        }
    });

    if (found.length > 0) {
        console.log('\n=== ' + path.basename(fp) + ' (' + found.length + ' lines) ===');
        found.slice(0, 20).forEach(l => console.log(l));
    }
}
