// =============================================================================
// frontend/src/utils/ErrorHandler.js
// PulseOne HTTP 상태코드별 세분화된 에러 처리
// =============================================================================

/**
 * @brief HTTP 상태코드별 에러 분류 및 처리
 * @details 엔지니어가 즉시 문제를 파악할 수 있도록 세분화
 */
class PulseOneErrorHandler {

    /**
     * @brief KST 타임스탬프 생성 (UTC+9)
     */
    static getKSTTimestamp() {
        const now = new Date();
        const kstOffset = 9 * 60 * 60 * 1000;
        const kstDate = new Date(now.getTime() + kstOffset);
        return kstDate.toISOString().replace('T', ' ').replace('Z', '');
    }


    /**
     * @brief HTTP 상태코드별 에러 카테고리 분류
     */
    static getErrorCategory(httpStatus) {
        // 연결 관련 (420-429)
        if (httpStatus >= 420 && httpStatus <= 429) return 'connection';

        // 프로토콜 관련 (430-449)  
        if (httpStatus >= 430 && httpStatus <= 449) return 'protocol';

        // 데이터 관련 (450-469)
        if (httpStatus >= 450 && httpStatus <= 469) return 'data';

        // 디바이스 관련 (470-489)
        if (httpStatus >= 470 && httpStatus <= 489) return 'device';

        // 설정 관련 (490-499)
        if (httpStatus >= 490 && httpStatus <= 499) return 'configuration';

        // 점검 관련 (510-519)
        if (httpStatus >= 510 && httpStatus <= 519) return 'maintenance';

        // 시스템 관련 (520-529)
        if (httpStatus >= 520 && httpStatus <= 529) return 'system';

        // 프로토콜별 (530+)
        if (httpStatus >= 530 && httpStatus <= 539) return 'modbus';
        if (httpStatus >= 540 && httpStatus <= 549) return 'mqtt';
        if (httpStatus >= 550 && httpStatus <= 559) return 'bacnet';

        return 'unknown';
    }

