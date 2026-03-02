/**
 * DeviceDetailModal + DeviceDataPointsBulkModal + DeviceRtuMonitorTab 남은 한글 처리 v11
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

// ============ DeviceDetailModal.tsx ============
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    [`message: \`"\${savedDevice.name}" 디바이스가 성공적으로 생성되었습니다.\\n\\nID: \${savedDevice.id}\\n\\n데이터포인트 설정`, `message: \`Device "\${savedDevice.name}" created successfully.\\n\\nID: \${savedDevice.id}\\n\\nData point settings`],
    ["throw new Error(response.error || '생성 실패')", "throw new Error(response.error || 'Create failed')"],
    ["title: savedDevice.sync_warning ? '저장 완료 (동기화 경고)' : '디바이스 수정 완료'", "title: savedDevice.sync_warning ? 'Saved (Sync Warning)' : 'Device Updated'"],
    [`message: \`"\${editData.name}" 디바이스가 성공적으로`, `message: \`Device "\${editData.name}" successfully`],
    ["디바이스가 성공적으로 수정되었습니다.", "updated successfully."],
    ["res.message || '수정 실패'", "res.message || 'Update failed'"],
    ["title: '수정 완료'", "title: 'Updated'"],
    ["title: '수정 실패'", "title: 'Update Failed'"],
    ["title: '생성 완료'", "title: 'Created'"],
    ["title: '생성 실패'", "title: 'Create Failed'"],
]);

// ============ DeviceDataPointsBulkModal.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ["title: '알림',\n        message: '템플릿 정보를 불러오는 데 실패했습니다.'", "title: 'Notice',\n        message: 'Failed to load template information.'"],
    ["title: '알림',\n        message: '이 템플릿에는 정의된 포인트가 없습니다.'", "title: 'Notice',\n        message: 'This template has no defined points.'"],
    ["title: '알림',\n        message: '템플릿 적용 중 오류가 발생했습니다.'", "title: 'Notice',\n        message: 'Error applying template.'"],
    ["`${flatPoints.length}개의 데이터 필드를 찾았습니다.\\n[주소] 열에 해당 토픽(Sub-topic)을 입력해주세요.`", "`Found ${flatPoints.length} data fields.\\nPlease enter the topic (Sub-topic) in the [Address] column.`"],
    [`msg.textContent = \`✅ \${count}개 비트 포인트 자동 생성 (protocol_params.bit_index 방식)\``, `msg.textContent = \`✅ ${`\${count}`} bit points auto-generated (protocol_params.bit_index)\``],
    ["errors.push('유효하지 않은 타입')", "errors.push('Invalid type')"],
    ["errors.push('유효하지 않은 권한')", "errors.push('Invalid access mode')"],
    ["errors.push('이미 존재하는 주소(DB)')", "errors.push('Address already exists in DB')"],
]);

// ============ DeviceRtuMonitorTab.tsx 남은 한글 ============
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    [">통신 테스트</", ">Communication Test</"],
    ["// 95% 성공률", "// 95% success rate"],
    [">최근 통신 로그<", ">Recent Comm Logs<"],
    [">성공률<", ">Success Rate<"],
    [">응답시간<", ">Response Time<"],
    [">총 시도<", ">Total Attempts<"],
    [">성공<", ">Success<"],
    [">실패<", ">Failed<"],
    [">최신순<", ">Latest<"],
    [">오래된순<", ">Oldest<"],
]);

// ============ DeviceDataPointsTab.tsx ============
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    ["const msg = val ? `값: ${val.value} (${val.quality})\\n시간: ${val.timestamp}` : 'Failed to re", "const msg = val ? `Value: ${val.value} (${val.quality})\\nTime: ${val.timestamp}` : 'Failed to re"],
    ["`Done: success ${successCount}${failCount > 0 ? `, failed ${failCount}` : ''} ", "`Done: success ${successCount}${failCount > 0 ? `, failed ${failCount}` : ''} "],
]);

// ============ DeviceBasicInfoTab.tsx ============  
total += fix('/app/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx', [
    ["title: '테스트 실패'", "title: 'Test Failed'"],
    ["title: '연결 테스트'", "title: 'Connection Test'"],
    ["confirmText: '테스트 실행'", "confirmText: 'Run Test'"],
    [">연결 테스트 중...<", ">Testing connection...<"],
    [">연결 테스트<", ">Connection Test<"],
    [">접속 정보<", ">Connection Info<"],
    [">기본 정보<", ">Basic Info<"],
    [">설치 위치<", ">Install Location<"],
]);

console.log('\nTotal:', total);
