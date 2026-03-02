/**
 * 파일별 한글 직접 수정 스크립트
 * AuditLogPage, PermissionManagement, HistoricalData, BackupRestore, SystemStatus
 */
const fs = require('fs');

// ============================================================
// 1. AuditLogPage.tsx - 테이블 헤더 한글 → t() 키로 교체
// (useTranslation('auditLog') 이미 사용 중)
// ============================================================
(function fixAuditLog() {
    const fp = '/app/src/pages/AuditLogPage.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');

    const replacements = [
        ['<th>일시</th>', "<th>{t('table.datetime')}</th>"],
        ['<th>작업자</th>', "<th>{t('table.operator')}</th>"],
        ['<th>작업</th>', "<th>{t('table.action')}</th>"],
        ['<th>대상</th>', "<th>{t('table.target')}</th>"],
        ['<th>대상 이름</th>', "<th>{t('table.targetName')}</th>"],
        ['<th>상세 내용</th>', "<th>{t('table.detail')}</th>"],
    ];

    let fixed = 0;
    for (const [old, next] of replacements) {
        if (c.includes(old)) { c = c.replace(old, next); fixed++; }
    }
    if (fixed > 0) {
        fs.writeFileSync(fp, c, 'utf-8');
        console.log('AuditLogPage: ' + fixed + ' fixes');
    }
})();

// ============================================================
// 2. PermissionManagement.tsx - 로딩 텍스트  
// ============================================================
(function fixPermission() {
    const fp = '/app/src/pages/PermissionManagement.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');

    const replacements = [
        ['<div className="loading-spinner small"></div> 데이터 로딩 중...', '<div className="loading-spinner small"></div> {t(\'common:loading\')}'],
    ];

    let fixed = 0;
    for (const [old, next] of replacements) {
        if (c.includes(old)) { c = c.replace(old, next); fixed++; }
    }
    if (fixed > 0) {
        // PermissionManagement는 'permissions' namespace만 사용. common 추가 필요
        c = c.replace(
            "const { t } = useTranslation('permissions');",
            "const { t } = useTranslation(['permissions', 'common']);"
        );
        fs.writeFileSync(fp, c, 'utf-8');
        console.log('PermissionManagement: ' + fixed + ' fixes');
    }
})();

// ============================================================
// 3. HistoricalData.tsx - advanced filter 한글 레이블/버튼
// ============================================================
(function fixHistorical() {
    const fp = '/app/src/pages/HistoricalData.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');

    const replacements = [
        ['<label>사이트 (공장)</label>', "<label>{t('advancedFilter.site')}</label>"],
        ['<label>디바이스</label>', "<label>{t('advancedFilter.device')}</label>"],
        ['<label>데이터 포인트</label>', "<label>{t('advancedFilter.dataPoint')}</label>"],
        ['<label>데이터 품질</label>', "<label>{t('advancedFilter.quality')}</label>"],
        ["{ val: 'good', label: '양호' },", "{ val: 'good', label: t('advancedFilter.qualityGood') },"],
        ["{ val: 'uncertain', label: '불확실' },", "{ val: 'uncertain', label: t('advancedFilter.qualityUncertain') },"],
        ["{ val: 'bad', label: '불량' }", "{ val: 'bad', label: t('advancedFilter.qualityBad') }"],
        ['>전체 선택</button>', ">{t('common:selectAll')}</button>"],
        ['>선택 해제</button>', ">{t('common:deselectAll')}</button>"],
        ['>해제</button>', ">{t('common:deselect')}</button>"],
        ['<h3>시계열 차트</h3>', "<h3>{t('chart.timeSeries')}</h3>"],
        ["'공장을 먼저 선택하세요'", "t('advancedFilter.selectSiteFirst')"],
        ["'디바이스를 먼저 선택하세요'", "t('advancedFilter.selectDeviceFirst')"],
        ["<p>시계열 차트가 여기에 표시됩니다</p>", "<p>{t('chart.placeholder')}</p>"],
        ["'표시할 데이터가 없습니다 (조회 조건을 확인하세요)'", "t('chart.noData')"],
    ];

    let fixed = 0;
    for (const [old, next] of replacements) {
        if (c.includes(old)) { c = c.replace(old, next); fixed++; }
    }
    if (fixed > 0) {
        fs.writeFileSync(fp, c, 'utf-8');
        console.log('HistoricalData: ' + fixed + ' fixes');
    }
})();

