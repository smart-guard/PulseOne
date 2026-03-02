/**
 * regex 기반 한글 단어 전역 치환 - ExportGatewayWizard/TemplateManagementTab 데이터 배열
 * label/desc 속성 안의 한글을 단어 단위로 교체
 */
const fs = require('fs');
const path = require('path');

const koreanRe = /[\uAC00-\uD7AF]/;

// 사전식 치환 맵
const DICT = [
    // label 내 한글
    [/label:\s*'([^']*[가-힣]+[^']*)'/g, (m, t) => `label: '${translateText(t)}'`],
    [/label:\s*"([^"]*[가-힣]+[^"]*)"/g, (m, t) => `label: "${translateText(t)}"`],
    // desc 내 한글
    [/desc:\s*'([^']*[가-힣]+[^']*)'/g, (m, t) => `desc: '${translateText(t)}'`],
    [/desc:\s*"([^"]*[가-힣]+[^"]*)"/g, (m, t) => `desc: "${translateText(t)}"`],
    // name 내 한글
    [/name:\s*'([^']*[가-힣]+[^']*)'/g, (m, t) => `name: '${translateText(t)}'`],
    // target_description, des 내 한글
    [/target_description:\s*"([^"]*[가-힣]+[^"]*)"/, (m, t) => `target_description: "${translateText(t)}"`],
    [/des:\s*"([^"]*[가-힣]+[^"]*)"/, (m, t) => `des: "${translateText(t)}"`],
    // 주석
    [/site_name:\s*"([^"]*[가-힣]+[^"]*)"/, (m, t) => `site_name: "${translateText(t)}"`],
    [/device_name:\s*"([^"]*[가-힣]+[^"]*)"/, (m, t) => `device_name: "${translateText(t)}"`],
];

// 간단한 단어 번역 함수
function translateText(input) {
    let t = input;
    // 포함될 수 있는 구 번역
    const phrases = [
        ['배열 강제', 'array forced'],
        ['값이 1이면 참', 'if value=1 then true'],
        ['1일때 0, 0일때 1로 반전하여', 'inverts 1→0, 0→1'],
        ['숫자 상태값을 읽기 쉬운 한글로 변환', 'converts numeric status to human-readable text'],
        ['상세 시각 포맷', 'Detailed datetime format'],
        ['표준 시각 포맷', 'Standard datetime format'],
        ['1970년 기준 밀리초', 'Milliseconds since 1970 epoch'],
        ['ISO 포맷', 'ISO format'],
        ['구로본사', 'HQ Seoul'],
        ['냉동기-01', 'Chiller-01'],
        ['프로파일에 정의된 데이터 설명', 'Data description defined in profile'],
        ['레거시 건물', 'Legacy building'],
        ['번호', 'number'],
        ['배 율', 'multiplier'],
        ['배열', 'array'],
        ['값 치환', 'Value mapping'],
        ['디지털 반전', 'Digital invert'],
        ['상태 텍스트', 'Status text'],
        ['스마트 로직', 'Smart Logic'],
        ['매핑 명칭', 'Mapping name'],
        ['NORMAL, WARNING, CRITICAL 등', 'NORMAL, WARNING, CRITICAL, etc.'],
        ['정상', 'Normal'],
        ['장애', 'Fault'],
        ['1:제어 가능, 0:읽기 전용', '1:controllable, 0:read-only'],
        ['num:숫자, bit:디지털, str:문자열', 'num:number, bit:digital, str:string'],
        ['표준 사이트', 'Standard site'],
        ['숫자 ID', 'numeric ID'],
        ['실제 사이트/건물 이름', 'Actual site/building name'],
        ['실제 장비 이름', 'Actual device name'],
        ['수집기 내부 원본 포인트명', 'Collector internal original point name'],
        ['원본 포인트', 'Original Point'],
        ['원본 건물', 'Original Building'],
        ['미터링 최소 한계값', 'Metering minimum limit'],
        ['미터링 최대 한계값', 'Metering maximum limit'],
        ['임계치 정보', 'Information Threshold'],
        ['임계치 위험', 'Extra Threshold'],
        ['통신끊김', 'disconnected'],
        ['주의', 'warning'],
        ['경고', 'critical'],
        ['건물 번호', 'Building number'],
        ['공장 넘버', 'Factory number'],
    ];
    for (const [ko, en] of phrases) {
        t = t.split(ko).join(en);
    }
    return t;
}

const TARGET_FILES = [
    '/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx',
    '/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx',
];

let total = 0;

for (const fp of TARGET_FILES) {
    if (!fs.existsSync(fp)) continue;
    let c = fs.readFileSync(fp, 'utf-8');
    const orig = c;

    for (const [pattern, replacer] of DICT) {
        c = c.replace(pattern, replacer);
    }

    // 남은 JSX 텍스트 노드 한글
    c = c.replace(/>\s*게이트웨이 기본 설정\s*</g, '>Gateway Settings<');
    c = c.replace(/>\s*새 게이트웨이 만들기\s*</g, '>Create New Gateway<');
    c = c.replace(/>\s*기존 게이트웨이 수정\s*</g, '>Edit Existing Gateway<');
    c = c.replace(/placeholder="예:/g, 'placeholder="e.g.,');
    c = c.replace(/message: `유효하지 않은 JSON 형식입니다/g, "message: `Invalid JSON format");
    c = c.replace(/JSON 형식이 올바르지 않거나 렌더링 중 오류가 발생했습니다/g, "JSON format invalid or rendering error occurred");

    if (c !== orig) {
        fs.writeFileSync(fp, c, 'utf-8');
        const remaining = (c.match(/[가-힣]/g) || []).length;
        const was = (orig.match(/[가-힣]/g) || []).length;
        console.log(`FIXED ${path.basename(fp)}: ${was} → ${remaining} Korean chars`);
        total++;
    }
}

console.log('\nDone:', total, 'files fixed');
