/**
 * ExportGatewayWizard + TemplateManagementTab + DeviceDetailModal 한글 처리
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

// ====== 공통 변수 패널 데이터 (ExportGatewayWizard + TemplateManagementTab 모두) ======
const VARIABLE_PANEL_REPLACEMENTS = [
    // 그룹명
    ["name: '매핑 (Target Mapping)'", "name: 'Mapping (Target Mapping)'"],
    ["name: '수집기 원본 (Collector)'", "name: 'Collector Origin'"],
    ["name: '데이터 속성 (Attributes)'", "name: 'Data Attributes'"],
    ["name: '메타데이터 (Metadata)'", "name: 'Metadata'"],
    ["name: '상태 및 알람 (Status)'", "name: 'Status & Alarm'"],
    ["name: '범위 및 한계 (Ranges)'", "name: 'Ranges & Limits'"],
    ["name: '시간 (Timestamp)'", "name: 'Timestamp'"],
    // 레이블
    ["label: '매핑 명칭 (TARGET KEY)'", "label: 'Mapping Name (TARGET KEY)'"],
    ["desc: '프로파일 탭에서 설정한 데이터의 최종 이름'", "desc: 'Final name of data configured in Profile tab'"],
    ["label: '데이터 값 (VALUE)'", "label: 'Data Value (VALUE)'"],
    ["desc: 'Scale/Offset이 반영된 실제 측정값'", "desc: 'Actual measured value with Scale/Offset applied'"],
    ["label: '타켓 설명 (DESC)'", "label: 'Target Description (DESC)'"],
    ["desc: '프로파일에서 설정한 데이터 설명'", "desc: 'Data description set in Profile'"],
    ["label: '건물 ID'", "label: 'Building ID'"],
    ["desc: '원본 건물(Building) ID'", "desc: 'Original Building ID'"],
    ["label: '포인트 ID'", "label: 'Point ID'"],
    ["desc: '원본 포인트(Point) ID'", "desc: 'Original Point ID'"],
    ["label: '원본 이름'", "label: 'Original Name'"],
    ["desc: '수집기 내부 원본 포인트명'", "desc: 'Internal collector original point name'"],
    ["label: '데이터 타입'", "label: 'Data Type'"],
    ["desc: 'num:숫자, bit:디지털, str:문자열'", "desc: 'num:number, bit:digital, str:string'"],
    ["desc: 'num:숫자, bit:디지털, str:문자열 ({{data_type}}도 가능)'", "desc: 'num:number, bit:digital, str:string ({{data_type}} also works)'"],
    ["label: '제어 가능 여부'", "label: 'Controllable'"],
    ["desc: '1:제어 가능, 0:읽기 전용 ({{is_writable}}도 가능)'", "desc: '1:controllable, 0:read-only ({{is_writable}} also works)'"],
    ["label: '사이트 ID'", "label: 'Site ID'"],
    ["desc: '표준 사이트 숫자 ID (Legacy: {{bd}})'", "desc: 'Standard site numeric ID (Legacy: {{bd}})'"],
    ["label: '사이트 명칭'", "label: 'Site Name'"],
    ["desc: '실제 사이트/건물 이름 (예: 구로본사)'", "desc: 'Actual site/building name (e.g., HQ Seoul)'"],
    ["label: '디바이스 명칭'", "label: 'Device Name'"],
    ["desc: '실제 장비 이름 (예: 냉동기-01)'", "desc: 'Actual device name (e.g., Chiller-01)'"],
    ["label: '통신 상태'", "label: 'Communication Status'"],
    ["desc: '0:정상, 1:통신끊김'", "desc: '0:normal, 1:disconnected'"],
    ["label: '알람 등급'", "label: 'Alarm Level'"],
    ["desc: '0:정상, 1:주의, 2:경고'", "desc: '0:normal, 1:warning, 2:critical'"],
    ["label: '알람 상태명'", "label: 'Alarm Status'"],
    ["label: '계측 범위(Min)'", "label: 'Measurement Range (Min)'"],
    ["desc: '미터링 최소 한계값'", "desc: 'Metering minimum limit value'"],
    ["desc: '미터링 최소 한계값 (배열 강제)'", "desc: 'Metering minimum limit (array forced)'"],
    ["label: '계측 범위(Max)'", "label: 'Measurement Range (Max)'"],
    ["desc: '미터링 최대 한계값'", "desc: 'Metering maximum limit value'"],
    ["desc: '미터링 최대 한계값 (배열 강제)'", "desc: 'Metering maximum limit (array forced)'"],
    ["label: '정보 한계'", "label: 'Information Limit'"],
    ["desc: '임계치 정보(Information Limit)'", "desc: 'Threshold information limit'"],
    ["label: '위험 한계'", "label: 'Danger Limit'"],
    ["desc: '임계치 위험(Extra Limit)'", "desc: 'Threshold danger limit'"],
    ["label: '표준 시간'", "label: 'Standard Timestamp'"],
];

let total = 0;
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', VARIABLE_PANEL_REPLACEMENTS);
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', VARIABLE_PANEL_REPLACEMENTS);

// ====== ExportGatewayWizard.tsx 추가 한글 ======
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    // 나머지 특이 label/desc
    ["label: '공장 ID'", "label: 'Factory ID'"],
    ["desc: '레거시 건물(Building) 번호'", "desc: 'Legacy building number'"],
    ["label: '제조사'", "label: 'Manufacturer'"],
    ["label: '모델'", "label: 'Model'"],
    ["label: '포인트명'", "label: 'Point Name'"],
    ["label: '공장명'", "label: 'Factory Name'"],
    ["label: '시작 시간'", "label: 'Start Time'"],
    ["label: '종료 시간'", "label: 'End Time'"],
    // confirm 메시지
    ["title: '설정 진행'", "title: 'Setup Progress'"],
    ["title: '저장 완료'", "title: 'Saved'"],
    ["title: '저장 실패'", "title: 'Save Failed'"],
    ["title: '오류'", "title: 'Error'"],
    ["title: '확인'", "title: 'Confirm'"],
    ["message: '변경 사항이 있습니다. 저장하시겠습니까?'", "message: 'You have unsaved changes. Save them?'"],
    ["message: '설정이 저장되었습니다.'", "message: 'Settings saved successfully.'"],
    // 위자드 스텝
    [">기본 설정<", ">Basic Settings<"],
    [">프로파일 설정<", ">Profile Settings<"],
    [">스케줄 설정<", ">Schedule Settings<"],
    [">완료<", ">Complete<"],
]);

// ====== DeviceDetailModal.tsx ======
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    // console.log 한글
    ["console.log('🔄 로그 탭에서 수정 모드로 전환됨 - 기본정보 탭으로 강제 이동')", "console.log('🔄 Edit mode from log tab - switching to basic info tab')"],
    ["console.log('📋 showCustomModal 호출:', config.type, config.title)", "console.log('📋 showCustomModal called:', config.type, config.title)"],
    ["console.log('✅ 모달 확인 버튼 클릭됨')", "console.log('✅ Modal confirm button clicked')"],
    ["console.log('🔥 콜백 실행 시작...')", "console.log('🔥 Callback execution started...')"],
    ["console.log('✅ 콜백 실행 완료')", "console.log('✅ Callback execution complete')"],
    ["console.error('❌ 콜백 실행 오류:', error)", "console.error('❌ Callback execution error:', error)"],
    ["console.log('❌ 모달 취소 버튼 클릭됨')", "console.log('❌ Modal cancel button clicked')"],
    ["console.error('❌ 취소 콜백 실행 오류:', error)", "console.error('❌ Cancel callback error:', error)"],
    ["throw new Error(response.error || '데이터포인트 조회 실패')", "throw new Error(response.error || 'DataPoint query failed')"],
    ["console.log('🔥 handleSave 함수 진입')", "console.log('🔥 handleSave entered')"],
    ["console.log('❌ editData가 없음')", "console.log('❌ editData is null')"],
    // 변경 사항 로그
    ["{ key: 'protocol_id', label: '프로토콜 ID' }", "{ key: 'protocol_id', label: 'Protocol ID' }"],
    ["{ key: 'protocol_instance_id', label: '프로토콜 인스턴스' }", "{ key: 'protocol_instance_id', label: 'Protocol Instance' }"],
    ["`- 상세설정(${f.label}): ${oStr} → ${nStr}`", "`- Detail setting(${f.label}): ${oStr} → ${nStr}`"],
    ["`- 데이터포인트 추가: ${added.length}건", "`- DataPoint added: ${added.length} item(s)"],
    ["`- 데이터포인트 삭제: ${removed.length}건", "`- DataPoint removed: ${removed.length} item(s)"],
    ["changes.push(`- [${dp.name}] 수정: ${dpChanges.join(', ')}`)", "changes.push(`- [${dp.name}] changed: ${dpChanges.join(', ')}`)"],
    // modal 메시지
    ["type: 'success', // 정보성 팝업", "type: 'success', // info popup"],
    ["const actionText = mode === 'create' ? '생성' : '수정'", "const actionText = mode === 'create' ? 'create' : 'update'"],
    ["? `아래 변경 사항을 반영하여 디바이스를 수정하시겠습니까?\\n\\n${changes.join('\\n')}`", "? `Apply the following changes to the device?\\n\\n${changes.join('\\n')}`"],
]);

console.log('\nTotal:', total, 'fixes');
