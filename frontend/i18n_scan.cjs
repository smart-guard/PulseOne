const fs = require('fs'), g = require('glob');
const base = '/app/src';
const files = [...g.sync(base + '/pages/**/*.tsx'), ...g.sync(base + '/components/modals/**/*.tsx')];
const res = [];
for (const f of files) {
    const c = fs.readFileSync(f, 'utf-8');
    let n = 0;
    for (const l of c.split('\n')) {
        const t2 = l.trim();
        if (t2.startsWith('//') || t2.includes('console.') || t2.startsWith('*') || t2.includes('Error(')) continue;
        const hasKo = /[\uAC00-\uD7A3]/.test(l);
        const hasTrans = l.includes("t('") || l.includes('t("');
        if (hasKo && !hasTrans) n++;
    }
    if (n > 3) res.push([n, f.split('/src/')[1]]);
}
res.sort((a, b) => b[0] - a[0]);
console.log('Untranslated files:', res.length);
res.slice(0, 15).forEach(x => console.log(String(x[0]).padStart(4), x[1]));
