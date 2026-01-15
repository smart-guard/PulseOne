import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/management.css';

interface NetworkInterface {
  id: string;
  name: string;
  type: 'ethernet' | 'wifi' | 'virtual';
  status: 'up' | 'down' | 'connecting';
  ipAddress: string;
  subnetMask: string;
  gateway: string;
  dnsServers: string[];
  macAddress: string;
  speed: string;
  duplex: 'full' | 'half';
  mtu: number;
  dhcp: boolean;
  connected: boolean;
  rxBytes: number;
  txBytes: number;
  lastUpdated: Date;
}

interface FirewallRule {
  id: string;
  name: string;
  enabled: boolean;
  action: 'allow' | 'deny';
  protocol: 'tcp' | 'udp' | 'icmp' | 'all';
  sourceIp: string;
  sourcePort: string;
  destinationIp: string;
  destinationPort: string;
  description: string;
  priority: number;
  createdAt: Date;
}

interface VPNConnection {
  id: string;
  name: string;
  type: 'openvpn' | 'ipsec' | 'wireguard';
  status: 'connected' | 'disconnected' | 'connecting';
  serverAddress: string;
  localIp: string;
  remoteIp: string;
  encryption: string;
  autoConnect: boolean;
  lastConnected?: Date;
  dataTransferred: number;
}

