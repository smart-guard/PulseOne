// collector/include/Client/RedisClient.h
#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace PulseOne {

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
  using MessageCallback = std::function<void(const std::string &channel,
                                             const std::string &message)>;

  // =============================================================================
  // 기본 연결 관리
  // =============================================================================

  /**
   * @brief Redis 서버 연결
   * @param host 서버 호스트
   * @param port 서버 포트
   * @param password 비밀번호 (선택사항)
   * @return 성공 시 true
   */
  virtual bool connect(const std::string &host, int port,
                       const std::string &password = "") = 0;

  /**
   * @brief 연결 해제
   */
  virtual void disconnect() = 0;

  /**
   * @brief 연결 상태 확인
   * @return 연결된 경우 true
   */
  virtual bool isConnected() const = 0;

  /**
   * @brief 구독 모드 설정 (Watchdog 간섭 방지용)
   * @param enabled 활성화 여부
   */
  virtual void setSubscriberMode(bool /*enabled*/) {}; // 기본 구현 (옵션)

  /**
   * @brief 구독 모드 여부 확인
   * @return 구독 모드이면 true
   */
  virtual bool isSubscriberMode() const { return false; }; // 기본 구현 (옵션)

  // =============================================================================
  // 기본 Key-Value 조작
  // =============================================================================

  /**
   * @brief 키-값 설정
   * @param key 키
   * @param value 값
   * @return 성공 시 true
   */
  virtual bool set(const std::string &key, const std::string &value) = 0;

  /**
   * @brief 키-값 설정 (만료 시간 포함)
   * @param key 키
   * @param value 값
   * @param expire_seconds 만료 시간 (초)
   * @return 성공 시 true
   */
  virtual bool setex(const std::string &key, const std::string &value,
                     int expire_seconds) = 0;

  /**
   * @brief 키 조회
   * @param key 키
   * @return 값 (없으면 빈 문자열)
   */
  virtual std::string get(const std::string &key) = 0;

  /**
   * @brief 키 삭제
   * @param key 키
   * @return 삭제된 키 수
   */
  virtual int del(const std::string &key) = 0;

  /**
   * @brief 키 존재 확인
   * @param key 키
   * @return 존재하면 true
   */
  virtual bool exists(const std::string &key) = 0;

  /**
   * @brief 키 만료 시간 설정
   * @param key 키
   * @param seconds 만료 시간 (초)
   * @return 성공 시 true
   */
  virtual bool expire(const std::string &key, int seconds) = 0;

  /**
   * @brief 키 TTL 조회
   * @param key 키
   * @return TTL (초), -1이면 만료 없음, -2면 키 없음
   */
  virtual int ttl(const std::string &key) = 0;
  /**
   * @brief 패턴으로 키 검색 (KEYS 명령)
   * @param pattern 검색 패턴 (예: "device:*", "point:*:latest")
   * @return 매칭되는 키 리스트
   * @warning 프로덕션 환경에서는 SCAN 사용 권장
   */
  virtual StringList keys(const std::string &pattern) = 0;
  /**
   * @brief 정수 값 증가
   * @param key 키
   * @param increment 증가값 (기본 1)
   * @return 증가 후 값
   */
  virtual int incr(const std::string &key, int increment = 1) = 0;
  // =============================================================================
  // Hash 조작
  // =============================================================================

  /**
   * @brief Hash 필드 설정
   * @param key Hash 키
   * @param field 필드명
   * @param value 값
   * @return 성공 시 true
   */
  virtual bool hset(const std::string &key, const std::string &field,
                    const std::string &value) = 0;

  /**
   * @brief Hash 필드 조회
   * @param key Hash 키
   * @param field 필드명
   * @return 값 (없으면 빈 문자열)
   */
  virtual std::string hget(const std::string &key,
                           const std::string &field) = 0;

  /**
   * @brief Hash 전체 조회
   * @param key Hash 키
   * @return 필드-값 맵
   */
  virtual StringMap hgetall(const std::string &key) = 0;

  /**
   * @brief Hash 필드 삭제
   * @param key Hash 키
   * @param field 필드명
   * @return 삭제된 필드 수
   */
  virtual int hdel(const std::string &key, const std::string &field) = 0;

  /**
   * @brief Hash 필드 존재 확인
   * @param key Hash 키
   * @param field 필드명
   * @return 존재하면 true
   */
  virtual bool hexists(const std::string &key, const std::string &field) = 0;

  /**
   * @brief Hash 필드 수 조회
   * @param key Hash 키
   * @return 필드 수
   */
  virtual int hlen(const std::string &key) = 0;

  // =============================================================================
  // List 조작
  // =============================================================================

  /**
   * @brief List 왼쪽에 추가
   * @param key List 키
   * @param value 값
   * @return 리스트 길이
   */
  virtual int lpush(const std::string &key, const std::string &value) = 0;

  /**
   * @brief List 오른쪽에 추가
   * @param key List 키
   * @param value 값
   * @return 리스트 길이
   */
  virtual int rpush(const std::string &key, const std::string &value) = 0;

  /**
   * @brief List 왼쪽에서 제거
   * @param key List 키
   * @return 제거된 값 (없으면 빈 문자열)
   */
  virtual std::string lpop(const std::string &key) = 0;

  /**
   * @brief List 오른쪽에서 제거
   * @param key List 키
   * @return 제거된 값 (없으면 빈 문자열)
   */
  virtual std::string rpop(const std::string &key) = 0;

  /**
   * @brief List 범위 조회
   * @param key List 키
   * @param start 시작 인덱스
   * @param stop 끝 인덱스
   * @return 값들의 리스트
   */
  virtual StringList lrange(const std::string &key, int start, int stop) = 0;

  /**
   * @brief List 길이 조회
   * @param key List 키
   * @return 리스트 길이
   */
  virtual int llen(const std::string &key) = 0;

  // =============================================================================
  // Set 조작
  // =============================================================================

  /**
   * @brief Set에 멤버 추가
   * @param key Set 키
   * @param member 멤버
   * @return 추가된 멤버 수
   */
  virtual int sadd(const std::string &key, const std::string &member) = 0;

  /**
   * @brief Set에서 멤버 제거
   * @param key Set 키
   * @param member 멤버
   * @return 제거된 멤버 수
   */
  virtual int srem(const std::string &key, const std::string &member) = 0;

  /**
   * @brief Set 멤버 확인
   * @param key Set 키
   * @param member 멤버
   * @return 존재하면 true
   */
  virtual bool sismember(const std::string &key, const std::string &member) = 0;

  /**
   * @brief Set 모든 멤버 조회
   * @param key Set 키
   * @return 멤버들의 리스트
   */
  virtual StringList smembers(const std::string &key) = 0;

  /**
   * @brief Set 크기 조회
   * @param key Set 키
   * @return Set 크기
   */
  virtual int scard(const std::string &key) = 0;

  // =============================================================================
  // Sorted Set 조작
  // =============================================================================

  /**
   * @brief Sorted Set에 멤버 추가
   * @param key Sorted Set 키
   * @param score 점수
   * @param member 멤버
   * @return 추가된 멤버 수
   */
  virtual int zadd(const std::string &key, double score,
                   const std::string &member) = 0;

  /**
   * @brief Sorted Set에서 멤버 제거
   * @param key Sorted Set 키
   * @param member 멤버
   * @return 제거된 멤버 수
   */
  virtual int zrem(const std::string &key, const std::string &member) = 0;

  /**
   * @brief Sorted Set 범위 조회 (점수 순)
   * @param key Sorted Set 키
   * @param start 시작 인덱스
   * @param stop 끝 인덱스
   * @return 멤버들의 리스트
   */
  virtual StringList zrange(const std::string &key, int start, int stop) = 0;

  /**
   * @brief Sorted Set 크기 조회
   * @param key Sorted Set 키
   * @return Set 크기
   */
  virtual int zcard(const std::string &key) = 0;

  // =============================================================================
  // Pub/Sub
  // =============================================================================

  /**
   * @brief 채널 구독
   * @param channel 채널명
   * @return 성공 시 true
   */
  virtual bool subscribe(const std::string &channel) = 0;

  /**
   * @brief 채널 구독 해제
   * @param channel 채널명
   * @return 성공 시 true
   */
  virtual bool unsubscribe(const std::string &channel) = 0;

  /**
   * @brief 메시지 발행
   * @param channel 채널명
   * @param message 메시지
   * @return 수신한 구독자 수
   */
  virtual int publish(const std::string &channel,
                      const std::string &message) = 0;

  /**
   * @brief 패턴 구독
   * @param pattern 패턴 ("channel:*")
   * @return 성공 시 true
   */
  virtual bool psubscribe(const std::string &pattern) = 0;

  /**
   * @brief 패턴 구독 해제
   * @param pattern 해제할 패턴
   * @return 성공 시 true
   */
  virtual bool punsubscribe(const std::string &pattern) = 0;

  /**
   * @brief 메시지 콜백 설정
   * @param callback 메시지 수신 시 호출될 콜백
   */
  virtual void setMessageCallback(MessageCallback callback) = 0;
  /**
   * @brief Pub/Sub 메시지 대기 (블로킹)
   * @param timeout_ms 타임아웃 (밀리초)
   * @return true if message received, false if timeout
   */
  virtual bool waitForMessage(int timeout_ms = 100) = 0;
  // =============================================================================
  // 배치 처리 (성능 최적화)
  // =============================================================================

  /**
   * @brief 다중 키 설정
   * @param key_values 키-값 쌍들
   * @return 성공 시 true
   */
  virtual bool mset(const StringMap &key_values) = 0;

  /**
   * @brief 다중 키 조회
   * @param keys 조회할 키들
   * @return 값들의 리스트
   */
  virtual StringList mget(const StringList &keys) = 0;

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

} // namespace PulseOne

#endif // REDIS_CLIENT_H