    /**
     * @brief HTTP 상태코드별 상세 에러 정보
     */
    static getErrorDetails(httpStatus) {
        const errorMap = {
            // ===== 연결 관련 (420-429) =====
            420: {
                type: 'connection_failed',
                severity: 'high',
                icon: '🔌',
                color: '#FF6B6B',
                autoRetry: true,
                userAction: false,
                hint: '네트워크 상태 확인',
                techDetail: 'TCP 연결 실패'
            },
            421: {
                type: 'connection_timeout',
                severity: 'medium',
                icon: '⏱️',
                color: '#FFA726',
                autoRetry: true,
                userAction: false,
                hint: '응답 대기 중',
                techDetail: '연결 타임아웃'
            },
            422: {
                type: 'connection_refused',
                severity: 'high',
                icon: '🚫',
                color: '#FF6B6B',
                autoRetry: false,
                userAction: true,
                hint: '디바이스 설정 확인',
                techDetail: '연결 거부됨'
            },
            423: {
                type: 'connection_lost',
                severity: 'medium',
                icon: '📡',
                color: '#FFA726',
                autoRetry: true,
                userAction: false,
                hint: '자동 재연결 시도 중',
                techDetail: '연결 끊어짐'
            },
            424: {
                type: 'authentication_failed',
                severity: 'high',
                icon: '🔐',
                color: '#FF6B6B',
                autoRetry: false,
                userAction: true,
                hint: '인증 정보 확인',
                techDetail: '인증 실패'
            },

            // ===== 프로토콜 관련 (430-449) =====
            430: {
                type: 'timeout',
                severity: 'medium',
                icon: '⏰',
                color: '#FFA726',
                autoRetry: true,
                userAction: false,
                hint: '응답 대기 중',
                techDetail: '일반 타임아웃'
            },
            431: {
                type: 'protocol_error',
                severity: 'medium',
                icon: '📋',
                color: '#FFA726',
                autoRetry: true,
                userAction: false,
                hint: '프로토콜 설정 확인',
                techDetail: '프로토콜 에러'
            },
            434: {
                type: 'checksum_error',
                severity: 'medium',
                icon: '🔍',
                color: '#FFA726',
                autoRetry: true,
                userAction: false,
                hint: '네트워크 품질 확인',
                techDetail: '체크섬 에러'
            },
            435: {
                type: 'frame_error',
                severity: 'medium',
                icon: '📦',
                color: '#FFA726',
                autoRetry: true,
                userAction: true,
                hint: '통신 케이블 점검',
                techDetail: '프레임 에러'
            },

            // ===== 데이터 관련 (450-469) =====
            450: {
                type: 'invalid_data',
                severity: 'medium',
                icon: '📊',
                color: '#FFA726',
                autoRetry: false,
                userAction: true,
                hint: '입력값 확인',
                techDetail: '잘못된 데이터'
            },
            451: {
                type: 'data_type_mismatch',
                severity: 'medium',
                icon: '🔢',
                color: '#FFA726',
                autoRetry: false,
                userAction: true,
                hint: '데이터 타입 확인',
                techDetail: '데이터 타입 불일치'
            },
            452: {
                type: 'data_out_of_range',
                severity: 'medium',
                icon: '📏',
                color: '#FFA726',
                autoRetry: false,
                userAction: true,
                hint: '값 범위 확인',
                techDetail: '데이터 범위 초과'
            },
            453: {
                type: 'data_format_error',
                severity: 'medium',
                icon: '📝',
                color: '#FFA726',
                autoRetry: false,
                userAction: true,
                hint: '데이터 포맷 확인',
                techDetail: '데이터 포맷 에러'
            },
            454: {
                type: 'data_stale',
                severity: 'low',
                icon: '🕰️',
                color: '#66BB6A',
                autoRetry: true,
                userAction: false,
                hint: '데이터 업데이트 대기',
                techDetail: '오래된 데이터'
            },

            // ===== 디바이스 관련 (470-489) =====
            470: {
                type: 'device_not_responding',
                severity: 'high',
                icon: '📵',
                color: '#FF6B6B',
                autoRetry: true,
                userAction: true,
                hint: '디바이스 전원 확인',
                techDetail: '디바이스 응답 없음'
            },
            471: {
                type: 'device_busy',
                severity: 'medium',
                icon: '⏳',
                color: '#FFA726',
                autoRetry: true,
                userAction: false,
                hint: '작업 완료까지 대기',
                techDetail: '디바이스 사용 중'
            },
            472: {
                type: 'device_error',
                severity: 'high',
                icon: '⚠️',
                color: '#FF6B6B',
                autoRetry: false,
                userAction: true,
                hint: '하드웨어 점검 필요',
                techDetail: '디바이스 하드웨어 에러'
            },
            473: {
                type: 'device_not_found',
                severity: 'high',
                icon: '🔍',
                color: '#FF6B6B',
                autoRetry: false,
                userAction: true,
                hint: '디바이스 설정 확인',
                techDetail: '디바이스 찾을 수 없음'
            },
            474: {
                type: 'device_offline',
                severity: 'high',
                icon: '📴',
                color: '#FF6B6B',
                autoRetry: true,
                userAction: true,
                hint: '디바이스 전원 및 연결 확인',
                techDetail: '디바이스 오프라인'
            },

            // ===== 점검 관련 (510-519) =====
            510: {
                type: 'maintenance_active',
                severity: 'low',
                icon: '🔧',
                color: '#66BB6A',
                autoRetry: true,
                userAction: false,
                hint: '점검 완료까지 대기',
                techDetail: '점검 모드 활성화'
            },
            513: {
                type: 'remote_control_blocked',
                severity: 'medium',
                icon: '🔒',
                color: '#FFA726',
                autoRetry: false,
                userAction: true,
                hint: '현장 제어 모드 해제 필요',
                techDetail: '원격 제어 차단됨'
            },
            514: {
                type: 'insufficient_permission',
                severity: 'medium',
                icon: '🔑',
                color: '#FFA726',
                autoRetry: false,
                userAction: true,
                hint: '관리자 권한 요청',
                techDetail: '권한 부족'
            },

            // ===== 프로토콜별 (530+) =====
            530: {
                type: 'modbus_exception',
                severity: 'medium',
                icon: '🔗',
                color: '#FFA726',
                autoRetry: true,
                userAction: false,
                hint: 'Modbus 설정 확인',
                techDetail: 'Modbus 프로토콜 예외'
            },
            540: {
                type: 'mqtt_publish_failed',
                severity: 'medium',
                icon: '📨',
                color: '#FFA726',
                autoRetry: true,
                userAction: false,
                hint: 'MQTT 브로커 상태 확인',
                techDetail: 'MQTT 메시지 발행 실패'
            },
            550: {
                type: 'bacnet_service_error',
                severity: 'medium',
                icon: '🏢',
                color: '#FFA726',
                autoRetry: true,
                userAction: false,
                hint: 'BACnet 서비스 설정 확인',
                techDetail: 'BACnet 서비스 에러'
            }
        };

        return errorMap[httpStatus] || {
            type: 'unknown_error',
            severity: 'medium',
            icon: '❓',
            color: '#9E9E9E',
            autoRetry: false,
            userAction: true,
            hint: '시스템 관리자 문의',
            techDetail: `알 수 없는 HTTP 상태코드: ${httpStatus}`
        };
    }