const NetworkSettings: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'interfaces' | 'firewall' | 'vpn' | 'monitoring'>('interfaces');
  const [interfaces, setInterfaces] = useState<NetworkInterface[]>([]);
  const [firewallRules, setFirewallRules] = useState<FirewallRule[]>([]);
  const [vpnConnections, setVPNConnections] = useState<VPNConnection[]>([]);
  const [selectedInterface, setSelectedInterface] = useState<NetworkInterface | null>(null);
  const [showInterfaceModal, setShowInterfaceModal] = useState(false);
  const [showFirewallModal, setShowFirewallModal] = useState(false);

  useEffect(() => {
    initializeMockData();
  }, []);

  const initializeMockData = () => {
    // 네트워크 인터페이스
    const mockInterfaces: NetworkInterface[] = [
      {
        id: 'eth0',
        name: '이더넷 0',
        type: 'ethernet',
        status: 'up',
        ipAddress: '192.168.1.100',
        subnetMask: '255.255.255.0',
        gateway: '192.168.1.1',
        dnsServers: ['8.8.8.8', '8.8.4.4'],
        macAddress: '00:1A:2B:3C:4D:5E',
        speed: '1 Gbps',
        duplex: 'full',
        mtu: 1500,
        dhcp: false,
        connected: true,
        rxBytes: 1024 * 1024 * 150, // 150MB
        txBytes: 1024 * 1024 * 75,  // 75MB
        lastUpdated: new Date()
      },
      {
        id: 'eth1',
        name: '이더넷 1',
        type: 'ethernet',
        status: 'down',
        ipAddress: '',
        subnetMask: '',
        gateway: '',
        dnsServers: [],
        macAddress: '00:1A:2B:3C:4D:5F',
        speed: '1 Gbps',
        duplex: 'full',
        mtu: 1500,
        dhcp: true,
        connected: false,
        rxBytes: 0,
        txBytes: 0,
        lastUpdated: new Date()
      },
      {
        id: 'wlan0',
        name: 'WiFi 어댑터',
        type: 'wifi',
        status: 'up',
        ipAddress: '192.168.0.50',
        subnetMask: '255.255.255.0',
        gateway: '192.168.0.1',
        dnsServers: ['192.168.0.1'],
        macAddress: 'A4:B6:C8:D1:E2:F3',
        speed: '300 Mbps',
        duplex: 'full',
        mtu: 1500,
        dhcp: true,
        connected: true,
        rxBytes: 1024 * 1024 * 45,  // 45MB
        txBytes: 1024 * 1024 * 20,  // 20MB
        lastUpdated: new Date()
      }
    ];

    // 방화벽 규칙
    const mockFirewallRules: FirewallRule[] = [
      {
        id: 'rule_1',
        name: 'SSH 접근 허용',
        enabled: true,
        action: 'allow',
        protocol: 'tcp',
        sourceIp: '192.168.1.0/24',
        sourcePort: 'any',
        destinationIp: 'any',
        destinationPort: '22',
        description: '내부 네트워크에서 SSH 접근 허용',
        priority: 1,
        createdAt: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000)
      },
      {
        id: 'rule_2',
        name: 'HTTP/HTTPS 허용',
        enabled: true,
        action: 'allow',
        protocol: 'tcp',
        sourceIp: 'any',
        sourcePort: 'any',
        destinationIp: 'any',
        destinationPort: '80,443',
        description: 'HTTP 및 HTTPS 트래픽 허용',
        priority: 2,
        createdAt: new Date(Date.now() - 5 * 24 * 60 * 60 * 1000)
      },
      {
        id: 'rule_3',
        name: '외부 FTP 차단',
        enabled: true,
        action: 'deny',
        protocol: 'tcp',
        sourceIp: '!192.168.0.0/16',
        sourcePort: 'any',
        destinationIp: 'any',
        destinationPort: '21',
        description: '외부에서 FTP 접근 차단',
        priority: 3,
        createdAt: new Date(Date.now() - 3 * 24 * 60 * 60 * 1000)
      }
    ];

    // VPN 연결
    const mockVPNConnections: VPNConnection[] = [
      {
        id: 'vpn_1',
        name: '본사 VPN',
        type: 'openvpn',
        status: 'connected',
        serverAddress: 'vpn.company.com:1194',
        localIp: '10.8.0.6',
        remoteIp: '10.8.0.1',
        encryption: 'AES-256-CBC',
        autoConnect: true,
        lastConnected: new Date(Date.now() - 2 * 60 * 60 * 1000),
        dataTransferred: 1024 * 1024 * 25 // 25MB
      },
      {
        id: 'vpn_2',
        name: '클라우드 백업',
        type: 'wireguard',
        status: 'disconnected',
        serverAddress: 'backup.cloud.com:51820',
        localIp: '',
        remoteIp: '',
        encryption: 'ChaCha20Poly1305',
        autoConnect: false,
        dataTransferred: 0
      }
    ];

    setInterfaces(mockInterfaces);
    setFirewallRules(mockFirewallRules);
    setVPNConnections(mockVPNConnections);
  };

  // 인터페이스 활성화/비활성화
  const handleToggleInterface = (interfaceId: string) => {
    setInterfaces(prev => prev.map(iface =>
      iface.id === interfaceId
        ? { ...iface, status: iface.status === 'up' ? 'down' : 'up', connected: iface.status !== 'up' }
        : iface
    ));
  };

  // VPN 연결/해제
  const handleToggleVPN = (vpnId: string) => {
    setVPNConnections(prev => prev.map(vpn =>
      vpn.id === vpnId
        ? {
          ...vpn,
          status: vpn.status === 'connected' ? 'disconnected' : 'connected',
          lastConnected: vpn.status !== 'connected' ? new Date() : vpn.lastConnected
        }
        : vpn
    ));
  };

  // 방화벽 규칙 토글
  const handleToggleFirewallRule = (ruleId: string) => {
    setFirewallRules(prev => prev.map(rule =>
      rule.id === ruleId ? { ...rule, enabled: !rule.enabled } : rule
    ));
  };

  // 바이트를 읽기 쉬운 형태로 변환
  const formatBytes = (bytes: number): string => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  // 상태 아이콘
  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'up':
      case 'connected':
        return 'fa-check-circle text-success-500';
      case 'down':
      case 'disconnected':
        return 'fa-times-circle text-error-500';
      case 'connecting':
        return 'fa-spinner fa-spin text-warning-500';
      default:
        return 'fa-question-circle text-neutral-400';
    }
  };

  return (
    <div className="user-management-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">네트워크 설정</h1>
        <div className="page-actions">
          <button className="btn btn-outline">
            <i className="fas fa-sync"></i>
            새로고침
          </button>
          <button className="btn btn-outline">
            <i className="fas fa-download"></i>
            내보내기
          </button>
        </div>
      </div>

      {/* 탭 네비게이션 */}
      <div className="tab-navigation">
        <button
          className={`tab-button ${activeTab === 'interfaces' ? 'active' : ''}`}
          onClick={() => setActiveTab('interfaces')}
        >
          <i className="fas fa-network-wired"></i>
          네트워크 인터페이스
          <span className="tab-badge">{interfaces.length}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'firewall' ? 'active' : ''}`}
          onClick={() => setActiveTab('firewall')}
        >
          <i className="fas fa-shield-alt"></i>
          방화벽 규칙
          <span className="tab-badge">{firewallRules.length}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'vpn' ? 'active' : ''}`}
          onClick={() => setActiveTab('vpn')}
        >
          <i className="fas fa-lock"></i>
          VPN 연결
          <span className="tab-badge">{vpnConnections.length}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'monitoring' ? 'active' : ''}`}
          onClick={() => setActiveTab('monitoring')}
        >
          <i className="fas fa-chart-line"></i>
          네트워크 모니터링
        </button>
      </div>

      {/* 네트워크 인터페이스 탭 */}
      {activeTab === 'interfaces' && (
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">네트워크 인터페이스</h2>
            <div className="users-actions">
              <button className="btn btn-primary">
                <i className="fas fa-plus"></i>
                가상 인터페이스 추가
              </button>
            </div>
          </div>

          <div className="space-y-4">
            {interfaces.map(iface => (
              <div key={iface.id} className="bg-white rounded-lg shadow-sm border border-neutral-200 p-6">
                <div className="flex items-center justify-between mb-4">
                  <div className="flex items-center gap-4">
                    <div className="w-12 h-12 bg-primary-100 rounded-lg flex items-center justify-center">
                      <i className={`fas ${iface.type === 'wifi' ? 'fa-wifi' : iface.type === 'ethernet' ? 'fa-ethernet' : 'fa-network-wired'} text-primary-600`}></i>
                    </div>
                    <div>
                      <h3 className="text-lg font-semibold text-neutral-800">{iface.name}</h3>
                      <div className="flex items-center gap-2">
                        <i className={`fas ${getStatusIcon(iface.status)}`}></i>
                        <span className="text-sm text-neutral-600">
                          {iface.status === 'up' ? '활성' : '비활성'} • {iface.type.toUpperCase()}
                        </span>
                      </div>
                    </div>
                  </div>
                  <div className="flex items-center gap-2">
                    <button
                      className={`btn btn-sm ${iface.status === 'up' ? 'btn-warning' : 'btn-success'}`}
                      onClick={() => handleToggleInterface(iface.id)}
                    >
                      <i className={`fas ${iface.status === 'up' ? 'fa-stop' : 'fa-play'}`}></i>
                      {iface.status === 'up' ? '비활성화' : '활성화'}
                    </button>
                    <button className="btn btn-sm btn-outline">
                      <i className="fas fa-cog"></i>
                      설정
                    </button>
                  </div>
                </div>

                <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
                  <div>
                    <label className="text-xs font-medium text-neutral-500">IP 주소</label>
                    <div className="text-sm text-neutral-800">{iface.ipAddress || 'N/A'}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">서브넷 마스크</label>
                    <div className="text-sm text-neutral-800">{iface.subnetMask || 'N/A'}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">게이트웨이</label>
                    <div className="text-sm text-neutral-800">{iface.gateway || 'N/A'}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">MAC 주소</label>
                    <div className="text-sm text-neutral-800 font-mono">{iface.macAddress}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">속도</label>
                    <div className="text-sm text-neutral-800">{iface.speed}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">MTU</label>
                    <div className="text-sm text-neutral-800">{iface.mtu}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">수신</label>
                    <div className="text-sm text-neutral-800">{formatBytes(iface.rxBytes)}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">송신</label>
                    <div className="text-sm text-neutral-800">{formatBytes(iface.txBytes)}</div>
                  </div>
                </div>

                {iface.dhcp && (
                  <div className="mt-4 p-3 bg-primary-50 rounded-lg">
                    <div className="flex items-center gap-2">
                      <i className="fas fa-info-circle text-primary-600"></i>
                      <span className="text-sm text-primary-700">DHCP 자동 설정 사용 중</span>
                    </div>
                  </div>
                )}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* 방화벽 규칙 탭 */}
      {activeTab === 'firewall' && (
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">방화벽 규칙</h2>
            <div className="users-actions">
              <button className="btn btn-primary">
                <i className="fas fa-plus"></i>
                새 규칙
              </button>
            </div>
          </div>

          <div className="users-table">
            <div className="table-header">
              <div className="header-cell">규칙명</div>
              <div className="header-cell">상태</div>
              <div className="header-cell">동작</div>
              <div className="header-cell">프로토콜</div>
              <div className="header-cell">소스</div>
              <div className="header-cell">목적지</div>
              <div className="header-cell">우선순위</div>
              <div className="header-cell">액션</div>
            </div>

            {firewallRules.map(rule => (
              <div key={rule.id} className="table-row">
                <div className="table-cell" data-label="규칙명">
                  <div className="user-name">{rule.name}</div>
                  <div className="user-email">{rule.description}</div>
                </div>

                <div className="table-cell" data-label="상태">
                  <label className="status-toggle">
                    <input
                      type="checkbox"
                      checked={rule.enabled}
                      onChange={() => handleToggleFirewallRule(rule.id)}
                    />
                    <span className="toggle-slider"></span>
                  </label>
                </div>

                <div className="table-cell" data-label="동작">
                  <span className={`role-badge ${rule.action === 'allow' ? 'role-engineer' : 'role-admin'}`}>
                    {rule.action === 'allow' ? '허용' : '차단'}
                  </span>
                </div>

                <div className="table-cell" data-label="프로토콜">
                  <div className="text-neutral-700">{rule.protocol.toUpperCase()}</div>
                </div>

                <div className="table-cell" data-label="소스">
                  <div className="text-neutral-700">{rule.sourceIp}:{rule.sourcePort}</div>
                </div>

                <div className="table-cell" data-label="목적지">
                  <div className="text-neutral-700">{rule.destinationIp}:{rule.destinationPort}</div>
                </div>

                <div className="table-cell" data-label="우선순위">
                  <span className="text-primary-600 font-semibold">{rule.priority}</span>
                </div>

                <div className="table-cell action-cell" data-label="액션">
                  <div className="action-buttons">
                    <button className="btn btn-sm btn-outline" title="편집">
                      <i className="fas fa-edit"></i>
                    </button>
                    <button className="btn btn-sm btn-error" title="삭제">
                      <i className="fas fa-trash"></i>
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* VPN 연결 탭 */}
      {activeTab === 'vpn' && (
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">VPN 연결</h2>
            <div className="users-actions">
              <button className="btn btn-primary">
                <i className="fas fa-plus"></i>
                새 VPN 연결
              </button>
            </div>
          </div>

          <div className="space-y-4">
            {vpnConnections.map(vpn => (
              <div key={vpn.id} className="bg-white rounded-lg shadow-sm border border-neutral-200 p-6">
                <div className="flex items-center justify-between mb-4">
                  <div className="flex items-center gap-4">
                    <div className="w-12 h-12 bg-success-100 rounded-lg flex items-center justify-center">
                      <i className="fas fa-lock text-success-600"></i>
                    </div>
                    <div>
                      <h3 className="text-lg font-semibold text-neutral-800">{vpn.name}</h3>
                      <div className="flex items-center gap-2">
                        <i className={`fas ${getStatusIcon(vpn.status)}`}></i>
                        <span className="text-sm text-neutral-600">
                          {vpn.status === 'connected' ? '연결됨' : '연결 안됨'} • {vpn.type.toUpperCase()}
                        </span>
                      </div>
                    </div>
                  </div>
                  <div className="flex items-center gap-2">
                    <button
                      className={`btn btn-sm ${vpn.status === 'connected' ? 'btn-warning' : 'btn-success'}`}
                      onClick={() => handleToggleVPN(vpn.id)}
                    >
                      <i className={`fas ${vpn.status === 'connected' ? 'fa-stop' : 'fa-play'}`}></i>
                      {vpn.status === 'connected' ? '연결 해제' : '연결'}
                    </button>
                    <button className="btn btn-sm btn-outline">
                      <i className="fas fa-cog"></i>
                      설정
                    </button>
                  </div>
                </div>

                <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
                  <div>
                    <label className="text-xs font-medium text-neutral-500">서버 주소</label>
                    <div className="text-sm text-neutral-800">{vpn.serverAddress}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">로컬 IP</label>
                    <div className="text-sm text-neutral-800">{vpn.localIp || 'N/A'}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">원격 IP</label>
                    <div className="text-sm text-neutral-800">{vpn.remoteIp || 'N/A'}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">암호화</label>
                    <div className="text-sm text-neutral-800">{vpn.encryption}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">자동 연결</label>
                    <div className="text-sm text-neutral-800">{vpn.autoConnect ? '활성' : '비활성'}</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">마지막 연결</label>
                    <div className="text-sm text-neutral-800">
                      {vpn.lastConnected?.toLocaleString() || 'N/A'}
                    </div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">데이터 전송량</label>
                    <div className="text-sm text-neutral-800">{formatBytes(vpn.dataTransferred)}</div>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* 네트워크 모니터링 탭 */}
      {activeTab === 'monitoring' && (
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">네트워크 모니터링</h2>
            <div className="users-actions">
              <button className="btn btn-outline">
                <i className="fas fa-download"></i>
                리포트 내보내기
              </button>
            </div>
          </div>

          {/* 네트워크 통계 */}
          <div className="user-stats-panel mb-6">
            <div className="stat-card">
              <div className="stat-icon">
                <i className="fas fa-network-wired text-primary-600"></i>
              </div>
              <div className="stat-value">{interfaces.filter(i => i.status === 'up').length}</div>
              <div className="stat-label">활성 인터페이스</div>
            </div>
            <div className="stat-card">
              <div className="stat-icon">
                <i className="fas fa-download text-success-600"></i>
              </div>
              <div className="stat-value">
                {formatBytes(interfaces.reduce((sum, i) => sum + i.rxBytes, 0))}
              </div>
              <div className="stat-label">총 수신</div>
            </div>
            <div className="stat-card">
              <div className="stat-icon">
                <i className="fas fa-upload text-warning-600"></i>
              </div>
              <div className="stat-value">
                {formatBytes(interfaces.reduce((sum, i) => sum + i.txBytes, 0))}
              </div>
              <div className="stat-label">총 송신</div>
            </div>
            <div className="stat-card">
              <div className="stat-icon">
                <i className="fas fa-lock text-success-600"></i>
              </div>
              <div className="stat-value">{vpnConnections.filter(v => v.status === 'connected').length}</div>
              <div className="stat-label">VPN 연결</div>
            </div>
          </div>

          {/* 실시간 트래픽 */}
          <div className="bg-white rounded-lg shadow-sm border border-neutral-200 p-6">
            <h3 className="text-lg font-semibold text-neutral-800 mb-4">실시간 네트워크 트래픽</h3>
            <div className="h-64 bg-neutral-50 rounded-lg flex items-center justify-center">
              <div className="text-center">
                <i className="fas fa-chart-line text-4xl text-neutral-300 mb-4"></i>
                <p className="text-neutral-500">네트워크 트래픽 차트가 여기에 표시됩니다</p>
                <p className="text-xs text-neutral-400 mt-2">Chart.js 또는 다른 차트 라이브러리로 구현</p>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default NetworkSettings;