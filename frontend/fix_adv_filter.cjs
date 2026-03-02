const fs = require('fs');
const fp = '/app/src/pages/HistoricalData.tsx';
let c = fs.readFileSync(fp, 'utf-8');

const lines = c.split('\n');

// 550~556줄 범위에서 search-btn 버튼 찾아서 수정
let fixed = false;
for (let i = 0; i < lines.length; i++) {
    if (lines[i].includes('className="search-btn"') &&
        i + 4 < lines.length &&
        lines[i + 4].includes("t('advancedFilter')")) {
        // 버튼에 style 추가
        lines[i] = lines[i].replace(
            'className="search-btn"',
            'className="search-btn" style={{ whiteSpace: \'nowrap\', display: \'flex\', alignItems: \'center\', gap: \'4px\' }}'
        );
        console.log('FIXED at line', i + 1);
        fixed = true;
        break;
    }
}

if (!fixed) {
    // 디버그: 해당 라인 출력
    lines.slice(547, 560).forEach((l, i) => console.log(i + 548, l));
} else {
    fs.writeFileSync(fp, lines.join('\n'), 'utf-8');
}