    /**
     * @brief API 응답 에러 처리 메인 함수
     */
    static handleApiError(response, deviceId = null) {
        const httpStatus = response.status;
        const category = this.getErrorCategory(httpStatus);
        const details = this.getErrorDetails(httpStatus);

        // 에러 정보 구성
        const errorInfo = {
            httpStatus,
            category,
            deviceId,
            timestamp: this.getKSTTimestamp(),
            ...details
        };

        // 응답 body에 추가 정보가 있으면 포함
        if (response.data && response.data.error_code) {
            errorInfo.backendErrorCode = response.data.error_code;
        }
        if (response.data && response.data.tech_details) {
            errorInfo.techDetail = response.data.tech_details;
        }

        // 에러별 처리 로직
        this.processError(errorInfo);

        return errorInfo;
    }

    /**
     * @brief 에러별 처리 로직 실행
     */
    static processError(errorInfo) {
        const { httpStatus, category, type, autoRetry, severity } = errorInfo;

        // 1. 로그 기록 (심각도별)
        this.logError(errorInfo);

        // 2. 자동 재시도 처리
        if (autoRetry) {
            this.scheduleRetry(errorInfo);
        }

        // 3. 사용자 알림 (심각도별)
        this.notifyUser(errorInfo);

        // 4. 통계 수집
        this.recordErrorStatistics(category, type, httpStatus);

        // 5. 카테고리별 특수 처리
        this.handleCategorySpecificActions(errorInfo);
    }

    /**
     * @brief 카테고리별 특수 처리
     */
    static handleCategorySpecificActions(errorInfo) {
        const { category, httpStatus, deviceId } = errorInfo;

        switch (category) {
            case 'connection':
                // 연결 문제 → 상태 표시등 빨간색
                this.updateConnectionIndicator(deviceId, 'disconnected');
                if (errorInfo.autoRetry) {
                    this.startConnectionRetryIndicator(deviceId);
                }
                break;

            case 'device':
                // 디바이스 문제 → 디바이스 상태 업데이트
                this.updateDeviceStatus(deviceId, 'error');
                if (httpStatus === 472) { // DEVICE_ERROR
                    this.triggerHardwareDiagnostics(deviceId);
                }
                break;

            case 'maintenance':
                // 점검 모드 → UI 점검 모드 표시
                this.showMaintenanceMode(deviceId);
                if (httpStatus === 510) { // MAINTENANCE_ACTIVE
                    this.estimateMaintenanceCompletion(deviceId);
                }
                break;

            case 'protocol':
                // 프로토콜 문제 → 통신 설정 점검 제안
                this.suggestProtocolCheck(deviceId, httpStatus);
                break;

            case 'modbus':
                // Modbus 전용 → Modbus 진단 도구 링크
                this.showModbusDiagnostics(deviceId);
                break;

            case 'mqtt':
                // MQTT 전용 → 브로커 상태 확인
                this.checkMqttBrokerStatus();
                break;

            case 'bacnet':
                // BACnet 전용 → BACnet 객체 탐색 제안
                this.suggestBacnetObjectScan(deviceId);
                break;
        }
    }

