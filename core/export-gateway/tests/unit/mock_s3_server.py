#!/usr/bin/env python3
"""
Mock S3 Server for S3TargetHandler Testing
Flask 기반 간단한 S3 호환 서버
포트 9000 (MinIO 기본 포트 사용)
"""

from flask import Flask, request, Response, jsonify
import hashlib
import time
from datetime import datetime
from collections import defaultdict

app = Flask(__name__)

# In-memory 저장소
storage = {}  # key -> {'content': bytes, 'metadata': dict, 'etag': str, 'timestamp': float}
buckets = set()  # 존재하는 버킷들

# 통계
stats = {
    'total_uploads': 0,
    'total_gets': 0,
    'total_heads': 0,
    'bucket_creates': 0
}

def calculate_etag(content):
    """ETag 계산 (MD5 해시)"""
    return hashlib.md5(content).hexdigest()

def get_timestamp():
    """현재 타임스탬프 (ISO 8601)"""
    return datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%S.%fZ')

@app.route('/')
def index():
    """루트 - 서버 정보"""
    return jsonify({
        'server': 'Mock S3 Server',
        'version': '1.0',
        'stats': stats,
        'buckets': list(buckets),
        'objects': len(storage)
    })

@app.route('/<bucket_name>', methods=['HEAD', 'PUT'])
def handle_bucket(bucket_name):
    """버킷 존재 확인 및 생성"""
    if request.method == 'HEAD':
        # 버킷 존재 확인
        if bucket_name in buckets:
            return Response(status=200)
        else:
            return Response(status=404)
    
    elif request.method == 'PUT':
        # 버킷 생성
        buckets.add(bucket_name)
        stats['bucket_creates'] += 1
        print(f"✅ Bucket created: {bucket_name}")
        return Response(status=200)

@app.route('/<bucket_name>/<path:object_key>', methods=['PUT', 'GET', 'HEAD', 'DELETE'])
def handle_object(bucket_name, object_key):
    """S3 객체 처리"""
    
    full_key = f"{bucket_name}/{object_key}"
    
    if request.method == 'PUT':
        # 객체 업로드
        content = request.get_data()
        
        # 버킷 자동 생성
        if bucket_name not in buckets:
            buckets.add(bucket_name)
        
        # ETag 계산
        etag = calculate_etag(content)
        
        # 메타데이터 추출 (x-amz-meta-* 헤더)
        metadata = {}
        for key, value in request.headers.items():
            if key.lower().startswith('x-amz-meta-'):
                meta_key = key[11:]  # "x-amz-meta-" 제거
                metadata[meta_key] = value
        
        # 저장
        storage[full_key] = {
            'content': content,
            'metadata': metadata,
            'etag': etag,
            'timestamp': time.time(),
            'content_type': request.headers.get('Content-Type', 'application/octet-stream')
        }
        
        stats['total_uploads'] += 1
        
        print(f"📥 PUT: {full_key} ({len(content)} bytes, ETag: {etag})")
        
        # 응답
        response = Response(status=200)
        response.headers['ETag'] = f'"{etag}"'
        response.headers['x-amz-request-id'] = f"mock-{int(time.time())}"
        return response
    
    elif request.method == 'HEAD':
        # 객체 메타데이터 조회
        stats['total_heads'] += 1
        
        if full_key in storage:
            obj = storage[full_key]
            response = Response(status=200)
            response.headers['ETag'] = f'"{obj["etag"]}"'
            response.headers['Content-Length'] = str(len(obj['content']))
            response.headers['Content-Type'] = obj['content_type']
            response.headers['Last-Modified'] = datetime.fromtimestamp(obj['timestamp']).strftime('%a, %d %b %Y %H:%M:%S GMT')
            
            # 커스텀 메타데이터
            for key, value in obj['metadata'].items():
                response.headers[f'x-amz-meta-{key}'] = value
            
            print(f"📋 HEAD: {full_key} (found)")
            return response
        else:
            print(f"❌ HEAD: {full_key} (not found)")
            return Response(status=404)
    
    elif request.method == 'GET':
        # 객체 다운로드
        stats['total_gets'] += 1
        
        if full_key in storage:
            obj = storage[full_key]
            response = Response(obj['content'], status=200)
            response.headers['ETag'] = f'"{obj["etag"]}"'
            response.headers['Content-Type'] = obj['content_type']
            response.headers['Last-Modified'] = datetime.fromtimestamp(obj['timestamp']).strftime('%a, %d %b %Y %H:%M:%S GMT')
            
            print(f"📤 GET: {full_key} ({len(obj['content'])} bytes)")
            return response
        else:
            print(f"❌ GET: {full_key} (not found)")
            return Response(status=404)
    
    elif request.method == 'DELETE':
        # 객체 삭제
        if full_key in storage:
            del storage[full_key]
            print(f"🗑️  DELETE: {full_key}")
            return Response(status=204)
        else:
            print(f"❌ DELETE: {full_key} (not found)")
            return Response(status=404)

@app.route('/stats', methods=['GET'])
def get_stats():
    """통계 조회"""
    return jsonify({
        'stats': stats,
        'buckets': list(buckets),
        'total_objects': len(storage),
        'objects': list(storage.keys())
    })

@app.route('/reset', methods=['POST'])
def reset():
    """저장소 초기화"""
    storage.clear()
    buckets.clear()
    stats['total_uploads'] = 0
    stats['total_gets'] = 0
    stats['total_heads'] = 0
    stats['bucket_creates'] = 0
    print("🔄 Storage reset")
    return jsonify({'message': 'Storage reset successfully'})

@app.errorhandler(404)
def not_found(error):
    return Response(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Error><Code>NoSuchKey</Code><Message>The specified key does not exist.</Message></Error>',
        status=404,
        content_type='application/xml'
    )

@app.errorhandler(500)
def internal_error(error):
    return Response(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Error><Code>InternalError</Code><Message>Internal server error</Message></Error>',
        status=500,
        content_type='application/xml'
    )

if __name__ == '__main__':
    print("=" * 60)
    print("🚀 Mock S3 Server Starting...")
    print("=" * 60)
    print("Port: 9000")
    print("Endpoints:")
    print("  - GET  /                     : Server info")
    print("  - GET  /stats                : Statistics")
    print("  - POST /reset                : Reset storage")
    print("  - HEAD /<bucket>             : Check bucket")
    print("  - PUT  /<bucket>             : Create bucket")
    print("  - PUT  /<bucket>/<key>       : Upload object")
    print("  - HEAD /<bucket>/<key>       : Get object metadata")
    print("  - GET  /<bucket>/<key>       : Download object")
    print("  - DELETE /<bucket>/<key>     : Delete object")
    print("=" * 60)
    
    # Flask 실행
    app.run(host='0.0.0.0', port=9000, debug=False, threaded=True)