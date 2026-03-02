/**
 * 남은 4개 파일 tsc 오류 수정
 */
const fs = require('fs');
const path = require('path');

// 1. BasicInfoForm.tsx - useTranslation import 추가
(function () {
    const fp = '/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');
    if (!c.includes("from 'react-i18next'")) {
        c = c.replace("import React", "import { useTranslation } from 'react-i18next';\nimport React");
        fs.writeFileSync(fp, c, 'utf-8');
        console.log('FIXED: BasicInfoForm import');
    }
})();

// 2. MqttBrokerDashboard.tsx - useTranslation import + const { t } 추가
(function () {
    const fp = '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');
    let changed = false;
    if (!c.includes("from 'react-i18next'")) {
        c = c.replace("import React", "import { useTranslation } from 'react-i18next';\nimport React");
        changed = true;
    }
    if (!c.includes("const { t }") && c.includes(') => {\n')) {
        c = c.replace(') => {\n', ') => {\n  const { t } = useTranslation([\'common\']);\n');
        changed = true;
    }
    if (changed) { fs.writeFileSync(fp, c, 'utf-8'); console.log('FIXED: MqttBrokerDashboard'); }
})();

// 3. GatewayListTab.tsx - useTranslation import + const { t } 추가
(function () {
    const fp = '/app/src/pages/export-gateway/tabs/GatewayListTab.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');

    // 현재 useTranslation 사용 여부 확인
    const hasImport = c.includes("from 'react-i18next'");
    const hasT = c.includes("const { t }") || c.includes('const {t}');

    if (!hasImport) {
        c = c.replace("import React", "import { useTranslation } from 'react-i18next';\nimport React");
    }
    if (!hasT) {
        // ') => {\n' 패턴으로 삽입
        if (c.includes(') => {\n')) {
            c = c.replace(') => {\n', ') => {\n  const { t } = useTranslation([\'common\']);\n');
        }
    }
    fs.writeFileSync(fp, c, 'utf-8');
    console.log('FIXED: GatewayListTab');
})();

// 4. TemplateManagementTab.tsx - t() 호출 가능하지 않은 오류
// TemplateManagementTab은 이미 useTranslation이 있을 것
// 547-548줄 확인
(function () {
    const fp = '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');
    const lines = c.split('\n');
    console.log('\nTemplateManagementTab 545-550:');
    lines.slice(544, 552).forEach((l, i) => console.log(i + 545, l.trim().substring(0, 100)));
})();
