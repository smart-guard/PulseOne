/**
 * 잔여 한글 대규모 처리 V3 - 15개 파일
 * alert/showModal/setError/JSX 레이블
 */
const fs = require('fs');
const path = require('path');

function fix(fp, replacements) {
    if (!fs.existsSync(fp)) { console.log('SKIP:', path.basename(fp)); return 0; }
    let c = fs.readFileSync(fp, 'utf-8');
    let n = 0;
    for (const [a, b] of replacements) {
        if (c.includes(a)) { c = c.split(a).join(b); n++; }
    }
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log('FIXED', n, 'in', path.basename(fp)); }
    return n;
}

let total = 0;

// ============ DeviceTemplateWizard.tsx ============
total += fix('/app/src/components/modals/DeviceTemplateWizard.tsx', [
    ["message: `${field.label} 필드는 필수입니다.`", "message: `${field.label} field is required.`"],
    ["message: `ID ${conflictId}번은 해당 경로(Endpoint)에서 이미 사용 중입니다.\\n다른 ID를 선택해주세요.`", "message: `ID ${conflictId} is already in use at this Endpoint.\\nPlease select a different ID.`"],
    ["title: '최종 등록 확인'", "title: 'Final Registration'"],
    ["message: '디바이스가 성공적으로 생성되었습니다.'", "message: 'Device created successfully.'"],
    ["message: res.message || '디바이스 생성에 실패했습니다.'", "message: res.message || 'Failed to create device.'"],
    ["title: '오류 발생'", "title: 'Error'"],
    ["message: '디바이스 생성 중 오류가 발생했습니다.'", "message: 'An error occurred while creating the device.'"],
    ["- 프로토콜: ${selectedProtocol?.display_name || '-'}", "- Protocol: ${selectedProtocol?.display_name || '-'}"],
    ["- 엔드포인트: ${finalEndpoint || '-'}", "- Endpoint: ${finalEndpoint || '-'}"],
    ["`\\n■ 상세 사양\\n${protocolParams}`", "`\\n■ Specifications\\n${protocolParams}`"],
    ["- 검증: 이상치 탐지(${isOutlierDetection ? 'ON' : 'OFF'}), 데드밴드(${isDeadband ? 'ON' : 'OFF'})", "- Validation: Outlier(${isOutlierDetection ? 'ON' : 'OFF'}), Deadband(${isDeadband ? 'ON' : 'OFF'})"],
    ["- 태그: ${deviceTags.join(', ')}", "- Tags: ${deviceTags.join(', ')}"],
]);

// ============ DataPointModal.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DataPointModal.tsx', [
    ["console.error('데이터포인트 저장 실패:', error)", "console.error('DataPoint save failed:', error)"],
    [`if (confirm(\`"${`$`}{dataPoint.name}" 데이터포인트를 삭제하시겠습니까?\`))`, `if (confirm(\`Delete data point "$\{dataPoint.name}"?\`))`],
    ["console.error('데이터포인트 삭제 실패:', error)", "console.error('DataPoint delete failed:', error)"],
    ["setTestResult(`❌ 읽기 실패: ${result.error_message}`)", "setTestResult(`❌ Read failed: ${result.error_message}`)"],
    ["setTestResult(`❌ 테스트 오류: ${error.message}`)", "setTestResult(`❌ Test error: ${error.message}`)"],
    ["setTestResult(`❌ 쓰기 실패: ${result.error_message}`)", "setTestResult(`❌ Write failed: ${result.error_message}`)"],
    ["><h3>📊 기본 정보</h3>", "><h3>📊 {t('common:basicInfo')}</h3>"],
    ["><label>포인트명 *</label>", "><label>{t('dataPoint.name')} *</label>"],
    ['placeholder="데이터포인트 이름"', "placeholder={t('dataPoint.namePlaceholder')}"],
]);

// ============ DeviceDataPointsTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    ["message: '데이터포인트 정보가 임시 반영되었습니다.\\n장치 모달 우측 하단의 [저장] 버튼을 클릭해야 서버에 최종 저장됩니다.'", "message: 'Data point changes applied temporarily.\\nClick [Save] at the bottom right to save to server.'"],
    ["`, 실패 ${failCount}건` : ''}", "`, failed ${failCount}` : ''} "],
    ["완료: 성공 ${successCount}건${failCount > 0 ? ", "Done: success ${successCount}${failCount > 0 ? "],
    ["(디바이스 전체", "(Save all device"],
    ["title: '일괄 등록 실패'", "title: 'Bulk Registration Failed'"],
    ['일괄 저장 중 오류가 발생했습니다:', "Bulk save error:"],
    ['><label>이름 *</label>', ">{t('common:name')} *</label>"],
    [">{protocolType === 'MQTT' ? 'Sub-Topic' : '주소 (Address)'} *</label>", ">{protocolType === 'MQTT' ? 'Sub-Topic' : 'Address'} *</label>"],
    ["placeholder=\"비워두면 레지스터 전체 값 사용\"", "placeholder=\"Leave empty to use full register value\""],
    ["<span className=\"hint\">FC03이 일반적인 레지스터. BOOL 포인트는 FC01</span>", "<span className=\"hint\">FC03 for general registers. BOOL points use FC01</span>"],
    [">Bit Index (0-15, 선택)</label>", ">Bit Index (0-15, optional)</label>"],
]);

