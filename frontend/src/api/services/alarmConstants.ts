export const ALARM_CATEGORIES = [
    { value: 'general', label: '일반 (General)', icon: 'fa-cube' },
    { value: 'electrical', label: '전력/전기 (Electrical)', icon: 'fa-bolt' },
    { value: 'hvac', label: '공조/빌딩 (HVAC/BA)', icon: 'fa-building' },
    { value: 'industrial', label: '산업 공정 (Industrial)', icon: 'fa-industry' },
    { value: 'env', label: '환경 센서 (Environmental)', icon: 'fa-leaf' },
    { value: 'security', label: '안전/보안 (Security)', icon: 'fa-shield-alt' },
];

export const ALARM_PRESETS = [
    // --- [그룹 1: 펌프 및 산업 장비] ---
    {
        id: 'pump_stop', title: '펌프 정지', desc: '상태값이 0인 경우 발생',
        icon: '🚨', logic: 'value === 0', severity: 'critical', type: 'digital', category: 'industrial'
    },
    {
        id: 'pump_overload', title: '펌프 과부하', desc: '전류값이 정격의 120% 초과',
        icon: '⚙️', logic: 'value > 120', severity: 'high', type: 'threshold', category: 'industrial'
    },
    {
        id: 'vibration_high', title: '진동 임계 초과', desc: '모터 진동이 10mm/s 초과',
        icon: '💢', logic: 'value > 10', severity: 'high', type: 'threshold', category: 'industrial'
    },
    {
        id: 'dry_run', title: '펌프 공회전', desc: '유량 없이 가동 중인 상태',
        icon: '🚱', logic: 'flow < 1 AND pump === 1', severity: 'critical', type: 'advanced', category: 'industrial'
    },
    {
        id: 'leak_detected', title: '누수 감지', desc: '바닥 누수 센서 트리거',
        icon: '💧', logic: 'value === 1', severity: 'critical', type: 'digital', category: 'industrial'
    },

    // --- [그룹 2: 환경 센서 및 공조] ---
    {
        id: 'high_temp', title: '고온 경고', desc: '80°C 초과 시 알람 발생',
        icon: '🌡️', logic: 'value > 80', severity: 'high', type: 'threshold', category: 'env'
    },
    {
        id: 'humidity_low', title: '저습도 경고', desc: '습도가 30% 미만으로 하락',
        icon: '🌵', logic: 'value < 30', severity: 'medium', type: 'threshold', category: 'env'
    },
    {
        id: 'co2_high', title: 'CO2 농도 높음', desc: '이산화탄소가 1000ppm 초과',
        icon: '🌬️', logic: 'value > 1000', severity: 'medium', type: 'threshold', category: 'hvac'
    },
    {
        id: 'pm25_high', title: '미세먼지 경보', desc: 'PM2.5 농도가 75 이상',
        icon: '🌫️', logic: 'value > 75', severity: 'high', type: 'threshold', category: 'hvac'
    },
    {
        id: 'air_flow_low', title: '풍량 저하', desc: '덕트 내 풍량이 기준치 미달',
        icon: '🌀', logic: 'value < 5', severity: 'medium', type: 'threshold', category: 'hvac'
    },

    // --- [그룹 3: 전력 및 전기] ---
    {
        id: 'over_voltage', title: '과전압 감지', desc: '전압이 240V를 초과',
        icon: '⚡', logic: 'value > 240', severity: 'high', type: 'threshold', category: 'electrical'
    },
    {
        id: 'under_voltage', title: '저전압 감지', desc: '전압이 190V 미만',
        icon: '📉', logic: 'value < 190', severity: 'high', type: 'threshold', category: 'electrical'
    },
    {
        id: 'power_factor_low', title: '역률 저하', desc: '역률이 0.85 미만으로 하강',
        icon: '📊', logic: 'value < 0.85', severity: 'medium', type: 'threshold', category: 'electrical'
    },
    {
        id: 'current_unbalance', title: '전류 불평형', desc: '상간 전류 차이가 10% 초과',
        icon: '⚖️', logic: 'diff > 10', severity: 'high', type: 'advanced', category: 'electrical'
    },
    {
        id: 'blackout', title: '정전 감지', desc: '입력 전원 공급 중단',
        icon: '🔌', logic: 'value === 0', severity: 'critical', type: 'digital', category: 'electrical'
    },
];
