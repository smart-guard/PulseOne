#!/usr/bin/env python3
"""
Mock MQTT Broker for MqttTargetHandler Testing
포트 1883 (TCP), 8883 (SSL - 미구현)
"""

import socket
import threading
import time
import struct
import json
from datetime import datetime

class MockMqttBroker:
    def __init__(self, host='0.0.0.0', port=1883):
        self.host = host
        self.port = port
        self.server_socket = None
        self.running = False
        self.clients = []
        self.published_messages = []  # (topic, payload, qos, retain)
        self.lock = threading.Lock()
        
    def start(self):
        """브로커 시작"""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(5)
        self.running = True
        
        print(f"🚀 Mock MQTT Broker started on {self.host}:{self.port}")
        
        # 클라이언트 수락 스레드
        accept_thread = threading.Thread(target=self.accept_clients, daemon=True)
        accept_thread.start()
        
    def accept_clients(self):
        """클라이언트 연결 수락"""
        while self.running:
            try:
                client_socket, addr = self.server_socket.accept()
                print(f"✅ Client connected from {addr}")
                
                client_thread = threading.Thread(
                    target=self.handle_client,
                    args=(client_socket, addr),
                    daemon=True
                )
                client_thread.start()
                
                with self.lock:
                    self.clients.append(client_socket)
                    
            except Exception as e:
                if self.running:
                    print(f"❌ Accept error: {e}")
                    
    def handle_client(self, client_socket, addr):
        """클라이언트 처리"""
        try:
            # MQTT CONNECT 패킷 수신
            data = client_socket.recv(1024)
            if not data:
                return
                
            # 간단한 MQTT 패킷 파싱
            if len(data) >= 2:
                packet_type = (data[0] >> 4) & 0x0F
                
                if packet_type == 1:  # CONNECT
                    print(f"📥 CONNECT received from {addr}")
                    
                    # CONNACK 전송 (성공)
                    connack = bytes([0x20, 0x02, 0x00, 0x00])
                    client_socket.send(connack)
                    print(f"📤 CONNACK sent to {addr}")
                    
                    # PUBLISH 패킷 수신 대기
                    while self.running:
                        try:
                            pub_data = client_socket.recv(4096)
                            if not pub_data:
                                break
                                
                            pub_type = (pub_data[0] >> 4) & 0x0F
                            
                            if pub_type == 3:  # PUBLISH
                                self.parse_publish(pub_data, addr)
                                
                                # QoS > 0이면 PUBACK 전송
                                qos = (pub_data[0] >> 1) & 0x03
                                if qos > 0 and len(pub_data) >= 4:
                                    # PUBACK (QoS 1) or PUBREC (QoS 2)
                                    if qos == 1:
                                        msg_id = struct.unpack('>H', pub_data[2:4])[0]
                                        puback = bytes([0x40, 0x02]) + struct.pack('>H', msg_id)
                                        client_socket.send(puback)
                                    
                            elif pub_type == 12:  # PINGREQ
                                # PINGRESP 전송
                                pingresp = bytes([0xD0, 0x00])
                                client_socket.send(pingresp)
                                
                            elif pub_type == 14:  # DISCONNECT
                                print(f"📥 DISCONNECT from {addr}")
                                break
                                
                        except socket.timeout:
                            continue
                        except Exception as e:
                            print(f"❌ Receive error: {e}")
                            break
                            
        except Exception as e:
            print(f"❌ Client handler error: {e}")
        finally:
            client_socket.close()
            with self.lock:
                if client_socket in self.clients:
                    self.clients.remove(client_socket)
            print(f"❌ Client disconnected: {addr}")
            
    def parse_publish(self, data, addr):
        """PUBLISH 패킷 파싱"""
        try:
            flags = data[0] & 0x0F
            qos = (data[0] >> 1) & 0x03
            retain = (data[0] & 0x01) == 1
            
            # 가변 헤더 길이 파싱
            remaining_length = 0
            multiplier = 1
            pos = 1
            
            while pos < len(data):
                byte = data[pos]
                remaining_length += (byte & 0x7F) * multiplier
                if (byte & 0x80) == 0:
                    break
                multiplier *= 128
                pos += 1
                
            pos += 1  # 가변 헤더 시작
            
            # 토픽 길이
            if pos + 2 > len(data):
                return
                
            topic_len = struct.unpack('>H', data[pos:pos+2])[0]
            pos += 2
            
            # 토픽
            if pos + topic_len > len(data):
                return
                
            topic = data[pos:pos+topic_len].decode('utf-8', errors='ignore')
            pos += topic_len
            
            # 메시지 ID (QoS > 0)
            if qos > 0:
                if pos + 2 > len(data):
                    return
                pos += 2
                
            # 페이로드
            payload = data[pos:].decode('utf-8', errors='ignore')
            
            with self.lock:
                self.published_messages.append({
                    'topic': topic,
                    'payload': payload,
                    'qos': qos,
                    'retain': retain,
                    'timestamp': datetime.now().isoformat()
                })
                
            print(f"📥 PUBLISH: topic={topic}, qos={qos}, retain={retain}, payload_len={len(payload)}")
            
        except Exception as e:
            print(f"❌ Parse PUBLISH error: {e}")
            
    def get_published_messages(self):
        """발행된 메시지 목록 가져오기"""
        with self.lock:
            return list(self.published_messages)
            
    def clear_messages(self):
        """메시지 목록 초기화"""
        with self.lock:
            self.published_messages.clear()
            
    def stop(self):
        """브로커 중지"""
        self.running = False
        
        # 모든 클라이언트 연결 종료
        with self.lock:
            for client in self.clients:
                try:
                    client.close()
                except:
                    pass
            self.clients.clear()
            
        if self.server_socket:
            self.server_socket.close()
            
        print("🛑 Mock MQTT Broker stopped")


def main():
    broker = MockMqttBroker(host='0.0.0.0', port=1883)
    broker.start()
    
    try:
        # 서버 실행 유지
        while True:
            time.sleep(1)
            
            # 발행된 메시지 출력 (디버그)
            messages = broker.get_published_messages()
            if messages:
                print(f"\n📊 Total messages received: {len(messages)}")
                
    except KeyboardInterrupt:
        print("\n⚠️  Stopping broker...")
        broker.stop()


if __name__ == '__main__':
    main()