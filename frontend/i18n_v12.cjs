/**
 * TemplateManagementTab 남은 한글 정밀 처리 v12
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

total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    ["'참일때값:거짓일때값}}'", "'true_val:false_val}}'"],
    ["[상세 내역]", "[Details]"],
    ["confirmText: '확인',", "confirmText: 'OK',"],
    ["title: '분석 실패'", "title: 'Analysis Failed'"],
    ["message: '유효한 JSON 샘플을 입력해 주세요. (배열 형태도 지원합니다)'", "message: 'Please enter a valid JSON sample. (Array format is also supported)'"],
    ["title: '템플릿 삭제 확인'", "title: 'Delete Template'"],
    ["message: '이 페이로드 템플릿을 삭제하시겠습니까? 이 템플릿을 사용하는 타겟들의 전송이 실패할 수 있습니다.'", "message: 'Delete this payload template? Targets using it may fail to transmit.'"],
    ["title: '삭제 실패'", "title: 'Delete Failed'"],
    ["message: '템플릿을 삭제하는 중 오류가 발생했습니다.'", "message: 'Error occurred while deleting the template.'"],
    [">페이로드 템플릿 설정</h3>", ">Payload Template Settings</h3>"],
    ["><th>시스템 타입</th>", "><th>System Type</th>"],
    ["><th>관리</th>", "><th>Management</th>"],
    [">등록된 템플릿이 없습니다.<", ">No templates registered.<"],
    [">데이터 탐색기</", ">Data Explorer</"],
    ["> 추천 항목</", "> Recommended</"],
    ["title=\"매핑 명칭 (TARGET KEY)\"", "title=\"Mapping Name (TARGET KEY)\""],
    [">매핑 명칭</", ">Mapping Name</"],
    ["title=\"데이터 값 (VALUE)\"", "title=\"Data Value (VALUE)\""],
    [">데이터 값</", ">Data Value</"],
    [">고급 스마트 로직</span>", ">Advanced Smart Logic</span>"],
    [">측정값 (measured_value)</option>", ">Measured Value (measured_value)</option>"],
    ["정상:장애", "Normal:Fault"],
]);

total += fix('/app/src/components/protocols/ProtocolDashboard.tsx', [
    ["changes.push(`기본 폴링 간격:", "changes.push(`Default polling interval:"],
    [">이 프로토콜</", ">This Protocol</"],
    [">프로토콜 설정<", ">Protocol Settings<"],
    [">저장<", ">Save<"],
    [">취소<", ">Cancel<"],
    [">편집<", ">Edit<"],
    ["title: '변경사항 저장'", "title: 'Save Changes'"],
    ["message: '변경사항을 저장하시겠습니까?'", "message: 'Save changes?'"],
    ["message: '프로토콜 정보가 성공적으로 수정되었습니다.'", "message: 'Protocol updated successfully.'"],
    ["title: '저장 완료'", "title: 'Saved'"],
]);

total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    ["> 상태 새로고침</", "> Refresh Status</"],
    [">브로커 정보<", ">Broker Info<"],
    [">브로커 설정<", ">Broker Settings<"],
    [">보안 설정<", ">Security Settings<"],
    [">접속 키<", ">Access Key<"],
    [">재발급<", ">Regenerate<"],
    [">복사<", ">Copy<"],
    [">포트<", ">Port<"],
    [">버전<", ">Version<"],
]);

console.log('\nTotal:', total);
