/**
 * i18n_translate3.cjs - 3차 치환 스크립트
 * 파일별 JSX 텍스트, confirm 팝업, 레이블 배열 한국어 직접 치환
 * 실행: docker exec docker-frontend-1 node /app/i18n_translate3.cjs
 */
const fs = require('fs');
const path = require('path');

const BASE = '/app/src';
const LOCALES = '/app/public/locales';

// =============================================================
// 헬퍼: 번역 파일 업데이트
// =============================================================
function setKey(lang, ns, keyPath, val) {
    const fp = path.join(LOCALES, lang, ns + '.json');
    let obj = {};
    try { obj = JSON.parse(fs.readFileSync(fp, 'utf-8')); } catch (e) { }
    const keys = keyPath.split('.');
    let cur = obj;
    for (let i = 0; i < keys.length - 1; i++) {
        if (!cur[keys[i]]) cur[keys[i]] = {};
        cur = cur[keys[i]];
    }
    cur[keys[keys.length - 1]] = val;
    fs.writeFileSync(fp, JSON.stringify(obj, null, 4), 'utf-8');
}

// =============================================================
// 헬퍼: 파일 내 특정 문자열 치환
// =============================================================
function replaceInFile(fp, pairs) {
    let c = fs.readFileSync(fp, 'utf-8');
    let changed = false;
    for (const [from, to] of pairs) {
        if (c.includes(from)) {
            c = c.split(from).join(to);
            changed = true;
        }
    }
    if (changed) fs.writeFileSync(fp, c, 'utf-8');
    return changed;
}

// =============================================================
// ExportGatewayWizard.tsx
// =============================================================
const GW_WIZ = path.join(BASE, 'pages/export-gateway/wizards/ExportGatewayWizard.tsx');
replaceInFile(GW_WIZ, [
    // Step 설명 배열
    ["{ title: '게이트웨이', description: '기본 정보' }", "{ title: t('gwWizard.step1'), description: t('gwWizard.step1Desc') }"],
    ["{ title: '프로파일', description: '데이터 포인트' }", "{ title: t('gwWizard.step2'), description: t('gwWizard.step2Desc') }"],
    ["{ title: '템플릿', description: '전송 포맷' }", "{ title: t('gwWizard.step3'), description: t('gwWizard.step3Desc') }"],
    ["{ title: '전송 타겟', description: '프로토콜/매핑' }", "{ title: t('gwWizard.step4'), description: t('gwWizard.step4Desc') }"],
    ["{ title: '스케줄', description: '전송 주기' }", "{ title: t('gwWizard.step5'), description: t('gwWizard.step5Desc') }"],
    // confirm 팝업
    ["confirm({ title: '입력 오류', message: '테넌트를 선택해주세요.", "confirm({ title: t('common:inputError'), message: t('gwWizard.selectTenant'),"],
    ["confirm({ title: '입력 오류', message: '게이트웨이 명칭을 입력해주세요.", "confirm({ title: t('common:inputError'), message: t('gwWizard.enterName'),"],
    ["confirm({ title: '입력 오류', message: '프로파일 명칭을 입력해주세요.", "confirm({ title: t('common:inputError'), message: t('gwWizard.enterProfileName'),"],
    ["confirm({ title: '입력 오류', message", "confirm({ title: t('common:inputError'), message"],
    ["await confirm({ title: '설정 완료', message: '게이트웨이 및 전송 설정이 완료되었습니다.'", "await confirm({ title: t('gwWizard.completeTitle'), message: t('gwWizard.completeMsg')"],
    ["title: '설정 실패',", "title: t('gwWizard.failedTitle'),"],
    // JSX 내 텍스트
    ["\n게이트웨이 기본 정보 설정\n", "\n{t('gwWizard.basicInfoTitle')}\n"],
    ["placeholder=\"테넌트 선택\"", "placeholder={t('gwWizard.selectTenantPh')}"],
    ["placeholder=\"사이트 선택\"", "placeholder={t('gwWizard.selectSitePh')}"],
]);
console.log('PROCESSED: ExportGatewayWizard.tsx');

