// ============================================================================
// frontend/src/api/unified.ts
// 통합된 API 서비스 (선택사항)
// ============================================================================

import { systemApiService } from '../services/apiService';
import { DeviceApiService } from './services/deviceApi';
import { AlarmApiService } from './services/alarmApi';

/**
 * 모든 API 서비스를 하나로 통합한 클래스
 * 필요에 따라 사용
 */
export class UnifiedApiService {
  // 🔧 시스템 관리 (기존)
  static system = systemApiService;
  
  // 📱 디바이스 관리 (신규)
  static device = DeviceApiService;
  
  // 🚨 알람 관리 (신규)
  static alarm = AlarmApiService;
  
  // 향후 추가될 서비스들
  // static user = UserApiService;
  // static virtualPoint = VirtualPointApiService;
  // static realTime = RealTimeApiService;
}