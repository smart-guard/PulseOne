/**
 * useTranslation 삽입 위치 수정 스크립트
 * 기존 잘못된 패턴을 찾아 올바른 위치로 이동
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
];

for (const fp of FILES) {
    if (!fs.existsSync(fp)) continue;
    let c = fs.readFileSync(fp, 'utf-8');
    const lines = c.split('\n');

    // 잘못 삽입된 위치 찾기: React.FC 패턴 안에 들어간 경우
    // 패턴: "React.FC..." 줄 다음에 "const { t }" 가 삽입됨
    // 이걸 제거하고, 컴포넌트 body 첫 줄(첫 번째 줄이 '{' 또는 'const ...' 패턴)에 추가

    // 1. 잘못된 삽입 제거 (React.FC 타입 선언 안에 들어간 경우)
    let changed = false;

    // 잘못된 패턴: React.FC<...> = (\n  const { t } = useTranslation(['common']);\n
    // 또는 interface 안에 삽입된 경우
    const badPattern = /(\n  const \{ t \} = useTranslation\(\['common'\]\);\n)/;

    if (badPattern.test(c)) {
        // 이미 올바른 위치에 있는지 확인
        // 컴포넌트 함수 body 안에 있으면 OK, 아니면 제거 후 재삽입
        const idx = c.indexOf("const { t } = useTranslation(['common']);");
        const before = c.substring(Math.max(0, idx - 200), idx);

        // 인터페이스나 타입 선언 안에 들어간 경우 제거
        const isInsideInterface = /interface\s+\w+[^{]*\{[^}]*$/.test(before) && !before.includes('=>');
        const isInsideTypes = before.trim().endsWith('{') && !before.includes('=>') && !before.includes('return');

        if (isInsideInterface || isInsideTypes) {
            // 잘못된 삽입 제거
            c = c.replace("\n  const { t } = useTranslation(['common']);\n", '\n');
            changed = true;
        }
    }

    // 2. const { t } 가 없으면 올바른 위치에 추가
    if (!c.includes("const { t }") && !c.includes('const {t}')) {
        // 컴포넌트 함수 시작 부분 찾기
        // 패턴: => {\n 다음 줄
        const arrowFnMatch = c.match(/=>\s*\{\n(\s*)(\/\*|if|const|let|var|return|use|\/\/)/);
        if (arrowFnMatch) {
            const insertAt = arrowFnMatch.index + arrowFnMatch[0].indexOf('\n') + 1;
            const indent = arrowFnMatch[1];
            c = c.substring(0, insertAt) + indent + "const { t } = useTranslation(['common']);\n" + c.substring(insertAt);
            changed = true;
        }
    }

    if (changed || c.includes("const { t } = useTranslation(['common']);")) {
        fs.writeFileSync(fp, c, 'utf-8');
        console.log('PROCESSED:', path.basename(fp));
        // 첫 40줄 확인
        const sample = c.split('\n').slice(20, 35);
        sample.forEach((l, i) => { if (l.includes('const { t }') || l.includes('useTranslation')) console.log('  ', i + 21, l.trim()); });
    }
}
