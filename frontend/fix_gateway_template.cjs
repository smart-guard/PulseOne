const fs = require('fs');

// 1. TemplateManagementTab - 남은 t. 패턴 처리
(function () {
    const fp = '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx';
    let c = fs.readFileSync(fp, 'utf-8');

    // t.system_type → tmpl.system_type
    // t.template_json → tmpl.template_json
    // { ...t, → { ...tmpl,
    // setEditingTemplate({ ...t, → ...tmpl,
    // {tmpl.is_active ? '활성' : '비활성'} → {tmpl.is_active ? t('common:active') : t('common:inactive')}

    const replacements = [
        ["{t.system_type}", "{tmpl.system_type}"],
        ["t.template_json", "tmpl.template_json"],
        ["setEditingTemplate({ ...t,", "setEditingTemplate({ ...tmpl,"],
        ["{ ...t, template_json:", "{ ...tmpl, template_json:"],
        ["{tmpl.is_active ? '활성' : '비활성'}", "{tmpl.is_active ? t('common:active') : t('common:inactive')}"],
    ];

    let n = 0;
    for (const [a, b] of replacements) {
        if (c.includes(a)) { c = c.split(a).join(b); n++; }
    }
    fs.writeFileSync(fp, c, 'utf-8');
    console.log('TemplateManagementTab:', n, 'fixes');
})();

// 2. GatewayListTab - const { t } 위치 확인 및 수정
(function () {
    const fp = '/app/src/pages/export-gateway/tabs/GatewayListTab.tsx';
    let c = fs.readFileSync(fp, 'utf-8');

    // const { t } 가 어디 있는지 확인
    const tIdx = c.indexOf("const { t } = useTranslation");
    if (tIdx === -1) {
        console.log('GatewayListTab: no const { t } found!');

        // 함수 body 찾아서 삽입
        const lines = c.split('\n');
        let inserted = false;
        for (let i = 0; i < lines.length; i++) {
            if (lines[i].includes('React.FC') && lines[i].includes(') => {')) {
                lines.splice(i + 1, 0, "  const { t } = useTranslation(['common']);");
                inserted = true;
                console.log('Inserted at line', i + 1);
                break;
            }
            // 첫 번째 useState 이전에 삽입
            if (lines[i].includes('= useState') && !inserted) {
                lines.splice(i, 0, "  const { t } = useTranslation(['common']);");
                inserted = true;
                console.log('Inserted before useState at line', i);
                break;
            }
        }
        if (inserted) {
            c = lines.join('\n');
            fs.writeFileSync(fp, c, 'utf-8');
            console.log('GatewayListTab: const { t } added');
        }
    } else {
        const before = c.substring(Math.max(0, tIdx - 100), tIdx);
        console.log('GatewayListTab: t found at idx', tIdx, '| before:', before.substring(-50).trim().substring(0, 50));
    }
})();
