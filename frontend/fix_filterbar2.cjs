const fs = require('fs');
const fp = '/app/src/components/common/FilterBar.tsx';
let c = fs.readFileSync(fp, 'utf-8');

// useTranslation 추가 및 초기화 하드코딩 제거
if (!c.includes("useTranslation")) {
    c = c.replace(
        "import React from 'react';",
        "import React from 'react';\nimport { useTranslation } from 'react-i18next';"
    );

    // 컴포넌트 내부에 const { t } = useTranslation() 추가
    c = c.replace(
        "}) => {\n    return (",
        "}) => {\n    const { t } = useTranslation();\n    return ("
    );

    // 초기화 하드코딩 → t() 
    c = c.replace(
        "                    초기화\n",
        "                    {t('common:reset')}\n"
    );

    fs.writeFileSync(fp, c, 'utf-8');
    console.log('FIXED: FilterBar i18n applied');
} else {
    // 이미 있는 경우 초기화만 수정
    if (c.includes("                    초기화")) {
        c = c.replace(
            "                    초기화\n",
            "                    {t('common:reset')}\n"
        );
        fs.writeFileSync(fp, c, 'utf-8');
        console.log('FIXED: 초기화 text only');
    } else {
        console.log('Already fixed or pattern not found');
    }
}

// 확인
const check = fs.readFileSync(fp, 'utf-8');
const hasKo = check.includes('초기화');
console.log('Korean remaining:', hasKo);
