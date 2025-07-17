import React, { useState, useEffect, useCallback } from 'react';
import '../styles/base.css';
import '../styles/virtual-points.css';

interface VirtualPoint {
  id: string;
  name: string;
  description: string;
  formula: string;
  dataType: 'number' | 'boolean' | 'string';
  unit?: string;
  category: string;
  factory: string;
  isEnabled: boolean;
  calculationInterval: number; // ms
  inputVariables: InputVariable[];
  currentValue?: any;
  lastCalculated?: Date;
  status: 'active' | 'error' | 'disabled';
  errorMessage?: string;
  tags: string[];
  createdAt: Date;
  updatedAt: Date;
  createdBy: string;
}

interface InputVariable {
  id: string;
  name: string;
  sourceType: 'data_point' | 'virtual_point' | 'constant';
  sourceId?: string;
  sourcePath?: string;
  constantValue?: any;
  dataType: 'number' | 'boolean' | 'string';
  currentValue?: any;
  description?: string;
}

interface FormulaFunction {
  name: string;
  description: string;
  syntax: string;
  category: 'math' | 'logic' | 'time' | 'string' | 'conversion';
  example: string;
}

const VirtualPoints: React.FC = () => {
  const [virtualPoints, setVirtualPoints] = useState<VirtualPoint[]>([]);
  const [filteredPoints, setFilteredPoints] = useState<VirtualPoint[]>([]);
  const [selectedPoint, setSelectedPoint] = useState<VirtualPoint | null>(null);
  const [isEditing, setIsEditing] = useState(false);
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showFormulaHelper, setShowFormulaHelper] = useState(false);
  const [searchTerm, setSearchTerm] = useState('');
  const [filterCategory, setFilterCategory] = useState('all');
  const [filterStatus, setFilterStatus] = useState('all');
  const [filterFactory, setFilterFactory] = useState('all');
  const [sortBy, setSortBy] = useState<'name' | 'category' | 'updated' | 'status'>('name');
  const [sortOrder, setSortOrder] = useState<'asc' | 'desc'>('asc');
  const [testFormula, setTestFormula] = useState('');
  const [testResult, setTestResult] = useState<any>(null);
  const [testError, setTestError] = useState<string | null>(null);

  // 새 가상포인트 생성용 폼 데이터
  const [formData, setFormData] = useState<Partial<VirtualPoint>>({
    name: '',
    description: '',
    formula: '',
    dataType: 'number',
    unit: '',
    category: '',
    factory: '',
    isEnabled: true,
    calculationInterval: 1000,
    inputVariables: [],
    tags: []
  });

  // 사용 가능한 함수 목록
  const formulaFunctions: FormulaFunction[] = [
    { name: 'abs', description: '절댓값', syntax: 'abs(x)', category: 'math', example: 'abs(-5) = 5' },
    { name: 'sqrt', description: '제곱근', syntax: 'sqrt(x)', category: 'math', example: 'sqrt(16) = 4' },
    { name: 'pow', description: '거듭제곱', syntax: 'pow(x, y)', category: 'math', example: 'pow(2, 3) = 8' },
    { name: 'sin', description: '사인', syntax: 'sin(x)', category: 'math', example: 'sin(PI/2) = 1' },
    { name: 'cos', description: '코사인', syntax: 'cos(x)', category: 'math', example: 'cos(0) = 1' },
    { name: 'tan', description: '탄젠트', syntax: 'tan(x)', category: 'math', example: 'tan(PI/4) = 1' },
    { name: 'log', description: '자연로그', syntax: 'log(x)', category: 'math', example: 'log(E) = 1' },
    { name: 'log10', description: '상용로그', syntax: 'log10(x)', category: 'math', example: 'log10(100) = 2' },
    { name: 'min', description: '최솟값', syntax: 'min(x, y, ...)', category: 'math', example: 'min(1, 2, 3) = 1' },
    { name: 'max', description: '최댓값', syntax: 'max(x, y, ...)', category: 'math', example: 'max(1, 2, 3) = 3' },
    { name: 'avg', description: '평균값', syntax: 'avg(x, y, ...)', category: 'math', example: 'avg(1, 2, 3) = 2' },
    { name: 'sum', description: '합계', syntax: 'sum(x, y, ...)', category: 'math', example: 'sum(1, 2, 3) = 6' },
    { name: 'if', description: '조건문', syntax: 'if(condition, true_value, false_value)', category: 'logic', example: 'if(x > 0, "positive", "negative")' },
    { name: 'and', description: '논리곱', syntax: 'and(x, y)', category: 'logic', example: 'and(true, false) = false' },
    { name: 'or', description: '논리합', syntax: 'or(x, y)', category: 'logic', example: 'or(true, false) = true' },
    { name: 'not', description: '논리부정', syntax: 'not(x)', category: 'logic', example: 'not(true) = false' },
    { name: 'now', description: '현재 시간', syntax: 'now()', category: 'time', example: 'now() = 2023-12-01T10:30:00' },
    { name: 'hour', description: '시간 추출', syntax: 'hour(timestamp)', category: 'time', example: 'hour(now()) = 10' },
    { name: 'minute', description: '분 추출', syntax: 'minute(timestamp)', category: 'time', example: 'minute(now()) = 30' },
    { name: 'dayOfWeek', description: '요일', syntax: 'dayOfWeek(timestamp)', category: 'time', example: 'dayOfWeek(now()) = 5' },
    { name: 'celsius2fahrenheit', description: '섭씨→화씨', syntax: 'celsius2fahrenheit(x)', category: 'conversion', example: 'celsius2fahrenheit(0) = 32' },
    { name: 'fahrenheit2celsius', description: '화씨→섭씨', syntax: 'fahrenheit2celsius(x)', category: 'conversion', example: 'fahrenheit2celsius(32) = 0' },
    { name: 'mbar2psi', description: 'mbar→psi', syntax: 'mbar2psi(x)', category: 'conversion', example: 'mbar2psi(1000) = 14.5' },
    { name: 'psi2mbar', description: 'psi→mbar', syntax: 'psi2mbar(x)', category: 'conversion', example: 'psi2mbar(14.5) = 1000' }
  ];

  // 초기 데이터 로드
  useEffect(() => {
    loadVirtualPoints();
  }, []);

  // 필터링
  useEffect(() => {
    applyFilters();
  }, [virtualPoints, searchTerm, filterCategory, filterStatus, filterFactory, sortBy, sortOrder]);

  const loadVirtualPoints = async () => {
    // 목업 데이터 생성
    const mockPoints: VirtualPoint[] = [
      {
        id: 'vp001',
        name: 'Average Temperature',
        description: '생산라인 평균 온도 계산',
        formula: '(temp1 + temp2 + temp3) / 3',
        dataType: 'number',
        unit: '°C',
        category: 'Temperature',
        factory: 'Seoul Factory',
        isEnabled: true,
        calculationInterval: 5000,
        inputVariables: [
          { id: 'temp1', name: 'Temperature Sensor 1', sourceType: 'data_point', sourcePath: 'plc001:temperature_01', dataType: 'number', currentValue: 25.3 },
          { id: 'temp2', name: 'Temperature Sensor 2', sourceType: 'data_point', sourcePath: 'plc001:temperature_02', dataType: 'number', currentValue: 26.1 },
          { id: 'temp3', name: 'Temperature Sensor 3', sourceType: 'data_point', sourcePath: 'plc001:temperature_03', dataType: 'number', currentValue: 24.8 }
        ],
        currentValue: 25.4,
        lastCalculated: new Date(Date.now() - 30000),
        status: 'active',
        tags: ['temperature', 'average', 'production'],
        createdAt: new Date('2023-11-01'),
        updatedAt: new Date('2023-11-15'),
        createdBy: 'admin'
      },
      {
        id: 'vp002',
        name: 'Total Power Consumption',
        description: '전체 전력 소비량 합계',
        formula: 'sum(motor1_power, motor2_power, hvac_power, lighting_power)',
        dataType: 'number',
        unit: 'kW',
        category: 'Power',
        factory: 'Seoul Factory',
        isEnabled: true,
        calculationInterval: 2000,
        inputVariables: [
          { id: 'motor1_power', name: 'Motor 1 Power', sourceType: 'data_point', sourcePath: 'plc002:motor1_current', dataType: 'number', currentValue: 12.5 },
          { id: 'motor2_power', name: 'Motor 2 Power', sourceType: 'data_point', sourcePath: 'plc002:motor2_current', dataType: 'number', currentValue: 8.3 },
          { id: 'hvac_power', name: 'HVAC Power', sourceType: 'data_point', sourcePath: 'hvac001:power_consumption', dataType: 'number', currentValue: 15.2 },
          { id: 'lighting_power', name: 'Lighting Power', sourceType: 'constant', constantValue: 5.0, dataType: 'number', currentValue: 5.0 }
        ],
        currentValue: 41.0,
        lastCalculated: new Date(Date.now() - 15000),
        status: 'active',
        tags: ['power', 'consumption', 'total'],
        createdAt: new Date('2023-10-15'),
        updatedAt: new Date('2023-11-20'),
        createdBy: 'engineer1'
      },
      {
        id: 'vp003',
        name: 'Production Efficiency',
        description: '생산 효율성 계산 (생산량/목표량 * 100)',
        formula: 'if(target_production > 0, (actual_production / target_production) * 100, 0)',
        dataType: 'number',
        unit: '%',
        category: 'Production',
        factory: 'Busan Factory',
        isEnabled: true,
        calculationInterval: 10000,
        inputVariables: [
          { id: 'actual_production', name: 'Actual Production', sourceType: 'data_point', sourcePath: 'counter001:production_count', dataType: 'number', currentValue: 850 },
          { id: 'target_production', name: 'Target Production', sourceType: 'constant', constantValue: 1000, dataType: 'number', currentValue: 1000 }
        ],
        currentValue: 85.0,
        lastCalculated: new Date(Date.now() - 45000),
        status: 'active',
        tags: ['production', 'efficiency', 'kpi'],
        createdAt: new Date('2023-09-20'),
        updatedAt: new Date('2023-11-10'),
        createdBy: 'manager1'
      },
      {
        id: 'vp004',
        name: 'Vibration Alert Status',
        description: '진동 경보 상태 (임계값 초과 여부)',
        formula: 'if(vibration_level > vibration_threshold, "ALERT", "NORMAL")',
        dataType: 'string',
        category: 'Safety',
        factory: 'Seoul Factory',
        isEnabled: false,
        calculationInterval: 1000,
        inputVariables: [
          { id: 'vibration_level', name: 'Vibration Level', sourceType: 'data_point', sourcePath: 'sensor001:vibration_rms', dataType: 'number', currentValue: 0.05 },
          { id: 'vibration_threshold', name: 'Vibration Threshold', sourceType: 'constant', constantValue: 0.1, dataType: 'number', currentValue: 0.1 }
        ],
        currentValue: 'NORMAL',
        lastCalculated: new Date(Date.now() - 120000),
        status: 'disabled',
        tags: ['vibration', 'safety', 'alert'],
        createdAt: new Date('2023-11-05'),
        updatedAt: new Date('2023-11-18'),
        createdBy: 'safety_engineer'
      },
      {
        id: 'vp005',
        name: 'Equipment Running Time Today',
        description: '오늘 장비 가동 시간 계산',
        formula: 'if(equipment_status == "RUNNING", (now() - start_of_day()) / 3600000, 0)',
        dataType: 'number',
        unit: 'hours',
        category: 'Maintenance',
        factory: 'Daegu Factory',
        isEnabled: true,
        calculationInterval: 60000,
        inputVariables: [
          { id: 'equipment_status', name: 'Equipment Status', sourceType: 'data_point', sourcePath: 'plc003:equipment_status', dataType: 'string', currentValue: 'RUNNING' }
        ],
        currentValue: 6.5,
        lastCalculated: new Date(Date.now() - 60000),
        status: 'active',
        tags: ['maintenance', 'runtime', 'equipment'],
        createdAt: new Date('2023-10-30'),
        updatedAt: new Date('2023-11-22'),
        createdBy: 'maintenance_tech'
      },
      {
        id: 'vp006',
        name: 'Flow Rate Error Check',
        description: '유량 센서 오류 감지',
        formula: 'abs(flow_sensor1 - flow_sensor2) > error_tolerance',
        dataType: 'boolean',
        category: 'Quality',
        factory: 'Seoul Factory',
        isEnabled: true,
        calculationInterval: 3000,
        inputVariables: [
          { id: 'flow_sensor1', name: 'Flow Sensor 1', sourceType: 'data_point', sourcePath: 'flow001:rate', dataType: 'number', currentValue: 45.2 },
          { id: 'flow_sensor2', name: 'Flow Sensor 2', sourceType: 'data_point', sourcePath: 'flow002:rate', dataType: 'number', currentValue: 44.8 },
          { id: 'error_tolerance', name: 'Error Tolerance', sourceType: 'constant', constantValue: 2.0, dataType: 'number', currentValue: 2.0 }
        ],
        currentValue: false,
        lastCalculated: new Date(Date.now() - 8000),
        status: 'error',
        errorMessage: 'Flow sensor 2 communication timeout',
        tags: ['quality', 'error-detection', 'flow'],
        createdAt: new Date('2023-11-12'),
        updatedAt: new Date('2023-11-25'),
        createdBy: 'quality_engineer'
      }
    ];

    setVirtualPoints(mockPoints);
  };

  const applyFilters = () => {
    let filtered = virtualPoints;

    // 검색 필터
    if (searchTerm) {
      filtered = filtered.filter(point =>
        point.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        point.description.toLowerCase().includes(searchTerm.toLowerCase()) ||
        point.formula.toLowerCase().includes(searchTerm.toLowerCase()) ||
        point.tags.some(tag => tag.toLowerCase().includes(searchTerm.toLowerCase()))
      );
    }

    // 카테고리 필터
    if (filterCategory !== 'all') {
      filtered = filtered.filter(point => point.category === filterCategory);
    }

    // 상태 필터
    if (filterStatus !== 'all') {
      filtered = filtered.filter(point => point.status === filterStatus);
    }

    // 공장 필터
    if (filterFactory !== 'all') {
      filtered = filtered.filter(point => point.factory === filterFactory);
    }

    // 정렬
    filtered.sort((a, b) => {
      let comparison = 0;
      switch (sortBy) {
        case 'name':
          comparison = a.name.localeCompare(b.name);
          break;
        case 'category':
          comparison = a.category.localeCompare(b.category);
          break;
        case 'updated':
          comparison = a.updatedAt.getTime() - b.updatedAt.getTime();
          break;
        case 'status':
          comparison = a.status.localeCompare(b.status);
          break;
      }
      return sortOrder === 'asc' ? comparison : -comparison;
    });

    setFilteredPoints(filtered);
  };

  const handleCreatePoint = () => {
    setFormData({
      name: '',
      description: '',
      formula: '',
      dataType: 'number',
      unit: '',
      category: '',
      factory: '',
      isEnabled: true,
      calculationInterval: 1000,
      inputVariables: [],
      tags: []
    });
    setIsEditing(false);
    setShowCreateModal(true);
  };

  const handleEditPoint = (point: VirtualPoint) => {
    setFormData(point);
    setSelectedPoint(point);
    setIsEditing(true);
    setShowCreateModal(true);
  };

  const handleSavePoint = async () => {
    try {
      if (isEditing && selectedPoint) {
        // 업데이트
        setVirtualPoints(prev => prev.map(p => 
          p.id === selectedPoint.id 
            ? { ...formData, id: selectedPoint.id, updatedAt: new Date() } as VirtualPoint
            : p
        ));
      } else {
        // 생성
        const newPoint: VirtualPoint = {
          ...formData,
          id: `vp_${Date.now()}`,
          currentValue: null,
          status: 'active',
          createdAt: new Date(),
          updatedAt: new Date(),
          createdBy: 'current_user'
        } as VirtualPoint;
        
        setVirtualPoints(prev => [...prev, newPoint]);
      }
      setShowCreateModal(false);
    } catch (error) {
      console.error('Failed to save virtual point:', error);
    }
  };

  const handleDeletePoint = async (pointId: string) => {
    if (confirm('이 가상포인트를 삭제하시겠습니까?')) {
      setVirtualPoints(prev => prev.filter(p => p.id !== pointId));
    }
  };

  const handleToggleEnabled = async (pointId: string) => {
    setVirtualPoints(prev => prev.map(p => 
      p.id === pointId 
        ? { ...p, isEnabled: !p.isEnabled, status: !p.isEnabled ? 'active' : 'disabled' }
        : p
    ));
  };

  const testFormulaCalculation = () => {
    try {
      // 간단한 수식 테스트 시뮬레이션
      const mockVariables: { [key: string]: any } = {
        temp1: 25.3,
        temp2: 26.1,
        temp3: 24.8,
        motor1_power: 12.5,
        motor2_power: 8.3,
        PI: Math.PI,
        E: Math.E
      };

      // 기본적인 수식 파싱 및 계산 시뮬레이션
      let formula = testFormula;
      
      // 변수 치환
      Object.keys(mockVariables).forEach(variable => {
        const regex = new RegExp(`\\b${variable}\\b`, 'g');
        formula = formula.replace(regex, mockVariables[variable].toString());
      });

      // 기본 함수들 치환
      formula = formula.replace(/\babs\(/g, 'Math.abs(');
      formula = formula.replace(/\bsqrt\(/g, 'Math.sqrt(');
      formula = formula.replace(/\bsin\(/g, 'Math.sin(');
      formula = formula.replace(/\bcos\(/g, 'Math.cos(');
      formula = formula.replace(/\btan\(/g, 'Math.tan(');
      formula = formula.replace(/\blog\(/g, 'Math.log(');
      formula = formula.replace(/\bpow\(/g, 'Math.pow(');
      formula = formula.replace(/\bmin\(/g, 'Math.min(');
      formula = formula.replace(/\bmax\(/g, 'Math.max(');

      // 안전한 eval 대신 간단한 계산 결과 시뮬레이션
      if (testFormula.includes('temp1 + temp2 + temp3')) {
        setTestResult((25.3 + 26.1 + 24.8).toFixed(2));
      } else if (testFormula.includes('sum(')) {
        setTestResult('41.0');
      } else if (testFormula.includes('avg(')) {
        setTestResult('25.4');
      } else {
        setTestResult('수식 테스트 결과');
      }
      
      setTestError(null);
    } catch (error) {
      setTestError(error instanceof Error ? error.message : '수식 오류');
      setTestResult(null);
    }
  };

  const insertFunction = (func: FormulaFunction) => {
    const textarea = document.getElementById('formula-input') as HTMLTextAreaElement;
    if (textarea) {
      const start = textarea.selectionStart;
      const end = textarea.selectionEnd;
      const currentFormula = formData.formula || '';
      const newFormula = currentFormula.substring(0, start) + func.syntax + currentFormula.substring(end);
      
      setFormData(prev => ({ ...prev, formula: newFormula }));
      
      // 커서 위치 조정
      setTimeout(() => {
        textarea.focus();
        textarea.setSelectionRange(start + func.syntax.length, start + func.syntax.length);
      }, 0);
    }
  };

  const addInputVariable = () => {
    const newVariable: InputVariable = {
      id: `var_${Date.now()}`,
      name: '',
      sourceType: 'data_point',
      dataType: 'number',
      description: ''
    };
    
    setFormData(prev => ({
      ...prev,
      inputVariables: [...(prev.inputVariables || []), newVariable]
    }));
  };

  const updateInputVariable = (index: number, updates: Partial<InputVariable>) => {
    setFormData(prev => ({
      ...prev,
      inputVariables: prev.inputVariables?.map((variable, i) => 
        i === index ? { ...variable, ...updates } : variable
      ) || []
    }));
  };

  const removeInputVariable = (index: number) => {
    setFormData(prev => ({
      ...prev,
      inputVariables: prev.inputVariables?.filter((_, i) => i !== index) || []
    }));
  };

  // 고유 값들 추출
  const uniqueCategories = [...new Set(virtualPoints.map(p => p.category))];
  const uniqueFactories = [...new Set(virtualPoints.map(p => p.factory))];

  return (
    <div className="virtual-points-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">가상포인트 관리</h1>
        <div className="page-actions">
          <button 
            className="btn btn-primary"
            onClick={handleCreatePoint}
          >
            <i className="fas fa-plus"></i>
            가상포인트 생성
          </button>
          <button 
            className="btn btn-outline"
            onClick={() => setShowFormulaHelper(!showFormulaHelper)}
          >
            <i className="fas fa-question-circle"></i>
            수식 도움말
          </button>
        </div>
      </div>

      {/* 필터 패널 */}
      <div className="filter-panel">
        <div className="filter-row">
          <div className="filter-group">
            <label>카테고리</label>
            <select
              value={filterCategory}
              onChange={(e) => setFilterCategory(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 카테고리</option>
              {uniqueCategories.map(category => (
                <option key={category} value={category}>{category}</option>
              ))}
            </select>
          </div>

          <div className="filter-group">
            <label>상태</label>
            <select
              value={filterStatus}
              onChange={(e) => setFilterStatus(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 상태</option>
              <option value="active">활성</option>
              <option value="disabled">비활성</option>
              <option value="error">오류</option>
            </select>
          </div>

          <div className="filter-group">
            <label>공장</label>
            <select
              value={filterFactory}
              onChange={(e) => setFilterFactory(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 공장</option>
              {uniqueFactories.map(factory => (
                <option key={factory} value={factory}>{factory}</option>
              ))}
            </select>
          </div>

          <div className="filter-group flex-1">
            <label>검색</label>
            <div className="search-container">
              <input
                type="text"
                placeholder="이름, 설명, 수식, 태그 검색..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="search-input"
              />
              <i className="fas fa-search search-icon"></i>
            </div>
          </div>

          <div className="filter-group">
            <label>정렬</label>
            <div className="sort-controls">
              <select
                value={sortBy}
                onChange={(e) => setSortBy(e.target.value as any)}
                className="sort-select"
              >
                <option value="name">이름순</option>
                <option value="category">카테고리순</option>
                <option value="updated">수정일순</option>
                <option value="status">상태순</option>
              </select>
              <button
                className={`sort-order-btn ${sortOrder}`}
                onClick={() => setSortOrder(prev => prev === 'asc' ? 'desc' : 'asc')}
              >
                <i className={`fas fa-sort-${sortOrder === 'asc' ? 'up' : 'down'}`}></i>
              </button>
            </div>
          </div>
        </div>
      </div>

      {/* 통계 정보 */}
      <div className="stats-panel">
        <div className="stat-card">
          <div className="stat-value">{virtualPoints.length}</div>
          <div className="stat-label">전체 가상포인트</div>
        </div>
        <div className="stat-card status-active">
          <div className="stat-value">{virtualPoints.filter(p => p.status === 'active').length}</div>
          <div className="stat-label">활성</div>
        </div>
        <div className="stat-card status-disabled">
          <div className="stat-value">{virtualPoints.filter(p => p.status === 'disabled').length}</div>
          <div className="stat-label">비활성</div>
        </div>
        <div className="stat-card status-error">
          <div className="stat-value">{virtualPoints.filter(p => p.status === 'error').length}</div>
          <div className="stat-label">오류</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{filteredPoints.length}</div>
          <div className="stat-label">필터 결과</div>
        </div>
      </div>

      {/* 가상포인트 목록 */}
      <div className="points-list">
        <div className="points-grid">
          {filteredPoints.map(point => (
            <div key={point.id} className={`point-card ${point.status}`}>
              <div className="point-header">
                <div className="point-title">
                  <h3>{point.name}</h3>
                  <div className="point-badges">
                    <span className={`status-badge ${point.status}`}>
                      {point.status === 'active' ? '활성' : 
                       point.status === 'disabled' ? '비활성' : '오류'}
                    </span>
                    <span className="category-badge">{point.category}</span>
                  </div>
                </div>
                <div className="point-actions">
                  <button
                    className={`toggle-btn ${point.isEnabled ? 'enabled' : 'disabled'}`}
                    onClick={() => handleToggleEnabled(point.id)}
                    title={point.isEnabled ? '비활성화' : '활성화'}
                  >
                    <i className={`fas ${point.isEnabled ? 'fa-toggle-on' : 'fa-toggle-off'}`}></i>
                  </button>
                  <button
                    className="edit-btn"
                    onClick={() => handleEditPoint(point)}
                    title="편집"
                  >
                    <i className="fas fa-edit"></i>
                  </button>
                  <button
                    className="delete-btn"
                    onClick={() => handleDeletePoint(point.id)}
                    title="삭제"
                  >
                    <i className="fas fa-trash"></i>
                  </button>
                </div>
              </div>

              <div className="point-content">
                <div className="point-description">
                  {point.description}
                </div>

                <div className="point-formula">
                  <label>수식:</label>
                  <code>{point.formula}</code>
                </div>

                <div className="point-current-value">
                  <label>현재값:</label>
                  <span className={`value ${point.status}`}>
                    {point.currentValue !== null && point.currentValue !== undefined 
                      ? `${point.currentValue}${point.unit ? ` ${point.unit}` : ''}`
                      : 'N/A'
                    }
                  </span>
                </div>

                <div className="point-metadata">
                  <div className="metadata-row">
                    <span>데이터 타입:</span>
                    <span>{point.dataType}</span>
                  </div>
                  <div className="metadata-row">
                    <span>계산 주기:</span>
                    <span>{point.calculationInterval}ms</span>
                  </div>
                  <div className="metadata-row">
                    <span>공장:</span>
                    <span>{point.factory}</span>
                  </div>
                  <div className="metadata-row">
                    <span>입력 변수:</span>
                    <span>{point.inputVariables.length}개</span>
                  </div>
                  <div className="metadata-row">
                    <span>마지막 계산:</span>
                    <span>{point.lastCalculated?.toLocaleString() || 'N/A'}</span>
                  </div>
                </div>

                {point.errorMessage && (
                  <div className="point-error">
                    <i className="fas fa-exclamation-triangle"></i>
                    {point.errorMessage}
                  </div>
                )}

                <div className="point-tags">
                  {point.tags.map(tag => (
                    <span key={tag} className="tag">{tag}</span>
                  ))}
                </div>
              </div>
            </div>
          ))}
        </div>

        {filteredPoints.length === 0 && (
          <div className="empty-state">
            <i className="fas fa-calculator empty-icon"></i>
            <div className="empty-title">가상포인트가 없습니다</div>
            <div className="empty-description">
              새 가상포인트를 생성하거나 필터 조건을 변경해보세요.
            </div>
          </div>
        )}
      </div>

      {/* 수식 도움말 패널 */}
      {showFormulaHelper && (
        <div className="formula-helper-panel">
          <div className="helper-header">
            <h3>수식 함수 도움말</h3>
            <button
              className="btn btn-sm btn-outline"
              onClick={() => setShowFormulaHelper(false)}
            >
              <i className="fas fa-times"></i>
            </button>
          </div>
          
          <div className="helper-content">
            <div className="function-categories">
              {['math', 'logic', 'time', 'conversion'].map(category => (
                <div key={category} className="function-category">
                  <h4>{category === 'math' ? '수학 함수' : 
                       category === 'logic' ? '논리 함수' :
                       category === 'time' ? '시간 함수' : '변환 함수'}</h4>
                  <div className="function-list">
                    {formulaFunctions
                      .filter(func => func.category === category)
                      .map(func => (
                        <div key={func.name} className="function-item">
                          <div className="function-name">{func.name}</div>
                          <div className="function-description">{func.description}</div>
                          <div className="function-syntax">
                            <code>{func.syntax}</code>
                          </div>
                          <div className="function-example">
                            예: <code>{func.example}</code>
                          </div>
                        </div>
                      ))}
                  </div>
                </div>
              ))}
            </div>
            
            <div className="formula-tester">
              <h4>수식 테스트</h4>
              <div className="test-input-group">
                <textarea
                  placeholder="테스트할 수식을 입력하세요 (예: (temp1 + temp2 + temp3) / 3)"
                  value={testFormula}
                  onChange={(e) => setTestFormula(e.target.value)}
                  className="test-formula-input"
                />
                <button
                  className="btn btn-primary"
                  onClick={testFormulaCalculation}
                  disabled={!testFormula.trim()}
                >
                  테스트
                </button>
              </div>
              
              {testResult !== null && (
                <div className="test-result success">
                  <strong>결과:</strong> {testResult}
                </div>
              )}
              
              {testError && (
                <div className="test-result error">
                  <strong>오류:</strong> {testError}
                </div>
              )}
            </div>
          </div>
        </div>
      )}

      {/* 생성/편집 모달 */}
      {showCreateModal && (
        <div className="modal-overlay">
          <div className="modal-container">
            <div className="modal-header">
              <h2>{isEditing ? '가상포인트 편집' : '새 가상포인트 생성'}</h2>
              <button
                className="modal-close-btn"
                onClick={() => setShowCreateModal(false)}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              <div className="form-section">
                <h3>기본 정보</h3>
                <div className="form-row">
                  <div className="form-group">
                    <label>이름 *</label>
                    <input
                      type="text"
                      value={formData.name || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, name: e.target.value }))}
                      className="form-input"
                      placeholder="가상포인트 이름"
                    />
                  </div>
                  <div className="form-group">
                    <label>카테고리 *</label>
                    <input
                      type="text"
                      value={formData.category || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, category: e.target.value }))}
                      className="form-input"
                      placeholder="카테고리"
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label>데이터 타입 *</label>
                    <select
                      value={formData.dataType || 'number'}
                      onChange={(e) => setFormData(prev => ({ ...prev, dataType: e.target.value as any }))}
                      className="form-select"
                    >
                      <option value="number">숫자</option>
                      <option value="boolean">불린</option>
                      <option value="string">문자열</option>
                    </select>
                  </div>
                  <div className="form-group">
                    <label>단위</label>
                    <input
                      type="text"
                      value={formData.unit || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, unit: e.target.value }))}
                      className="form-input"
                      placeholder="단위 (예: °C, kW, %)"
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label>공장 *</label>
                    <select
                      value={formData.factory || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, factory: e.target.value }))}
                      className="form-select"
                    >
                      <option value="">공장 선택</option>
                      {uniqueFactories.map(factory => (
                        <option key={factory} value={factory}>{factory}</option>
                      ))}
                    </select>
                  </div>
                  <div className="form-group">
                    <label>계산 주기 (ms) *</label>
                    <input
                      type="number"
                      value={formData.calculationInterval || 1000}
                      onChange={(e) => setFormData(prev => ({ ...prev, calculationInterval: Number(e.target.value) }))}
                      className="form-input"
                      min="100"
                      step="100"
                    />
                  </div>
                </div>

                <div className="form-group">
                  <label>설명</label>
                  <textarea
                    value={formData.description || ''}
                    onChange={(e) => setFormData(prev => ({ ...prev, description: e.target.value }))}
                    className="form-textarea"
                    placeholder="가상포인트 설명"
                    rows={3}
                  />
                </div>
              </div>

              <div className="form-section">
                <h3>입력 변수</h3>
                <div className="variables-list">
                  {formData.inputVariables?.map((variable, index) => (
                    <div key={variable.id} className="variable-item">
                      <div className="variable-header">
                        <h4>변수 {index + 1}</h4>
                        <button
                          className="btn btn-sm btn-error"
                          onClick={() => removeInputVariable(index)}
                        >
                          <i className="fas fa-trash"></i>
                        </button>
                      </div>
                      
                      <div className="variable-form">
                        <div className="form-row">
                          <div className="form-group">
                            <label>변수명 *</label>
                            <input
                              type="text"
                              value={variable.name}
                              onChange={(e) => updateInputVariable(index, { name: e.target.value })}
                              className="form-input"
                              placeholder="변수명 (수식에서 사용)"
                            />
                          </div>
                          <div className="form-group">
                            <label>소스 타입 *</label>
                            <select
                              value={variable.sourceType}
                              onChange={(e) => updateInputVariable(index, { sourceType: e.target.value as any })}
                              className="form-select"
                            >
                              <option value="data_point">데이터 포인트</option>
                              <option value="virtual_point">가상포인트</option>
                              <option value="constant">상수</option>
                            </select>
                          </div>
                        </div>

                        <div className="form-row">
                          <div className="form-group">
                            <label>데이터 타입 *</label>
                            <select
                              value={variable.dataType}
                              onChange={(e) => updateInputVariable(index, { dataType: e.target.value as any })}
                              className="form-select"
                            >
                              <option value="number">숫자</option>
                              <option value="boolean">불린</option>
                              <option value="string">문자열</option>
                            </select>
                          </div>
                          <div className="form-group">
                            {variable.sourceType === 'constant' ? (
                              <>
                                <label>상수값 *</label>
                                <input
                                  type={variable.dataType === 'number' ? 'number' : 'text'}
                                  value={variable.constantValue || ''}
                                  onChange={(e) => updateInputVariable(index, { 
                                    constantValue: variable.dataType === 'number' ? Number(e.target.value) : e.target.value 
                                  })}
                                  className="form-input"
                                  placeholder="상수값"
                                />
                              </>
                            ) : (
                              <>
                                <label>소스 경로 *</label>
                                <input
                                  type="text"
                                  value={variable.sourcePath || ''}
                                  onChange={(e) => updateInputVariable(index, { sourcePath: e.target.value })}
                                  className="form-input"
                                  placeholder="예: plc001:temperature_01"
                                />
                              </>
                            )}
                          </div>
                        </div>

                        <div className="form-group">
                          <label>설명</label>
                          <input
                            type="text"
                            value={variable.description || ''}
                            onChange={(e) => updateInputVariable(index, { description: e.target.value })}
                            className="form-input"
                            placeholder="변수 설명"
                          />
                        </div>
                      </div>
                    </div>
                  ))}
                </div>

                <button
                  className="btn btn-outline"
                  onClick={addInputVariable}
                >
                  <i className="fas fa-plus"></i>
                  입력 변수 추가
                </button>
              </div>

              <div className="form-section">
                <h3>수식</h3>
                <div className="formula-editor">
                  <div className="formula-input-container">
                    <textarea
                      id="formula-input"
                      value={formData.formula || ''}
                      onChange={(e) => setFormData(prev => ({ ...prev, formula: e.target.value }))}
                      className="formula-textarea"
                      placeholder="수식을 입력하세요 (예: (temp1 + temp2 + temp3) / 3)"
                      rows={4}
                    />
                  </div>
                  
                  <div className="formula-functions">
                    <h4>함수 삽입</h4>
                    <div className="function-buttons">
                      {formulaFunctions.slice(0, 8).map(func => (
                        <button
                          key={func.name}
                          className="function-btn"
                          onClick={() => insertFunction(func)}
                          title={func.description}
                        >
                          {func.name}
                        </button>
                      ))}
                    </div>
                  </div>
                </div>
              </div>

              <div className="form-section">
                <h3>기타 설정</h3>
                <div className="form-row">
                  <div className="form-group">
                    <label>태그</label>
                    <input
                      type="text"
                      value={formData.tags?.join(', ') || ''}
                      onChange={(e) => setFormData(prev => ({ 
                        ...prev, 
                        tags: e.target.value.split(',').map(tag => tag.trim()).filter(tag => tag) 
                      }))}
                      className="form-input"
                      placeholder="태그를 쉼표로 구분하여 입력"
                    />
                  </div>
                  <div className="form-group">
                    <label className="checkbox-label">
                      <input
                        type="checkbox"
                        checked={formData.isEnabled || false}
                        onChange={(e) => setFormData(prev => ({ ...prev, isEnabled: e.target.checked }))}
                      />
                      생성 후 즉시 활성화
                    </label>
                  </div>
                </div>
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-outline"
                onClick={() => setShowCreateModal(false)}
              >
                취소
              </button>
              <button
                className="btn btn-primary"
                onClick={handleSavePoint}
                disabled={!formData.name || !formData.formula || !formData.factory}
              >
                {isEditing ? '수정' : '생성'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default VirtualPoints;

