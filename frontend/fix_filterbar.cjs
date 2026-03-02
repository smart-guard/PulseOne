const fs = require('fs');
const fp = '/app/src/styles/management.css';
let c = fs.readFileSync(fp, 'utf-8');

// mgmt-filter-bar: flex-wrap nowrap → wrap 허용 (항목이 많으면 자연스럽게 두 줄)
// mgmt-filter-bar-inner: 역시 wrap 허용  
// mgmt-filter-actions: 항상 같은 줄 유지 (flex-shrink:0 유지)

const old1 = `.mgmt-filter-bar {
    padding: var(--space-3) var(--space-4);
    display: flex;
    align-items: center;
    gap: var(--filter-gap);
    flex-wrap: nowrap;
    position: relative;
    background: white;
    overflow: visible;
    margin-bottom: var(--space-2);
    min-height: 56px;
    box-sizing: border-box;
}`;

const new1 = `.mgmt-filter-bar {
    padding: var(--space-3) var(--space-4);
    display: flex;
    align-items: center;
    gap: var(--filter-gap);
    flex-wrap: wrap;
    position: relative;
    background: white;
    overflow: visible;
    margin-bottom: var(--space-2);
    min-height: 56px;
    box-sizing: border-box;
}`;

const old2 = `.mgmt-filter-bar-inner {
    display: flex;
    align-items: center;
    flex-wrap: nowrap;
    /* RESTORED: NO WRAPPING */
    gap: var(--filter-gap);
    flex: 1;
    min-width: 0;
}`;

const new2 = `.mgmt-filter-bar-inner {
    display: flex;
    align-items: center;
    flex-wrap: wrap;
    gap: var(--filter-gap);
    flex: 1 1 auto;
    min-width: 0;
}`;

const old3 = `.mgmt-filter-actions {
    display: flex;
    align-items: center;
    gap: var(--space-2);
    flex-shrink: 0;
    margin-left: auto;
    /* Push to right if space allows */
}`;

const new3 = `.mgmt-filter-actions {
    display: flex;
    align-items: center;
    gap: var(--space-2);
    flex-shrink: 0;
    margin-left: auto;
    white-space: nowrap;
}`;

let changed = 0;
if (c.includes(old1)) { c = c.replace(old1, new1); changed++; console.log('FIXED: mgmt-filter-bar wrap'); }
else console.log('WARN: mgmt-filter-bar pattern not found');

if (c.includes(old2)) { c = c.replace(old2, new2); changed++; console.log('FIXED: mgmt-filter-bar-inner wrap'); }
else console.log('WARN: mgmt-filter-bar-inner pattern not found');

if (c.includes(old3)) { c = c.replace(old3, new3); changed++; console.log('FIXED: mgmt-filter-actions'); }
else console.log('WARN: mgmt-filter-actions pattern not found');

if (changed > 0) {
    fs.writeFileSync(fp, c, 'utf-8');
    console.log(`Total ${changed} fixes applied`);
}
