/**
 * network.json 누락 키 추가
 */
const fs = require('fs');

const localeBase = '/app/public/locales';

function addKeys(enPath, koPath, enKeys, koKeys) {
    function merge(filePath, newKeys) {
        if (!fs.existsSync(filePath)) return;
        const obj = JSON.parse(fs.readFileSync(filePath, 'utf-8'));
        let changed = false;
        for (const [keyPath, val] of Object.entries(newKeys)) {
            const parts = keyPath.split('.');
            let node = obj;
            for (let i = 0; i < parts.length - 1; i++) {
                if (!node[parts[i]]) node[parts[i]] = {};
                node = node[parts[i]];
            }
            if (node[parts[parts.length - 1]] === undefined) {
                node[parts[parts.length - 1]] = val;
                changed = true;
            }
        }
        if (changed) {
            fs.writeFileSync(filePath, JSON.stringify(obj, null, 4), 'utf-8');
            console.log('Updated: ' + filePath.split('/').slice(-2).join('/'));
        }
    }
    merge(enPath, enKeys);
    merge(koPath, koKeys);
}

// network.json 키
addKeys(
    localeBase + '/en/network.json',
    localeBase + '/ko/network.json',
    {
        'interface.title': 'Network Interfaces',
        'status.active': 'Active',
        'status.inactive': 'Inactive',
        'action.enable': 'Enable',
        'action.disable': 'Disable',
        'field.ipAddress': 'IP Address',
        'field.subnetMask': 'Subnet Mask',
        'field.gateway': 'Gateway',
        'field.macAddress': 'MAC Address',
        'field.speed': 'Speed',
        'field.received': 'Received',
        'field.sent': 'Sent',
    },
    {
        'interface.title': '네트워크 인터페이스',
        'status.active': '활성',
        'status.inactive': '비활성',
        'action.enable': '활성화',
        'action.disable': '비활성화',
        'field.ipAddress': 'IP 주소',
        'field.subnetMask': '서브넷 마스크',
        'field.gateway': '게이트웨이',
        'field.macAddress': 'MAC 주소',
        'field.speed': '속도',
        'field.received': '수신',
        'field.sent': '송신',
    }
);

console.log('Done!');
