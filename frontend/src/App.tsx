import React from 'react'
import Dashboard from './pages/Dashboard'
import './styles/base.css'

const App = () => {
  return (
    <div style={{ padding: '2rem', fontFamily: 'sans-serif' }}>
      <h1>PulseOne Web Dashboard</h1>
      <Dashboard />
    </div>
  )
}

export default App