    /**
     * @brief 사용자 알림 (심각도별 차별화)
     */
    static notifyUser(errorInfo) {
        const { severity, icon, color, hint, userAction, httpStatus, deviceId } = errorInfo;

        const notification = {
            title: `${icon} ${this.getErrorTitle(httpStatus)}`,
            message: this.getUserMessage(httpStatus, deviceId),
            color: color,
            duration: this.getNotificationDuration(severity),
            actions: userAction ? this.getUserActions(httpStatus) : []
        };

        // 심각도별 알림 방식
        switch (severity) {
            case 'critical':
                this.showCriticalAlert(notification);
                this.playAlarmSound();
                break;
            case 'high':
                this.showHighPriorityNotification(notification);
                break;
            case 'medium':
                this.showStandardNotification(notification);
                break;
            case 'low':
                this.showLowPriorityToast(notification);
                break;
        }
    }

    /**
     * @brief HTTP 상태코드별 제목
     */
    static getErrorTitle(httpStatus) {
        const titles = {
            // 연결 관련
            420: '연결 실패',
            421: '연결 타임아웃',
            422: '연결 거부',
            423: '연결 끊어짐',
            424: '인증 실패',

            // 프로토콜 관련
            430: '응답 타임아웃',
            431: '프로토콜 에러',
            434: '체크섬 에러',
            435: '통신 프레임 에러',

            // 데이터 관련
            450: '잘못된 데이터',
            451: '데이터 타입 오류',
            452: '값 범위 초과',
            453: '데이터 포맷 오류',
            454: '오래된 데이터',

            // 디바이스 관련
            470: '디바이스 응답 없음',
            471: '디바이스 사용 중',
            472: '하드웨어 에러',
            473: '디바이스 없음',
            474: '디바이스 오프라인',

            // 설정 관련
            490: '설정 오류',
            491: '설정 누락',
            492: '설정 에러',
            493: '매개변수 오류',

            // 점검 관련
            510: '점검 중',
            513: '원격 제어 차단',
            514: '권한 부족',

            // 프로토콜별
            530: 'Modbus 예외',
            540: 'MQTT 발행 실패',
            550: 'BACnet 서비스 에러'
        };

        return titles[httpStatus] || `에러 ${httpStatus}`;
    }

    /**
     * @brief 사용자 조치 버튼들
     */
    static getUserActions(httpStatus) {
        switch (httpStatus) {
            case 422: // CONNECTION_REFUSED
                return [
                    { text: '설정 확인', action: 'checkDeviceConfig' },
                    { text: '연결 테스트', action: 'testConnection' }
                ];

            case 472: // DEVICE_ERROR
                return [
                    { text: '진단 실행', action: 'runDiagnostics' },
                    { text: '하드웨어 점검', action: 'scheduleHardwareCheck' }
                ];

            case 473: // DEVICE_NOT_FOUND
                return [
                    { text: '디바이스 검색', action: 'scanDevices' },
                    { text: '설정 수정', action: 'editDeviceConfig' }
                ];

            case 513: // REMOTE_CONTROL_BLOCKED
                return [
                    { text: '현장 모드 해제', action: 'disableLocalMode' },
                    { text: '권한 요청', action: 'requestPermission' }
                ];

            case 530: // MODBUS_EXCEPTION
                return [
                    { text: 'Modbus 진단', action: 'runModbusDiagnostics' },
                    { text: '레지스터 맵 확인', action: 'checkRegisterMap' }
                ];

            default:
                return [
                    { text: '재시도', action: 'retry' },
                    { text: '도움말', action: 'showHelp' }
                ];
        }
    }

