/**
 * ExportGatewayWizard JSX 한글 정밀 처리 v14
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
    // tooltip 한글
    ["tooltip=\"시스템 내에서 유일한 식별자입니다.\"", "tooltip=\"Unique identifier within the system.\""],
    ["tooltip=\"데이터를 수신할 허용 IP입니다. Docker 내부 통신시 127", "tooltip=\"Allowed IP to receive data. Use 127"],
    ["tooltip=\"데이터 수신 범위를 설정합니다. 'Selective'", "tooltip=\"Set data reception scope. 'Selective'"],
    // 섹션 제목
    ["게이트웨이 기본 정보 설정", "Gateway Basic Information"],
    ["사용할 프로파일 선택", "Select Profile to Use"],
    ["데이터 매핑 및 외부 필드명 설정", "Data Mapping & External Field Names"],
    ["사용할 템플릿 선택", "Select Template to Use"],
    ["사용할 타겟 선택 및 수정", "Select & Edit Targets"],
    // placeholder
    ['placeholder="설정할 프로파일을 검색하거나 선택하세요"', 'placeholder="Search or select a profile"'],
    ['placeholder="e.g., 표준 JSON 페이로드"', 'placeholder="e.g., Standard JSON Payload"'],
    ['placeholder="e.g., AWS IoT, MS Azure 등"', 'placeholder="e.g., AWS IoT, MS Azure, etc."'],
    ['placeholder="타겟 선택 (다중 선택 가능)"', 'placeholder="Select targets (multi-select)"'],
    ['placeholder="인증 키 (x-api-key 또는 Token)"', 'placeholder="Auth key (x-api-key or Token)"'],
    ['placeholder="프로파일 명칭 (예: Factory A Standard)"', 'placeholder="Profile name (e.g., Factory A Standard)"'],
    ['placeholder="설명..."', 'placeholder="Description..."'],
    // 텍스트 노드
    ["{p.description || '이 프로파일에 대한 상세 설명이 없습니다.'}", "{p.description || 'No description available for this profile.'}"],
    [">내부 포인트명</th>", ">Internal Point Name</th>"],
    [">매핑 명칭 (TARGET KEY)</th>", ">Mapping Name (TARGET KEY)</th>"],
    // Radio 버튼
    [">기존 프로파일 선택</R", ">Use Existing Profile</R"],
    [">새 프로파일 생성</R", ">Create New Profile</R"],
    [">기존 템플릿 사용</Ra", ">Use Existing Template</Ra"],
    [">새 템플릿 정의</Radio.Bu", ">Create New Template</Radio.Bu"],
    [">기존 타겟 연결</Radio.Button>", ">Connect Existing Target</Radio.Button>"],
    [">새 타겟 생성</Radio.Button>", ">Create New Target</Radio.Button>"],
    [">빌더 (Simple)</Radio.Button>", ">Builder (Simple)</Radio.Button>"],
    [">코드 (Advanced)</Radio.Button>", ">Code (Advanced)</Radio.Button>"],
    // div 텍스트
    [">선택된 포인트", ">Selected Points"],
    [">선택된 타겟 설정 수정", ">Edit Selected Target Settings"],
    [">전송 우선순위:</span>", ">Transmission Priority:</span>"],
    [">템플릿 명칭</div>", ">Template Name</div>"],
    [">시스템 유형</div>", ">System Type</div>"],
    [">페이로드 구성</span>", ">Payload Structure</span>"],
    [">엔지니어 가이드:</strong>", ">Engineer's Guide:</strong>"],
    ["마법사에서는 'Advanced' 모드만 지원합니다.<br />상세 빌더는 [템플릿 관리] 탭을 이용해주세요.", "Only 'Advanced' mode is supported in the wizard.<br />Use [Template Management] tab for detailed builder."],
    ['return \'선택된 템플릿 정보가 없습니다.\'', "return 'No template selected.'"],
    [">적용</", ">Apply</"],
    [">템플릿 JSON 미리보기</", ">Template JSON Preview</"],
]);

console.log('\nTotal:', total);
