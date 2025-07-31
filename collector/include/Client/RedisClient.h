// collector/include/Client/RedisClient.h
#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <chrono>

/**
 * @brief Redis 클라이언트 추상 인터페이스
 * @details 기존 PulseOne 패턴을 준수하는 Redis 연동 인터페이스
 * @author PulseOne Development Team
 * @date 2025-07-31
 */
class RedisClient {
public:
    // =============================================================================
    // 타입 정의 (기존 패턴 준수)
    // =============================================================================
    
    using StringMap = std::map<std::string, std::string>;
    using StringList = std::vector<std::string>;
    using KeyValuePair = std::pair<std::string, std::string>;
    using MessageCallback = std::function<void(const std::string& channel, const std::string& message)>;
    
    // =============================================================================
    // 기본 연결 관리 (기존 인터페이스 유지)
    // =============================================================================
    
    /**
     * @brief Redis 서버 연결
     * @param host 서버 호스트
     * @param port 서버 포트 
     * @param password 비밀번호 (선택사항)
     * @return 성공 시 true
     */
    virtual bool connect(const std::string& host, int port, const std::string& password = "") = 0;
    
    /**
     * @brief 연결 해제
     */
    virtual void disconnect() = 0;
    
    /**
     * @brief 연결 상태 확인
     * @return 연결된 경우 true
     */
    virtual bool isConnected() const = 0;
    
    // =============================================================================
    // 기본 Key-Value 조작 (기존 인터페이스 확장)
    // =============================================================================
    
    /**
     * @brief 키-값 설정 (기존)
     */
    virtual bool set(const std::string& key, const std::string& value) = 0;
    
    /**
     * @brief 값 조회 (기존)
     */
    virtual std::string get(const std::string& key) = 0;
    
    /**
     * @brief TTL과 함께 키-값 설정
     * @param key 키
     * @param value 값
     * @param ttl_seconds TTL (초)
     * @return 성공 시 true
     */
    virtual bool setex(const std::string& key, const std::string& value, int ttl_seconds) = 0;
    
    /**
     * @brief 키 존재 확인
     * @param key 확인할 키
     * @return 존재하면 true
     */
    virtual bool exists(const std::string& key) = 0;
    
    /**
     * @brief 키 삭제
     * @param key 삭제할 키
     * @return 삭제된 키의 수
     */
    virtual int del(const std::string& key) = 0;
    
    /**
     * @brief 다중 키 삭제
     * @param keys 삭제할 키들
     * @return 삭제된 키의 수
     */
    virtual int del(const StringList& keys) = 0;
    
    /**
     * @brief 키의 TTL 조회
     * @param key 확인할 키
     * @return TTL (초), -1: 만료시간 없음, -2: 키 없음
     */
    virtual int ttl(const std::string& key) = 0;
    
    // =============================================================================
    // Hash 조작 (PulseOne 데이터 구조용)
    // =============================================================================
    
    /**
     * @brief Hash 필드 설정
     * @param key Hash 키
     * @param field 필드명
     * @param value 값
     * @return 성공 시 true
     */
    virtual bool hset(const std::string& key, const std::string& field, const std::string& value) = 0;
    
    /**
     * @brief Hash 필드 조회
     * @param key Hash 키
     * @param field 필드명
     * @return 필드 값 (없으면 빈 문자열)
     */
    virtual std::string hget(const std::string& key, const std::string& field) = 0;
    
    /**
     * @brief Hash 전체 조회
     * @param key Hash 키
     * @return 모든 필드-값 쌍
     */
    virtual StringMap hgetall(const std::string& key) = 0;
    
    /**
     * @brief Hash 다중 필드 설정
     * @param key Hash 키
     * @param field_values 필드-값 쌍들
     * @return 성공 시 true
     */
    virtual bool hmset(const std::string& key, const StringMap& field_values) = 0;
    
    /**
     * @brief Hash 필드 삭제
     * @param key Hash 키
     * @param field 삭제할 필드
     * @return 삭제된 필드 수
     */
    virtual int hdel(const std::string& key, const std::string& field) = 0;
    
    // =============================================================================
    // List 조작 (큐/로그용)
    // =============================================================================
    
    /**
     * @brief List 왼쪽에 값 추가
     * @param key List 키
     * @param value 추가할 값
     * @return List 길이
     */
    virtual int lpush(const std::string& key, const std::string& value) = 0;
    
    /**
     * @brief List 오른쪽에 값 추가
     * @param key List 키  
     * @param value 추가할 값
     * @return List 길이
     */
    virtual int rpush(const std::string& key, const std::string& value) = 0;
    
    /**
     * @brief List 왼쪽에서 값 제거 후 반환
     * @param key List 키
     * @return 제거된 값 (리스트가 비어있으면 빈 문자열)
     */
    virtual std::string lpop(const std::string& key) = 0;
    
