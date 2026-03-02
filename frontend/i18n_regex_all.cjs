/**
 * Regex 기반 한글 JSX 텍스트 노드 자동 치환 스크립트
 * - JSX 내 >한글텍스트< 패턴을 찾아 t() 키로 변환
 * - 이미 t()로 감싸진 건 건너뜀
 * - 공통 키 매핑 테이블 사용
 */
const fs = require('fs');
const path = require('path');

const koreanRe = /[\uAC00-\uD7AF]/;

// 공통 번역 매핑: 한글 텍스트 → t() 키
const TEXT_MAP = {
    '저장': "t('common:save')",
    '취소': "t('common:cancel')",
    '확인': "t('common:confirm')",
    '삭제': "t('common:delete')",
    '닫기': "t('common:close')",
    '수정': "t('common:edit')",
    '추가': "t('common:add')",
    '등록': "t('common:register')",
    '초기화': "t('common:reset')",
    '새로고침': "t('common:refresh')",
    '적용': "t('common:apply')",
    '복사': "t('common:copy')",
    '이전': "t('common:prev')",
    '다음': "t('common:next')",
    '완료': "t('common:complete')",
    '검색': "t('common:search')",
    '전체 선택': "t('common:selectAll')",
    '선택 해제': "t('common:deselectAll')",
    '전체': "t('common:all')",
    '활성': "t('common:active')",
    '비활성': "t('common:inactive')",
    '활성화': "t('common:enable')",
    '비활성화': "t('common:disable')",
    '이름': "t('common:name')",
    '설명': "t('common:description')",
    '상태': "t('common:status')",
    '유형': "t('common:type')",
    '값': "t('common:value')",
    '주소': "t('common:address')",
    '포트': "t('common:port')",
    '액션': "t('common:actions')",
    '연결': "t('common:connect')",
    '연결 해제': "t('common:disconnect')",
    '연결됨': "t('common:connected')",
    '연결 안됨': "t('common:disconnected')",
    '로딩 중...': "t('common:loading')",
    '로딩중...': "t('common:loading')",
    '데이터 없음': "t('common:noData')",
    '데이터가 없습니다': "t('common:noData')",
    '미리보기': "t('common:preview')",
    '업로드': "t('common:upload')",
    '다운로드': "t('common:download')",
    '내보내기': "t('common:export')",
    '가져오기': "t('common:import')",
    '생성': "t('common:create')",
    '알 수 없음': "t('common:unknown')",
    '없음': "t('common:none')",
};

// 처리할 파일 목록 (스캔 결과 상위)
const TARGET_DIRS = [
    '/app/src/components/modals/DeviceModal',
    '/app/src/components/modals/SiteModal',
    '/app/src/components/modals/TenantModal',
    '/app/src/components/modals/UserModal',
    '/app/src/components/modals/RoleModal',
    '/app/src/components/modals/ProtocolModal',
    '/app/src/components/modals/ManufacturerModal',
    '/app/src/components/VirtualPoints/VirtualPointModal',
    '/app/src/components/protocols',
    '/app/src/pages/export-gateway/tabs',
];

function getFiles(dir) {
    if (!fs.existsSync(dir)) return [];
    const items = fs.readdirSync(dir);
    const result = [];
    for (const item of items) {
        const fullPath = path.join(dir, item);
        const stat = fs.statSync(fullPath);
        if (stat.isDirectory()) result.push(...getFiles(fullPath));
        else if (item.endsWith('.tsx') || item.endsWith('.ts')) result.push(fullPath);
    }
    return result;
}

let totalFixed = 0;
let totalFiles = 0;

const allFiles = [];
for (const dir of TARGET_DIRS) allFiles.push(...getFiles(dir));

for (const fp of allFiles) {
    let c = fs.readFileSync(fp, 'utf-8');
    const original = c;
    let fileFixed = 0;

    // 패턴 1: >한글텍스트< (trim 포함)
    // 예: >저장< → >{t('common:save')}<
    for (const [ko, tCall] of Object.entries(TEXT_MAP)) {
        // 이미 t()로 처리된 건 건너뜀
        const escapedKo = ko.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

        // >텍스트< (공백 허용)
        const tagRe = new RegExp(`>\\s*${escapedKo}\\s*<`, 'g');
        if (tagRe.test(c)) {
            c = c.replace(new RegExp(`>\\s*${escapedKo}\\s*<`, 'g'), `>{${tCall}}<`);
            fileFixed++;
        }
    }

    // 패턴 2: placeholder="한글" 
    c = c.replace(/placeholder="검색\.\.\."/g, "placeholder={t('common:searchPlaceholder')}");
    c = c.replace(/placeholder="검색"/g, "placeholder={t('common:search')}");

    if (c !== original) {
        fs.writeFileSync(fp, c, 'utf-8');
        totalFixed += fileFixed;
        totalFiles++;
        console.log(`FIXED ${fileFixed} in ${path.relative('/app/src', fp)}`);
    }
}

console.log(`\nTotal: ${totalFixed} fixes in ${totalFiles} files`);