// ============================================================
// 4. BackupRestore.tsx - 테이블 헤더, confirmText, 버튼
// ============================================================
(function fixBackup() {
    const fp = '/app/src/pages/BackupRestore.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');

    const replacements = [
        ["<th style={{ width: '40%', paddingLeft: '24px' }}>백업명 / 파일명</th>", "<th style={{ width: '40%', paddingLeft: '24px' }}>{t('table.nameFile')}</th>"],
        ["<th style={{ width: '10%' }}>상태</th>", "<th style={{ width: '10%' }}>{t('table.status')}</th>"],
        ["<th style={{ width: '12%' }}>크기</th>", "<th style={{ width: '12%' }}>{t('table.size')}</th>"],
        ["<th style={{ width: '18%' }}>생성 정보</th>", "<th style={{ width: '18%' }}>{t('table.createdInfo')}</th>"],
        ["<th style={{ width: '12%' }}>파일 유효성</th>", "<th style={{ width: '12%' }}>{t('table.fileValidity')}</th>"],
        ["<th style={{ width: '8%', textAlign: 'right', paddingRight: '24px' }}>액션</th>", "<th style={{ width: '8%', textAlign: 'right', paddingRight: '24px' }}>{t('table.actions')}</th>"],
        ['confirmText: \'확인\'', "confirmText: t('common:confirm')"],
        ['title="복원"', 'title={t(\'action.restore\')}'],
        ['title="삭제"', 'title={t(\'common:delete\')}'],
        ["자동 백업 활성화", "{t('autoBackup.enable')}"],
        ["백업 수행 시간 (KST)", "{t('autoBackup.scheduleTime')}"],
    ];

    let fixed = 0;
    for (const [old, next] of replacements) {
        if (c.includes(old)) { c = c.replace(old, next); fixed++; }
    }
    if (fixed > 0) {
        // common namespace 추가 필요
        if (!c.includes("'common'")) {
            c = c.replace(
                /const \{ t \} = useTranslation\('([^']+)'\);/,
                "const { t } = useTranslation(['$1', 'common']);"
            );
        }
        fs.writeFileSync(fp, c, 'utf-8');
        console.log('BackupRestore: ' + fixed + ' fixes');
    }
})();

// ============================================================
// 5. SystemStatus.tsx - alert 메시지 → 콘솔 영어로 교체 
// (alert는 브라우저 기본 팝업, 번역 어려우므로 영어로 교체)
// ============================================================
(function fixSystemStatus() {
    const fp = '/app/src/pages/SystemStatus.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');

    const replacements = [
        ["'데이터를 불러오는데 실패했습니다.'", "'Failed to load data.'"],
        ["'서버와 통신하는 중 오류가 발생했습니다.'", "'Communication error with server.'"],
        ["'산업용 데이터 외부 송출 및 연동 서비스'", "'Industrial data export and integration service'"],
        ["alert(`${service.displayName}는 필수 서비스로 제어할 수 없습니다.`)", "alert(`${service.displayName} is a required service and cannot be controlled.`)"],
        ["alert(`${service.displayName} ${action} 실패: ${msg}`)", "alert(`${service.displayName} ${action} failed: ${msg}`)"],
        ["alert(`${service.displayName} ${action} 중 오류가 발생했습니다.`)", "alert(`${service.displayName} ${action} error occurred.`)"],
        ["alert(`${device.name} ${action} 실패: ${response.message}`)", "alert(`${device.name} ${action} failed: ${response.message}`)"],
        ["alert(`${device.name} ${action} 중 오류가 발생했습니다.`)", "alert(`${device.name} ${action} error occurred.`)"],
        ["alert(`${type === 'digital' ? 'DO' : 'AO'} 제어 명령이 성공적으로 전송되었습니다.`)", "alert(`${type === 'digital' ? 'DO' : 'AO'} control command sent successfully.`)"],
        ["alert(`${type === 'digital' ? 'DO' : 'AO'} 제어 실패: ${response.message}`)", "alert(`${type === 'digital' ? 'DO' : 'AO'} control failed: ${response.message}`)"],
        ["alert(`제어 명령 전송 중 오류가 발생했습니다.`)", "alert('Control command error occurred.')"],
        ["alert('제어 가능한 서비스가 없습니다.')", "alert('No controllable services available.')"],
        ["action === 'stop' ? '정지' : '재시작'", "action === 'stop' ? 'stop' : 'restart'"],
        ["action === 'start' ? '시작' :", "action === 'start' ? 'start' :"],
        ["alert(`${controllableServices.length}개 서비스 ${action} 완료`)", "alert(`${controllableServices.length} service(s) ${action} completed`)"],
        ["alert('일괄 서비스 제어 중 오류가 발생했습니다.')", "alert('Bulk service control error occurred.')"],
        ["alert('서비스를 선택해주세요.')", "alert('Please select a service.')"],
        ["alert('제어 가능한 서비스가 선택되지 않았습니다.')", "alert('No controllable services selected.')"],
        ["alert(`${selectedServiceObjects.length}개 서비스 ${action} 완료`)", "alert(`${selectedServiceObjects.length} service(s) ${action} completed`)"],
        ["'알 수 없는 오류'", "'Unknown error'"],
    ];

    let fixed = 0;
    for (const [old, next] of replacements) {
        if (c.includes(old)) { c = c.replace(old, next); fixed++; }
    }
    if (fixed > 0) {
        fs.writeFileSync(fp, c, 'utf-8');
        console.log('SystemStatus: ' + fixed + ' fixes');
    }
})();

console.log('\nDone!');
