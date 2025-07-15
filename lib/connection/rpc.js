// lib/connection/rpc.js
const jayson = require('jayson');
const config = require('../config');

class RPCClient {
  constructor(customConfig = {}) {
    const rpc = {
      host: customConfig.host || config.rpc?.host || process.env.RPC_HOST || 'localhost',
      port: customConfig.port || config.rpc?.port || process.env.RPC_PORT || 4000
    };

    if (!rpc.host || !rpc.port) {
      throw new Error('RPC 설정이 누락되었습니다');
    }

    this.client = jayson.Client.http({ host: rpc.host, port: rpc.port });
  }

  /**
   * JSON-RPC 메서드 호출
   * @param {string} method - 호출할 메서드 이름
   * @param {Array|Object} params - 파라미터
   * @returns {Promise<any>}
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

module.exports = RPCClient;
