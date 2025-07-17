import React from 'react'

const Dashboard = () => {
  return (
    <div>
      <Section title="📡 중앙 관리 서버">
        <ul>
          <li>Backend API</li>
          <li>Web Dashboard</li>
          <li>Service Registry</li>
          <li>Configuration Manager</li>
        </ul>
      </Section>

      <Section title="🛰️ Collector 노드">
        <ul>
          <li>Factory A - Building 1: Modbus, MQTT 수집기</li>
          <li>Factory B - Building 2: BACnet, Modbus 수집기</li>
        </ul>
      </Section>

      <Section title="🗄️ Database Cluster">
        <ul>
          <li>Redis Cluster: 캐싱</li>
          <li>PostgreSQL Cluster: 설정 및 메타데이터</li>
          <li>InfluxDB Cluster: 시계열 데이터 저장</li>
        </ul>
      </Section>

      <Section title="📬 Message Queue Cluster">
        <ul>
          <li>RabbitMQ Cluster: 비동기 통신</li>
        </ul>
      </Section>
    </div>
  )
}

const Section = ({ title, children }: { title: string; children: React.ReactNode }) => (
  <div style={{ marginBottom: '2rem' }}>
    <h2>{title}</h2>
    {children}
  </div>
)

export default Dashboard