    /**
     * @brief 실시간 에러 대시보드용 데이터
     */
    static getErrorDashboardData() {
        return {
            // 카테고리별 에러 개수
            categories: {
                connection: this.getErrorCount('connection'),
                device: this.getErrorCount('device'),
                protocol: this.getErrorCount('protocol'),
                maintenance: this.getErrorCount('maintenance'),
                system: this.getErrorCount('system')
            },

            // 최근 에러들 (시간순)
            recentErrors: this.getRecentErrors(10),

            // 심각도별 분포
            severityDistribution: {
                critical: this.getErrorCountBySeverity('critical'),
                high: this.getErrorCountBySeverity('high'),
                medium: this.getErrorCountBySeverity('medium'),
                low: this.getErrorCountBySeverity('low')
            },

            // 디바이스별 에러 현황
            deviceErrors: this.getErrorsByDevice(),

            // 자동 복구 성공률
            autoRecoveryRate: this.getAutoRecoverySuccessRate()
        };
    }

    /**
     * @brief 엔지니어용 상세 진단 정보
     */
    static getEngineerDiagnostics(httpStatus, deviceId) {
        const details = this.getErrorDetails(httpStatus);

        return {
            httpStatus,
            deviceId,
            category: this.getErrorCategory(httpStatus),
            technicalDetails: details.techDetail,
            suggestedActions: this.getEngineerActions(httpStatus),
            relatedLogs: this.getRelatedLogs(deviceId, httpStatus),
            similarIssues: this.findSimilarIssues(httpStatus),
            troubleshootingSteps: this.getTroubleshootingSteps(httpStatus)
        };
    }

    /**
     * @brief 엔지니어용 조치 사항
     */
    static getEngineerActions(httpStatus) {
        const actionMap = {
            420: ['네트워크 연결 확인', '방화벽 설정 점검', 'IP 주소 핑 테스트'],
            421: ['타임아웃 설정 조정', '네트워크 레이턴시 측정', '디바이스 부하 확인'],
            422: ['포트 번호 확인', '서비스 실행 상태 점검', 'iptables 규칙 확인'],
            472: ['하드웨어 진단 실행', '에러 로그 분석', '제조사 기술지원 문의'],
            473: ['디바이스 스캔 실행', 'IP 주소 확인', '설정 파일 검증'],
            530: ['슬레이브 ID 확인', '레지스터 주소 검증', 'Modbus 통신 로그 분석'],
            540: ['MQTT 브로커 연결 상태', 'QoS 설정 확인', '토픽 권한 검증'],
            550: ['BACnet 객체 ID 확인', '속성 접근 권한 검증', 'BACnet 장치 스캔']
        };

        return actionMap[httpStatus] || ['시스템 로그 확인', '설정 재검토', '기술지원 문의'];
    }
}

// =============================================================================
// Vue.js/React 컴포넌트에서 사용할 수 있는 헬퍼 함수들
// =============================================================================

/**
 * @brief API 호출 래퍼 (자동 에러 처리 포함)
 */
export const apiCall = async (url, options = {}, deviceId = null) => {
    try {
        const response = await fetch(url, options);

        // 200번대가 아니면 에러 처리
        if (!response.ok) {
            const errorInfo = PulseOneErrorHandler.handleApiError(response, deviceId);
            throw new PulseOneError(errorInfo);
        }

        return await response.json();

    } catch (error) {
        if (error instanceof PulseOneError) {
            throw error; // 이미 처리된 에러는 그대로 전달
        }

        // 네트워크 에러 등 예외적인 경우
        const fallbackError = {
            httpStatus: 0,
            category: 'network',
            type: 'network_error',
            severity: 'high',
            message: '네트워크 연결을 확인하세요.',
            techDetail: error.message
        };

        PulseOneErrorHandler.processError(fallbackError);
        throw new PulseOneError(fallbackError);
    }
};

/**
 * @brief PulseOne 전용 에러 클래스
 */
class PulseOneError extends Error {
    constructor(errorInfo) {
        super(errorInfo.message || errorInfo.hint);
        this.name = 'PulseOneError';
        this.httpStatus = errorInfo.httpStatus;
        this.category = errorInfo.category;
        this.type = errorInfo.type;
        this.severity = errorInfo.severity;
        this.deviceId = errorInfo.deviceId;
        this.techDetail = errorInfo.techDetail;
        this.recoverable = errorInfo.autoRetry;
        this.userActionable = errorInfo.userAction;
    }
}

