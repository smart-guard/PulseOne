const fs = require('fs');
const fp = '/app/src/pages/export-gateway/tabs/GatewayListTab.tsx';
let c = fs.readFileSync(fp, 'utf-8');
const lines = c.split('\n');

// 현재 const { t } 위치 확인
let tLine = -1;
let firstUseStateLine = -1;
lines.forEach((l, i) => {
    if (l.includes("const { t } = useTranslation")) tLine = i;
    if (l.includes('= useState') && firstUseStateLine === -1) firstUseStateLine = i;
});

console.log('Current t line:', tLine + 1, ':', (lines[tLine] || '').trim().substring(0, 80));
console.log('First useState line:', firstUseStateLine + 1);
console.log('446 line:', (lines[445] || '').trim().substring(0, 100));

// 현재 t 위치가 props 안이면 (firstUseStateLine 이전이면) 이동
if (tLine > -1 && tLine < firstUseStateLine) {
    // props 안에 있음 → 제거하고 useState 앞에 추가
    const tContent = lines[tLine];
    lines.splice(tLine, 1); // 제거
    // useStateLine이 하나 앞당겨짐
    const newStateLine = firstUseStateLine - 1;
    lines.splice(newStateLine, 0, tContent);
    c = lines.join('\n');
    fs.writeFileSync(fp, c, 'utf-8');
    console.log('MOVED: t from props to body');
} else if (tLine === -1) {
    // t 없음 → useState 앞에 추가
    const tContent = "  const { t } = useTranslation(['common']);";
    lines.splice(firstUseStateLine, 0, tContent);
    c = lines.join('\n');
    fs.writeFileSync(fp, c, 'utf-8');
    console.log('ADDED: t before useState');
} else {
    console.log('OK: t in body already');
}
