const fs = require('fs');
const fp = '/app/src/pages/SystemStatus.tsx';
let c = fs.readFileSync(fp, 'utf-8');

// 잘못된 코드 수정: confirm 중복 + 문법 오류
const bad = '`Confirm ${action} for ${controllableServices.length} service(s)?`; if (!window.confirm(msg)) return; const confirm2 = \'\'; //\n        action === \'stop\' ? \'stop\' : \'restart\'\n      }하시겠습니까?`';

const good = '`Confirm ${action} for ${controllableServices.length} service(s)?`';

if (c.includes(bad)) {
    c = c.replace(bad, good);
    fs.writeFileSync(fp, c, 'utf-8');
    console.log('FIXED: SystemStatus.tsx 225 syntax error');
} else {
    // 직접 225줄 확인
    const lines = c.split('\n');
    lines.slice(220, 235).forEach((l, i) => console.log(i + 221, JSON.stringify(l).substring(0, 100)));
}
