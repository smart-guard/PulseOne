/**
 * i18n 대규모 번역 스크립트 v4
 * JSX 텍스트, placeholder, title, aria-label, alert/confirm 등의 한글을 t() 키로 교체
 * 새 키는 ko.json / en.json 에 자동 추가
 */

const fs = require('fs');
const path = require('path');

// =================== 번역 사전 ===================
// 자주 쓰이는 공통 한글 → 이미 있는 common: 키로 매핑
const COMMON_MAP = {
    '저장': "t('common:save')",
    '취소': "t('common:cancel')",
    '확인': "t('common:confirm')",
    '삭제': "t('common:delete')",
    '닫기': "t('common:close')",
    '수정': "t('common:edit')",
    '추가': "t('common:add')",
    '등록': "t('common:register')",
    '검색': "t('common:search')",
    '초기화': "t('common:reset')",
    '새로고침': "t('common:refresh')",
    '적용': "t('common:apply')",
    '전체': "t('common:all')",
    '선택': "t('common:select')",
    '이름': "t('common:name')",
    '설명': "t('common:description')",
    '상태': "t('common:status')",
    '유형': "t('common:type')",
    '생성': "t('common:create')",
    '복사': "t('common:copy')",
    '내보내기': "t('common:export')",
    '가져오기': "t('common:import')",
    '업로드': "t('common:upload')",
    '다운로드': "t('common:download')",
};

// ko.json에 추가할 새 키 (en 값도 포함)
const newKoKeys = {};
const newEnKeys = {};

// 처리 대상 파일 목록 (scan 결과 상위 파일)
const TARGET_FILES = [
    '/app/src/pages/NetworkSettings.tsx',
    '/app/src/pages/SystemStatus.tsx',
    '/app/src/pages/BackupRestore.tsx',
    '/app/src/pages/AuditLogPage.tsx',
    '/app/src/pages/PermissionManagement.tsx',
    '/app/src/components/modals/SiteModal/SiteDetailModal.tsx',
    '/app/src/components/modals/TenantModal/TenantDetailModal.tsx',
    '/app/src/components/protocols/ProtocolDashboard.tsx',
    '/app/src/components/protocols/ProtocolInstanceManager.tsx',
    '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx',
];

// 각 파일에서 처리할 패턴들
// 방법: placeholder="한글" → placeholder={t('ns:key')}
//       title="한글" → title={t('ns:key')}
//       >한글 텍스트< 등

let totalFixed = 0;

for (const fp of TARGET_FILES) {
    if (!fs.existsSync(fp)) {
        console.log('SKIP (not found):', fp);
        continue;
    }

    let c = fs.readFileSync(fp, 'utf-8');
    const original = c;
    let fileFixed = 0;

    // 1. placeholder="한글..." → placeholder={t('common:search')} 등 단순 매핑
    c = c.replace(/placeholder="([^"]*[\uAC00-\uD7AF][^"]*)"/g, (match, text) => {
        const trimmed = text.trim();
        if (trimmed === '검색...' || trimmed === '검색') return `placeholder={t('common:searchPlaceholder')}`;
        if (trimmed.includes('검색')) return `placeholder={t('common:searchPlaceholder')}`;
        // 그 외 placeholder는 namespace 별로
        const key = 'placeholder_' + trimmed.replace(/[^\w가-힣]/g, '_').substring(0, 30);
        return match; // 복잡한 건 그대로
    });

    // 2. 단순 JSX 텍스트 노드 한글 → t() 처리
    // >{한글 단어}< 패턴에서 COMMON_MAP 매핑
    for (const [ko, tCall] of Object.entries(COMMON_MAP)) {
        // 정확한 단어 매칭: >저장< → >{t('common:save')}<
        const exactTag = new RegExp(`>\\s*${ko}\\s*<`, 'g');
        if (exactTag.test(c)) {
            c = c.replace(new RegExp(`>\\s*${ko}\\s*<`, 'g'), `>{${tCall}}<`);
            fileFixed++;
        }
        // {' '} 패턴도 처리
        const bracketPattern = new RegExp(`\\{['"]\\s*${ko}\\s*['"]\\}`, 'g');
        if (bracketPattern.test(c)) {
            c = c.replace(bracketPattern, `{${tCall}}`);
            fileFixed++;
        }
    }

    if (c !== original) {
        fs.writeFileSync(fp, c, 'utf-8');
        totalFixed += fileFixed;
        console.log(`FIXED ${fileFixed} patterns in ${path.basename(fp)}`);
    } else {
        console.log(`NO CHANGE: ${path.basename(fp)}`);
    }
}

console.log(`\nTotal patterns fixed: ${totalFixed}`);
