// ===========================================================================
// backend/lib/connection/rpc.js - ConfigManager 사용하도록 수정
// ===========================================================================
const jayson = require('jayson');
const ConfigManager = require('../config/ConfigManager');

const config = ConfigManager.getInstance();

class RPCClient {
  constructor(customConfig = {}) {
    this.config = {
      host: customConfig.host || config.get('RPC_HOST', 'localhost'),
      port: customConfig.port || config.getNumber('RPC_PORT', 4000),
      timeout: customConfig.timeout || config.getNumber('RPC_TIMEOUT_MS', 5000)
    };

    if (!this.config.host || !this.config.port) {
      throw new Error('RPC 설정이 누락되었습니다');
    }

    this.client = jayson.Client.http({
      host: this.config.host,
      port: this.config.port,
      timeout: this.config.timeout
    });

    console.log(`📋 RPC 클라이언트 설정:
   호스트: ${this.config.host}
   포트: ${this.config.port}
   타임아웃: ${this.config.timeout}ms`);
  }

  /**
   * JSON-RPC 메서드 호출
   * @param {string} method - 호출할 메서드 이름
   * @param {Array|Object} params - 전달할 파라미터
   * @param {Object} options - 추가 옵션 (timeout 등)
   * @returns {Promise<any>} - 호출 결과를 반환
   */
  async call(method, params = [], options = {}) {
    return new Promise((resolve, reject) => {
      const timeout = options.timeout || this.config.timeout;
      
      const timer = setTimeout(() => {
        reject(new Error(`RPC 호출 타임아웃: ${method}`));
      }, timeout);

      this.client.request(method, params, (err, error, result) => {
        clearTimeout(timer);
        
        if (err) {
          console.error(`❌ RPC 네트워크 오류 (${method}):`, err.message);
          return reject(err);
        }
        
        if (error) {
          console.error(`❌ RPC 서버 오류 (${method}):`, error);
          return reject(error);
        }
        
        console.log(`✅ RPC 호출 성공: ${method}`);
        resolve(result);
      });
    });
  }

  /**
   * 배치 RPC 호출
   * @param {Array} requests - 요청 배열 [{method, params}, ...]
   * @returns {Promise<Array>} - 결과 배열
   */
  async batchCall(requests) {
    const batchRequests = requests.map(req => 
      this.client.request(req.method, req.params || [])
    );

    return new Promise((resolve, reject) => {
      this.client.request(batchRequests, (err, results) => {
        if (err) {
          console.error('❌ RPC 배치 호출 실패:', err.message);
          return reject(err);
        }
        
        console.log(`✅ RPC 배치 호출 성공: ${requests.length}개`);
        resolve(results);
      });
    });
  }

  /**
   * RPC 서버 상태 확인
   * @returns {Promise<boolean>} - 연결 상태
   */
  async ping() {
    try {
      await this.call('ping', [], { timeout: 3000 });
      return true;
    } catch (error) {
      console.warn('⚠️ RPC 서버 응답 없음:', error.message);
      return false;
    }
  }

  /**
   * 연결 정보 조회
   */
  getConnectionInfo() {
    return {
      host: this.config.host,
      port: this.config.port,
      timeout: this.config.timeout,
      url: `http://${this.config.host}:${this.config.port}`
    };
  }
}

// 기본 인스턴스 생성
let defaultRPCClient;
try {
  defaultRPCClient = new RPCClient();
  console.log('✅ 기본 RPC 클라이언트 초기화 성공');
} catch (error) {
  console.warn('⚠️ 기본 RPC 클라이언트 초기화 실패:', error.message);
  console.log('⚠️  RPC 없이 계속 진행합니다.');
  defaultRPCClient = null;
}

module.exports = defaultRPCClient;
module.exports.createClient = (customConfig = {}) => new RPCClient(customConfig);
module.exports.RPCClient = RPCClient;