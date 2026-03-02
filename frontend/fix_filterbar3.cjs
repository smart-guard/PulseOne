const fs = require('fs');
const fp = '/app/src/styles/management.css';
let c = fs.readFileSync(fp, 'utf-8');

// mgmt-filter-bar 안의 label을 숨김 (Severity, State 등 라벨이 영어에서 너무 길어 두 줄로 빠짐)
// 대신 title attribute나 placeholder로 식별 가능
const insertAfter = `.mgmt-filter-actions {
    display: flex;
    align-items: center;
    gap: var(--space-2);
    flex-shrink: 0;
    margin-left: auto;
    white-space: nowrap;
}`;

const addition = `

/* FilterBar 내 라벨 숨김 - 공간 절약 (select option이 역할 대신) */
.mgmt-filter-bar .mgmt-filter-group > label {
    display: none !important;
}

/* FilterBar를 항상 한 줄로 유지 */
.mgmt-filter-bar {
    flex-wrap: nowrap !important;
}

.mgmt-filter-bar .mgmt-filter-bar-inner {
    flex-wrap: nowrap !important;
}`;

if (!c.includes('FilterBar 내 라벨 숨김')) {
    c = c.replace(insertAfter, insertAfter + addition);
    fs.writeFileSync(fp, c, 'utf-8');
    console.log('FIXED: FilterBar labels hidden + nowrap restored');
} else {
    console.log('Already applied');
}
