/**
 * ExportGatewayWizard throw/Error + JSX 레이블 정밀 교체 v9
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

total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    // desc 배열 남은 것들
    ["desc: 'NORMAL, WARNING, CRITICAL 등'", "desc: 'NORMAL, WARNING, CRITICAL, etc.'"],
    ["desc: '표준 시각 포맷'", "desc: 'Standard datetime format'"],
    ["desc: '1970년 기준 밀리초'", "desc: 'Milliseconds since 1970'"],
    [`target_description: "프로파일에 정의된 데이터 설명"`, `target_description: "Data description defined in profile"`],
    [`des: "프로파일에 정의된 데이터 설명"`, `des: "Data description defined in profile"`],
    // throw Error 메시지
    ['"게이트웨이 등록 실패"', '"Gateway registration failed"'],
    ['"게이트웨이 정보 수정 실패"', '"Gateway update failed"'],
    ['"게이트웨이 ID 확보 실패"', '"Failed to obtain gateway ID"'],
    ['"프로파일 생성 실패"', '"Profile creation failed"'],
    ['"프로파일 ID 확보 실패"', '"Failed to obtain profile ID"'],
    ['"프로파일 할당 실패"', '"Profile assignment failed"'],
    ['"프로파일 최종 할당 실패"', '"Final profile assignment failed"'],
    ['"템플릿 생성 실패"', '"Template creation failed"'],
    ['"전송 타겟 생성 실패"', '"Transmission target creation failed"'],
    ['"매핑 저장 실패"', '"Mapping save failed"'],
    ["'저장 중 오류가 발생했습니다. 로그를 확인해주세요.'", "'Error occurred while saving. Please check the logs.'"],
    // JSX 한글 레이블
    [">게이트웨이 기본 정보 설정<", ">Gateway Basic Information Settings<"],
    [">테넌트 선택 (Admin)</div>", ">Tenant Selection (Admin)</div>"],
    [">사이트 선택 (Admin)</div>", ">Site Selection (Admin)</div>"],
    [">전체 사이트 (Shared)</Select.Option>", ">All Sites (Shared)</Select.Option>"],
    ['label="게이트웨이 명칭 (Unique Name)"', 'label="Gateway Name (Unique Name)"'],
    ['label="IP 주소 (접속 허용 주소)"', 'label="IP Address (Allowed Address)"'],
    ['label="알람 구독 모드 (Alarm Subscription)"', 'label="Alarm Subscription Mode"'],
    [">전체 수신 (All)</Radio>", ">All (All)</Radio>"],
    [">이용 디바이스 한정 (Selective)</Radio>", ">Selective Devices (Selective)</Radio>"],
    ['label="설명 (Description)"', 'label="Description"'],
    ['placeholder="관리용 상세 설명"', 'placeholder="Management description"'],
    ["placeholder=\"예: GW_Factory_A_Line1\"", "placeholder=\"e.g., GW_Factory_A_Line1\""],
    [">기존 프로파일 선택</R", ">Select Existing Profile</R"],
    // 추가 JSX
    [">새 프로파일 생성<", ">Create New Profile<"],
    [">프로파일 선택<", ">Select Profile<"],
    [">단계 1<", ">Step 1<"],
    [">단계 2<", ">Step 2<"],
    [">단계 3<", ">Step 3<"],
]);

// TemplateManagementTab 남은 것들
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    ["desc: 'NORMAL, WARNING, CRITICAL 등'", "desc: 'NORMAL, WARNING, CRITICAL, etc.'"],
    ["{tmpl.is_active ? t('common:active') : t('common:inactive')}", "{tmpl.is_active ? 'Active' : 'Inactive'}"],
    // 텍스트 에디터 관련
    [">템플릿 내용<", ">Template Content<"],
    [">미리보기<", ">Preview<"],
    [">변수 패널<", ">Variable Panel<"],
    [">변수 삽입<", ">Insert Variable<"],
    [">복사<", ">{t('common:copy')}<"],
    [">저장됨<", ">Saved<"],
    [">저장 중...<", ">Saving...<"],
    // 탭
    [">목록<", ">List<"],
    [">편집<", ">Edit<"],
]);

// DeviceDetailModal - 남은 탭명 
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    [">기본 정보<", ">Basic Info<"],
    [">데이터 포인트<", ">Data Points<"],
    [">로그<", ">Log<"],
    [">설정<", ">Settings<"],
    [">모니터링<", ">Monitoring<"],
    [">RTU 네트워크<", ">RTU Network<"],
    [">'생성'", ">'create'"],
    [">'수정'", ">'update'"],
]);

// DeviceDataPointsBulkModal 남은 것들
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ["'JSON 형식으로 입력 (배열)', ", "'JSON array format', "],
    [">JSON 형식으로 입력<", ">Enter in JSON format<"],
    [">CSV 형식으로 입력<", ">Enter in CSV format<"],
    [">일괄 추가<", ">Bulk Add<"],
    [">파일 업로드<", ">File Upload<"],
    [">수동 입력<", ">Manual Input<"],
    [">적용<", ">{t('common:apply')}<"],
    [">미리보기<", ">Preview<"],
]);

console.log('\nTotal:', total);
