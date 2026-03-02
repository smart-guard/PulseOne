const fs = require('fs');
const fp = '/app/src/components/modals/LogDetailModal.tsx';
let c = fs.readFileSync(fp, 'utf-8');

const replacements = [
    ['>기본 정보</div>', ">{t('logDetail.basicInfo')}</div>"],
    ['>상태 / 발생 시간</label>', ">{t('logDetail.statusTime')}</label>"],
    ['>타겟 시스템</label>', ">{t('logDetail.targetSystem')}</label>"],
    ['>프로필 / 게이트웨이</label>', ">{t('logDetail.profileGateway')}</label>"],
    ['>오류 정보</label>', ">{t('logDetail.errorInfo')}</label>"],
    ["log.error_message || '오류 메시지 없음'", "log.error_message || t('logDetail.noErrorMessage')"],
    ['>데이터 상세</div>', ">{t('logDetail.dataDetail')}</div>"],
    ['>전송 데이터 (Source)</label>', ">{t('logDetail.sourceData')}</label>"],
    [">응답 데이터 (Response)</label>", ">{t('logDetail.responseData')}</label>"],
];

let fixed = 0;
for (const [old, next] of replacements) {
    if (c.includes(old)) { c = c.replace(old, next); fixed++; }
}

if (fixed > 0) {
    fs.writeFileSync(fp, c, 'utf-8');
    console.log('LogDetailModal: ' + fixed + ' fixes');
}

// 남은 한글 확인
const remaining = c.split('\n').filter(l =>
    /[\uAC00-\uD7AF]/.test(l) && !l.trim().startsWith('//') && !l.trim().startsWith('*')
    && (l.includes('>') || l.includes('"'))
).length;
console.log('Remaining rendering Korean lines:', remaining);