// ============ DeviceDataPointsBulkModal.tsx - 남은 한글 ============
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ["// 레지스터 기본 주소 (예: \"40001\")", "// Register base address (e.g., \"40001\")"],
]);

// ============ DeviceRtuMonitorTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    ["<option value=\"1min\">1 {t('common:minute')}</option>", "<option value=\"1min\">1 min</option>"],
    ["console.log('RTU 통계 로드 완료:', data.data)", "console.log('RTU stats loaded:', data.data)"],
    ["console.log('API 응답 실패, 시뮬레이션 데이터 사용')", "console.log('API failed, using simulation data')"],
    ["console.error('통계 로드 실패:', err)", "console.error('Stats load failed:', err)"],
    ["console.log('RTU 모니터링 중지')", "console.log('RTU monitoring stopped')"],
    ["console.log('RTU 통신 테스트 결과:', result)", "console.log('RTU comm test result:', result)"],
    ["console.error('통신 테스트 실패:', err)", "console.error('Comm test failed:', err)"],
]);

// ============ DeviceRtuNetworkTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx', [
    ["console.error('RTU config 파싱 실패:', error)", "console.error('RTU config parse failed:', error)"],
    ["throw new Error(response.error || '슬래이브 디바이스 조회 실패')", "throw new Error(response.error || 'Slave device query failed')"],
    ["console.error('슬래이브 디바이스 로드 실패:', error)", "console.error('Slave device load failed:', error)"],
    ["throw new Error('마스터 디바이스가 RTU 프로토콜이 아닙니다')", "throw new Error('Master device is not RTU protocol')"],
    ["console.log('마스터 디바이스 정보 로드 완료')", "console.log('Master device info loaded')"],
    ["throw new Error(response.error || '마스터 디바이스 조회 실패')", "throw new Error(response.error || 'Master device query failed')"],
    ["console.error('마스터 디바이스 로드 실패:', error)", "console.error('Master device load failed:', error)"],
    ["throw new Error(response.error || '디바이스 상태 변경 실패')", "throw new Error(response.error || 'Device status change failed')"],
    ["console.error('슬래이브 토글 실패:', error)", "console.error('Slave toggle failed:', error)"],
]);

// ============ DeviceBasicInfoTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx', [
    ["console.error('❌ 프로토콜 목록 로드 실패:', error)", "console.error('❌ Protocol list load failed:', error)"],
    ["console.error('❌ 그룹 목록 로드 실패:', error)", "console.error('❌ Group list load failed:', error)"],
    ["console.error('❌ 콜렉터 목록 로드 실패:', error)", "console.error('❌ Collector list load failed:', error)"],
    ["console.error('❌ 제조사 목록 로드 실패:', error)", "console.error('❌ Manufacturer list load failed:', error)"],
    ["console.error('❌ 프로토콜 인스턴스 로드 실패:', error)", "console.error('❌ Protocol instance load failed:', error)"],
    ["console.error('❌ 모델 목록 로드 실패:', error)", "console.error('❌ Model list load failed:', error)"],
    ["console.error('Slave ID 중복 체크 실패:', error)", "console.error('Slave ID duplicate check failed:', error)"],
    ["console.error('❌ 사이트 목록 로드 실패:', error)", "console.error('❌ Site list load failed:', error)"],
    ["title: '테스트 실패'", "title: 'Test Failed'"],
    ["message: `❌ 서버 시스템 오류: ${response.error}`", "message: `❌ Server system error: ${response.error}`"],
]);

// ============ ProtocolEditor.tsx ============
total += fix('/app/src/components/modals/ProtocolModal/ProtocolEditor.tsx', [
    ["throw new Error(response.message || '프로토콜 로드 실패')", "throw new Error(response.message || 'Protocol load failed')"],
    ["? err.message : '프로토콜 로드 실패'", "? err.message : 'Protocol load failed'"],
    ["title: '프로토콜 수정 확인'", "title: 'Edit Protocol'"],
    ["confirmText: '수정하기'", "confirmText: 'Edit'"],
    ["throw new Error('프로토콜 타입과 표시명은 필수입니다.')", "throw new Error('Protocol type and display name are required.')"],
    ["throw new Error('지원되지 않는 모드입니다.')", "throw new Error('Unsupported mode.')"],
    ["title: '저장 완료'", "title: 'Saved'"],
    ["throw new Error(response?.message || '저장 실패')", "throw new Error(response?.message || 'Save failed')"],
    ["프로토콜 \"${protocol.display_name || protocol.protocol_type}\"을(를) 수정하시겠습니까?\\n\\n수정된 설", "Edit protocol \"${protocol.display_name || protocol.protocol_type}\"?\\n\\nChanged settings"],
    ["`프로토콜 \"${protocol.display_name}\" 설정이 성공적으로 수정되었습니다.`", "`Protocol \"${protocol.display_name}\" settings updated successfully.`"],
]);

