/**
 * GatewayListTab + MqttBrokerDashboard + ProtocolDashboard + DeviceRtuMonitorTab 한글 처리 v10
 */
const fs = require('fs');

function fix(fp, repls) {
    if (!fs.existsSync(fp)) return 0;
    let c = fs.readFileSync(fp, 'utf-8');
    let n = 0;
    for (const [a, b] of repls) {
        if (c.includes(a)) { c = c.split(a).join(b); n++; }
    }
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log('FIXED', n, require('path').basename(fp)); }
    return n;
}

let total = 0;

// ============ GatewayListTab.tsx ============
total += fix('/app/src/pages/export-gateway/tabs/GatewayListTab.tsx', [
    ["title: '게이트웨이 삭제'", "title: 'Delete Gateway'"],
    [`message: \`"${`$`}{gw.name}" 게이트웨이를 정말 삭제하시겠습니까?\\n이 작업은 되돌릴 수 없습니다.\``, `message: \`Delete gateway "$\{gw.name}"?\\nThis action cannot be undone.\``],
    ["message: e.message || '삭제 중 오류가 발생했습니다.'", "message: e.message || 'Error during deletion.'"],
    ["title: '게이트웨이 시작 확인'", "title: 'Start Gateway'"],
    ["confirmText: '시작'", "confirmText: 'Start'"],
    ["message: '게이트웨이 프로세스가 성공적으로 시작되었습니다.'", "message: 'Gateway process started successfully.'"],
    ["title: '시작 완료'", "title: 'Started'"],
    ["message: res.message || '프로세스 시작에 실패했습니다.'", "message: res.message || 'Failed to start process.'"],
    ["title: '시작 실패'", "title: 'Start Failed'"],
    ["title: '시작 에러'", "title: 'Start Error'"],
    ["title: '중지 확인'", "title: 'Stop Gateway'"],
    ["confirmText: '중지'", "confirmText: 'Stop'"],
    ["message: '게이트웨이 프로세스가 성공적으로 중지되었습니다.'", "message: 'Gateway process stopped successfully.'"],
    ["title: '중지 완료'", "title: 'Stopped'"],
    ["message: res.message || '프로세스 중지에 실패했습니다.'", "message: res.message || 'Failed to stop process.'"],
    ["title: '중지 실패'", "title: 'Stop Failed'"],
    ["title: '재시작 확인'", "title: 'Restart Gateway'"],
    ["confirmText: '재시작'", "confirmText: 'Restart'"],
    ["message: '게이트웨이 프로세스가 성공적으로 재시작되었습니다.'", "message: 'Gateway process restarted successfully.'"],
    ["title: '재시작 완료'", "title: 'Restarted'"],
    ["message: res.message || '프로세스 재시작에 실패했습니다.'", "message: res.message || 'Failed to restart process.'"],
    ["title: '재시작 실패'", "title: 'Restart Failed'"],
]);

// ============ MqttBrokerDashboard.tsx ============
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    ["{ id: 'monitoring', label: '실시간 관제'", "{ id: 'monitoring', label: 'Live Monitor'"],
    ["{ id: 'clients', label: '접속 기기 목록'", "{ id: 'clients', label: 'Connected Devices'"],
    ["{ id: 'info', label: '기술 상세 사양'", "{ id: 'info', label: 'Technical Specs'"],
    ["{ id: 'security', label: '브로커 보안 설정'", "{ id: 'security', label: 'Security Settings'"],
    ['label="현재 활성 세션"', 'label="Active Sessions"'],
    ['label="메시지 수신속도"', 'label="Msg Receive Rate"'],
    ['label="메시지 전송속도"', 'label="Msg Send Rate"'],
    ['label="엔진 점유 메모리"', 'label="Memory Usage"'],
      ["> 상태 새로고침</", "> Refresh Status</"],
    ]);

// ============ ProtocolDashboard.tsx ============
total += fix('/app/src/components/protocols/ProtocolDashboard.tsx', [
    ["changes.push(`시리얼 지원: ${protocol.uses_serial}", "changes.push(`Serial support: ${protocol.uses_serial}"],
    ["label=\"연결 상태\"", "label=\"Connection Status\""],
    ["value={protocol.is_enabled ? '정상' : '비활성'}", "value={protocol.is_enabled ? 'Normal' : 'Inactive'}"],
    ["label=\"연결된 인스턴스\"", "label=\"Connected Instances\""],
    ["label=\"연결된 장치\"", "label=\"Connected Devices\""],
    ["label=\"오류로그(24h)\"", "label=\"Error Logs(24h)\""],
    ["changes.push(`기본 폴링 간격:", "changes.push(`Default polling interval:"],
]);

// ============ DeviceRtuMonitorTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    ["이 탭은 Modbus RTU 프로토콜을 사용하는 디바이스에서만 표시됩니다.", "This tab is only shown for devices using Modbus RTU protocol."],
    ["`연결 성공! 응답시간: ${result.response_time_ms}ms`", "`Connection success! Response time: ${result.response_time_ms}ms`"],
    ["`연결 실패: ${result.error_message}`", "`Connection failed: ${result.error_message}`"],
    ["><option value=\"1min\">1분</option>", "><option value=\"1min\">1 min</option>"],
    ["><option value=\"5min\">5분</option>", "><option value=\"5min\">5 min</option>"],
    ["><option value=\"15min\">15분</option>", "><option value=\"15min\">15 min</option>"],
    ["><option value=\"1hour\">1시간</option>", "><option value=\"1hour\">1 hour</option>"],
    [">통신 테스트</", ">Comm Test</"],
    ["><label>현재 응답시간</label>", "><label>Current Response Time</label>"],
    ["><label>평균 응답시간</label>", "><label>Avg Response Time</label>"],
]);

// ============ DeviceTemplateWizard.tsx ============
total += fix('/app/src/components/modals/DeviceTemplateWizard.tsx', [
    ["아래 설정으로 디바이스를 생성하시겠습니까?", "Create device with following settings?"],
    ["■ 기본 정보", "■ Basic Info"],
    ["- 디바이스 명: ${deviceName}", "- Device Name: ${deviceName}"],
    ["- 설치 사이트: ${selectedSiteName}", "- Site: ${selectedSiteName}"],
    ["- 담당 콜렉터: ${selectedCollectorName}", "- Collector: ${selectedCollectorName}"],
    ["■ 통신 접속", "■ Communication"],
    ["■ 운영 및 안정성", "■ Operation & Reliability"],
    ["- 폴링 / 타임아웃: ${pollingInterval}ms / ${timeout}ms", "- Polling / Timeout: ${pollingInterval}ms / ${timeout}ms"],
    ["- 재시도: ${retryCount}회 (간격: ${retryInterval}ms)", "- Retry: ${retryCount}x (interval: ${retryInterval}ms)"],
    ["- 백오프: 초기 ${backoffTime}ms (최대 ${maxBackoffTime}ms, x${backoffMultiplier}배율)", "- Backoff: initial ${backoffTime}ms (max ${maxBackoffTime}ms, x${backoffMultiplier})"],
    ["- 버퍼 / 큐: R:${readBufferSize} / W:${writeBufferSize} / Q:${queueSize}", "- Buffer/Queue: R:${readBufferSize} / W:${writeBufferSize} / Q:${queueSize}"],
]);

// ============ DataPointModal.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DataPointModal.tsx', [
    ["`✅ 읽기 성공: ${result.current_value} (응답시간: ${result.response_time_ms}ms)`", "`✅ Read success: ${result.current_value} (response: ${result.response_time_ms}ms)`"],
    ["`❌ 테스트 실패: ${response.error}`", "`❌ Test failed: ${response.error}`"],
    ["`✅ 쓰기 성공: ${result.written_value} (응답시간: ${result.response_time_ms}ms)`", "`✅ Write success: ${result.written_value} (response: ${result.response_time_ms}ms)`"],
    ["{dataPoint?.description || '없음'}", "{dataPoint?.description || 'N/A'}"],
    ['placeholder="데이터포인트 설명"', 'placeholder="Data point description"'],
    ["><label>주소 *</label>", "><label>Address *</label>"],
]);

// ============ DeviceDataPointsTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    ["><label>이름 *</label>", "><label>Name *</label>"],
    ["><span className=\"hint\">한 레지스터를 비트 단위로 쪼갤 때 사용 (0=LSB)</span>", "><span className=\"hint\">Used to split one register into bits (0=LSB)</span>"],
    ["><label>접근 모드</label>", "><label>Access Mode</label>"],
    ["><label>그룹명 (Group)</label>", "><label>Group Name</label>"],
    ["><label>태그 (Tags, 콤마 구분)</label>", "><label>Tags (comma separated)</label>"],
    [">포인트 활성화</", ">Enable Point</"],
]);

console.log('\nTotal:', total);
