/**
 * ManufacturerDetailModal + MqttBrokerDashboard + GroupSidePanel + AlarmWebSocketService + ScheduleManagementTab 처리 v25
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

// ====== ManufacturerDetailModal.tsx ======
total += fix('/app/src/components/modals/ManufacturerModal/ManufacturerDetailModal.tsx', [
    ["'대한민국': 'South Korea'", "'대한민국': 'South Korea'"],
    ["'한국': 'South Korea'", "'한국': 'South Korea'"],
    ["'미국': 'USA'", "'미국': 'USA'"],
    ["'일본': 'Japan'", "'일본': 'Japan'"],
    ["'중국': 'China'", "'중국': 'China'"],
    ["'독일': 'Germany'", "'독일': 'Germany'"],
    ["'영국': 'UK'", "'영국': 'UK'"],
    ["setError('제조사 정보를 불러오는데 실패했습니다.')", "setError('Failed to load manufacturer information.')"],
    ["setError('제조사명은 필수 항목입니다.')", "setError('Manufacturer name is required.')"],
    ["title: '변경 사항 저장'", "title: 'Save Changes'"],
    ["message: '입력하신 변경 내용을 저장하시겠습니까?'", "message: 'Save the entered changes?'"],
    ["message: '제조사 정보가 성공적으로 수정되었습니다.'", "message: 'Manufacturer updated successfully.'"],
    ["setError(res.message || '업데이트에 실패했습니다.')", "setError(res.message || 'Update failed.')"],
    ["setError(err.message || '서버 통신 중 오류가 발생했습니다.')", "setError(err.message || 'Server communication error.')"],
]);

// ====== MqttBrokerDashboard.tsx ======
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    [">저장 완료</", ">Saved</"],
    ["> 사양 편집 모드</", "> Spec Edit Mode</"],
    ["> 상태 새로고침</i>", "> Refresh Status</i>"],
]);

// ====== GroupSidePanel.tsx ======
total += fix('/app/src/pages/DeviceList/components/GroupSidePanel.tsx', [
    ["title: '생성 실패'", "title: 'Create Failed'"],
    ["message: `그룹 생성에 실패했습니다: ${res.error || '알 수 없는 오류'}`", "message: `Group creation failed: ${res.error || 'Unknown error'}`"],
    ["confirmText: '확인'", "confirmText: 'OK'"],
    ["title: '오류 발생'", "title: 'Error'"],
    ["message: `오류가 발생했습니다: ${err.message || '알 수 없는 오류'}`", "message: `An error occurred: ${err.message || 'Unknown error'}`"],
    ["title: '수정 실패'", "title: 'Update Failed'"],
    ["message: `그룹 수정에 실패했습니다: ${res.error || '알 수 없는 오류'}`", "message: `Group update failed: ${res.error || 'Unknown error'}`"],
    ["title: '삭제 확인'", "title: 'Confirm Delete'"],
    ["message: `'${state.editingGroup.name}' 그룹을 삭제하시겠습니까?`", "message: `Delete group '${state.editingGroup.name}'?`"],
    ["title: '삭제 실패'", "title: 'Delete Failed'"],
    ["message: `그룹 삭제에 실패했습니다: ${res.error || '알 수 없는 오류'}`", "message: `Group deletion failed: ${res.error || 'Unknown error'}`"],
]);

// ====== ScheduleManagementTab.tsx ======
total += fix('/app/src/pages/export-gateway/tabs/ScheduleManagementTab.tsx', [
    ["title: '수정사항 없음'", "title: 'No Changes'"],
    ["message: '수정된 정보가 없습니다.'", "message: 'No changes to save.'"],
    ["message: '스케줄이 성공적으로 저장되었습니다.'", "message: 'Schedule saved successfully.'"],
    ["title: '저장 완료'", "title: 'Saved'"],
    ["message: '저장 중 오류가 발생했습니다.'", "message: 'Error occurred while saving.'"],
    ["title: '저장 실패'", "title: 'Save Failed'"],
]);

// ====== AlarmWebSocketService.ts ======
total += fix('/app/src/services/AlarmWebSocketService.ts', [
    ["message: 'Frontend WebSocket 연결 확인'", "message: 'Frontend WebSocket connection confirmed'"],
    ["this.notifyError(`WebSocket 연결 실패: ${error.message}`)", "this.notifyError(`WebSocket connection failed: ${error.message}`)"],
    ["this.notifyError(`WebSocket 에러: ${error.message || '알 수 없는 에러'}`)", "this.notifyError(`WebSocket error: ${error.message || 'Unknown error'}`)"],
    ["reject(new Error('Socket.IO 연결 타임아웃'))", "reject(new Error('Socket.IO connection timeout'))"],
    ["this.notifyError('WebSocket 연결 없음')", "this.notifyError('No WebSocket connection')"],
    ["this.notifyError(`테스트 알람 전송 실패: ${error}`)", "this.notifyError(`Test alarm send failed: ${error}`)"],
    ["this.reconnectAttempts = 0; // 재연결 횟수 리셋", "this.reconnectAttempts = 0; // reset reconnect count"],
]);

console.log('\nTotal:', total);
