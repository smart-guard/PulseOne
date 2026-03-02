/**
 * v30 - GatewayListTab + BasicInfoForm + InputVariableSourceSelector + ExportGatewayWizard + MqttBrokerDashboard
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

// GatewayListTab
total += fix('/app/src/pages/export-gateway/tabs/GatewayListTab.tsx', [
    ["message: `\"${gw.name}\" Gateway 프로세스를 Start하시겠습니까?`", "message: `Start the \"${gw.name}\" gateway process?`"],
    ["message: 'API 호출 중 Error가 발생했습니다.'", "message: 'An error occurred during API call.'"],
    ["message: `\"${gw.name}\" Gateway 프로세스를 Stop하시겠습니까?`", "message: `Stop the \"${gw.name}\" gateway process?`"],
    ["message: 'Gateway 프로세스를 Stop했습니다.'", "message: 'Gateway process stopped.'"],
    ["title: 'Stop 에러'", "title: 'Stop Error'"],
    ["title: 'Gateway 재Start OK'", "title: 'Gateway Restart'"],
    ["message: `\"${gw.name}\" Gateway 프로세스를 재Start하시겠습니까?`", "message: `Restart the \"${gw.name}\" gateway process?`"],
    ["message: 'Gateway 프로세스를 재Start했습니다.'", "message: 'Gateway process restarted.'"],
    ["title: '재Start 에러'", "title: 'Restart Error'"],
]);

// BasicInfoForm
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx', [
    ["description: 'All Time 대비 설비가 실제로 가동된 Time의 비중을 Calculate합니다.'", "description: 'Calculates the ratio of actual operating time to total time.'"],
    ["name: '차이 Calculate (Delta)'", "name: 'Difference (Delta)'"],
    ["description: '입구와 출구, 혹은 두 지점 간의 데이터 차이를 Calculate합니다.'", "description: 'Calculates the data difference between two points (e.g., inlet and outlet).'"],
    [">업종별 표준 Settings을 한 번의 클릭으로 ", ">Apply industry-standard settings in one click "],
    [">1단계: 기본 식별<", ">Step 1: Basic Identification<"],
    [">다른 Point와 중복되지 않는 고유한 Name을", ">Use a unique name not duplicated by other points"],
    [">min류 Category<", ">Category<"],
    ['placeholder="New Category 직접 Input..."', 'placeholder="Enter new category..."'],
    [">Description 및 Notes<", ">Description & Notes<"],
    ['placeholder="해당 가상Point의 Calculate 목적이나 데이터 소스에 대한 Description을 Input하세요."', 'placeholder="Describe the purpose or data source of this virtual point."'],
]);

// InputVariableSourceSelector
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/InputVariableSourceSelector.tsx', [
    ["// Device Filter링 및 성능 최적화", "// Device filtering and performance optimization"],
    ["// 데이터 Loading 함수들 - useCallback 최적화", "// data loading functions - useCallback optimized"],
    ["// Device Filter가 Settings되어 있는데 결과가 없는 경우 Logs", "// log when device filter is set but no results"],
    [`console.log(\`✅ 데이터Point \${result.points.length} 로드됨\`)`, `console.log(\`✅ DataPoint \${result.points.length} loaded\`)`],
    [`console.warn(\`⚠️ Device ID \${deviceFilter}에 대한 데이터Point가 없습니다.\`)`, `console.warn(\`⚠️ Device ID \${deviceFilter}: no DataPoints found.\`)`],
    [`console.log(\`✅ 가상Point \${result.data.length} 로드됨\`)`, `console.log(\`✅ VirtualPoint \${result.data.length} loaded\`)`],
    ["console.error('❌ 가상Point Loading Failed:", "console.error('❌ VirtualPoint Loading Failed:"],
    ["// 백엔드 Failed 시 목 데이터", "// mock data on backend failure"],
    ["console.log('🎭 목 가상Point 데이터 Use')", "console.log('🎭 using mock VirtualPoint data')"],
    ["// 데이터포인트일 때만 로드", "// only load for data_point type"],
]);

// ExportGatewayWizard - 라인 872 (title= JSX 속성 한글 검사)
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    ["> 게이트웨이 생성<", "> Create Gateway<"],
    ["> 게이트웨이 편집<", "> Edit Gateway<"],
]);

// MqttBrokerDashboard - 라인 363/376/390 (label)
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    [">최대 메모리 사용률<", ">Max Memory Usage<"],
    [">알림 허용 기준<", ">Alert Threshold<"],
    [">메모리 관리 정책<", ">Memory Policy<"],
    [">연결 제한<", ">Connection Limit<"],
    [">채널 제한<", ">Channel Limit<"],
    [">큐 제한<", ">Queue Limit<"],
]);

console.log('\nTotal:', total);
