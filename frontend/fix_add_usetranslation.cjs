/**
 * VirtualPoint/Protocol 컴포넌트에 useTranslation 추가
 */
const fs = require('fs');

const FILES_TO_FIX = [
    '/app/src/components/VirtualPoints/VirtualPointModal/ScopeSelector.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/ValidationPanel.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/VirtualPointModal.tsx',
    '/app/src/components/VirtualPoints/VirtualPointModal/FormulaEditor.tsx',
    '/app/src/components/protocols/ProtocolDashboard.tsx',
    '/app/src/components/protocols/ProtocolInstanceManager.tsx',
];

for (const fp of FILES_TO_FIX) {
    if (!fs.existsSync(fp)) continue;
    let c = fs.readFileSync(fp, 'utf-8');

    // useTranslation import가 없으면 추가
    if (!c.includes('useTranslation')) {
        c = c.replace(
            "import React",
            "import { useTranslation } from 'react-i18next';\nimport React"
        );
    }

    // 컴포넌트 내에 const { t } = ... 없으면 추가
    if (!c.includes('const { t }') && !c.includes("const {t}")) {
        // 함수형 컴포넌트 첫 줄 찾기
        // useTranslation 을 컴포넌트 body 처음에 삽입
        // FC 패턴: = () => {  또는  = (props) => {  등
        c = c.replace(
            /^(.*React\.FC[^{]*\{)\s*\n/m,
            (match, prefix) => prefix + '\n  const { t } = useTranslation([\'common\']);\n'
        );
    }

    fs.writeFileSync(fp, c, 'utf-8');
    console.log('FIXED: useTranslation added to', require('path').basename(fp));
}