// =============================================================
// TemplateManagementTab.tsx
// =============================================================
const TMPL_TAB = path.join(BASE, 'pages/export-gateway/tabs/TemplateManagementTab.tsx');
replaceInFile(TMPL_TAB, [
    // confirm 팝업
    ["title: '변경사항 유실 주의',", "title: t('common:loseChangesTitle'),"],
    ["message: '수정 중인 내용이 있습니다. 저장하지 않고 닫으시면 모든 데이터가 사라집니다. 정말 닫으시겠습니까?',", "message: t('common:loseChangesMsg'),"],
    ["confirmText: '닫기',", "confirmText: t('common:close'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
    ["title: '수정사항 없음',", "title: t('common:noChanges'),"],
    ["message: '수정된 정보가 없습니다.',", "message: t('common:noChangesMsg'),"],
    ["title: 'JSON 형식 오류',", "title: t('templateTab.jsonError'),"],
    ["message: '유효하지 않은 JSON 형식입니다. 코드를 수정해 주세요.',", "message: t('templateTab.jsonErrorMsg'),"],
    ["title: '저장 완료',", "title: t('common:saveSuccess'),"],
    ["message: '템플릿이 성공적으로 저장되었습니다.',", "message: t('templateTab.saveSuccessMsg'),"],
    ["title: '저장 실패',", "title: t('common:saveFailed'),"],
    ["message: '템플릿을 저장하는 중 오류가 발생했습니다.',", "message: t('templateTab.saveFailedMsg'),"],
]);
console.log('PROCESSED: TemplateManagementTab.tsx');

// =============================================================
// TargetManagementTab.tsx - 남은 JSX 텍스트들
// =============================================================
const TGT_TAB = path.join(BASE, 'pages/export-gateway/tabs/TargetManagementTab.tsx');
replaceInFile(TGT_TAB, [
    // JSX inline 텍스트
    ["전송 타겟 설정</h3>", "{t('targetTab.title')}</h3>"],
    ['"전송 타겟 수정"', "t('targetTab.editTitle')"],
    ['"전송 타겟 추가"', "t('targetTab.createTitle')"],
    [">기본 정보</div>", ">{t('targetTab.basicInfo')}</div>"],
    [">타겟 명칭</label>", ">{t('targetTab.targetName')}</label>"],
    [">소속 테넌트", ">{t('targetTab.tenant')"],
    [">전송 프로토콜</label>", ">{t('targetTab.protocol')}</label>"],
    [">전송 설정 상세</div>", ">{t('targetTab.detailConfig')}</div>"],
    [">고급 설정 (JSON)</div>", ">{t('targetTab.advancedJson')}</div>"],
    [">이 타겟 활성화</label>", ">{t('targetTab.enableTarget')}</label>"],
    [">이름</th>", ">{t('targetTab.colName')}</th>"],
    [">타입</th>", ">{t('targetTab.colType')}</th>"],
    [">상태</th>", ">{t('targetTab.colStatus')}</th>"],
    [">관리</th>", ">{t('targetTab.colAction')}</th>"],
    [">수정</button>", ">{t('common:edit')}</button>"],
    [">삭제</button>", ">{t('common:delete')}</button>"],
    [">취소</button>", ">{t('common:cancel')}</button>"],
    [">저장하기</button>", ">{t('common:save')}</button>"],
    ["테스트 중...", "{t('targetTab.testing')}"],
    ["접속 테스트", "{t('targetTab.testConn')}"],
    // confirm 팝업
    ["title: '변경사항 유실 주의',", "title: t('common:loseChangesTitle'),"],
    ["message: '수정 중인 내용이 있습니다. 저장하지 않고 닫으시면 모든 데이터가 사라집니다. 정말 닫으시겠습니까?',", "message: t('common:loseChangesMsg'),"],
    ["title: '수정사항 없음',", "title: t('common:noChanges'),"],
    ["message: '수정된 정보가 없습니다.',", "message: t('common:noChangesMsg'),"],
    ["title: '저장 완료',", "title: t('common:saveSuccess'),"],
    ["message: '전송 타겟 정보가 성공적으로 저장되었습니다.',", "message: t('targetTab.saveSuccessMsg'),"],
    ["title: '저장 실패',", "title: t('common:saveFailed'),"],
    ["title: '테스트 성공',", "title: t('targetTab.testSuccess'),"],
    ["title: '테스트 실패',", "title: t('targetTab.testFailed'),"],
    ["title: '테스트 오류',", "title: t('targetTab.testError'),"],
    ["title: '입력 부족',", "title: t('common:inputError'),"],
    ["title: '타겟 삭제 확인',", "title: t('targetTab.deleteConfirmTitle'),"],
    ["message: '정말 이 전송 타겟을 삭제하시겠습니까?',", "message: t('targetTab.deleteConfirmMsg'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["confirmText: '닫기',", "confirmText: t('common:close'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
]);
console.log('PROCESSED: TargetManagementTab.tsx');

// =============================================================
// GatewayListTab.tsx
// =============================================================
const GW_TAB = path.join(BASE, 'pages/export-gateway/tabs/GatewayListTab.tsx');
replaceInFile(GW_TAB, [
    [">게이트웨이 목록</h3>", ">{t('gwTab.title')}</h3>"],
    [">게이트웨이 추가</button>", ">{t('gwTab.add')}</button>"],
    [">이름</th>", ">{t('gwTab.colName')}</th>"],
    [">상태</th>", ">{t('gwTab.colStatus')}</th>"],
    [">사이트</th>", ">{t('gwTab.colSite')}</th>"],
    [">관리</th>", ">{t('gwTab.colAction')}</th>"],
    [">수정</button>", ">{t('common:edit')}</button>"],
    [">삭제</button>", ">{t('common:delete')}</button>"],
    ["title: '삭제 확인',", "title: t('common:deleteConfirm'),"],
    ["title: '게이트웨이 삭제 확인',", "title: t('gwTab.deleteConfirmTitle'),"],
    ["message: '정말 이 게이트웨이를 삭제하시겠습니까?", "message: t('gwTab.deleteConfirmMsg'),"],
    ["title: '삭제 성공',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: GatewayListTab.tsx');

// =============================================================
// ScheduleManagementTab.tsx
// =============================================================
const SCH_TAB = path.join(BASE, 'pages/export-gateway/tabs/ScheduleManagementTab.tsx');
replaceInFile(SCH_TAB, [
    [">스케줄 관리</h3>", ">{t('scheduleTab.title')}</h3>"],
    [">스케줄 추가</button>", ">{t('scheduleTab.add')}</button>"],
    [">이름</th>", ">{t('scheduleTab.colName')}</th>"],
    [">주기</th>", ">{t('scheduleTab.colInterval')}</th>"],
    [">활성화</th>", ">{t('scheduleTab.colEnabled')}</th>"],
    [">관리</th>", ">{t('scheduleTab.colAction')}</th>"],
    [">수정</button>", ">{t('common:edit')}</button>"],
    [">삭제</button>", ">{t('common:delete')}</button>"],
    [">저장</button>", ">{t('common:save')}</button>"],
    [">취소</button>", ">{t('common:cancel')}</button>"],
    ["title: '변경사항 유실 주의',", "title: t('common:loseChangesTitle'),"],
    ["title: '스케줄 삭제 확인',", "title: t('scheduleTab.deleteConfirm'),"],
    ["title: '삭제 성공',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["title: '저장 완료',", "title: t('common:saveSuccess'),"],
    ["title: '저장 실패',", "title: t('common:saveFailed'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
    ["confirmText: '닫기',", "confirmText: t('common:close'),"],
]);
console.log('PROCESSED: ScheduleManagementTab.tsx');

// =============================================================
// ProfileManagementTab.tsx
// =============================================================
const PROF_TAB = path.join(BASE, 'pages/export-gateway/tabs/ProfileManagementTab.tsx');
replaceInFile(PROF_TAB, [
    [">프로파일 관리</h3>", ">{t('profileTab.title')}</h3>"],
    [">프로파일 추가</button>", ">{t('profileTab.add')}</button>"],
    ["title: '변경사항 유실 주의',", "title: t('common:loseChangesTitle'),"],
    ["message: '수정 중인 내용이 있습니다.", "message: t('common:loseChangesMsg'),"],
    ["title: '수정사항 없음',", "title: t('common:noChanges'),"],
    ["title: '저장 완료',", "title: t('common:saveSuccess'),"],
    ["title: '저장 실패',", "title: t('common:saveFailed'),"],
    ["title: '프로파일 삭제 확인',", "title: t('profileTab.deleteConfirm'),"],
    ["title: '삭제 성공',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["confirmText: '닫기',", "confirmText: t('common:close'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
]);
console.log('PROCESSED: ProfileManagementTab.tsx');

// =============================================================
// NetworkSettings.tsx
// =============================================================
const NET_SETTINGS = path.join(BASE, 'pages/NetworkSettings.tsx');
replaceInFile(NET_SETTINGS, [
    ["title: '저장 완료',", "title: t('network:saveSuccess'),"],
    ["title: '저장 실패',", "title: t('network:saveFailed'),"],
    ["title: '삭제 확인',", "title: t('common:deleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["title: '연결 테스트',", "title: t('network:testConn'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: NetworkSettings.tsx');

// =============================================================
// HistoricalData.tsx
// =============================================================
const HIST = path.join(BASE, 'pages/HistoricalData.tsx');
replaceInFile(HIST, [
    ["title: '조회 오류',", "title: t('historicalData:queryError'),"],
    ["title: '내보내기 오류',", "title: t('historicalData:exportError'),"],
    ["title: '내보내기 완료',", "title: t('historicalData:exportSuccess'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: HistoricalData.tsx');

// =============================================================
// SystemStatus.tsx
// =============================================================
const SYS_STATUS = path.join(BASE, 'pages/SystemStatus.tsx');
replaceInFile(SYS_STATUS, [
    ["title: '오류',", "title: t('common:error'),"],
    ["title: '성공',", "title: t('common:success'),"],
    ["title: '경고',", "title: t('common:warning'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: SystemStatus.tsx');

// =============================================================
// BackupRestore.tsx
// =============================================================
const BACKUP = path.join(BASE, 'pages/BackupRestore.tsx');
replaceInFile(BACKUP, [
    ["title: '백업 완료',", "title: t('backup:backupSuccess'),"],
    ["title: '백업 실패',", "title: t('backup:backupFailed'),"],
    ["title: '복원 확인',", "title: t('backup:restoreConfirm'),"],
    ["title: '복원 완료',", "title: t('backup:restoreSuccess'),"],
    ["title: '복원 실패',", "title: t('backup:restoreFailed'),"],
    ["title: '삭제 확인',", "title: t('common:deleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '백업 시작',", "confirmText: t('backup:startBackup'),"],
    ["confirmText: '복원 시작',", "confirmText: t('backup:startRestore'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: BackupRestore.tsx');

// =============================================================
// Dashboard.tsx
// =============================================================
const DASHBOARD = path.join(BASE, 'pages/Dashboard.tsx');
replaceInFile(DASHBOARD, [
    ["title: '오류',", "title: t('dashboard:error'),"],
    ["title: '로딩 오류',", "title: t('dashboard:loadError'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
]);
console.log('PROCESSED: Dashboard.tsx');

// =============================================================
// ActiveAlarms.tsx, AlarmHistory.tsx
// =============================================================
const ACT_ALARMS = path.join(BASE, 'pages/ActiveAlarms.tsx');
replaceInFile(ACT_ALARMS, [
    ["title: '알람 확인',", "title: t('alarms:acknowledgeTitle'),"],
    ["title: '알람 해제',", "title: t('alarms:clearTitle'),"],
    ["title: '일괄 확인',", "title: t('alarms:bulkAcknowledgeTitle'),"],
    ["title: '일괄 해제',", "title: t('alarms:bulkClearTitle'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: ActiveAlarms.tsx');

const ALARM_HIST = path.join(BASE, 'pages/AlarmHistory.tsx');
replaceInFile(ALARM_HIST, [
    ["title: '조회 오류',", "title: t('alarms:queryError'),"],
    ["title: '내보내기',", "title: t('alarms:export'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: AlarmHistory.tsx');

// =============================================================
// UserManagement.tsx
// =============================================================
const USER_MGMT = path.join(BASE, 'pages/UserManagement.tsx');
replaceInFile(USER_MGMT, [
    ["title: '사용자 삭제 확인',", "title: t('settings:userDeleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: UserManagement.tsx');

// =============================================================
// ManufacturerManagementPage.tsx
// =============================================================
const MFR_MGMT = path.join(BASE, 'pages/ManufacturerManagementPage.tsx');
replaceInFile(MFR_MGMT, [
    ["title: '제조사 삭제 확인',", "title: t('manufacturers:deleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: ManufacturerManagementPage.tsx');

// =============================================================
// DeviceTemplatesPage.tsx
// =============================================================
const DEV_TMPLS = path.join(BASE, 'pages/DeviceTemplatesPage.tsx');
replaceInFile(DEV_TMPLS, [
    ["title: '템플릿 삭제 확인',", "title: t('deviceTemplates:deleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: DeviceTemplatesPage.tsx');

// =============================================================
// DeviceList/index.tsx
// =============================================================
const DEV_LIST = path.join(BASE, 'pages/DeviceList/index.tsx');
replaceInFile(DEV_LIST, [
    ["title: '장치 삭제 확인',", "title: t('devices:deleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: DeviceList/index.tsx');

// =============================================================
// GroupSidePanel.tsx
// =============================================================
const GROUP_SIDE = path.join(BASE, 'pages/DeviceList/components/GroupSidePanel.tsx');
replaceInFile(GROUP_SIDE, [
    ["title: '그룹 삭제 확인',", "title: t('devices:groupDeleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["title: '그룹 추가',", "title: t('devices:addGroup'),"],
    ["title: '그룹 수정',", "title: t('devices:editGroup'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: GroupSidePanel.tsx');

// =============================================================
// RealTimeMonitor.tsx
// =============================================================
const RT_MON = path.join(BASE, 'pages/RealTimeMonitor.tsx');
replaceInFile(RT_MON, [
    ["title: '오류',", "title: t('monitor:error'),"],
    ["title: '연결 오류',", "title: t('monitor:connError'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
]);
console.log('PROCESSED: RealTimeMonitor.tsx');

// =============================================================
// ControlSchedulePage.tsx
// =============================================================
const CTRL_SCH = path.join(BASE, 'pages/ControlSchedulePage.tsx');
replaceInFile(CTRL_SCH, [
    ["title: '스케줄 삭제 확인',", "title: t('control:scheduleDeleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["title: '저장 완료',", "title: t('common:saveSuccess'),"],
    ["title: '저장 실패',", "title: t('common:saveFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: ControlSchedulePage.tsx');

// =============================================================
// Login/Audit/Permission/ExportHistory pages
// =============================================================
const LOGIN_HIST = path.join(BASE, 'pages/LoginHistory.tsx');
replaceInFile(LOGIN_HIST, [
    ["title: '오류',", "title: t('loginHistory:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
]);
console.log('PROCESSED: LoginHistory.tsx');

const AUDIT = path.join(BASE, 'pages/AuditLogPage.tsx');
replaceInFile(AUDIT, [
    ["title: '오류',", "title: t('auditLog:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
]);
console.log('PROCESSED: AuditLogPage.tsx');

const EXPORT_HIST = path.join(BASE, 'pages/ExportHistory.tsx');
replaceInFile(EXPORT_HIST, [
    ["title: '오류',", "title: t('dataExport:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
]);
console.log('PROCESSED: ExportHistory.tsx');

// =============================================================
// ErrorMonitoring.tsx
// =============================================================
const ERR_MON = path.join(BASE, 'pages/ErrorMonitoring.tsx');
replaceInFile(ERR_MON, [
    ["title: '오류',", "title: t('errorMonitoring:error'),"],
    ["title: '삭제 확인',", "title: t('common:deleteConfirm'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: ErrorMonitoring.tsx');

// =============================================================
// VirtualPoints.tsx
// =============================================================
const VIRT = path.join(BASE, 'pages/VirtualPoints.tsx');
replaceInFile(VIRT, [
    ["title: '삭제 확인',", "title: t('common:deleteConfirm'),"],
    ["title: '저장 완료',", "title: t('common:saveSuccess'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: VirtualPoints.tsx');

// =============================================================
// SystemSettings.tsx
// =============================================================
const SYS_SETTINGS = path.join(BASE, 'pages/SystemSettings.tsx');
replaceInFile(SYS_SETTINGS, [
    ["title: '저장 완료',", "title: t('systemSettings:saveSuccess'),"],
    ["title: '저장 실패',", "title: t('systemSettings:saveFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: SystemSettings.tsx');

// =============================================================
// ExportGatewaySettings.tsx
// =============================================================
const GW_SETTINGS = path.join(BASE, 'pages/ExportGatewaySettings.tsx');
replaceInFile(GW_SETTINGS, [
    ["title: '오류',", "title: t('dataExport:error'),"],
    ["title: '저장 완료',", "title: t('common:saveSuccess'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: ExportGatewaySettings.tsx');

// =============================================================
// ProtocolManagement.tsx
// =============================================================
const PROTO_MGMT = path.join(BASE, 'pages/ProtocolManagement.tsx');
replaceInFile(PROTO_MGMT, [
    ["title: '프로토콜 삭제 확인',", "title: t('protocols:deleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: ProtocolManagement.tsx');

// =============================================================
// AlarmRuleTemplates.tsx, AlarmSettings.tsx
// =============================================================
const ALARM_TMPLS = path.join(BASE, 'pages/AlarmRuleTemplates.tsx');
replaceInFile(ALARM_TMPLS, [
    ["title: '삭제 확인',", "title: t('common:deleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: AlarmRuleTemplates.tsx');

// =============================================================
// SiteManagementPage.tsx, TenantManagementPage.tsx, PermissionManagement.tsx
// =============================================================
const SITE_MGMT = path.join(BASE, 'pages/SiteManagementPage.tsx');
replaceInFile(SITE_MGMT, [
    ["title: '오류',", "title: t('sites:error'),"],
    ["title: '삭제 확인',", "title: t('common:deleteConfirm'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: SiteManagementPage.tsx');

const TENANT_MGMT = path.join(BASE, 'pages/TenantManagementPage.tsx');
replaceInFile(TENANT_MGMT, [
    ["title: '오류',", "title: t('tenants:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
]);
console.log('PROCESSED: TenantManagementPage.tsx');

const PERM_MGMT = path.join(BASE, 'pages/PermissionManagement.tsx');
replaceInFile(PERM_MGMT, [
    ["title: '오류',", "title: t('permissions:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
]);
console.log('PROCESSED: PermissionManagement.tsx');

// =============================================================
// ConfigEditor pages
// =============================================================
const CFG_EDITOR = path.join(BASE, 'pages/ConfigEditor/index.tsx');
replaceInFile(CFG_EDITOR, [
    ["title: '저장 완료',", "title: t('settings:saveSuccess'),"],
    ["title: '저장 실패',", "title: t('settings:saveFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: ConfigEditor/index.tsx');

const ENV_EDITOR = path.join(BASE, 'pages/ConfigEditor/EnvFileEditor.tsx');
replaceInFile(ENV_EDITOR, [
    ["title: '저장 완료',", "title: t('settings:saveSuccess'),"],
    ["title: '저장 실패',", "title: t('settings:saveFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: ConfigEditor/EnvFileEditor.tsx');

// =============================================================
// DeviceDetailModal.tsx - 남은 confirm 팝업들
// =============================================================
const DEV_DETAIL = path.join(BASE, 'components/modals/DeviceDetailModal.tsx');
replaceInFile(DEV_DETAIL, [
    ["title: `디바이스 ${actionText} 확인`,", "title: t('msg.confirmTitle'),"],
    ["title: '변경 내용 없음',", "title: t('msg.noChanges'),"],
    ["message: '수정된 내용이 없습니다.',", "message: t('msg.noChangesMsg'),"],
    ["confirmText: '데이터포인트 설정',", "confirmText: t('msg.setupDataPoints'),"],
    ["title: `디바이스 ${actionText} 확인`", "title: t('msg.confirmTitle')"],
    ["확인", "t('common:confirm')"],
    ["취소", "t('common:cancel')"],
    // label 배열
    ["label: '고객사(Tenant)'", "label: t('field.tenant')"],
    ["label: '설치 사이트'", "label: t('field.site')"],
    ["label: '장치 그룹'", "label: t('field.group')"],
    ["label: '담당 콜렉터(Edge Server)'", "label: t('field.collector')"],
    ["label: '설명'", "label: t('field.description')"],
    ["label: '제조사'", "label: t('basicTab.manufacturer')"],
    ["label: '모델'", "label: t('basicTab.model')"],
    ["label: '시리얼 번호'", "label: t('basicTab.serialNo')"],
    ["label: '타입'", "label: t('field.type')"],
    ["label: '프로토콜 타입'", "label: t('field.protocol')"],
    ["label: '폴링 간격'", "label: t('settingsTab.pollInterval')"],
    ["label: '타임아웃'", "label: t('settingsTab.timeout')"],
    ["label: '재시도 횟수'", "label: t('settingsTab.retries')"],
    ["label: '활성화 상태'", "label: t('status.active')"],
    ["label: '자동 데이터 등록 (Auto-Discovery)'", "label: t('field.autoDiscovery')"],
    ["label: '수집 간격'", "label: t('field.collectInterval')"],
    ["label: '최대 재시도'", "label: t('field.maxRetry')"],
    ["label: '이름'", "label: t('field.name')"],
    ["label: '주소'", "label: t('dpTab.address')"],
    ["label: '단위'", "label: t('dpTab.unit')"],
    ["label: '권한'", "label: t('field.permission')"],
    ["label: '배율'", "label: t('dpTab.scale')"],
    ["label: '오프셋'", "label: t('dpTab.offset')"],
    // 활성/비활성
    ["displayOld = oldVal ? '활성' : '비활성'", "displayOld = oldVal ? t('status.active') : t('status.inactive')"],
    ["displayNew = newVal ? '활성' : '비활성'", "displayNew = newVal ? t('status.active') : t('status.inactive')"],
]);
console.log('PROCESSED: DeviceDetailModal.tsx');

// =============================================================
// SiteDetailModal - 남은 텍스트
// =============================================================
const SITE_DETAIL = path.join(BASE, 'components/modals/SiteModal/SiteDetailModal.tsx');
replaceInFile(SITE_DETAIL, [
    ["title: '저장 완료',", "title: t('msg.saveSuccess'),"],
    ["title: '저장 실패',", "title: t('msg.saveFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["title: '삭제 확인',", "title: t('common:deleteConfirm'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
]);
console.log('PROCESSED: SiteDetailModal.tsx');

// =============================================================
// TenantDetailModal - 남은 텍스트
// =============================================================
const TENANT_DETAIL = path.join(BASE, 'components/modals/TenantModal/TenantDetailModal.tsx');
replaceInFile(TENANT_DETAIL, [
    ["title: '저장 완료',", "title: t('msg.saveSuccess'),"],
    ["title: '저장 실패',", "title: t('msg.saveFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: TenantDetailModal.tsx');

// =============================================================
// DeviceBasicInfoTab - 남은 텍스트
// =============================================================
const DEV_BASIC = path.join(BASE, 'components/modals/DeviceModal/DeviceBasicInfoTab.tsx');
replaceInFile(DEV_BASIC, [
    ["placeholder: '이름을 입력하세요'", "placeholder: t('common:pleaseEnter')"],
    ["placeholder: '설명을 입력하세요'", "placeholder: t('common:pleaseEnter')"],
    ["placeholder=\"이름을 입력하세요\"", "placeholder={t('common:pleaseEnter')}"],
    ["placeholder=\"설명을 입력하세요\"", "placeholder={t('common:pleaseEnter')}"],
    ["placeholder=\"선택하세요\"", "placeholder={t('common:pleaseSelect')}"],
    ["placeholder=\"항목 선택\"", "placeholder={t('common:pleaseSelect')}"],
    // JSX 레이블들
    [">장치 이름</label>", ">{t('basicTab.deviceName')}</label>"],
    [">제조사</label>", ">{t('basicTab.manufacturer')}</label>"],
    [">모델</label>", ">{t('basicTab.model')}</label>"],
    [">시리얼 번호</label>", ">{t('basicTab.serialNo')}</label>"],
    [">설치 위치</label>", ">{t('basicTab.location')}</label>"],
    [">설명</label>", ">{t('basicTab.description')}</label>"],
    [">프로토콜</label>", ">{t('basicTab.protocol')}</label>"],
    [">사이트</label>", ">{t('basicTab.site')}</label>"],
    [">수집기</label>", ">{t('basicTab.collector')}</label>"],
    [">그룹</label>", ">{t('basicTab.group')}</label>"],
    [">활성화</label>", ">{t('basicTab.enabled')}</label>"],
]);
console.log('PROCESSED: DeviceBasicInfoTab.tsx');

// =============================================================
// DeviceSettingsTab, DeviceRtuMonitorTab, DeviceRtuNetworkTab
// =============================================================
const DEV_SETTINGS = path.join(BASE, 'components/modals/DeviceModal/DeviceSettingsTab.tsx');
replaceInFile(DEV_SETTINGS, [
    [">폴링 주기</label>", ">{t('settingsTab.pollInterval')}</label>"],
    [">타임아웃</label>", ">{t('settingsTab.timeout')}</label>"],
    [">재시도 횟수</label>", ">{t('settingsTab.retries')}</label>"],
    [">연결 유지</label>", ">{t('settingsTab.keepAlive')}</label>"],
    [">장치 설정</h3>", ">{t('settingsTab.title')}</h3>"],
    ["title: '저장 완료',", "title: t('common:saveSuccess'),"],
    ["title: '저장 실패',", "title: t('common:saveFailed'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: DeviceSettingsTab.tsx');

const DEV_RTU_MON = path.join(BASE, 'components/modals/DeviceModal/DeviceRtuMonitorTab.tsx');
replaceInFile(DEV_RTU_MON, [
    [">현재값</th>", ">{t('rtuMonitorTab.currentVal')}</th>"],
    [">포인트명</th>", ">{t('rtuMonitorTab.pointName')}</th>"],
    [">단위</th>", ">{t('rtuMonitorTab.unit')}</th>"],
    [">타임스탬프</th>", ">{t('rtuMonitorTab.timestamp')}</th>"],
    [">모니터링 시작</button>", ">{t('rtuMonitorTab.start')}</button>"],
    [">모니터링 중지</button>", ">{t('rtuMonitorTab.stop')}</button>"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
]);
console.log('PROCESSED: DeviceRtuMonitorTab.tsx');

const DEV_RTU_NET = path.join(BASE, 'components/modals/DeviceModal/DeviceRtuNetworkTab.tsx');
replaceInFile(DEV_RTU_NET, [
    [">포트</label>", ">{t('rtuNetTab.port')}</label>"],
    [">보드레이트</label>", ">{t('rtuNetTab.baudrate')}</label>"],
    [">데이터비트</label>", ">{t('rtuNetTab.dataBits')}</label>"],
    [">패리티</label>", ">{t('rtuNetTab.parity')}</label>"],
    [">스톱비트</label>", ">{t('rtuNetTab.stopBits')}</label>"],
    ["title: '저장 완료',", "title: t('common:saveSuccess'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
]);
console.log('PROCESSED: DeviceRtuNetworkTab.tsx');

// =============================================================
// AlarmCreateEditModal - 남은 텍스트
// =============================================================
const ALARM_MODAL = path.join(BASE, 'components/modals/AlarmCreateEditModal.tsx');
replaceInFile(ALARM_MODAL, [
    ["title: '저장 완료',", "title: t('alarmModal.saveSuccess'),"],
    ["title: '저장 실패',", "title: t('alarmModal.saveFailed'),"],
    ["title: '삭제 확인',", "title: t('common:deleteConfirm'),"],
    ["title: '삭제 완료',", "title: t('common:deleteSuccess'),"],
    ["title: '삭제 실패',", "title: t('common:deleteFailed'),"],
    ["title: '오류',", "title: t('common:error'),"],
    ["confirmText: '확인',", "confirmText: t('common:confirm'),"],
    ["confirmText: '삭제',", "confirmText: t('common:delete'),"],
    ["cancelText: '취소',", "cancelText: t('common:cancel'),"],
]);
console.log('PROCESSED: AlarmCreateEditModal.tsx');

// =============================================================
// common: namespace locale 업데이트
// =============================================================
const commonKeys = {
    ko: {
        inputError: '입력 오류',
        loseChangesTitle: '변경사항 유실 주의',
        loseChangesMsg: '수정 중인 내용이 있습니다. 저장하지 않고 닫으시면 모든 데이터가 사라집니다.',
        noChanges: '변경사항 없음',
        noChangesMsg: '수정된 정보가 없습니다.',
        saveSuccess: '저장 완료',
        saveFailed: '저장 실패',
        deleteConfirm: '삭제 확인',
        deleteSuccess: '삭제 완료',
        deleteFailed: '삭제 실패',
        pleaseEnter: '입력하세요',
        pleaseSelect: '선택하세요',
        test: '테스트',
        seconds: '초',
        milliseconds: '밀리초',
    },
    en: {
        inputError: 'Input Error',
        loseChangesTitle: 'Unsaved Changes',
        loseChangesMsg: 'You have unsaved changes. Closing will lose all data.',
        noChanges: 'No Changes',
        noChangesMsg: 'No changes were made.',
        saveSuccess: 'Saved',
        saveFailed: 'Save Failed',
        deleteConfirm: 'Confirm Delete',
        deleteSuccess: 'Deleted',
        deleteFailed: 'Delete Failed',
        pleaseEnter: 'Please enter',
        pleaseSelect: 'Please select',
        test: 'Test',
        seconds: 'seconds',
        milliseconds: 'ms',
    }
};

for (const [lang, keys] of Object.entries(commonKeys)) {
    for (const [k, v] of Object.entries(keys)) {
        setKey(lang, 'common', k, v);
    }
}

// dataExport namespace 업데이트
const gwWizardKeys = {
    ko: {
        'gwWizard.step1': '게이트웨이',
        'gwWizard.step1Desc': '기본 정보',
        'gwWizard.step2': '프로파일',
        'gwWizard.step2Desc': '데이터 포인트',
        'gwWizard.step3': '템플릿',
        'gwWizard.step3Desc': '전송 포맷',
        'gwWizard.step4': '전송 타겟',
        'gwWizard.step4Desc': '프로토콜/매핑',
        'gwWizard.step5': '스케줄',
        'gwWizard.step5Desc': '전송 주기',
        'gwWizard.selectTenant': '테넌트를 선택해주세요.',
        'gwWizard.enterName': '게이트웨이 명칭을 입력해주세요.',
        'gwWizard.enterProfileName': '프로파일 명칭을 입력해주세요.',
        'gwWizard.completeTitle': '설정 완료',
        'gwWizard.completeMsg': '게이트웨이 및 전송 설정이 완료되었습니다.',
        'gwWizard.failedTitle': '설정 실패',
        'gwWizard.basicInfoTitle': '게이트웨이 기본 정보 설정',
        'gwWizard.selectTenantPh': '테넌트 선택',
        'gwWizard.selectSitePh': '사이트 선택',
        'templateTab.jsonError': 'JSON 형식 오류',
        'templateTab.jsonErrorMsg': '유효하지 않은 JSON 형식입니다.',
        'templateTab.saveSuccessMsg': '템플릿이 성공적으로 저장되었습니다.',
        'templateTab.saveFailedMsg': '템플릿을 저장하는 중 오류가 발생했습니다.',
        'targetTab.title': '전송 타겟 설정',
        'targetTab.createTitle': '전송 타겟 추가',
        'targetTab.editTitle': '전송 타겟 수정',
        'targetTab.basicInfo': '기본 정보',
        'targetTab.targetName': '타겟 명칭',
        'targetTab.tenant': '소속 테넌트',
        'targetTab.protocol': '전송 프로토콜',
        'targetTab.detailConfig': '전송 설정 상세',
        'targetTab.advancedJson': '고급 설정 (JSON)',
        'targetTab.enableTarget': '이 타겟 활성화',
        'targetTab.colName': '이름',
        'targetTab.colType': '타입',
        'targetTab.colStatus': '상태',
        'targetTab.colAction': '관리',
        'targetTab.testing': '테스트 중...',
        'targetTab.testConn': '접속 테스트',
        'targetTab.saveSuccessMsg': '전송 타겟 정보가 성공적으로 저장되었습니다.',
        'targetTab.testSuccess': '테스트 성공',
        'targetTab.testFailed': '테스트 실패',
        'targetTab.testError': '테스트 오류',
        'targetTab.deleteConfirmTitle': '타겟 삭제 확인',
        'targetTab.deleteConfirmMsg': '정말 이 전송 타겟을 삭제하시겠습니까?',
        'gwTab.title': '게이트웨이 목록',
        'gwTab.add': '게이트웨이 추가',
        'gwTab.colName': '이름',
        'gwTab.colStatus': '상태',
        'gwTab.colSite': '사이트',
        'gwTab.colAction': '관리',
        'gwTab.deleteConfirmTitle': '게이트웨이 삭제 확인',
        'gwTab.deleteConfirmMsg': '정말 이 게이트웨이를 삭제하시겠습니까?',
        'scheduleTab.title': '스케줄 관리',
        'scheduleTab.add': '스케줄 추가',
        'scheduleTab.colName': '이름',
        'scheduleTab.colInterval': '주기',
        'scheduleTab.colEnabled': '활성화',
        'scheduleTab.colAction': '관리',
        'scheduleTab.deleteConfirm': '스케줄 삭제 확인',
        'profileTab.title': '프로파일 관리',
        'profileTab.add': '프로파일 추가',
        'profileTab.deleteConfirm': '프로파일 삭제 확인',
    },
    en: {
        'gwWizard.step1': 'Gateway',
        'gwWizard.step1Desc': 'Basic Info',
        'gwWizard.step2': 'Profile',
        'gwWizard.step2Desc': 'Data Points',
        'gwWizard.step3': 'Template',
        'gwWizard.step3Desc': 'Payload Format',
        'gwWizard.step4': 'Target',
        'gwWizard.step4Desc': 'Protocol/Mapping',
        'gwWizard.step5': 'Schedule',
        'gwWizard.step5Desc': 'Send Interval',
        'gwWizard.selectTenant': 'Please select a tenant.',
        'gwWizard.enterName': 'Please enter a gateway name.',
        'gwWizard.enterProfileName': 'Please enter a profile name.',
        'gwWizard.completeTitle': 'Setup Complete',
        'gwWizard.completeMsg': 'Gateway and export settings have been configured.',
        'gwWizard.failedTitle': 'Setup Failed',
        'gwWizard.basicInfoTitle': 'Gateway Basic Info',
        'gwWizard.selectTenantPh': 'Select Tenant',
        'gwWizard.selectSitePh': 'Select Site',
        'templateTab.jsonError': 'JSON Format Error',
        'templateTab.jsonErrorMsg': 'Invalid JSON format.',
        'templateTab.saveSuccessMsg': 'Template saved successfully.',
        'templateTab.saveFailedMsg': 'Failed to save template.',
        'targetTab.title': 'Target Settings',
        'targetTab.createTitle': 'Add Target',
        'targetTab.editTitle': 'Edit Target',
        'targetTab.basicInfo': 'Basic Info',
        'targetTab.targetName': 'Target Name',
        'targetTab.tenant': 'Tenant',
        'targetTab.protocol': 'Protocol',
        'targetTab.detailConfig': 'Detail Config',
        'targetTab.advancedJson': 'Advanced (JSON)',
        'targetTab.enableTarget': 'Enable this target',
        'targetTab.colName': 'Name',
        'targetTab.colType': 'Type',
        'targetTab.colStatus': 'Status',
        'targetTab.colAction': 'Actions',
        'targetTab.testing': 'Testing...',
        'targetTab.testConn': 'Test Connection',
        'targetTab.saveSuccessMsg': 'Target saved successfully.',
        'targetTab.testSuccess': 'Test Successful',
        'targetTab.testFailed': 'Test Failed',
        'targetTab.testError': 'Test Error',
        'targetTab.deleteConfirmTitle': 'Confirm Delete Target',
        'targetTab.deleteConfirmMsg': 'Are you sure you want to delete this target?',
        'gwTab.title': 'Gateway List',
        'gwTab.add': 'Add Gateway',
        'gwTab.colName': 'Name',
        'gwTab.colStatus': 'Status',
        'gwTab.colSite': 'Site',
        'gwTab.colAction': 'Actions',
        'gwTab.deleteConfirmTitle': 'Confirm Delete Gateway',
        'gwTab.deleteConfirmMsg': 'Are you sure you want to delete this gateway?',
        'scheduleTab.title': 'Schedule Management',
        'scheduleTab.add': 'Add Schedule',
        'scheduleTab.colName': 'Name',
        'scheduleTab.colInterval': 'Interval',
        'scheduleTab.colEnabled': 'Enabled',
        'scheduleTab.colAction': 'Actions',
        'scheduleTab.deleteConfirm': 'Confirm Delete Schedule',
        'profileTab.title': 'Profile Management',
        'profileTab.add': 'Add Profile',
        'profileTab.deleteConfirm': 'Confirm Delete Profile',
    }
};

for (const [lang, keys] of Object.entries(gwWizardKeys)) {
    for (const [k, v] of Object.entries(keys)) {
        setKey(lang, 'dataExport', k, v);
    }
}

console.log('\n=== 3차 스크립트 완료 ===');
console.log('번역 파일 업데이트 완료');
