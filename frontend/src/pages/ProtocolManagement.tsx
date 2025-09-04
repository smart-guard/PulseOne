import React, { useState, useEffect } from 'react';

// 프로토콜 데이터 타입
interface Protocol {
  id: number;
  protocol_type: string;
  display_name: string;
  description: string;
  category: string;
  default_port?: number;
  is_enabled: boolean;
  is_deprecated: boolean;
  device_count: number;
  enabled_count: number;
  connected_count: number;
}

interface ProtocolStats {
  total_protocols: number;
  enabled_protocols: number;
  deprecated_protocols: number;
  categories: Array<{
    category: string;
    count: number;
    percentage: number;
  }>;
}

const ProtocolManagement: React.FC = () => {
  const [protocols, setProtocols] = useState<Protocol[]>([]);
  const [stats, setStats] = useState<ProtocolStats | null>(null);
  const [loading, setLoading] = useState(true);
  const [viewMode, setViewMode] = useState<'cards' | 'table'>('cards');
  const [selectedCategory, setSelectedCategory] = useState<string>('all');
  const [searchTerm, setSearchTerm] = useState('');

  // 샘플 데이터
  const sampleProtocols: Protocol[] = [
    {
      id: 1,
      protocol_type: 'MODBUS_TCP',
      display_name: 'Modbus TCP',
      description: 'Modbus TCP/IP Protocol for Ethernet-based industrial networks',
      category: 'industrial',
      default_port: 502,
      is_enabled: true,
      is_deprecated: false,
      device_count: 12,
      enabled_count: 10,
      connected_count: 8
    },
    {
      id: 2,
      protocol_type: 'MODBUS_RTU',
      display_name: 'Modbus RTU',
      description: 'Modbus RTU Serial Protocol for RS-485/RS-232 networks',
      category: 'industrial',
      default_port: null,
      is_enabled: true,
      is_deprecated: false,
      device_count: 8,
      enabled_count: 6,
      connected_count: 5
    },
    {
      id: 3,
      protocol_type: 'MQTT',
      display_name: 'MQTT',
      description: 'Message Queuing Telemetry Transport - Lightweight messaging protocol for IoT',
      category: 'iot',
      default_port: 1883,
      is_enabled: true,
      is_deprecated: false,
      device_count: 15,
      enabled_count: 12,
      connected_count: 11
    },
    {
      id: 4,
      protocol_type: 'BACNET',
      display_name: 'BACnet/IP',
      description: 'Building Automation and Control Networks over IP',
      category: 'building_automation',
      default_port: 47808,
      is_enabled: true,
      is_deprecated: false,
      device_count: 6,
      enabled_count: 5,
      connected_count: 4
    },
    {
      id: 5,
      protocol_type: 'HTTP_REST',
      display_name: 'HTTP REST',
      description: 'RESTful HTTP API for web-based device communication',
      category: 'web',
      default_port: 80,
      is_enabled: false,
      is_deprecated: true,
      device_count: 2,
      enabled_count: 0,
      connected_count: 0
    }
  ];

  const sampleStats: ProtocolStats = {
    total_protocols: 11,
    enabled_protocols: 9,
    deprecated_protocols: 2,
    categories: [
      { category: 'industrial', count: 5, percentage: 45.5 },
      { category: 'iot', count: 3, percentage: 27.3 },
      { category: 'building_automation', count: 2, percentage: 18.2 },
      { category: 'web', count: 1, percentage: 9.1 }
    ]
  };

  useEffect(() => {
    // 데이터 로딩 시뮬레이션
    setTimeout(() => {
      setProtocols(sampleProtocols);
      setStats(sampleStats);
      setLoading(false);
    }, 500);
  }, []);

  const getCategoryIcon = (category: string) => {
    const icons = {
      'industrial': '🏭',
      'iot': '🌐',
      'building_automation': '🏢',
      'network': '🔗',
      'web': '🌍'
    };
    return icons[category] || '📡';
  };

  const getCategoryColor = (category: string) => {
    const colors = {
      'industrial': '#3b82f6',
      'iot': '#10b981',
      'building_automation': '#f59e0b',
      'network': '#8b5cf6',
      'web': '#ef4444'
    };
    return colors[category] || '#6b7280';
  };

  const filteredProtocols = protocols.filter(protocol => {
    const matchesCategory = selectedCategory === 'all' || protocol.category === selectedCategory;
    const matchesSearch = protocol.display_name.toLowerCase().includes(searchTerm.toLowerCase()) ||
                         protocol.description.toLowerCase().includes(searchTerm.toLowerCase());
    return matchesCategory && matchesSearch;
  });

  if (loading) {
    return (
      <div style={{ 
        display: 'flex', 
        justifyContent: 'center', 
        alignItems: 'center', 
        height: '400px' 
      }}>
        <div>로딩 중...</div>
      </div>
    );
  }

  return (
    <div style={{ 
      width: '100%', 
      maxWidth: 'none', 
      padding: '24px',
      backgroundColor: '#f8fafc' 
    }}>
      {/* 헤더 */}
      <div style={{ 
        display: 'flex', 
        justifyContent: 'space-between', 
        alignItems: 'center', 
        marginBottom: '32px' 
      }}>
        <div>
          <h1 style={{ 
            fontSize: '28px', 
            fontWeight: '700', 
            color: '#1e293b', 
            margin: 0, 
            marginBottom: '8px' 
          }}>
            프로토콜 관리
          </h1>
          <p style={{ 
            color: '#64748b', 
            margin: 0,
            fontSize: '16px'
          }}>
            통신 프로토콜의 조회, 편집, 등록을 관리합니다
          </p>
        </div>
        <button style={{
          backgroundColor: '#3b82f6',
          color: 'white',
          border: 'none',
          borderRadius: '8px',
          padding: '12px 24px',
          fontSize: '14px',
          fontWeight: '500',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          ➕ 새 프로토콜 등록
        </button>
      </div>

      {/* 통계 카드들 - 4열 가로 배치 */}
      <div style={{ 
        display: 'grid', 
        gridTemplateColumns: 'repeat(4, 1fr)', 
        gap: '24px', 
        marginBottom: '32px' 
      }}>
        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0'
        }}>
          <div style={{ 
            display: 'flex', 
            alignItems: 'center', 
            justifyContent: 'space-between',
            marginBottom: '12px'
          }}>
            <h3 style={{ 
              margin: 0, 
              color: '#64748b', 
              fontSize: '14px',
              fontWeight: '500'
            }}>전체 프로토콜</h3>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '8px',
              backgroundColor: '#dbeafe',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>📡</div>
          </div>
          <div style={{ 
            fontSize: '32px', 
            fontWeight: '700', 
            color: '#1e293b' 
          }}>
            {stats?.total_protocols || 0}
          </div>
        </div>

        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0'
        }}>
          <div style={{ 
            display: 'flex', 
            alignItems: 'center', 
            justifyContent: 'space-between',
            marginBottom: '12px'
          }}>
            <h3 style={{ 
              margin: 0, 
              color: '#64748b', 
              fontSize: '14px',
              fontWeight: '500'
            }}>활성화됨</h3>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '8px',
              backgroundColor: '#dcfce7',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>✅</div>
          </div>
          <div style={{ 
            fontSize: '32px', 
            fontWeight: '700', 
            color: '#16a34a' 
          }}>
            {stats?.enabled_protocols || 0}
          </div>
        </div>

        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0'
        }}>
          <div style={{ 
            display: 'flex', 
            alignItems: 'center', 
            justifyContent: 'space-between',
            marginBottom: '12px'
          }}>
            <h3 style={{ 
              margin: 0, 
              color: '#64748b', 
              fontSize: '14px',
              fontWeight: '500'
            }}>지원 중단</h3>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '8px',
              backgroundColor: '#fef3c7',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>⚠️</div>
          </div>
          <div style={{ 
            fontSize: '32px', 
            fontWeight: '700', 
            color: '#d97706' 
          }}>
            {stats?.deprecated_protocols || 0}
          </div>
        </div>

        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0'
        }}>
          <div style={{ 
            display: 'flex', 
            alignItems: 'center', 
            justifyContent: 'space-between',
            marginBottom: '12px'
          }}>
            <h3 style={{ 
              margin: 0, 
              color: '#64748b', 
              fontSize: '14px',
              fontWeight: '500'
            }}>사용중인 디바이스</h3>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '8px',
              backgroundColor: '#e0f2fe',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>🌐</div>
          </div>
          <div style={{ 
            fontSize: '32px', 
            fontWeight: '700', 
            color: '#0284c7' 
          }}>
            {protocols.reduce((sum, p) => sum + p.device_count, 0)}
          </div>
        </div>
      </div>

      {/* 필터 및 검색 */}
      <div style={{
        backgroundColor: 'white',
        borderRadius: '12px',
        padding: '20px',
        marginBottom: '24px',
        boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
        border: '1px solid #e2e8f0',
        display: 'flex',
        alignItems: 'center',
        gap: '16px',
        flexWrap: 'wrap'
      }}>
        <div>
          <label style={{ 
            display: 'block', 
            marginBottom: '4px', 
            fontSize: '14px',
            fontWeight: '500',
            color: '#374151' 
          }}>
            검색
          </label>
          <input
            type="text"
            placeholder="프로토콜명, 타입, 설명 검색..."
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            style={{
              padding: '8px 12px',
              border: '1px solid #d1d5db',
              borderRadius: '6px',
              fontSize: '14px',
              width: '250px'
            }}
          />
        </div>
        
        <div>
          <label style={{ 
            display: 'block', 
            marginBottom: '4px', 
            fontSize: '14px',
            fontWeight: '500',
            color: '#374151' 
          }}>
            카테고리
          </label>
          <select
            value={selectedCategory}
            onChange={(e) => setSelectedCategory(e.target.value)}
            style={{
              padding: '8px 12px',
              border: '1px solid #d1d5db',
              borderRadius: '6px',
              fontSize: '14px',
              width: '180px'
            }}
          >
            <option value="all">전체 카테고리</option>
            <option value="industrial">산업용</option>
            <option value="iot">IoT</option>
            <option value="building_automation">빌딩 자동화</option>
            <option value="web">웹</option>
            <option value="network">네트워크</option>
          </select>
        </div>

        <div>
          <label style={{ 
            display: 'block', 
            marginBottom: '4px', 
            fontSize: '14px',
            fontWeight: '500',
            color: '#374151' 
          }}>
            상태
          </label>
          <select style={{
            padding: '8px 12px',
            border: '1px solid #d1d5db',
            borderRadius: '6px',
            fontSize: '14px',
            width: '120px'
          }}>
            <option value="all">전체 상태</option>
            <option value="enabled">활성</option>
            <option value="disabled">비활성</option>
          </select>
        </div>

        <div style={{ marginLeft: 'auto', display: 'flex', gap: '8px' }}>
          <button
            onClick={() => setViewMode('cards')}
            style={{
              padding: '8px 12px',
              border: viewMode === 'cards' ? '1px solid #3b82f6' : '1px solid #d1d5db',
              backgroundColor: viewMode === 'cards' ? '#dbeafe' : 'white',
              color: viewMode === 'cards' ? '#3b82f6' : '#6b7280',
              borderRadius: '6px',
              fontSize: '14px',
              cursor: 'pointer'
            }}
          >
            📱 카드형
          </button>
          <button
            onClick={() => setViewMode('table')}
            style={{
              padding: '8px 12px',
              border: viewMode === 'table' ? '1px solid #3b82f6' : '1px solid #d1d5db',
              backgroundColor: viewMode === 'table' ? '#dbeafe' : 'white',
              color: viewMode === 'table' ? '#3b82f6' : '#6b7280',
              borderRadius: '6px',
              fontSize: '14px',
              cursor: 'pointer'
            }}
          >
            📋 테이블형
          </button>
        </div>

        <button style={{
          padding: '8px 16px',
          backgroundColor: '#f3f4f6',
          border: '1px solid #d1d5db',
          borderRadius: '6px',
          fontSize: '14px',
          cursor: 'pointer'
        }}>
          🔄 필터 초기화
        </button>
      </div>

      {/* 프로토콜 목록 - 카드형 */}
      {viewMode === 'cards' ? (
        <div style={{ 
          display: 'grid', 
          gridTemplateColumns: 'repeat(auto-fill, minmax(400px, 1fr))', 
          gap: '20px' 
        }}>
          {filteredProtocols.map(protocol => (
            <div 
              key={protocol.id}
              style={{
                backgroundColor: 'white',
                borderRadius: '12px',
                padding: '24px',
                boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
                border: '1px solid #e2e8f0',
                transition: 'all 0.2s ease',
                cursor: 'pointer'
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.transform = 'translateY(-2px)';
                e.currentTarget.style.boxShadow = '0 4px 6px -1px rgb(0 0 0 / 0.1)';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.transform = 'translateY(0)';
                e.currentTarget.style.boxShadow = '0 1px 3px 0 rgb(0 0 0 / 0.1)';
              }}
            >
              {/* 카드 헤더 */}
              <div style={{ 
                display: 'flex', 
                justifyContent: 'space-between', 
                alignItems: 'flex-start',
                marginBottom: '16px'
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                  <div style={{
                    width: '48px',
                    height: '48px',
                    borderRadius: '8px',
                    backgroundColor: `${getCategoryColor(protocol.category)}20`,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '20px'
                  }}>
                    {getCategoryIcon(protocol.category)}
                  </div>
                  <div>
                    <h3 style={{ 
                      margin: 0, 
                      fontSize: '18px', 
                      fontWeight: '600',
                      color: '#1e293b'
                    }}>
                      {protocol.display_name}
                    </h3>
                    <div style={{ 
                      fontSize: '12px',
                      color: '#64748b',
                      marginTop: '2px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px'
                    }}>
                      <span>{protocol.protocol_type}</span>
                      <span>•</span>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: getCategoryColor(protocol.category),
                        color: 'white',
                        borderRadius: '4px',
                        fontSize: '10px',
                        fontWeight: '500'
                      }}>
                        {protocol.category}
                      </span>
                    </div>
                  </div>
                </div>
                
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  {protocol.is_enabled ? (
                    <span style={{
                      width: '8px',
                      height: '8px',
                      backgroundColor: '#16a34a',
                      borderRadius: '50%',
                      display: 'inline-block'
                    }}></span>
                  ) : (
                    <span style={{
                      width: '8px',
                      height: '8px',
                      backgroundColor: '#ef4444',
                      borderRadius: '50%',
                      display: 'inline-block'
                    }}></span>
                  )}
                  {protocol.is_deprecated && (
                    <span style={{
                      fontSize: '12px',
                      backgroundColor: '#fef3c7',
                      color: '#92400e',
                      padding: '2px 6px',
                      borderRadius: '4px',
                      fontWeight: '500'
                    }}>
                      지원중단
                    </span>
                  )}
                </div>
              </div>

              {/* 설명 */}
              <p style={{ 
                margin: 0, 
                marginBottom: '16px',
                fontSize: '14px', 
                color: '#64748b',
                lineHeight: '1.5'
              }}>
                {protocol.description}
              </p>

              {/* 포트 정보 */}
              {protocol.default_port && (
                <div style={{ 
                  marginBottom: '16px',
                  fontSize: '14px',
                  color: '#374151'
                }}>
                  <span style={{ fontWeight: '500' }}>기본 포트:</span> {protocol.default_port}
                </div>
              )}

              {/* 통계 */}
              <div style={{ 
                display: 'grid', 
                gridTemplateColumns: 'repeat(3, 1fr)', 
                gap: '12px',
                marginBottom: '16px'
              }}>
                <div style={{ textAlign: 'center' }}>
                  <div style={{ 
                    fontSize: '18px', 
                    fontWeight: '700',
                    color: '#1e293b'
                  }}>
                    {protocol.device_count}
                  </div>
                  <div style={{ 
                    fontSize: '12px', 
                    color: '#64748b' 
                  }}>
                    총 디바이스
                  </div>
                </div>
                <div style={{ textAlign: 'center' }}>
                  <div style={{ 
                    fontSize: '18px', 
                    fontWeight: '700',
                    color: '#16a34a'
                  }}>
                    {protocol.enabled_count}
                  </div>
                  <div style={{ 
                    fontSize: '12px', 
                    color: '#64748b' 
                  }}>
                    활성 디바이스
                  </div>
                </div>
                <div style={{ textAlign: 'center' }}>
                  <div style={{ 
                    fontSize: '18px', 
                    fontWeight: '700',
                    color: '#0284c7'
                  }}>
                    {protocol.connected_count}
                  </div>
                  <div style={{ 
                    fontSize: '12px', 
                    color: '#64748b' 
                  }}>
                    연결 중
                  </div>
                </div>
              </div>

              {/* 액션 버튼 */}
              <div style={{ 
                display: 'flex', 
                gap: '8px',
                paddingTop: '16px',
                borderTop: '1px solid #f1f5f9'
              }}>
                <button style={{
                  flex: 1,
                  padding: '8px 12px',
                  backgroundColor: '#f8fafc',
                  border: '1px solid #e2e8f0',
                  borderRadius: '6px',
                  fontSize: '14px',
                  cursor: 'pointer'
                }}>
                  👁️ 상세보기
                </button>
                <button style={{
                  flex: 1,
                  padding: '8px 12px',
                  backgroundColor: '#f8fafc',
                  border: '1px solid #e2e8f0',
                  borderRadius: '6px',
                  fontSize: '14px',
                  cursor: 'pointer'
                }}>
                  ✏️ 편집
                </button>
                <button style={{
                  padding: '8px 12px',
                  backgroundColor: '#f8fafc',
                  border: '1px solid #e2e8f0',
                  borderRadius: '6px',
                  fontSize: '14px',
                  cursor: 'pointer'
                }}>
                  🔗 테스트
                </button>
              </div>
            </div>
          ))}
        </div>
      ) : (
        /* 테이블형 뷰 */
        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0',
          overflow: 'hidden'
        }}>
          <table style={{ width: '100%', borderCollapse: 'collapse' }}>
            <thead>
              <tr style={{ backgroundColor: '#f8fafc' }}>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'left', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  프로토콜
                </th>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'left', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  카테고리
                </th>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'center', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  포트
                </th>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'center', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  디바이스 수
                </th>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'center', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  상태
                </th>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'center', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  액션
                </th>
              </tr>
            </thead>
            <tbody>
              {filteredProtocols.map((protocol, index) => (
                <tr 
                  key={protocol.id}
                  style={{ 
                    backgroundColor: index % 2 === 0 ? 'white' : '#f8fafc',
                    transition: 'background-color 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = '#f1f5f9';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = index % 2 === 0 ? 'white' : '#f8fafc';
                  }}
                >
                  <td style={{ padding: '16px', borderBottom: '1px solid #f1f5f9' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                      <div style={{
                        width: '32px',
                        height: '32px',
                        borderRadius: '6px',
                        backgroundColor: `${getCategoryColor(protocol.category)}20`,
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        fontSize: '16px'
                      }}>
                        {getCategoryIcon(protocol.category)}
                      </div>
                      <div>
                        <div style={{ 
                          fontWeight: '600', 
                          color: '#1e293b',
                          fontSize: '14px'
                        }}>
                          {protocol.display_name}
                        </div>
                        <div style={{ 
                          color: '#64748b',
                          fontSize: '12px',
                          marginTop: '2px'
                        }}>
                          {protocol.protocol_type}
                        </div>
                      </div>
                    </div>
                  </td>
                  <td style={{ padding: '16px', borderBottom: '1px solid #f1f5f9' }}>
                    <span style={{
                      padding: '4px 8px',
                      backgroundColor: getCategoryColor(protocol.category),
                      color: 'white',
                      borderRadius: '6px',
                      fontSize: '12px',
                      fontWeight: '500'
                    }}>
                      {protocol.category}
                    </span>
                  </td>
                  <td style={{ 
                    padding: '16px', 
                    borderBottom: '1px solid #f1f5f9',
                    textAlign: 'center',
                    color: '#374151',
                    fontSize: '14px'
                  }}>
                    {protocol.default_port || '-'}
                  </td>
                  <td style={{ 
                    padding: '16px', 
                    borderBottom: '1px solid #f1f5f9',
                    textAlign: 'center'
                  }}>
                    <div style={{ fontSize: '16px', fontWeight: '600', color: '#1e293b' }}>
                      {protocol.device_count}
                    </div>
                    <div style={{ fontSize: '12px', color: '#64748b' }}>
                      {protocol.connected_count} 연결중
                    </div>
                  </td>
                  <td style={{ 
                    padding: '16px', 
                    borderBottom: '1px solid #f1f5f9',
                    textAlign: 'center'
                  }}>
                    <span style={{
                      padding: '4px 8px',
                      backgroundColor: protocol.is_enabled ? '#dcfce7' : '#fee2e2',
                      color: protocol.is_enabled ? '#166534' : '#991b1b',
                      borderRadius: '6px',
                      fontSize: '12px',
                      fontWeight: '500'
                    }}>
                      {protocol.is_enabled ? '활성' : '비활성'}
                    </span>
                  </td>
                  <td style={{ 
                    padding: '16px', 
                    borderBottom: '1px solid #f1f5f9',
                    textAlign: 'center'
                  }}>
                    <div style={{ display: 'flex', gap: '4px', justifyContent: 'center' }}>
                      <button 
                        onClick={() => console.log('프로토콜 상세보기:', protocol.id)}
                        style={{
                          padding: '6px 8px',
                          backgroundColor: '#f8fafc',
                          border: '1px solid #e2e8f0',
                          borderRadius: '4px',
                          fontSize: '12px',
                          cursor: 'pointer'
                        }}
                      >
                        👁️
                      </button>
                      <button 
                        onClick={() => console.log('프로토콜 편집:', protocol.id)}
                        style={{
                          padding: '6px 8px',
                          backgroundColor: '#f8fafc',
                          border: '1px solid #e2e8f0',
                          borderRadius: '4px',
                          fontSize: '12px',
                          cursor: 'pointer'
                        }}
                      >
                        ✏️
                      </button>
                      <button 
                        onClick={() => handleProtocolAction('test', protocol.id)}
                        style={{
                          padding: '6px 8px',
                          backgroundColor: '#f8fafc',
                          border: '1px solid #e2e8f0',
                          borderRadius: '4px',
                          fontSize: '12px',
                          cursor: 'pointer'
                        }}
                      >
                        🔗
                      </button>
                      {protocol.is_enabled ? (
                        <button 
                          onClick={() => handleProtocolAction('disable', protocol.id)}
                          style={{
                            padding: '6px 8px',
                            backgroundColor: '#fef3c7',
                            border: '1px solid #f59e0b',
                            borderRadius: '4px',
                            fontSize: '12px',
                            cursor: 'pointer'
                          }}
                        >
                          ⏸️
                        </button>
                      ) : (
                        <button 
                          onClick={() => handleProtocolAction('enable', protocol.id)}
                          style={{
                            padding: '6px 8px',
                            backgroundColor: '#dcfce7',
                            border: '1px solid #16a34a',
                            borderRadius: '4px',
                            fontSize: '12px',
                            cursor: 'pointer'
                          }}
                        >
                          ▶️
                        </button>
                      )}
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
};

export default ProtocolManagement;