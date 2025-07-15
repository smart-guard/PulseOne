// lib/connection/rpc.js
const jayson = require('jayson');
const config = require('../../config');

/**
 * RPCClient 클래스는 JSON-RPC 서버와 통신하기 위한 클라이언트를 제공합니다.
 * 기본 설정은 환경변수 및 config 파일을 기반으로 하며, 필요 시 사용자 정의 설정도 지원합니다.
 */
class RPCClient {
  constructor(customConfig = {}) {
    const rpc = {
      host: customConfig.host || config.rpc?.host || process.env.RPC_HOST || 'localhost',
      port: parseInt(customConfig.port || config.rpc?.port || process.env.RPC_PORT || '4000', 10)
    };

    if (!rpc.host || !rpc.port) {
      throw new Error('RPC 설정이 누락되었습니다');
    }

    this.client = jayson.Client.http({ host: rpc.host, port: rpc.port });
  }

  /**
   * JSON-RPC 메서드 호출
   * @param {string} method - 호출할 메서드 이름
   * @param {Array|Object} params - 전달할 파라미터
   * @returns {Promise<any>} - 호출 결과를 반환
   */
  call(method, params = []) {
    return new Promise((resolve, reject) => {
      this.client.request(method, params, (err, error, result) => {
        if (err) return reject(err);
        if (error) return reject(error);
        resolve(result);
      });
    });
  }
}

// 기본 인스턴스 (config 설정 기반)
const defaultRPCClient = new RPCClient();

// 내보내기
module.exports = defaultRPCClient;

// 추가 인스턴스를 원할 경우 이 함수로 생성 가능
module.exports.createClient = (customConfig = {}) => new RPCClient(customConfig);
