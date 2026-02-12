//=============================================================================
// collector/src/Drivers/Bacnet/BACnetBIPPort.cpp
// BACnet/IP Datalink Port Implementation
//=============================================================================

#include "Platform/PlatformCompat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if PULSEONE_WINDOWS
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "Drivers/Bacnet/BACnetDriver.h"
#include "Drivers/Bacnet/BACnetServiceManager.h"
#include "Logging/LogManager.h"

// BACnet Stack Headers
#if defined(HAVE_BACNET) || defined(HAVE_BACNET_STACK)
extern "C" {
#include <bacnet/bacdef.h>
#include <bacnet/datalink/bip.h>
#include <bacnet/datalink/bvlc.h>
#include <bacnet/npdu.h>
}
#endif

// External global pointer from BACnetServiceManager.cpp
namespace PulseOne {
namespace Drivers {
extern BACnetServiceManager *g_pPulseOneBACnetServiceManager;
}
} // namespace PulseOne

using namespace PulseOne::Drivers;

extern "C" {

/**
 * @brief Initialize the BACnet/IP datalink
 */
bool bip_init(char *ifname) {
  (void)ifname;
  // Driver handles socket creation, so we just return success
  return true;
}

/**
 * @brief Cleanup the BACnet/IP datalink
 */
void bip_cleanup(void) {
  // Driver handles socket closure
}

/**
 * @brief Get the local BACnet address
 */
void bip_get_my_address(BACNET_ADDRESS *my_address) {
  if (my_address) {
    memset(my_address, 0, sizeof(BACNET_ADDRESS));
    my_address->net = 0; // Local network
    my_address->len = 6; // IPv4 + Port

    // Return 127.0.0.1:47808 as a placeholder
    my_address->adr[0] = 127;
    my_address->adr[1] = 0;
    my_address->adr[2] = 0;
    my_address->adr[3] = 1;
    my_address->adr[4] = 0xBA; // 47808
    my_address->adr[5] = 0xC0;
  }
}

/**
 * @brief Get the broadcast BACnet address
 */
void bip_get_broadcast_address(BACNET_ADDRESS *dest_address) {
  if (dest_address) {
    memset(dest_address, 0, sizeof(BACNET_ADDRESS));
    dest_address->net = 0;
    dest_address->len = 6;
    memset(&dest_address->adr[0], 0xFF, 4); // 255.255.255.255
    dest_address->adr[4] = 0xBA;            // 47808 port high
    dest_address->adr[5] = 0xC0;            // 47808 port low
  }
}

/**
 * @brief Send a PDU over BACnet/IP
 */
int bip_send_pdu(BACNET_ADDRESS *dest, BACNET_NPDU_DATA *npdu_data,
                 uint8_t *pdu, unsigned pdu_len) {
  (void)npdu_data;

  if (!g_pPulseOneBACnetServiceManager)
    return -1;

  // Convert dest to sockaddr_in
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;

  if (dest->len == 6) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &dest->adr[0], ip_str, INET_ADDRSTRLEN);
    uint16_t port = (dest->adr[4] << 8) | dest->adr[5];

    LogManager::getInstance().Debug(
        "[BIP] Sending PDU to " + std::string(ip_str) + ":" +
        std::to_string(port) + " (len: " + std::to_string(pdu_len) + ")");

    memcpy(&addr.sin_addr.s_addr, &dest->adr[0], 4);
    memcpy(&addr.sin_port, &dest->adr[4], 2);
  } else {
    LogManager::getInstance().Debug(
        "[BIP] Sending Broadcast PDU (len: " + std::to_string(pdu_len) + ")");
    // Fallback for broadcast or other types
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    addr.sin_port = htons(47808);
  }

  // Send via service manager's driver's socket
  return g_pPulseOneBACnetServiceManager->SendRawPacket(
      (uint8_t *)&addr, sizeof(addr), pdu, pdu_len);
}

/**
 * @brief Receive a PDU over BACnet/IP
 */
uint16_t bip_receive(BACNET_ADDRESS *src, uint8_t *pdu, uint16_t max_pdu,
                     unsigned timeout) {
  if (!g_pPulseOneBACnetServiceManager)
    return 0;

  int socket_fd = g_pPulseOneBACnetServiceManager->GetSocketFd();
  if (socket_fd < 0)
    return 0;

  // Implement timeout with select() to avoid blocking indefinitely
  struct timeval tv;
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout % 1000) * 1000;

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(socket_fd, &fds);

  int result = select(socket_fd + 1, &fds, NULL, NULL, &tv);
  if (result <= 0)
    return 0; // Timeout or error

  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);

  int received = recvfrom(socket_fd, (char *)pdu, max_pdu, 0,
                          (struct sockaddr *)&addr, &addr_len);

  if (received > 0 && src) {
    memset(src, 0, sizeof(BACNET_ADDRESS));
    src->net = 0;
    src->len = 6;
    memcpy(&src->adr[0], &addr.sin_addr.s_addr, 4);
    memcpy(&src->adr[4], &addr.sin_port, 2);

    char src_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr.s_addr, src_ip, INET_ADDRSTRLEN);
    uint16_t src_port = ntohs(addr.sin_port);

    LogManager::getInstance().Debug(
        "[BIP] Received PDU from " + std::string(src_ip) + ":" +
        std::to_string(src_port) + " (len: " + std::to_string(received) + ")");
  }

  return (uint16_t)received;
}

} // extern "C"
