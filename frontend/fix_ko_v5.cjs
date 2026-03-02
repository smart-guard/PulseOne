/**
 * 키 이름 불일치 수정 + 백업/번역 키 추가
 */
const fs = require('fs');

// ============================================================
// 1. AuditLogPage.tsx: table.datetime → table.time (이미 있는 키)
// ============================================================
(function fixAuditLogKeys() {
    const fp = '/app/src/pages/AuditLogPage.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');
    let fixed = 0;
    if (c.includes("t('table.datetime')")) {
        c = c.replace(/t\('table\.datetime'\)/g, "t('table.time')");
        fixed++;
    }
    if (fixed > 0) {
        fs.writeFileSync(fp, c, 'utf-8');
        console.log('AuditLogPage key fix: datetime→time');
    }
})();

// ============================================================
// 2. HistoricalData.tsx: advancedFilter.xxx → 최상위 키로 수정
// JSON에는: site, device, dataPoint, selectAll, deselectAll, 
//           selectSiteFirst, selectDeviceFirst, chart.*
// ============================================================
(function fixHistoricalKeys() {
    const fp = '/app/src/pages/HistoricalData.tsx';
    if (!fs.existsSync(fp)) return;
    let c = fs.readFileSync(fp, 'utf-8');

    const replacements = [
        ["t('advancedFilter.site')", "t('site')"],
        ["t('advancedFilter.device')", "t('device')"],
        ["t('advancedFilter.dataPoint')", "t('dataPoint')"],
        ["t('advancedFilter.quality')", "t('dataQuality')"],
        ["t('advancedFilter.qualityGood')", "t('quality.good')"],
        ["t('advancedFilter.qualityUncertain')", "t('quality.uncertain')"],
        ["t('advancedFilter.qualityBad')", "t('quality.bad')"],
        ["t('advancedFilter.selectSiteFirst')", "t('selectSiteFirst')"],
        ["t('advancedFilter.selectDeviceFirst')", "t('selectDeviceFirst')"],
        ["t('common:selectAll')", "t('selectAll')"],
        ["t('common:deselectAll')", "t('deselectAll')"],
        ["t('common:deselect')", "t('deselectAll')"],
        ["t('chart.timeSeries')", "t('chart.title')"],
        ["t('chart.placeholder')", "t('chart.placeholder')"],
        ["t('chart.noData')", "t('chart.noData')"],
    ];

    let fixed = 0;
    for (const [old, next] of replacements) {
        if (c.includes(old)) { c = c.replace(new RegExp(old.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'g'), next); fixed++; }
    }
    if (fixed > 0) {
        fs.writeFileSync(fp, c, 'utf-8');
        console.log('HistoricalData key fix:', fixed);
    }
})();

// ============================================================
// 3. locale JSON에 누락 키 추가 (en + ko)
// ============================================================
function addMissingKeys(enPath, koPath, newEnKeys, newKoKeys) {
    // en
    if (fs.existsSync(enPath)) {
        const en = JSON.parse(fs.readFileSync(enPath, 'utf-8'));
        let changed = false;
        for (const [keyPath, val] of Object.entries(newEnKeys)) {
            const parts = keyPath.split('.');
            let obj = en;
            for (let i = 0; i < parts.length - 1; i++) {
                if (!obj[parts[i]]) obj[parts[i]] = {};
                obj = obj[parts[i]];
            }
            if (obj[parts[parts.length - 1]] === undefined) {
                obj[parts[parts.length - 1]] = val;
                changed = true;
            }
        }
        if (changed) {
            fs.writeFileSync(enPath, JSON.stringify(en, null, 4), 'utf-8');
            console.log('Updated en:', enPath.split('/').pop());
        }
    }

    // ko
    if (fs.existsSync(koPath)) {
        const ko = JSON.parse(fs.readFileSync(koPath, 'utf-8'));
        let changed = false;
        for (const [keyPath, val] of Object.entries(newKoKeys)) {
            const parts = keyPath.split('.');
            let obj = ko;
            for (let i = 0; i < parts.length - 1; i++) {
                if (!obj[parts[i]]) obj[parts[i]] = {};
                obj = obj[parts[i]];
            }
            if (obj[parts[parts.length - 1]] === undefined) {
                obj[parts[parts.length - 1]] = val;
                changed = true;
            }
        }
        if (changed) {
            fs.writeFileSync(koPath, JSON.stringify(ko, null, 4), 'utf-8');
            console.log('Updated ko:', koPath.split('/').pop());
        }
    }
}

const localeBase = '/app/public/locales';

// historicalData.json - chart 키 추가
addMissingKeys(
    localeBase + '/en/historicalData.json',
    localeBase + '/ko/historicalData.json',
    {
        'chart.title': 'Time Series Chart',
        'chart.placeholder': 'Time series chart will be displayed here',
        'chart.noData': 'No data to display (check query conditions)',
    },
    {
        'chart.title': '시계열 차트',
        'chart.placeholder': '시계열 차트가 여기에 표시됩니다',
        'chart.noData': '표시할 데이터가 없습니다 (조회 조건을 확인하세요)',
    }
);

// backup.json - 새 키 추가
addMissingKeys(
    localeBase + '/en/backup.json',
    localeBase + '/ko/backup.json',
    {
        'table.nameFile': 'Backup Name / Filename',
        'table.status': 'Status',
        'table.size': 'Size',
        'table.createdInfo': 'Created Info',
        'table.fileValidity': 'File Validity',
        'table.actions': 'Actions',
        'autoBackup.enable': 'Enable Auto Backup',
        'autoBackup.scheduleTime': 'Backup Schedule Time (KST)',
    },
    {
        'table.nameFile': '백업명 / 파일명',
        'table.status': '상태',
        'table.size': '크기',
        'table.createdInfo': '생성 정보',
        'table.fileValidity': '파일 유효성',
        'table.actions': '액션',
        'autoBackup.enable': '자동 백업 활성화',
        'autoBackup.scheduleTime': '백업 수행 시간 (KST)',
    }
);

// common.json - loading 키 확인
addMissingKeys(
    localeBase + '/en/common.json',
    localeBase + '/ko/common.json',
    { 'loading': 'Loading...' },
    { 'loading': '로딩 중...' }
);

console.log('Done!');
