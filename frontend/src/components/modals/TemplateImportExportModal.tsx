// ============================================================================
// frontend/src/components/modals/TemplateImportExportModal.tsx
// CSV 기반 템플릿 내보내기/가져오기 모달
// ============================================================================

import React, { useState, useRef } from 'react';
import { useTranslation } from 'react-i18next';

export interface TemplateImportExportModalProps {
  isOpen: boolean;
  mode: 'import' | 'export';
  templates?: any[];
  onClose: () => void;
  onImport?: (templates: any[]) => Promise<void>;
  onExport?: (selectedTemplateIds: number[]) => Promise<void>;
  loading?: boolean;
}

const TemplateImportExportModal: React.FC<TemplateImportExportModalProps> = ({
  isOpen,
  mode,
  templates = [],
  onClose,
  onImport,
  onExport,
  loading = false
}) => {
  const [selectedTemplates, setSelectedTemplates] = useState<number[]>([]);
    const { t } = useTranslation(['deviceTemplates', 'common']);
  const [csvData, setCsvData] = useState('');
  const [parseResults, setParseResults] = useState<any>(null);
  const [importResults, setImportResults] = useState<any>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);

  if (!isOpen) return null;

  // CSV 헤더 정의
  const CSV_HEADERS = [
    'name',                    // 템플릿명
    'description',             // 설명
    'category',                // 카테고리
    'condition_type',          // 조건 타입
    'severity',                // 심각도
    'threshold',               // 임계값
    'high_limit',              // 상한값
    'low_limit',               // 하한값
    'deadband',                // 데드밴드
    'message_template',        // 메시지 템플릿
    'applicable_data_types',   // 적용 가능 데이터 타입
    'notification_enabled',    // 알림 활성화
    'auto_clear'               // 자동 해제
  ];

  // 템플릿을 CSV 형태로 변환
  const templateToCsvRow = (template: any): string[] => {
    return [
      template.name || '',
      template.description || '',
      template.category || '',
      template.condition_type || '',
      template.severity || '',
      template.default_config?.threshold || '',
      template.default_config?.high_limit || '',
      template.default_config?.low_limit || '',
      template.default_config?.deadband || '',
      template.message_template || '',
      Array.isArray(template.applicable_data_types) 
        ? template.applicable_data_types.join(';') 
        : '',
      template.notification_enabled ? 'TRUE' : 'FALSE',
      template.auto_clear ? 'TRUE' : 'FALSE'
    ];
  };

  // CSV에서 템플릿으로 변환
  const csvRowToTemplate = (row: string[]): any => {
    if (row.length < CSV_HEADERS.length) {
      throw new Error(`CSV row has insufficient columns. Required: ${CSV_HEADERS.length}, Actual: ${row.length}`);
    }

    return {
      name: row[0]?.trim(),
      description: row[1]?.trim(),
      category: row[2]?.trim() || 'custom',
      condition_type: row[3]?.trim() || 'threshold',
      severity: row[4]?.trim() || 'medium',
      default_config: {
        threshold: row[5] ? parseFloat(row[5]) : undefined,
        high_limit: row[6] ? parseFloat(row[6]) : undefined,
        low_limit: row[7] ? parseFloat(row[7]) : undefined,
        deadband: row[8] ? parseFloat(row[8]) : undefined
      },
      message_template: row[9]?.trim(),
      applicable_data_types: row[10]?.trim() 
        ? row[10].split(';').map(t => t.trim()).filter(t => t)
        : ['float'],
      notification_enabled: row[11]?.toUpperCase() === 'TRUE',
      auto_clear: row[12]?.toUpperCase() === 'TRUE'
    };
  };

  // CSV 문자열 생성
  const generateCsv = () => {
    const selectedData = templates.filter(t => selectedTemplates.includes(t.id));
    const csvRows = [
      CSV_HEADERS.join(','),
      ...selectedData.map(template => 
        templateToCsvRow(template).map(field => 
          field.includes(',') || field.includes('"') 
            ? `"${field.replace(/"/g, '""')}"` 
            : field
        ).join(',')
      )
    ];
    return csvRows.join('\n');
  };

  // CSV 파일 다운로드
  const handleDownload = () => {
    const csvContent = generateCsv();
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement('a');
    const url = URL.createObjectURL(blob);
    
    link.setAttribute('href', url);
    link.setAttribute('download', `alarm_templates_${new Date().toISOString().split('T')[0]}.csv`);
    link.style.visibility = 'hidden';
    
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    
    URL.revokeObjectURL(url);
    
    if (onExport) {
      onExport(selectedTemplates);
    }
  };

  // 파일 선택
  const handleFileSelect = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
      const content = e.target?.result as string;
      setCsvData(content);
      parseCsvData(content);
    };
    reader.readAsText(file);
  };

  // CSV 데이터 파싱
  const parseCsvData = (csvContent: string) => {
    try {
      const lines = csvContent.split('\n').filter(line => line.trim());
      if (lines.length < 2) {
        throw new Error('CSV file is empty or contains only headers.');
      }

      const headerLine = lines[0];
      const dataLines = lines.slice(1);

      // 헤더 검증
      const headers = headerLine.split(',').map(h => h.trim().replace(/"/g, ''));
      const missingHeaders = CSV_HEADERS.filter(h => !headers.includes(h));
      
      if (missingHeaders.length > 0) {
        throw new Error(`Required headers are missing: ${missingHeaders.join(', ')}`);
      }

      // 데이터 파싱
      const parsedTemplates = [];
      const errors = [];

      for (let i = 0; i < dataLines.length; i++) {
        try {
          const row = dataLines[i].split(',').map(cell => cell.trim().replace(/^"|"$/g, ''));
          const template = csvRowToTemplate(row);
          
          // 기본 유효성 검사
          if (!template.name) {
            errors.push(`Row ${i + 2}: Template name is required`);
            continue;
          }
          
          parsedTemplates.push({
            ...template,
            rowNumber: i + 2
          });
        } catch (error) {
          errors.push(`Row ${i + 2}: ${error instanceof Error ? error.message : 'Parse error'}`);
        }
      }

      setParseResults({
        success: errors.length === 0,
        templates: parsedTemplates,
        errors,
        totalRows: dataLines.length,
        validRows: parsedTemplates.length
      });

    } catch (error) {
      setParseResults({
        success: false,
        templates: [],
        errors: [error instanceof Error ? error.message : 'CSV parse error'],
        totalRows: 0,
        validRows: 0
      });
    }
  };

  // 템플릿 가져오기 실행
  const handleImport = async () => {
    if (!parseResults?.templates || parseResults.templates.length === 0) {
      alert('No templates to import.');
      return;
    }

    try {
      if (onImport) {
        await onImport(parseResults.templates);
        setImportResults({
          success: true,
          imported: parseResults.templates.length,
          message: `${parseResults.templates.length} template(s) imported successfully.`
        });
      }
    } catch (error) {
      setImportResults({
        success: false,
        imported: 0,
        message: error instanceof Error ? error.message : 'Import failed'
      });
    }
  };

  // 전체 선택/해제
  const handleSelectAll = (checked: boolean) => {
    setSelectedTemplates(checked ? templates.map(t => t.id) : []);
  };

  // Export 모드 렌더링
  const renderExportMode = () => (
    <div>
      <div style={{ marginBottom: '20px' }}>
        <h3 style={{ margin: '0 0 12px 0', fontSize: '18px', fontWeight: '600' }}>
          Select Templates to Export
        </h3>
        <p style={{ margin: '0 0 16px 0', fontSize: '14px', color: '#6b7280' }}>
          Export as CSV format editable in Excel.
        </p>
        
        <div style={{ marginBottom: '16px' }}>
          <label style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <input
              type="checkbox"
              checked={selectedTemplates.length === templates.length && templates.length > 0}
              onChange={(e) => handleSelectAll(e.target.checked)}
            />
            <span style={{ fontSize: '14px', fontWeight: '500' }}>{t('labels.selectAllTemplates', {ns: 'deviceTemplates'})}</span>
          </label>
        </div>
      </div>

      <div style={{ 
        maxHeight: '300px', 
        overflowY: 'auto', 
        border: '1px solid #e5e7eb', 
        borderRadius: '8px' 
      }}>
        {templates.map(template => (
          <div 
            key={template.id} 
            style={{ 
              padding: '12px 16px', 
              borderBottom: '1px solid #f1f5f9',
              display: 'flex',
              alignItems: 'center',
              gap: '12px'
            }}
          >
            <input
              type="checkbox"
              checked={selectedTemplates.includes(template.id)}
              onChange={(e) => {
                if (e.target.checked) {
                  setSelectedTemplates([...selectedTemplates, template.id]);
                } else {
                  setSelectedTemplates(selectedTemplates.filter(id => id !== template.id));
                }
              }}
            />
            <div style={{ flex: 1 }}>
              <div style={{ fontWeight: '500', fontSize: '14px' }}>{template.name}</div>
              <div style={{ fontSize: '12px', color: '#6b7280' }}>
                {template.category} • {template.condition_type} • {template.severity}
              </div>
            </div>
          </div>
        ))}
      </div>

      {selectedTemplates.length > 0 && (
        <div style={{ 
          marginTop: '16px', 
          padding: '12px', 
          background: '#eff6ff', 
          borderRadius: '8px',
          fontSize: '14px',
          color: '#1e40af'
        }}>
          {selectedTemplates.length} template(s) selected.
        </div>
      )}
    </div>
  );

  // Import 모드 렌더링
  const renderImportMode = () => (
    <div>
      <div style={{ marginBottom: '20px' }}>
        <h3 style={{ margin: '0 0 12px 0', fontSize: '18px', fontWeight: '600' }}>
          Import Templates from CSV File
        </h3>
        <p style={{ margin: '0 0 16px 0', fontSize: '14px', color: '#6b7280' }}>
          Upload the CSV file generated from export.
        </p>
      </div>

      {/* 파일 업로드 */}
      <div style={{ marginBottom: '20px' }}>
        <input
          ref={fileInputRef}
          type="file"
          accept=".csv"
          onChange={handleFileSelect}
          style={{ display: 'none' }}
        />
        <button
          onClick={() => fileInputRef.current?.click()}
          style={{
            padding: '12px 24px',
            border: '2px dashed #3b82f6',
            borderRadius: '8px',
            background: '#eff6ff',
            color: '#3b82f6',
            cursor: 'pointer',
            fontSize: '14px',
            fontWeight: '500',
            width: '100%'
          }}
        >
          Select CSV File
        </button>
      </div>

      {/* 파싱 결과 */}
      {parseResults && (
        <div style={{ marginBottom: '20px' }}>
          <div style={{
            padding: '16px',
            borderRadius: '8px',
            border: `1px solid ${parseResults.success ? '#22c55e' : '#ef4444'}`,
            background: parseResults.success ? '#dcfce7' : '#fee2e2',
            color: parseResults.success ? '#166534' : '#dc2626'
          }}>
            <div style={{ fontWeight: '600', marginBottom: '8px' }}>
              {parseResults.success ? 'Parse Success' : 'Parse Error'}
            </div>
            <div style={{ fontSize: '14px' }}>
              {parseResults.validRows} of {parseResults.totalRows} row(s) valid
            </div>
            
            {parseResults.errors.length > 0 && (
              <div style={{ marginTop: '12px' }}>
                <div style={{ fontWeight: '500', marginBottom: '4px' }}>{t('labels.errorList', {ns: 'deviceTemplates'})}</div>
                <ul style={{ margin: 0, paddingLeft: '20px' }}>
                  {parseResults.errors.slice(0, 5).map((error, idx) => (
                    <li key={idx} style={{ fontSize: '12px' }}>{error}</li>
                  ))}
                  {parseResults.errors.length > 5 && (
                    <li style={{ fontSize: '12px', color: '#9ca3af' }}>
                      ... and {parseResults.errors.length - 5} more
                    </li>
                  )}
                </ul>
              </div>
            )}
          </div>
        </div>
      )}

      {/* 가져오기 결과 */}
      {importResults && (
        <div style={{
          padding: '16px',
          borderRadius: '8px',
          border: `1px solid ${importResults.success ? '#22c55e' : '#ef4444'}`,
          background: importResults.success ? '#dcfce7' : '#fee2e2',
          color: importResults.success ? '#166534' : '#dc2626',
          marginBottom: '20px'
        }}>
          <div style={{ fontWeight: '600' }}>{importResults.message}</div>
        </div>
      )}

      {/* CSV Format Guide */}
      <div style={{
        padding: '16px',
        background: '#f8fafc',
        borderRadius: '8px',
        border: '1px solid #e5e7eb'
      }}>
        <div style={{ fontWeight: '500', marginBottom: '8px', fontSize: '14px' }}>
          CSV Format Guide
        </div>
        <div style={{ fontSize: '12px', color: '#6b7280', lineHeight: '1.5' }}>
          • First row must be the header<br/>
          • Required columns: {CSV_HEADERS.slice(0, 5).join(', ')}<br/>
          • Boolean values: TRUE or FALSE<br/>
          • Separate applicable data types with semicolons (;)
        </div>
      </div>
    </div>
  );

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.5)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 9999,
        padding: '20px'
      }}
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div
        style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          boxShadow: '0 20px 25px -5px rgba(0, 0, 0, 0.1)',
          width: '100%',
          maxWidth: '600px',
          maxHeight: '90vh',
          display: 'flex',
          flexDirection: 'column'
        }}
      >
        {/* 헤더 */}
        <div style={{ padding: '24px', borderBottom: '1px solid #e5e7eb' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <h2 style={{ fontSize: '24px', fontWeight: '700', margin: 0, color: '#111827' }}>
              Template {mode === 'export' ? 'Export' : 'Import'}
            </h2>
            <button
              onClick={onClose}
              style={{
                background: 'none',
                border: 'none',
                fontSize: '24px',
                cursor: 'pointer',
                color: '#6b7280',
                padding: '4px'
              }}
            >
              ×
            </button>
          </div>
        </div>

        {/* 컨텐츠 */}
        <div style={{ flex: 1, overflowY: 'auto', padding: '24px' }}>
          {mode === 'export' ? renderExportMode() : renderImportMode()}
        </div>

        {/* 푸터 */}
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          padding: '24px',
          borderTop: '1px solid #e5e7eb',
          background: '#f8fafc'
        }}>
          <div>
            {mode === 'export' && selectedTemplates.length > 0 && (
              <span style={{ fontSize: '14px', color: '#6b7280' }}>
                {selectedTemplates.length} template(s) selected
              </span>
            )}
          </div>

          <div style={{ display: 'flex', gap: '12px' }}>
            <button
              onClick={onClose}
              disabled={loading}
              style={{
                padding: '12px 24px',
                border: '1px solid #d1d5db',
                borderRadius: '8px',
                background: '#ffffff',
                color: '#374151',
                cursor: loading ? 'not-allowed' : 'pointer',
                fontSize: '14px',
                fontWeight: '500',
                opacity: loading ? 0.6 : 1
              }}
            >
              Cancel
            </button>

            {mode === 'export' ? (
              <button
                onClick={handleDownload}
                disabled={selectedTemplates.length === 0 || loading}
                style={{
                  padding: '12px 24px',
                  border: 'none',
                  borderRadius: '8px',
                  background: selectedTemplates.length === 0 || loading ? '#9ca3af' : '#3b82f6',
                  color: 'white',
                  cursor: selectedTemplates.length === 0 || loading ? 'not-allowed' : 'pointer',
                  fontSize: '14px',
                  fontWeight: '500'
                }}
              >
                CSV Download
              </button>
            ) : (
              <button
                onClick={handleImport}
                disabled={!parseResults?.success || loading}
                style={{
                  padding: '12px 24px',
                  border: 'none',
                  borderRadius: '8px',
                  background: !parseResults?.success || loading ? '#9ca3af' : '#10b981',
                  color: 'white',
                  cursor: !parseResults?.success || loading ? 'not-allowed' : 'pointer',
                  fontSize: '14px',
                  fontWeight: '500'
                }}
              >
                {loading ? 'Importing...' : 'Import Template'}
              </button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default TemplateImportExportModal;