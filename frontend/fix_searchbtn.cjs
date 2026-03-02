const fs = require('fs');
const fp = '/app/src/styles/historical-data.css';
let c = fs.readFileSync(fp, 'utf-8');

// search-btn에 white-space: nowrap, flex-shrink: 0 추가
const old = '  margin: 0 !important;\n}';
const MARKER = '#root .search-btn {';

const idx = c.indexOf(MARKER);
if (idx === -1) {
    console.log('search-btn class not found');
    process.exit(1);
}

// search-btn 블록 끝 위치 (첫 번째 } 찾기)
let blockEnd = c.indexOf('\n}', idx);
const target = c.slice(idx, blockEnd + 2);
console.log('Found block:', target.slice(0, 80));

const replacement = target.replace(
    '  margin: 0 !important;\n}',
    '  margin: 0 !important;\n  white-space: nowrap !important;\n  flex-shrink: 0 !important;\n}'
);

if (target === replacement) {
    console.log('Already has nowrap or pattern mismatch');
} else {
    c = c.replace(target, replacement);
    fs.writeFileSync(fp, c, 'utf-8');
    console.log('FIXED: search-btn nowrap added');
}
