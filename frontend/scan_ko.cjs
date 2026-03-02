const fs = require('fs');
const path = require('path');
const koreanRegex = /[\uAC00-\uD7AF\u1100-\u11FF\u3130-\u318F]/;

function scanDir(dir) {
    const results = [];
    let items;
    try { items = fs.readdirSync(dir); } catch (e) { return results; }
    for (const item of items) {
        const fullPath = path.join(dir, item);
        let stat;
        try { stat = fs.statSync(fullPath); } catch (e) { continue; }
        if (stat.isDirectory() && item !== 'node_modules' && item !== '.git') {
            const sub = scanDir(fullPath);
            for (const s of sub) results.push(s);
        } else if (item.endsWith('.tsx') || item.endsWith('.ts')) {
            const content = fs.readFileSync(fullPath, 'utf-8');
            const lines = content.split('\n');
            let koCount = 0;
            for (const l of lines) {
                const trimmed = l.trim();
                if (koreanRegex.test(l) && !trimmed.startsWith('//') && !trimmed.startsWith('*')) {
                    koCount++;
                }
            }
            if (koCount > 0) {
                results.push({ file: fullPath.replace('/app/src/', ''), count: koCount });
            }
        }
    }
    return results;
}

const results = scanDir('/app/src');
results.sort(function (a, b) { return b.count - a.count; });
console.log('Top 30 files with Korean:');
const top = results.slice(0, 30);
for (const r of top) {
    console.log(r.count + '\t' + r.file);
}
console.log('Total files:', results.length);
