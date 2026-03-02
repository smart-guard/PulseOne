/**
 * ExportGatewayWizard 남은 한글 처리 + DeviceDetailModal 심층 처리
 * DeviceRtuScanDialog도 포함
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

// ExportGatewayWizard - 남은 variable panel label/desc
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    ["label: '공장 ID'", "label: 'Factory ID'"],
    ["label: '공장명'", "label: 'Factory Name'"],
    ["label: '제조사'", "label: 'Manufacturer'"],
    ["label: '모델'", "label: 'Model'"],
    ["label: '포인트명'", "label: 'Point Name'"],
    ["label: '시작 시간'", "label: 'Start Time'"],
    ["label: '종료 시간'", "label: 'End Time'"],
    ["label: '건물 ID'", "label: 'Building ID'"],
    ["desc: '레거시 건물(Building) 번호'", "desc: 'Legacy building number'"],
    ["desc: '표준 사이트 숫자 ID (Legacy: {{bd}})'", "desc: 'Standard site numeric ID (Legacy: {{bd}})'"],
    ["desc: '실제 사이트/건물 이름'", "desc: 'Actual site/building name'"],
    ["label: '계층 경로'", "label: 'Hierarchy Path'"],
    ["label: '수집 시간'", "label: 'Collection Time'"],
    ["desc: '데이터가 수집된 실제 서버 시간'", "desc: 'Actual server time when data was collected'"],
    // 위자드 UI 한글
    [">다음 단계<", ">Next<"],
    [">이전 단계<", ">Previous<"],
    [">게이트웨이 설정<", ">Gateway Settings<"],
    [">타겟 설정<", ">Target Settings<"],
    [">프로파일 설정<", ">Profile Settings<"],
    // confirm/alert
    ["message: '저장되지 않은 변경사항이 있습니다. 계속하시겠습니까?'", "message: 'You have unsaved changes. Continue?'"],
    ["message: '게이트웨이가 성공적으로 생성되었습니다.'", "message: 'Gateway created successfully.'"],
    ["message: '게이트웨이가 성공적으로 수정되었습니다.'", "message: 'Gateway updated successfully.'"],
    ["message: res.message || '게이트웨이 저장에 실패했습니다.'", "message: res.message || 'Failed to save gateway.'"],
    ["message: '게이트웨이 저장 중 오류가 발생했습니다.'", "message: 'Error occurred while saving gateway.'"],
    ["title: '저장 확인'", "title: 'Confirm Save'"],
    ["title: '나가기 확인'", "title: 'Confirm Exit'"],
    ["confirmText: '나가기'", "confirmText: 'Exit'"],
    ["confirmText: '저장 후 나가기'", "confirmText: 'Save and Exit'"],
]);

// DeviceDetailModal - 추가 처리
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ["? `아래 변경 사항을 반영하여 디바이스를 ${actionText}하시겠습니까?`", "? `Apply the following changes to ${actionText} the device?`"],
    ["`디바이스를 ${actionText}하시겠습니까?`", "`Confirm device ${actionText}?`"],
    ["title: `디바이스 ${actionText} 확인`", "title: `Confirm Device ${actionText}`"],
    ["message: `디바이스가 성공적으로 ${actionText}되었습니다.`", "message: `Device ${actionText}d successfully.`"],
    ["message: res.message || `디바이스 ${actionText}에 실패했습니다.`", "message: res.message || `Device ${actionText} failed.`"],
    ["message: `디바이스 ${actionText} 중 오류가 발생했습니다.`", "message: `Error during device ${actionText}.`"],
    // 탭 이름
    [">기본 정보<", ">{t('common:basicInfo')}<"],
    [">데이터 포인트<", ">{t('device.tab.dataPoints')}<"],
    [">로그<", ">{t('common:logs')}<"],
    [">설정<", ">{t('common:settings')}<"],
    [">모니터링<", ">{t('common:monitoring')}<"],
    // 기타 텍스트
    ["console.error('❌ editData 없음')", "console.error('❌ editData is null')"],
    ["console.log('✅ 디바이스 저장 성공')", "console.log('✅ Device saved successfully')"],
    ["console.error('❌ 디바이스 저장 실패:', error)", "console.error('❌ Device save failed:', error)"],
]);

// DeviceRtuScanDialog
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx', [
    ["title: '스캔 시작'", "title: 'Start Scan'"],
    ["title: '스캔 완료'", "title: 'Scan Complete'"],
    ["title: '스캔 실패'", "title: 'Scan Failed'"],
    ["message: '네트워크 스캔을 시작하시겠습니까?'", "message: 'Start network scan?'"],
    ["message: '스캔이 완료되었습니다.'", "message: 'Scan completed.'"],
    ["message: res.message || '스캔에 실패했습니다.'", "message: res.message || 'Scan failed.'"],
    ["confirmText: '스캔 시작'", "confirmText: 'Start Scan'"],
    [">IP 범위<", ">IP Range<"],
    [">스캔 결과<", ">Scan Results<"],
    [">장치 추가<", ">Add Device<"],
    ["placeholder=\"시작 IP\"", "placeholder=\"Start IP\""],
    ["placeholder=\"종료 IP\"", "placeholder=\"End IP\""],
]);

console.log('\nTotal:', total);
