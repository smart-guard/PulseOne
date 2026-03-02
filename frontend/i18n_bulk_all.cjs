/**
 * 전체 대규모 i18n 처리 스크립트
 * - 렌더링 컨텍스트(JSX)의 한글을 t() 키로 교체
 * - API 서비스/타입 파일은 건너뜀
 * - 이미 t()로 감싸진 건 건너뜀
 */
const fs = require('fs');
const path = require('path');

const koreanRe = /[\uAC00-\uD7AF]/;

// 공통 한글→영어 직접 치환 (단순 텍스트 노드)
// key: 찾을 JSX 패턴, value: 바꿀 패턴
const PATTERNS = [
    // 저장/취소/확인/삭제 등 공통 버튼
    [/>저장</, '>{t(\'common:save\')}<'],
    [/>취소</, '>{t(\'common:cancel\')}<'],
    [/>확인</, '>{t(\'common:confirm\')}<'],
    [/>삭제</, '>{t(\'common:delete\')}<'],
    [/>닫기</, '>{t(\'common:close\')}<'],
    [/>수정</, '>{t(\'common:edit\')}<'],
    [/>추가</, '>{t(\'common:add\')}<'],
    [/>등록</, '>{t(\'common:register\')}<'],
    [/>초기화</, '>{t(\'common:reset\')}<'],
    [/>새로고침</, '>{t(\'common:refresh\')}<'],
    [/>적용</, '>{t(\'common:apply\')}<'],
    [/>복사</, '>{t(\'common:copy\')}<'],
    [/>이전</, '>{t(\'common:prev\')}<'],
    [/>다음</, '>{t(\'common:next\')}<'],
    [/>완료</, '>{t(\'common:complete\')}<'],
];