// ============ MqttBrokerDashboard.tsx ============
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    ["? 'MQTT 브로커 서비스를 활성화하시겠습니까?'", "? 'Activate MQTT broker service?'"],
    [": '브로커를 비활성화하면 연결된 모든 센서의 통신이 중단됩니다. 계속하시겠습니까?'", ": 'Deactivating the broker will stop all connected sensor communications. Continue?'"],
    ["title: enabled ? '서비스 활성화' : '서비스 비활성화'", "title: enabled ? 'Enable Service' : 'Disable Service'"],
    ["confirmText: enabled ? '활성화' : '서비스 중단'", "confirmText: enabled ? 'Enable' : 'Stop Service'"],
    ["title: 'API Key 재발급'", "title: 'Regenerate API Key'"],
    ["message: 'API Key를 재발급하면 기존 Key를 사용하는 모든 센서의 접속이 끊어집니다. 계속하시겠습니까?'", "message: 'Regenerating the API Key will disconnect all sensors using the current key. Continue?'"],
    ["confirmText: '재발급 실행'", "confirmText: 'Regenerate'"],
    ["{isBrokerEnabled ? (brokerStatus?.is_healthy ? '운영 중' : '주의') : '비활성'}", "{isBrokerEnabled ? (brokerStatus?.is_healthy ? 'Running' : 'Warning') : 'Inactive'}"],
]);

// ============ ProtocolDashboard.tsx ============
total += fix('/app/src/components/protocols/ProtocolDashboard.tsx', [
    ["changes.push(`프로토콜 명칭: ${protocol.display_name}", "changes.push(`Protocol name: ${protocol.display_name}"],
    ["changes.push(`기본 포트: ${protocol.default_port}", "changes.push(`Default port: ${protocol.default_port}"],
    ["changes.push(`시리얼 지원: ${protocol.uses_serial}", "changes.push(`Serial support: ${protocol.uses_serial}"],
    ["changes.push(`브로커 필수: ${protocol", "changes.push(`Broker required: ${protocol"],
    ["changes.push('상세 설명 내용 변경')", "changes.push('Description changed')"],
    ["changes.push(`카테고리: ${protocol.category}", "changes.push(`Category: ${protocol.category}"],
    ["// 🔥 상하좌우 여백을 주어 보더가 잘리지 않도록 함", "// padding to prevent border clipping"],
]);

// ============ SiteDetailModal.tsx ============
total += fix('/app/src/components/modals/SiteModal/SiteDetailModal.tsx', [
    ["message: `'${site.name}' 사이트를 삭제하시겠습니까? 하위 사이트가 있는 경우 삭제할 수 없습니다.`", "message: `Delete site '${site.name}'? Sites with sub-sites cannot be deleted.`"],
    ["message: '사이트가 성공적으로 삭제되었습니다.'", "message: 'Site deleted successfully.'"],
    ["setError(res.message || '삭제에 실패했습니다. 하위 사이트 유무를 확인하세요.')", "setError(res.message || 'Delete failed. Check for sub-sites.')"],
    ["setError(err.message || '삭제 중 오류가 발생했습니다.')", "setError(err.message || 'Error during deletion.')"],
    [">사이트 정보를 불러오지 못했습니다.</div>", ">Failed to load site information.</div>"],
    [">사이트명 *</div>", ">{t('site.name')} *</div>"],
    [">사이트 유형</div>", ">{t('site.type')}</div>"],
    ["<option value=\"company\">본사</option>", "<option value=\"company\">{t('site.typeHeadquarters')}</option>"],
    ["<option value=\"office\">오피스/지사</option>", "<option value=\"office\">{t('site.typeOffice')}</option>"],
    ["<option value=\"factory\">공장/플랜트</option>", "<option value=\"factory\">{t('site.typeFactory')}</option>"],
]);

// ============ BasicInfoForm.tsx (VirtualPoints) ============
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx', [
    ["['에너지', '생산량', '설비 효율', '환경', '품질', '전력']", "['Energy', 'Production', 'Facility Efficiency', 'Environment', 'Quality', 'Power']"],
    ["name: '단순 합산 (Totalizer)'", "name: 'Simple Sum (Totalizer)'"],
    ["category: '에너지'", "category: 'Energy'"],
    ["description: '여러 포인트의 전력 소무량을 합산하여 총 사용량을 계산합니다.'", "description: 'Calculates total power consumption by summing multiple points.'"],
    ["name: '생산 효율 (Efficiency)'", "name: 'Production Efficiency'"],
    ["category: '생산량'", "category: 'Production'"],
    ["description: '목표 대비 실제 생산 실적을 백분율로 환산합니다.'", "description: 'Converts actual production vs target into a percentage.'"],
    ["name: '임계치 알림 (Overload)'", "name: 'Threshold Alert (Overload)'"],
    ["category: '전력'", "category: 'Power'"],
    ["description: '전류/전력값이 설정된 임계치를 넘었는지 감시합니다.'", "description: 'Monitors whether current/power values exceed the set threshold.'"],
]);

console.log('\nTotal:', total, 'fixes');
