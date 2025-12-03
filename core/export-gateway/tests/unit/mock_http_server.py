#!/usr/bin/env python3
"""
Mock HTTP Server for HttpTargetHandler Testing
테스트용 HTTP 서버 - 다양한 시나리오 지원
"""

from flask import Flask, request, jsonify
import time
import sys
import os

app = Flask(__name__)

# 전역 카운터
request_count = 0
auth_fail_count = 0

@app.route('/health', methods=['GET'])
def health():
    """헬스체크 엔드포인트"""
    return jsonify({"status": "ok", "service": "mock-http-server"}), 200

@app.route('/api/alarm', methods=['POST'])
def post_alarm():
    """정상 POST 요청"""
    global request_count
    request_count += 1
    
    data = request.json
    print(f"📬 Received alarm: Building {data.get('bd')}, Point {data.get('nm')}")
    
    return jsonify({
        "success": True,
        "message": "Alarm received",
        "request_id": f"req_{request_count}",
        "received_data": data
    }), 200

@app.route('/api/alarm', methods=['GET'])
def get_alarm():
    """GET 요청"""
    return jsonify({"method": "GET", "success": True}), 200

@app.route('/api/alarm', methods=['PUT'])
def put_alarm():
    """PUT 요청"""
    data = request.json
    return jsonify({"method": "PUT", "success": True, "data": data}), 200

@app.route('/auth/basic', methods=['POST'])
def basic_auth():
    """Basic Auth 테스트"""
    auth = request.authorization
    if auth and auth.username == 'testuser' and auth.password == 'testpass':
        return jsonify({"success": True, "message": "Authenticated"}), 200
    else:
        global auth_fail_count
        auth_fail_count += 1
        return jsonify({"success": False, "message": "Unauthorized"}), 401

@app.route('/auth/bearer', methods=['POST'])
def bearer_auth():
    """Bearer Token Auth 테스트"""
    auth_header = request.headers.get('Authorization', '')
    if auth_header == 'Bearer test-token-12345':
        return jsonify({"success": True, "message": "Token valid"}), 200
    else:
        return jsonify({"success": False, "message": "Invalid token"}), 401

@app.route('/auth/apikey', methods=['POST'])
def apikey_auth():
    """API Key 테스트"""
    api_key = request.headers.get('X-API-Key', '')
    if api_key == 'secret-api-key-xyz':
        return jsonify({"success": True, "message": "API key valid"}), 200
    else:
        return jsonify({"success": False, "message": "Invalid API key"}), 403

@app.route('/headers/echo', methods=['POST'])
def echo_headers():
    """커스텀 헤더 에코"""
    headers_dict = dict(request.headers)
    return jsonify({
        "success": True,
        "headers": headers_dict,
        "custom_header": request.headers.get('X-Custom-Header', 'not_found')
    }), 200

@app.route('/delay/<int:seconds>', methods=['POST'])
def delayed_response(seconds):
    """지연된 응답 (타임아웃 테스트용)"""
    print(f"⏳ Delaying response for {seconds} seconds...")
    time.sleep(seconds)
    return jsonify({"message": f"Delayed {seconds}s"}), 200

@app.route('/retry/fail/<int:times>', methods=['POST'])
def retry_fail(times):
    """재시도 테스트 - 처음 N번은 실패"""
    global request_count
    
    if request_count < times:
        print(f"❌ Retry test: Failing {request_count}/{times}")
        return jsonify({"error": "Temporary failure"}), 500
    else:
        print(f"✅ Retry test: Success after {times} attempts")
        return jsonify({"success": True, "message": "Success after retries"}), 200

@app.route('/status/<int:code>', methods=['POST', 'GET'])
def status_code(code):
    """특정 HTTP 상태 코드 반환"""
    messages = {
        200: "OK",
        201: "Created",
        400: "Bad Request",
        401: "Unauthorized",
        403: "Forbidden",
        404: "Not Found",
        500: "Internal Server Error",
        502: "Bad Gateway",
        503: "Service Unavailable"
    }
    return jsonify({"status": code, "message": messages.get(code, "Unknown")}), code

@app.route('/large', methods=['POST'])
def large_response():
    """대용량 응답"""
    data = request.json
    large_data = {
        "success": True,
        "received_size": len(str(data)),
        "large_payload": "X" * 10240  # 10KB
    }
    return jsonify(large_data), 200

@app.route('/stats', methods=['GET'])
def get_stats():
    """서버 통계"""
    global request_count, auth_fail_count
    return jsonify({
        "total_requests": request_count,
        "auth_failures": auth_fail_count,
        "uptime": "N/A"
    }), 200

@app.route('/reset', methods=['POST'])
def reset_stats():
    """통계 초기화"""
    global request_count, auth_fail_count
    request_count = 0
    auth_fail_count = 0
    return jsonify({"message": "Stats reset"}), 200

if __name__ == '__main__':
    port = int(os.environ.get('PORT', 8765))
    print("=" * 60)
    print(f"🚀 Mock HTTP Server Starting on port {port}")
    print("=" * 60)
    print(f"   Health Check: http://localhost:{port}/health")
    print(f"   Alarm API:    http://localhost:{port}/api/alarm")
    print(f"   Auth Tests:   http://localhost:{port}/auth/*")
    print(f"   Stats:        http://localhost:{port}/stats")
    print("=" * 60)
    
    # 프로덕션 모드 비활성화 (테스트용)
    app.run(host='0.0.0.0', port=port, debug=False, threaded=True)