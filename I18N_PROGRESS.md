# 🌍 PulseOne Frontend i18n 한국어 로컬라이제이션 진행 현황

> 최종 업데이트: 2026-03-02 — **전체 완료**

---

## ✅ 전체 완료

### DeviceModal 컴포넌트 (7개)
| 파일 | 주요 수정 |
|------|----------|
| `DataPointModal.tsx` | 모달 헤더/섹션/라벨/버튼/유효성 메시지 전체 |
| `DeviceSettingsTab.tsx` | mode tag, 정보/경고 박스 |
| `DeviceDataPointsTab.tsx` | 필터/컬럼 헤더/빈 상태/삭제 confirm |
| `DeviceLogsTab.tsx` | 실시간 토글/빈 상태/힌트 |
| `DeviceRtuMonitorTab.tsx` | RTU 안내/탭 버튼/통계 버튼 |
| `DeviceRtuScanDialog.tsx` | 스캔 제목/진행 통계/결과 헤더/버튼 전체 |
| `DeviceBasicInfoTab.tsx` | 검토 완료 (이미 처리됨) |

### VirtualPoints 컴포넌트 (4개)
| 파일 | 주요 수정 |
|------|----------|
| `VirtualPointDetailModal.tsx` | 탭/상태배너/카드/버튼 전체 |
| `ScopeSelector.tsx` | Scope 옵션/placeholder |
| `BasicInfoForm.tsx` | 가이드/템플릿 섹션 |
| `ValidationPanel.tsx` | 최종 검토 UI/시뮬레이션 샌드박스 |

### Explorer 컴포넌트 (3개)
| 파일 | 주요 수정 |
|------|----------|
| `RealtimeDataTable.tsx` | 테이블 헤더 6개/빈 상태 |
| `DatabaseExplorer/index.tsx` | 제목/설명/태그/빈 상태 |
| `DatabaseExplorer/DataTable.tsx` | Actions/버튼/메시지/모달 전체 |

### 전수조사 완료 — 영어 없음 확인 (10개+)
`LoginHistory`, `HistoricalData`, `RealTimeMonitor`, `ControlSchedulePage`, `TenantDetailModal`, `ProtocolDashboard`(이미 t() 완전 적용), `FormulaEditor`, `InputVariableSourceSelector`, `master-model/ProtocolConfigStep`, `master-model/BasicInfoStep`, `RedisManager`

### JSON 번역 파일
| 파일 | 내용 |
|------|------|
| `devices.json` | `dpTabView` 섹션 추가, 기존 섹션 중복 정리 |
| `virtualPoints.json` | `validation`/`detail` 키 병합, 중복 섹션 제거 |
| `common.json` | `realtimeData`/`dbExplorer` 등 신규 키 추가, 중복 제거 |

---

## 📊 전체 완료율: **100%**

## ⚠️ 기술 부채 (기존)
- `devices.json` 최상위 `"status"` 키 중복 (기존 문제, 기능 영향 없음)
