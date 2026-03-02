/**
 * v35 - TemplateManagementTab + ExportGatewayWizard 잔여 패턴 정밀 처리
 */
const fs = require('fs');
const path = require('path');

function fix(fp, repls) {
    if (!fs.existsSync(fp)) return 0;
    let c = fs.readFileSync(fp, 'utf-8');
    let n = 0;
    for (const [a, b] of repls) {
        if (c.includes(a)) { c = c.split(a).join(b); n++; }
    }
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log(`FIXED ${n} ${path.basename(fp)}`); }
    return n;
}

let total = 0;

// TemplateManagementTab
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    ["if value=1 then true, 0이면 거짓값을 Output 예: {map:target_key:A:B}'", "if value=1 then true, 0 then false. e.g., {map:target_key:A:B}'"],
    ["팁: 중괄호를 제외한 나머지 구조가 올바른 JSON인지 OK해 wk세요.", "Tip: Check if the structure excluding curly braces is valid JSON."],
    [">대상 데이터</label>", ">Target Data</label>"],
    [">참(1)d 때</label>", ">When True (1)</label>"],
    [">거짓(0)d 때</label>", ">When False (0)</label>"],
    [">태그 삽입\n", ">Insert Tag\n"],
    [">포맷 Filter</div>", ">Format Filter</div>"],
    [">JSON 배열 강제 (|array)\n", ">Force JSON Array (|array)\n"],
    [">Template Create 가이드: 3단계면 충min합니다!</div>", ">Template Creation Guide: Just 3 steps!</div>"],
    [">왼쪽에서 데이터 Select</div>", ">Select data from the left</div>"],
    [">표에 JSON Name Input</div>", ">Enter JSON Name in the table</div>"],
    [">Preview OK 후 Save</div>", ">Preview and Save</div>"],
    [">스마트 Payload Create기</div>", ">Smart Payload Builder</div>"],
    [">기존 JSON 샘플을 붙여넣으면 PulseOne Template으로 자동 변환합니다.</div>", ">Paste existing JSON sample to auto-convert to PulseOne template.</div>"],
    ["'숨기기'", "'Hide'"],
    ["'샘플 Paste'", "'Paste Sample'"],
    ["placeholder='[{\"bd\":9, \"ty\":\"num\", ...}] 형태의 JSON 샘플을 여기에 붙여넣으세요.'", "placeholder='Paste your JSON sample here, e.g., [{\"bd\":9, \"ty\":\"num\", ...}]'"],
    [">Analytics 및 Apply</button>", ">Analyze & Apply</button>"],
    [">Template 명칭</label>", ">Template Name</label>"],
    ['placeholder="e.g., 표준 JSON Payload"', 'placeholder="e.g., Standard JSON Payload"'],
]);

// ExportGatewayWizard
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    ["'Gateway Register 마법사'", "'Gateway Registration Wizard'"],
    ["권장", "recommended"],
    ["할당된 Device Alarm만 Process합니다.", "Only processes assigned device alarms."],
    ["를 주입하세요. 범용 설계를 위해 사이트별 고정 변수가 아닌 메타데이터 중심의 스키마를 권장합니다.", "to inject. Recommend metadata-centric schema over site-specific fixed variables for universal design."],
    [">전송 모드 가이드\n", ">Transmission Mode Guide\n"],
    ["트래픽의 예측이 가능하며 리Port Create에 적합합니다.", "Traffic is predictable and ideal for report generation."],
    ["실Time Alarm 및 즉각적인 대응이 필요한 경우에 Use합니다.", "Use when real-time alarms and immediate response are required."],
    [">정기 Transmit (Scheduled)</Radio.Button>", ">Scheduled Transmission</Radio.Button>"],
    [">이벤트 기반 (Real-time)</Radio.Button>", ">Event-based (Real-time)</Radio.Button>"],
    [">Schedule Settings 방식</span>", ">Schedule Settings Method</span>"],
    [">기존 Schedule Apply</Radio.Button>", ">Apply Existing Schedule</Radio.Button>"],
    [">Schedule 명칭</span>", ">Schedule Name</span>"],
    ['placeholder="e.g., 5min wk기 정기 Transmit"', 'placeholder="e.g., 5min periodic transmission"'],
    [">Cron 표현식</span>", ">Cron Expression</span>"],
    ["* 표준 Cron Format을 Support합니다 (min 시 d 월 요d)", "* Supports standard Cron format (min hour day month weekday)"],
]);

console.log('\nTotal:', total);
