/**
 * NetworkSettings.tsx 한글 처리
 * - 목데이터(mock data) 한글: 영어로 교체
 * - 렌더링 라벨: t() 키로 교체
 */
const fs = require('fs');
const fp = '/app/src/pages/NetworkSettings.tsx';
if (!fs.existsSync(fp)) { console.log('NOT FOUND'); process.exit(0); }

let c = fs.readFileSync(fp, 'utf-8');

// Mock data 한글 → 영어 (네트워크 인터페이스 목데이터)
const mockReplacements = [
    ["name: '이더넷 0'", "name: 'Ethernet 0'"],
    ["name: '이더넷 1'", "name: 'Ethernet 1'"],
    ["name: 'WiFi 어댑터'", "name: 'WiFi Adapter'"],
    ["name: 'SSH 접근 허용'", "name: 'Allow SSH Access'"],
    ["description: '내부 네트워크에서 SSH 접근 허용'", "description: 'Allow SSH access from internal network'"],
    ["name: 'HTTP/HTTPS 허용'", "name: 'Allow HTTP/HTTPS'"],
    ["description: 'HTTP 및 HTTPS 트래픽 허용'", "description: 'Allow HTTP and HTTPS traffic'"],
    ["name: '외부 FTP 차단'", "name: 'Block External FTP'"],
    ["description: '외부에서 FTP 접근 차단'", "description: 'Block FTP access from external'"],
    ["name: '본사 VPN'", "name: 'HQ VPN'"],
    ["name: '클라우드 백업'", "name: 'Cloud Backup'"],
];

// JSX 렌더링 한글 → t() (network.json 키 사용)
const renderReplacements = [
    ["<h2 className=\"users-title\">네트워크 인터페이스</h2>", "<h2 className=\"users-title\">{t('interface.title')}</h2>"],
    ["{iface.status === 'up' ? '활성' : '비활성'}", "{iface.status === 'up' ? t('status.active') : t('status.inactive')}"],
    ["{iface.status === 'up' ? '비활성화' : '활성화'}", "{iface.status === 'up' ? t('action.disable') : t('action.enable')}"],
    ["<label className=\"text-xs font-medium text-neutral-500\">IP 주소</label>", "<label className=\"text-xs font-medium text-neutral-500\">{t('field.ipAddress')}</label>"],
    ["<label className=\"text-xs font-medium text-neutral-500\">서브넷 마스크</label>", "<label className=\"text-xs font-medium text-neutral-500\">{t('field.subnetMask')}</label>"],
    ["<label className=\"text-xs font-medium text-neutral-500\">게이트웨이</label>", "<label className=\"text-xs font-medium text-neutral-500\">{t('field.gateway')}</label>"],
    ["<label className=\"text-xs font-medium text-neutral-500\">MAC 주소</label>", "<label className=\"text-xs font-medium text-neutral-500\">{t('field.macAddress')}</label>"],
    ["<label className=\"text-xs font-medium text-neutral-500\">속도</label>", "<label className=\"text-xs font-medium text-neutral-500\">{t('field.speed')}</label>"],
    ["<label className=\"text-xs font-medium text-neutral-500\">수신</label>", "<label className=\"text-xs font-medium text-neutral-500\">{t('field.received')}</label>"],
];

let fixed = 0;
for (const [old, next] of [...mockReplacements, ...renderReplacements]) {
    if (c.includes(old)) { c = c.replace(old, next); fixed++; }
}

if (fixed > 0) {
    fs.writeFileSync(fp, c, 'utf-8');
    console.log('NetworkSettings: ' + fixed + ' fixes');
} else {
    console.log('No changes made');
}