// 파일별 직접 치환 목록 (파일경로 → 배열)
const FILE_REPLACEMENTS = {

    // ===== 그룹 2: 디바이스 관련 =====

    '/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx': [
        ['>데이터 포인트<', ">{t('dataPoint.title')}<"],
        ['>포인트 추가<', ">{t('dataPoint.add')}<"],
        ['>포인트 삭제<', ">{t('dataPoint.delete')}<"],
        ['>전체 선택<', ">{t('common:selectAll')}<"],
        ['>선택 해제<', ">{t('common:deselectAll')}<"],
        ['>포인트명<', ">{t('dataPoint.name')}<"],
        ['>데이터 타입<', ">{t('dataPoint.dataType')}<"],
        ['>주소<', ">{t('dataPoint.address')}<"],
        ['>설명<', ">{t('common:description')}<"],
        ['placeholder="포인트명 입력"', "placeholder={t('dataPoint.namePlaceholder')}"],
        ['placeholder="설명 입력"', "placeholder={t('common:descriptionPlaceholder')}"],
        ['>활성화<', ">{t('common:enable')}<"],
        ['>비활성화<', ">{t('common:disable')}<"],
    ],

    '/app/src/components/modals/DeviceModal/DataPointModal.tsx': [
        ['>데이터 포인트 추가<', ">{t('dataPoint.addModal.title')}<"],
        ['>데이터 포인트 편집<', ">{t('dataPoint.editModal.title')}<"],
        ['placeholder="포인트명"', "placeholder={t('dataPoint.name')}"],
        ['placeholder="주소 (예: 1, 0x0001)"', "placeholder={t('dataPoint.addressPlaceholder')}"],
        ['>저장<', ">{t('common:save')}<"],
        ['>취소<', ">{t('common:cancel')}<"],
        ['>포인트명<', ">{t('dataPoint.name')}<"],
        ['>데이터 타입<', ">{t('dataPoint.dataType')}<"],
        ['>주소<', ">{t('dataPoint.address')}<"],
        ['>읽기 전용<', ">{t('dataPoint.readOnly')}<"],
        ['>스케일링<', ">{t('dataPoint.scaling')}<"],
        ['>설명<', ">{t('common:description')}<"],
    ],

    '/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx': [
        ['>일괄 데이터 포인트 추가<', ">{t('dataPoint.bulkAdd.title')}<"],
        ['>CSV 파일로 일괄 추가<', ">{t('dataPoint.bulkAdd.csvTitle')}<"],
        ['>파일 선택<', ">{t('common:selectFile')}<"],
        ['>업로드<', ">{t('common:upload')}<"],
        ['>취소<', ">{t('common:cancel')}<"],
        ['>CSV 형식 안내<', ">{t('dataPoint.bulkAdd.csvGuide')}<"],
        ['>포인트명, 데이터타입, 주소 순서로 입력<', ">{t('dataPoint.bulkAdd.csvFormat')}<"],
        ['>미리보기<', ">{t('common:preview')}<"],
        ['파일을 선택하세요', "t('common:selectFile')"],
    ],

    '/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx': [
        ['>실시간 모니터링<', ">{t('monitor.realtime')}<"],
        ['>값<', ">{t('common:value')}<"],
        ['>품질<', ">{t('common:quality')}<"],
        ['>타임스탬프<', ">{t('common:timestamp')}<"],
        ['>포인트명<', ">{t('dataPoint.name')}<"],
        ['>데이터 없음<', ">{t('common:noData')}<"],
        ['데이터를 불러오는 중...', "t('common:loading')"],
        ['>연결 상태<', ">{t('common:connectionStatus')}<"],
        ['>연결됨<', ">{t('common:connected')}<"],
        ['>연결 안됨<', ">{t('common:disconnected')}<"],
    ],

    '/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx': [
        ['>네트워크 설정<', ">{t('network.settings')}<"],
        ['>IP 주소<', ">{t('network.ipAddress')}<"],
        ['>포트<', ">{t('network.port')}<"],
        ['>프로토콜<', ">{t('common:protocol')}<"],
        ['>저장<', ">{t('common:save')}<"],
        ['>취소<', ">{t('common:cancel')}<"],
        ['placeholder="IP 주소"', "placeholder={t('network.ipAddress')}"],
        ['placeholder="포트"', "placeholder={t('network.port')}"],
        ['>타임아웃<', ">{t('network.timeout')}<"],
        ['>재시도 횟수<', ">{t('network.retryCount')}<"],
    ],

    '/app/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx': [
        ['>네트워크 스캔<', ">{t('device.networkScan')}<"],
        ['>스캔 시작<', ">{t('device.scanStart')}<"],
        ['>스캔 중...<', ">{t('device.scanning')}<"],
        ['>스캔 완료<', ">{t('device.scanDone')}<"],
        ['>닫기<', ">{t('common:close')}<"],
        ['IP 범위', "t('network.ipRange')"],
        ['>장치 추가<', ">{t('device.addDevice')}<"],
    ],

    '/app/src/components/modals/DeviceTemplateWizard.tsx': [
        ['>디바이스 템플릿 생성<', ">{t('deviceTemplate.create')}<"],
        ['>디바이스 템플릿 수정<', ">{t('deviceTemplate.edit')}<"],
        ['>이전<', ">{t('common:prev')}<"],
        ['>다음<', ">{t('common:next')}<"],
        ['>완료<', ">{t('common:complete')}<"],
        ['>취소<', ">{t('common:cancel')}<"],
        ['>기본 정보<', ">{t('deviceTemplate.basicInfo')}<"],
        ['>데이터 포인트<', ">{t('deviceTemplate.dataPoints')}<"],
        ['>템플릿명<', ">{t('deviceTemplate.name')}<"],
        ['>제조사<', ">{t('deviceTemplate.manufacturer')}<"],
        ['>프로토콜<', ">{t('common:protocol')}<"],
        ['placeholder="템플릿명"', "placeholder={t('deviceTemplate.namePlaceholder')}"],
    ],

    // ===== 그룹 3: 데이터/익스포트 관련 =====

    '/app/src/pages/export-gateway/tabs/GatewayListTab.tsx': [
        ['>게이트웨이 목록<', ">{t('gateway.listTitle')}<"],
        ['>게이트웨이 추가<', ">{t('gateway.add')}<"],
        ['>이름<', ">{t('common:name')}<"],
        ['>상태<', ">{t('common:status')}<"],
        ['>유형<', ">{t('common:type')}<"],
        ['>액션<', ">{t('common:actions')}<"],
        ['>활성<', ">{t('common:active')}<"],
        ['>비활성<', ">{t('common:inactive')}<"],
        ['>수정<', ">{t('common:edit')}<"],
        ['>삭제<', ">{t('common:delete')}<"],
        ['데이터 없음', "t('common:noData')"],
    ],

    '/app/src/pages/export-gateway/tabs/ScheduleManagementTab.tsx': [
        ['>스케줄 관리<', ">{t('schedule.title')}<"],
        ['>스케줄 추가<', ">{t('schedule.add')}<"],
        ['>이름<', ">{t('common:name')}<"],
        ['>주기<', ">{t('schedule.interval')}<"],
        ['>상태<', ">{t('common:status')}<"],
        ['>활성화<', ">{t('common:enable')}<"],
        ['>비활성화<', ">{t('common:disable')}<"],
        ['>수정<', ">{t('common:edit')}<"],
        ['>삭제<', ">{t('common:delete')}<"],
    ],

    // ===== 그룹 4: 사이트/테넌트/사용자 =====

    '/app/src/components/modals/SiteModal/SiteDetailModal.tsx': [
        ['>사이트 정보<', ">{t('site.info')}<"],
        ['>이름<', ">{t('common:name')}<"],
        ['>설명<', ">{t('common:description')}<"],
        ['>위치<', ">{t('site.location')}<"],
        ['>저장<', ">{t('common:save')}<"],
        ['>취소<', ">{t('common:cancel')}<"],
        ['>닫기<', ">{t('common:close')}<"],
        ['placeholder="사이트명"', "placeholder={t('site.namePlaceholder')}"],
        ['placeholder="설명"', "placeholder={t('common:descriptionPlaceholder')}"],
        ['>수정<', ">{t('common:edit')}<"],
        ['>삭제<', ">{t('common:delete')}<"],
    ],

    '/app/src/components/modals/TenantModal/TenantDetailModal.tsx': [
        ['>테넌트 정보<', ">{t('tenant.info')}<"],
        ['>이름<', ">{t('common:name')}<"],
        ['>설명<', ">{t('common:description')}<"],
        ['>저장<', ">{t('common:save')}<"],
        ['>취소<', ">{t('common:cancel')}<"],
        ['>닫기<', ">{t('common:close')}<"],
        ['>수정<', ">{t('common:edit')}<"],
        ['>삭제<', ">{t('common:delete')}<"],
        ['placeholder="테넌트명"', "placeholder={t('tenant.namePlaceholder')}"],
    ],

    // ===== 그룹 5: 프로토콜/VirtualPoints =====

    '/app/src/components/protocols/ProtocolDashboard.tsx': [
        ['>프로토콜 대시보드<', ">{t('protocol.dashboard')}<"],
        ['>연결 상태<', ">{t('common:connectionStatus')}<"],
        ['>연결됨<', ">{t('common:connected')}<"],
        ['>연결 안됨<', ">{t('common:disconnected')}<"],
        ['>통계<', ">{t('common:statistics')}<"],
        ['>새로고침<', ">{t('common:refresh')}<"],
        ['>활성<', ">{t('common:active')}<"],
        ['>비활성<', ">{t('common:inactive')}<"],
    ],

    '/app/src/components/protocols/ProtocolInstanceManager.tsx': [
        ['>프로토콜 인스턴스<', ">{t('protocol.instance')}<"],
        ['>추가<', ">{t('common:add')}<"],
        ['>수정<', ">{t('common:edit')}<"],
        ['>삭제<', ">{t('common:delete')}<"],
        ['>이름<', ">{t('common:name')}<"],
        ['>유형<', ">{t('common:type')}<"],
        ['>상태<', ">{t('common:status')}<"],
    ],

    '/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx': [
        ['>MQTT 브로커<', ">{t('protocol.mqttBroker')}<"],
        ['>연결<', ">{t('common:connect')}<"],
        ['>연결 해제<', ">{t('common:disconnect')}<"],
        ['>토픽<', ">{t('protocol.topic')}<"],
        ['>메시지<', ">{t('common:message')}<"],
        ['>게시<', ">{t('protocol.publish')}<"],
        ['>구독<', ">{t('protocol.subscribe')}<"],
    ],

    '/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx': [
        ['>기본 정보<', ">{t('virtualPoint.basicInfo')}<"],
        ['>포인트명<', ">{t('virtualPoint.name')}<"],
        ['>설명<', ">{t('common:description')}<"],
        ['>저장<', ">{t('common:save')}<"],
        ['>취소<', ">{t('common:cancel')}<"],
        ['placeholder="포인트명"', "placeholder={t('virtualPoint.namePlaceholder')}"],
    ],

    '/app/src/components/VirtualPoints/VirtualPointModal/InputVariableSourceSelector.tsx': [
        ['>입력 변수 소스<', ">{t('virtualPoint.inputSource')}<"],
        ['>디바이스<', ">{t('common:device')}<"],
        ['>포인트<', ">{t('common:point')}<"],
        ['>추가<', ">{t('common:add')}<"],
        ['>삭제<', ">{t('common:delete')}<"],
        ['데이터 없음', "t('common:noData')"],
    ],
};

let totalFixed = 0;

for (const [fp, replacements] of Object.entries(FILE_REPLACEMENTS)) {
    if (!fs.existsSync(fp)) {
        console.log('SKIP:', fp.split('/').pop());
        continue;
    }

    let c = fs.readFileSync(fp, 'utf-8');
    const original = c;
    let fileFixed = 0;

    for (const [oldStr, newStr] of replacements) {
        if (typeof oldStr === 'string' && c.includes(oldStr)) {
            c = c.split(oldStr).join(newStr);
            fileFixed++;
        } else if (oldStr instanceof RegExp && oldStr.test(c)) {
            c = c.replace(new RegExp(oldStr.source, 'g'), newStr);
            fileFixed++;
        }
    }

    if (c !== original) {
        fs.writeFileSync(fp, c, 'utf-8');
        totalFixed += fileFixed;
        console.log('FIXED ' + fileFixed + ' in ' + path.basename(fp));
    }
}

console.log('\nTotal:', totalFixed, 'fixes across', Object.keys(FILE_REPLACEMENTS).length, 'files');
