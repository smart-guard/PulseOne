/**
 * 대규모 파일별 정확한 한글 치환 스크립트 V2
 * showModal, alert, JSX 레이블 등 포함
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

// ============ DataPointModal.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DataPointModal.tsx', [
    ["'포인트명은 필수입니다'", "'Point name is required'"],
    ["'주소는 필수입니다'", "'Address is required'"],
    ["'데이터 타입은 필수입니다'", "'Data type is required'"],
    ["|| '생성 실패'", "|| 'Failed to create'"],
    ["|| '수정 실패'", "|| 'Failed to update'"],
    ["alert(`저장 실패: ${error.message}`)", "alert(`Save failed: ${error.message}`)"],
    ["alert(`삭제 실패: ${error.message}`)", "alert(`Delete failed: ${error.message}`)"],
    ["'데이터포인트를 삭제하시겠습니까?'", "'Delete this data point?'"],
    ["|| '삭제 실패'", "|| 'Failed to delete'"],
    ["{mode === 'create' ? '새 데이터포인트 추가' :", "{mode === 'create' ? t('dataPoint.addNew') :"],
    ["mode === 'edit' ? '데이터포인트 편집' : '데이터포인트 상세'}", "mode === 'edit' ? t('dataPoint.edit') : t('dataPoint.detail')}"],
    ["><h3>📊 기본 정보</h3>", "><h3>{t('common:basicInfo')}</h3>"],
    ["><label>포인트명 *</label>", "><label>{t('dataPoint.name')} *</label>"],
]);

// ============ DeviceDataPointsTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    ["title: '입력 오류'", "title: 'Input Error'"],
    ["message: '필수 입력값(이름, 주소)을 확인해주세요.'", "message: 'Please check required fields (name, address).'"],
    ["title: '변경 내용 없음'", "title: 'No Changes'"],
    ["message: '수정된 내용이 없습니다.'", "message: 'No changes have been made.'"],
    ["title: '저장 완료'", "title: 'Saved'"],
    ["title: '저장 실패'", "title: 'Save Failed'"],
    ["message: '저장 중 오류가 발생했습니다.'", "message: 'An error occurred while saving.'"],
    ["title: '읽기 테스트 결과'", "title: 'Read Test Result'"],
    ["title: '읽기 실패'", "title: 'Read Failed'"],
    ["message: '데이터를 읽어오지 못했습니다.'", "message: 'Failed to read data.'"],
    ["title: '통신 오류'", "title: 'Communication Error'"],
    ["message: '백엔드 서버와 통신할 수 없습니다.'", "message: 'Cannot communicate with backend server.'"],
    ["title: '일괄 등록 완료'", "title: 'Bulk Registration Complete'"],
    ["'값을 읽지 못했습니다.'", "'Failed to read value.'"],
]);

// ============ DeviceDataPointsBulkModal.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ["title: '알림',\n        message: '템플릿 정보를 불러오는 데 실패했습니다.'", "title: 'Notice',\n        message: 'Failed to load template information.'"],
    ["title: '알림',\n        message: '이 템플릿에는 정의된 포인트가 없습니다.'", "title: 'Notice',\n        message: 'This template has no defined points.'"],
    ["title: '템플릿 적용 확인'", "title: 'Apply Template'"],
    ["message: '기존에 입력된 데이터가 있습니다. 템플릿 데이터로 덮어씌우시겠습니까?'", "message: 'Existing data will be overwritten. Apply template?'"],
    ["confirmText: '덮어씌우기'", "confirmText: 'Overwrite'"],
    ["title: '알림',\n        message: '템플릿 적용 중 오류가 발생했습니다.'", "title: 'Notice',\n        message: 'Error applying template.'"],
    ["title: '파싱 완료'", "title: 'Parsing Complete'"],
    ["title: '파싱 실패'", "title: 'Parsing Failed'"],
    ["message: '유효한 JSON 형식이 아닙니다. 형식을 확인해주세요.'", "message: 'Invalid JSON format. Please check the format.'"],
    ["errors.push('이름 필수')", "errors.push('Name required')"],
    ["errors.push('주소 필수')", "errors.push('Address required')"],
]);

// ============ DeviceRtuMonitorTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    [">RTU 디바이스가 아닙니다</h3>", ">Not an RTU device</h3>"],
    ["|| '통계 로드 실패'", "|| 'Stats load failed'"],
    ["'통계를 불러올 수 없습니다'", "'Unable to load statistics'"],
    ["|| '통신 테스트 실패'", "|| 'Communication test failed'"],
    ["'통신 테스트 중 오류가 발생했습니다'", "'Error during communication test'"],
    ["alert(`통신 테스트 실패: ${err instanceof Error ? err.message : 'Unknown error'}`)", "alert(`Communication test failed: ${err instanceof Error ? err.message : 'Unknown error'}`)"],
    ["{isMonitoring ? '모니터링 중지' : '모니터링 시작'}", "{isMonitoring ? t('monitor.stop') : t('monitor.start')}"],
    ["><option value=\"1min\">1분</option>", "><option value=\"1min\">1 {t('common:minute')}</option>"],
]);

// ============ SiteDetailModal.tsx ============
total += fix('/app/src/components/modals/SiteModal/SiteDetailModal.tsx', [
    ["setError('사이트 정보를 불러오는데 실패했습니다.')", "setError('Failed to load site information.')"],
    ["setError('데이터를 불러오는데 실패했습니다.')", "setError('Failed to load data.')"],
    ["setError(res.message || 'Collector 배정에 실패했습니다.')", "setError(res.message || 'Failed to assign Collector.')"],
    ["setError(err.message || 'Collector 배정 중 오류가 발생했습니다.')", "setError(err.message || 'Error assigning Collector.')"],
    ["setError(res.message || 'Collector 해제에 실패했습니다.')", "setError(res.message || 'Failed to release Collector.')"],
    ["setError(err.message || 'Collector 해제 중 오류가 발생했습니다.')", "setError(err.message || 'Error releasing Collector.')"],
    ["title: '변경 사항 저장'", "title: 'Save Changes'"],
    ["message: '입력하신 변경 내용을 저장하시겠습니까?'", "message: 'Save the changes you made?'"],
    ["message: '사이트 정보가 성공적으로 수정되었습니다.'", "message: 'Site information updated successfully.'"],
    ["setError(res.message || '업데이트에 실패했습니다.')", "setError(res.message || 'Update failed.')"],
    ["setError(err.message || '서버 통신 중 오류가 발생했습니다.')", "setError(err.message || 'Server communication error.')"],
    ["title: '사이트 삭제 확인'", "title: 'Delete Site'"],
    ["title: '삭제 완료'", "title: 'Deleted'"],
]);

// ============ DeviceTemplateWizard.tsx ============
total += fix('/app/src/components/modals/DeviceTemplateWizard.tsx', [
    ["setError('초기 데이터를 불러오는 중 오류가 발생했습니다.')", "setError('Error loading initial data.')"],
    ["setError('템플릿 정보를 불러오는 중 오류가 발생했습니다.')", "setError('Error loading template information.')"],
    ["setError('모델 정보를 불러오는 중 오류가 발생했습니다.')", "setError('Error loading model information.')"],
    ["setError('마스터 모델 목록을 불러오는 중 오류가 발생했습니다.')", "setError('Error loading master model list.')"],
    ["title: '필수 정보 누락'", "title: 'Required Info Missing'"],
    ["message: '모든 필수 정보를 입력해주세요.'", "message: 'Please fill in all required fields.'"],
    ["title: '접속 정보 누락'", "title: 'Connection Info Missing'"],
    ["message: '접속 IP 주소와 포트 번호는 필수입니다.'", "message: 'IP address and port number are required.'"],
    ["title: '설정 누락'", "title: 'Setting Missing'"],
    ["title: 'ID 충돌 감지'", "title: 'ID Conflict Detected'"],
    ["- 프로토콜: ${selectedProtocol?.display_name || '-'}", "- Protocol: ${selectedProtocol?.display_name || '-'}"],
    ["- 엔드포인트: ${finalEndpoint || '-'}", "- Endpoint: ${finalEndpoint || '-'}"],
]);

// ============ NetworkSettings.tsx (남은 항목) ============
total += fix('/app/src/pages/NetworkSettings.tsx', [
    [">송신</label>", ">{t('field.sent')}</label>"],
    [">DHCP 자동 설정 사용 중</span>", ">{t('network.dhcpAuto')}</span>"],
    [">방화벽 규칙</h2>", ">{t('network.firewallRules')}</h2>"],
    [">규칙명</div>", ">{t('network.ruleName')}</div>"],
    [">동작</div>", ">{t('network.action')}</div>"],
    [">소스</div>", ">{t('network.source')}</div>"],
    [">목적지</div>", ">{t('network.destination')}</div>"],
    [">우선순위</div>", ">{t('network.priority')}</div>"],
    ["data-label=\"규칙명\"", "data-label={t('network.ruleName')}"],
    ["data-label=\"상태\"", "data-label={t('common:status')}"],
    ["data-label=\"동작\"", "data-label={t('network.action')}"],
    ["data-label=\"프로토콜\"", "data-label={t('common:protocol')}"],
    ["data-label=\"소스\"", "data-label={t('network.source')}"],
    ["data-label=\"목적지\"", "data-label={t('network.destination')}"],
    ["data-label=\"우선순위\"", "data-label={t('network.priority')}"],
    ["data-label=\"액션\"", "data-label={t('common:actions')}"],
    ["{rule.action === 'allow' ? '허용' : '차단'}", "{rule.action === 'allow' ? t('network.allow') : t('network.block')}"],
]);

// ============ SystemStatus.tsx (남은 항목) ============
total += fix('/app/src/pages/SystemStatus.tsx', [
    ["`${controllableServices.length}개의 서비스를 ${action === 'start' ? 'start' :", "`Confirm ${action} for ${controllableServices.length} service(s)?`; if (!window.confirm(msg)) return; const confirm2 = ''; //"],
    ["alert('선택된 서비스 제어 중 오류가 발생했습니다.')", "alert('Error controlling selected services.')"],
    ["'방금 전'", "'just now'"],
    [">데이터를 불러오는 중...<", ">Loading data...<"],
    ["title=\"중지\"", "title=\"Stop\""],
    ["title=\"시작\"", "title=\"Start\""],
    ["title=\"재시작\"", "title=\"Restart\""],
    ["> 정지</", "> Stop</"],
    ["> 시작</", "> Start</"],
    ["> 재시작</", "> Restart</"],
    ["> 핵심 인프라</", "> Core Infrastructure</"],
    ["> 수집기 파티션</", "> Collector Partition</"],
    [">각 엣지 서버별 데이터 수집 엔진이 독립적으로 ", ">Each edge server runs its data collection engine independently "],
    [">상세 서비스 목록</h3>", ">Service List</h3>"],
    ["> 일괄 재시작</", "> Bulk Restart</"],
]);

// ============ BackupRestore.tsx (남은 항목) ============
total += fix('/app/src/pages/BackupRestore.tsx', [
    ["confirmText: '확인'", "confirmText: t('common:confirm')"],
    ["alert(`${t('confirm.saveDoneTitle')} 실패: ${result.message}`)", "alert(`${t('confirm.saveDoneTitle')} failed: ${result.message}`)"],
    ["alert(`백업 실패: ${result.message}`)", "alert(`Backup failed: ${result.message}`)"],
]);

console.log('\nTotal:', total, 'fixes');