    /**
     * @brief List 오른쪽에서 값 제거 후 반환
     * @param key List 키
     * @return 제거된 값 (리스트가 비어있으면 빈 문자열)
     */
    virtual std::string rpop(const std::string& key) = 0;
    
    /**
     * @brief List 범위 조회
     * @param key List 키
     * @param start 시작 인덱스
     * @param stop 끝 인덱스 (-1: 끝까지)
     * @return 값들의 리스트
     */
    virtual StringList lrange(const std::string& key, int start, int stop) = 0;
    
    /**
     * @brief List 길이 조회
     * @param key List 키
     * @return List 길이
     */
    virtual int llen(const std::string& key) = 0;
    
    // =============================================================================
    // Sorted Set 조작 (시계열 데이터용)
    // =============================================================================
    
    /**
     * @brief Sorted Set에 값 추가
     * @param key Set 키
     * @param score 점수 (정렬 기준)
     * @param member 멤버
     * @return 새로 추가된 멤버 수
     */
    virtual int zadd(const std::string& key, double score, const std::string& member) = 0;
    
    /**
     * @brief Sorted Set 범위 조회 (점수 기준)
     * @param key Set 키
     * @param min_score 최소 점수
     * @param max_score 최대 점수
     * @return 멤버들의 리스트
     */
    virtual StringList zrangebyscore(const std::string& key, double min_score, double max_score) = 0;
    
    /**
     * @brief Sorted Set 크기 조회
     * @param key Set 키
     * @return Set 크기
     */
    virtual int zcard(const std::string& key) = 0;
    
    /**
     * @brief Sorted Set에서 점수 범위의 멤버 삭제
     * @param key Set 키
     * @param min_score 최소 점수
     * @param max_score 최대 점수
     * @return 삭제된 멤버 수
     */
    virtual int zremrangebyscore(const std::string& key, double min_score, double max_score) = 0;
    
    // =============================================================================
    // Pub/Sub 메시징 (기존 확장)
    // =============================================================================
    
    /**
     * @brief 메시지 발행 (기존)
     */
    virtual bool publish(const std::string& channel, const std::string& message) = 0;
    
    /**
     * @brief 채널 구독 (기존)
     */
    virtual bool subscribe(const std::string& channel) = 0;
    
    /**
     * @brief 채널 구독 해제 (기존)
     */
    virtual bool unsubscribe(const std::string& channel) = 0;
    
    /**
     * @brief 패턴 구독
     * @param pattern 구독할 패턴 (예: "pulseone.*")
     * @return 성공 시 true
     */
    virtual bool psubscribe(const std::string& pattern) = 0;
    
    /**
     * @brief 패턴 구독 해제
     * @param pattern 해제할 패턴
     * @return 성공 시 true
     */
    virtual bool punsubscribe(const std::string& pattern) = 0;
    
    /**
     * @brief 메시지 콜백 설정
     * @param callback 메시지 수신 시 호출될 콜백
     */
    virtual void setMessageCallback(MessageCallback callback) = 0;
    
    // =============================================================================
    // 배치 처리 (성능 최적화)
    // =============================================================================
    
    /**
     * @brief 다중 키 설정
     * @param key_values 키-값 쌍들
     * @return 성공 시 true
     */
    virtual bool mset(const StringMap& key_values) = 0;
    
    /**
     * @brief 다중 키 조회
     * @param keys 조회할 키들
     * @return 값들의 리스트
     */
    virtual StringList mget(const StringList& keys) = 0;
    
    // =============================================================================
    // 트랜잭션 지원
    // =============================================================================
    
    /**
     * @brief 트랜잭션 시작
     * @return 성공 시 true
     */
    virtual bool multi() = 0;
    
    /**
     * @brief 트랜잭션 실행
     * @return 성공 시 true
     */
    virtual bool exec() = 0;
    
    /**
     * @brief 트랜잭션 취소
     * @return 성공 시 true
     */
    virtual bool discard() = 0;
    
    // =============================================================================
    // 상태 및 진단
    // =============================================================================
    
    /**
     * @brief Redis 서버 정보 조회
     * @return 서버 정보 맵
     */
    virtual StringMap info() = 0;
    
    /**
     * @brief 연결 테스트
     * @return 연결이 유효하면 true
     */
    virtual bool ping() = 0;
    
    /**
     * @brief 데이터베이스 선택
     * @param db_index 데이터베이스 인덱스 (0-15)
     * @return 성공 시 true
     */
    virtual bool select(int db_index) = 0;
    
    /**
     * @brief 현재 데이터베이스의 키 수 조회
     * @return 키의 수
     */
    virtual int dbsize() = 0;
    
    // =============================================================================
    // 가상 소멸자
    // =============================================================================
    
    virtual ~RedisClient() = default;
};

#endif // REDIS_CLIENT_H