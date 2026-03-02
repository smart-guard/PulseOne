/**
 * const { t } = useTranslation 을 props 구조분해에서 함수 body로 이동
 */
const fs = require('fs');
const path = require('path');

const FILES = [
    '/app/src/components/VirtualPoints/VirtualPointModal/FormulaEditor.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/ScopeSelector.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/ValidationPanel.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/VirtualPointModal.tsx',
    '/app/src/components/protocols/ProtocolDashboard.tsx',
    '/app/src/components/protocols/ProtocolInstanceManager.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx',
];

const T_LINE = "  const { t } = useTranslation(['common']);\n";

for (const fp of FILES) {
    if (!fs.existsSync(fp)) continue;
    let c = fs.readFileSync(fp, 'utf-8');
    let changed = false;

    // 잘못된 패턴: React.FC<...> = ({  \n  const { t }... 제거
    // 구조분해 파라미터 안에 들어간 경우
    if (c.includes(T_LINE)) {
        const tIdx = c.indexOf(T_LINE);
        const before = c.substring(Math.max(0, tIdx - 300), tIdx);

        // 함수 body 시작 ') => {' 또는 ') => {\n' 이 이미 지났는지 확인
        const arrowBodyStart = before.lastIndexOf(') => {');
        const propsStart = before.lastIndexOf(': React.FC');

        // props 구조분해 안에 있는 경우 (arrowBodyStart이 없거나 propsStart 이후에 있는 경우)
        if (arrowBodyStart < propsStart) {
            // const { t } 제거
            c = c.replace(T_LINE, '');

            // 함수 body 시작 ') => {' 이후에 삽입
            c = c.replace(') => {\n', ') => {\n' + T_LINE);
            changed = true;
            console.log('MOVED t() to body in:', path.basename(fp));
        } else {
            console.log('OK - t() already in body:', path.basename(fp));
        }
    } else if (c.includes("const { t }") || c.includes('const {t}')) {
        console.log('OK - t() exists:', path.basename(fp));
    } else {
        // t() 없음, ') => {' 패턴에 추가
        if (c.includes(') => {\n')) {
            c = c.replace(') => {\n', ') => {\n' + T_LINE);
            changed = true;
            console.log('ADDED t() to body in:', path.basename(fp));
        } else {
            console.log('SKIP (no arrow fn):', path.basename(fp));
        }
    }

    if (changed) fs.writeFileSync(fp, c, 'utf-8');
}