// =============================================================================
// 사용 예시
// =============================================================================

/*
// Vue 컴포넌트에서 사용
async startDevice(deviceId) {
    try {
        const result = await apiCall(`/api/devices/${deviceId}/start`, {
            method: 'POST'
        }, deviceId);
        
        this.showSuccess(`디바이스 ${deviceId} 시작됨`);
        return result;
        
    } catch (error) {
        if (error instanceof PulseOneError) {
            // 에러 타입별 처리
            switch (error.httpStatus) {
                case 470: // DEVICE_NOT_RESPONDING
                    this.showDeviceOfflineDialog(deviceId);
                    break;
                case 510: // MAINTENANCE_ACTIVE  
                    this.showMaintenanceWaitDialog(deviceId);
                    break;
                case 513: // REMOTE_CONTROL_BLOCKED
                    this.showPermissionRequestDialog(deviceId);
                    break;
                default:
                    // 기본 에러 처리는 이미 PulseOneErrorHandler에서 완료
                    console.error('Device start failed:', error);
            }
        }
        throw error;
    }
}

// React Hook으로 사용
const useDeviceControl = (deviceId) => {
    const [isLoading, setIsLoading] = useState(false);
    const [error, setError] = useState(null);
    
    const controlDevice = async (action, params = {}) => {
        setIsLoading(true);
        setError(null);
        
        try {
            const result = await apiCall(`/api/devices/${deviceId}/${action}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(params)
            }, deviceId);
            
            return result;
            
        } catch (err) {
            setError(err);
            
            // 카테고리별 특수 처리
            if (err.category === 'device' && err.httpStatus === 472) {
                // 하드웨어 에러 → 자동 진단 제안
                showHardwareDiagnosticsModal(deviceId);
            }
            
            throw err;
        } finally {
            setIsLoading(false);
        }
    };
    
    return { controlDevice, isLoading, error };
};
*/

// =============================================================================
// 에러 통계 및 모니터링
// =============================================================================

class ErrorStatistics {
    static errorCounts = new Map();
    static errorHistory = [];
    static deviceErrorMap = new Map();

    static recordError(errorInfo) {
        const key = `${errorInfo.category}_${errorInfo.httpStatus}`;

        // 에러 카운트 증가
        this.errorCounts.set(key, (this.errorCounts.get(key) || 0) + 1);

        // 에러 히스토리 저장 (최근 1000개만)
        this.errorHistory.unshift({
            ...errorInfo,
            id: Date.now() + Math.random()
        });
        if (this.errorHistory.length > 1000) {
            this.errorHistory = this.errorHistory.slice(0, 1000);
        }

        // 디바이스별 에러 매핑
        if (errorInfo.deviceId) {
            if (!this.deviceErrorMap.has(errorInfo.deviceId)) {
                this.deviceErrorMap.set(errorInfo.deviceId, []);
            }
            this.deviceErrorMap.get(errorInfo.deviceId).unshift(errorInfo);
        }
    }

    static getTopErrors(limit = 10) {
        return Array.from(this.errorCounts.entries())
            .sort((a, b) => b[1] - a[1])
            .slice(0, limit)
            .map(([key, count]) => ({ type: key, count }));
    }

    static getDeviceErrorSummary(deviceId) {
        const deviceErrors = this.deviceErrorMap.get(deviceId) || [];
        const last24h = deviceErrors.filter(e =>
            Date.now() - e.timestamp.getTime() < 24 * 60 * 60 * 1000
        );

        return {
            totalErrors: deviceErrors.length,
            last24hErrors: last24h.length,
            mostCommonError: this.getMostCommonErrorForDevice(deviceId),
            currentStatus: this.getCurrentDeviceStatus(deviceId),
            recommendedAction: this.getRecommendedAction(deviceId)
        };
    }
}

export { PulseOneErrorHandler, PulseOneError, ErrorStatistics, apiCall };