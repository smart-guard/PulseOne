/**
 * ProtocolDashboard + ProtocolInstanceManager + ControlSchedulePage + DataExport + DeviceDataPointsTab 처리 v24
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
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log('FIXED', n, path.basename(fp)); }
    return n;
}

let total = 0;

// ====== ProtocolDashboard.tsx ======
total += fix('/app/src/components/protocols/ProtocolDashboard.tsx', [
    ["changes.push(`시리얼 지원: ${protocol.uses_serial}", "changes.push(`Serial support: ${protocol.uses_serial}"],
    ["{ id: 'instances', label: '인스턴스'", "{ id: 'instances', label: 'Instances'"],
    ["{ id: 'security', label: '보안/인증'", "{ id: 'security', label: 'Security/Auth'"],
    ["{ id: 'devices', label: '장치 목록'", "{ id: 'devices', label: 'Device List'"],
    ["{/* 탭 컨텐츠 */}", "{/* Tab Contents */}"],
    [">실시간 모니터링할 장치가 없습니다. 프로토콜을 사용하는 장치를 먼저 등록해주세요.</p>", ">No devices to monitor in real-time. Please register devices using this protocol first.</p>"],
]);

// ====== ProtocolInstanceManager.tsx ======
total += fix('/app/src/components/protocols/ProtocolInstanceManager.tsx', [
    ["changes.push(`이름: ${or", "changes.push(`Name: ${or"],
    ["changes.push(`설명 수정됨`)", "changes.push(`Description changed`)"],
    ["changes.push(`활성화 상태: ${orig", "changes.push(`Enabled: ${orig"],
    ["changes.push(`소유 테넌트: ${oldTenant}", "changes.push(`Owner tenant: ${oldTenant}"],
    ["changes.push(`제어 모드: ${ori", "changes.push(`Control mode: ${ori"],
    ["title: '알림'", "title: 'Notice'"],
    ["message: '변경사항이 없습니다.'", "message: 'No changes.'"],
    ["confirmText: '확인'", "confirmText: 'OK'"],
    ["title: '설정 수정 확인'", "title: 'Confirm Settings Update'"],
    ["message: `다음 변경사항을 저장하시겠습니까?", "message: `Save the following changes?"],
]);

// ====== ControlSchedulePage.tsx ======
total += fix('/app/src/pages/ControlSchedulePage.tsx', [
    ["'* * * * *': '매분'", "'* * * * *': 'Every minute'"],
    ["'0 * * * *': '매시간 정각'", "'0 * * * *': 'Every hour'"],
    ["'0 9 * * *': '매일 오전 9시'", "'0 9 * * *': 'Daily 09:00'"],
    ["'0 18 * * *': '매일 오후 6시'", "'0 18 * * *': 'Daily 18:00'"],
    ["'0 9 * * 1': '매주 월요일 오전 9시'", "'0 9 * * 1': 'Weekly Mon 09:00'"],
    ["'0 9 * * 1-5': '평일 오전 9시'", "'0 9 * * 1-5': 'Weekdays 09:00'"],
    ["return presets[expr] || `반복 (${expr})`", "return presets[expr] || `Repeat (${expr})`"],
    ["{enabled ? '활성' : '비활성'}", "{enabled ? 'Active' : 'Inactive'}"],
    ["{ label: '매분', value: '* * * * *' }", "{ label: 'Every minute', value: '* * * * *' }"],
    ["{ label: '매시간', value: '0 * * * *' }", "{ label: 'Every hour', value: '0 * * * *' }"],
    ["{ label: '매일 오전 9시', value: '0 9 * * *' }", "{ label: 'Daily 09:00', value: '0 9 * * *' }"],
    ["{ label: '매일 오후 6시', value: '0 18 * * *' }", "{ label: 'Daily 18:00', value: '0 18 * * *' }"],
    ["{ label: '평일 오전 9시', value: '0 9 * * 1-5' }", "{ label: 'Weekdays 09:00', value: '0 9 * * 1-5' }"],
]);

// ====== DataExport.tsx ======
total += fix('/app/src/pages/DataExport.tsx', [
    ["{ id: 'daily-prod', title: '일일 생산 데이터', desc: '전일 생산 핵심 지표 및 장치 상태 데이터를 CSV로 추출합니다.',", "{ id: 'daily-prod', title: 'Daily Production Data', desc: 'Exports previous day production KPIs and device status data in CSV.',"],
    ["{ id: 'weekly-alarm', title: '주간 알람 리포트', desc: '최근 7일간 발생한 모든 알람 및 조치 이력을 PDF로 생성합니다", "{ id: 'weekly-alarm', title: 'Weekly Alarm Report', desc: 'Generates PDF of all alarms and actions from the last 7 days"],
    ["{ id: 'monthly-status', title: '월간 장치 통계', desc: '한 달간의 장치 가동률 및 주요 상태 변화를 분석 리포트로 내보", "{ id: 'monthly-status', title: 'Monthly Device Stats', desc: 'Exports monthly availability and status changes as analysis report"],
    ["{ id: 'ops-log', title: '시스템 운영 로그', desc: '시스템 접속 및 장치 제어 로그를 JSON 형식으로 내보냅니다.', ico", "{ id: 'ops-log', title: 'System Operation Log', desc: 'Exports system access and device control logs in JSON format.', ico"],
    ["title: '데이터 내보내기 시작'", "title: 'Start Data Export'"],
    ["message: `${exportName} 작업을 시작하시겠습니까? 데이터 양에 따라 수 분이 소요될 수 있습니다.`", "message: `Start ${exportName} export? This may take a few minutes depending on data size.`"],
    ["confirmText: '시작'", "confirmText: 'Start'"],
    ["// 임시 파일 크기", "// temp file size"],
    ["alert(`내보내기 완료: ${result.data.filename}\\n총 ${result.data.total_records}건의 데이터가 추출되었습니", "alert(`Export complete: ${result.data.filename}\\nTotal ${result.data.total_records} records extracted"],
    ["throw new Error(result.message || '내보내기 실패')", "throw new Error(result.message || 'Export failed')"],
]);

// ====== DeviceDataPointsTab.tsx ======
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    [">포인트 활성화\n", ">Enable Point\n"],
    ["<label>최소값 (Min)</label>", "<label>Min Value</label>"],
    ["<label>최대값 (Max)</label>", "<label>Max Value</label>"],
    [">데이터 로깅 활성화\n", ">Enable Data Logging\n"],
    ["<label>로깅 간격 (ms)</label>", "<label>Logging Interval (ms)</label>"],
    ["<span className=\"hint\">0 = 변경 시마다 로깅 (COV)</span>", "<span className=\"hint\">0 = log on every change (COV)</span>"],
    ["<label>로깅 데드밴드 (Deadband)</label>", "<label>Logging Deadband</label>"],
    ["<span className=\"hint\">값 변화량이 이보다 클 때만 저장</span>", "<span className=\"hint\">Only save when value changes by more than this</span>"],
    [">알람 사용\n", ">Enable Alarm\n"],
]);

console.log('\nTotal:', total);
