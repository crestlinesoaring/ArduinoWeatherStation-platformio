/*
 * Optional pre-upload network checks (gateway ICMP ping, link status).
 * Enable with -D NET_CONNECTIVITY_CHECKS in platformio.ini.
 */
#ifdef NET_CONNECTIVITY_CHECKS

#include <Ethernet.h>
#include <utility/w5100.h>

#ifndef NET_GATEWAY_PING_SOCKET
#define NET_GATEWAY_PING_SOCKET 7
#endif

static const uint8_t ICMP_ECHOREQ = 8;
static const uint8_t ICMP_ECHOREP = 0;
static const uint8_t ICMP_ECHO_DATA_LEN = 32;
static const uint16_t ICMP_ECHO_PACKET_LEN = 8 + ICMP_ECHO_DATA_LEN;

static uint8_t wxPingSeq = 0;

static uint16_t wxPingChecksum(const uint8_t* data, uint16_t len) {
  uint32_t sum = 0;
  for (uint16_t i = 0; i + 1 < len; i += 2) {
    sum += ((uint16_t)data[i] << 8) | data[i + 1];
  }
  if (len & 1) {
    sum += (uint16_t)data[len - 1] << 8;
  }
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }
  return (uint16_t)(~sum);
}

static void wxPingWriteData(uint8_t s, uint16_t dataOffset, const uint8_t* data, uint16_t len) {
  uint16_t ptr = W5100.readSnTX_WR(s);
  ptr += dataOffset;
  uint16_t offset = ptr & W5100.SMASK;
  uint16_t dstAddr = offset + W5100.SBASE(s);

  if (W5100.hasOffsetAddressMapping() || offset + len <= W5100.SSIZE) {
    W5100.write(dstAddr, data, len);
  } else {
    uint16_t size = W5100.SSIZE - offset;
    W5100.write(dstAddr, data, size);
    W5100.write(W5100.SBASE(s), data + size, len - size);
  }
  W5100.writeSnTX_WR(s, ptr + len);
}

static void wxPingReadData(uint8_t s, uint16_t src, uint8_t* dst, uint16_t len) {
  uint16_t srcMask = src & W5100.SMASK;
  uint16_t srcPtr = W5100.RBASE(s) + srcMask;

  if (W5100.hasOffsetAddressMapping() || srcMask + len <= W5100.SSIZE) {
    W5100.read(srcPtr, dst, len);
  } else {
    uint16_t size = W5100.SSIZE - srcMask;
    W5100.read(srcPtr, dst, size);
    W5100.read(W5100.RBASE(s), dst + size, len - size);
  }
}

static uint16_t wxPingRxSize(uint8_t s) {
  uint16_t val;
  uint16_t prev = W5100.readSnRX_RSR(s);
  do {
    val = W5100.readSnRX_RSR(s);
    if (val == prev) return val;
    prev = val;
  } while (true);
}

static void wxPingCloseSocket(uint8_t s) {
  W5100.execCmdSn(s, Sock_CLOSE);
  W5100.writeSnIR(s, 0xFF);
}

static bool wxPingOpenSocket(uint8_t s) {
  wxPingCloseSocket(s);
  W5100.writeSnMR(s, SnMR::IPRAW);
  W5100.writeSnPROTO(s, IPPROTO::ICMP);
  W5100.writeSnPORT(s, 0);
  W5100.execCmdSn(s, Sock_OPEN);
  return W5100.readSnSR(s) == SnSR::IPRAW;
}

static bool wxPingSend(uint8_t s, const IPAddress& addr, const uint8_t* packet, uint16_t len) {
  uint8_t addri[] = {addr[0], addr[1], addr[2], addr[3]};
  W5100.writeSnDIPR(s, addri);
  W5100.writeSnDPORT(s, 0);
  W5100.writeSnTTL(s, 64);

  uint32_t start = millis();
  while (millis() - start < 500) {
    if (W5100.readSnTX_FSR(s) >= len) break;
    wdt_reset();
    yield();
  }
  if (W5100.readSnTX_FSR(s) < len) return false;

  wxPingWriteData(s, 0, packet, len);
  W5100.execCmdSn(s, Sock_SEND);

  start = millis();
  while (millis() - start < 500) {
    uint8_t ir = W5100.readSnIR(s);
    if (ir & SnIR::SEND_OK) {
      W5100.writeSnIR(s, SnIR::SEND_OK);
      return true;
    }
    if (ir & SnIR::TIMEOUT) {
      W5100.writeSnIR(s, SnIR::TIMEOUT);
      return false;
    }
    wdt_reset();
    yield();
  }
  return false;
}

static bool wxPingReceive(uint8_t s, uint16_t id, uint16_t seq, uint16_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (wxPingRxSize(s) < 6) {
      wdt_reset();
      yield();
      continue;
    }

    uint8_t meta[6];
    uint16_t rxPtr = W5100.readSnRX_RD(s);
    wxPingReadData(s, rxPtr, meta, sizeof(meta));
    rxPtr += sizeof(meta);

    uint16_t dataLen = ((uint16_t)meta[4] << 8) | meta[5];
    if (dataLen < 8) {
      W5100.writeSnRX_RD(s, rxPtr + dataLen);
      W5100.execCmdSn(s, Sock_RECV);
      continue;
    }
    if (dataLen > ICMP_ECHO_PACKET_LEN) dataLen = ICMP_ECHO_PACKET_LEN;

    uint8_t reply[ICMP_ECHO_PACKET_LEN];
    wxPingReadData(s, rxPtr, reply, dataLen);
    rxPtr += dataLen;
    W5100.writeSnRX_RD(s, rxPtr);
    W5100.execCmdSn(s, Sock_RECV);

    if (reply[0] == ICMP_ECHOREP) {
      uint16_t replyId = ((uint16_t)reply[4] << 8) | reply[5];
      uint16_t replySeq = ((uint16_t)reply[6] << 8) | reply[7];
      if (replyId == id && replySeq == seq) return true;
    }
  }
  return false;
}

bool waitForEthernetLink(uint16_t timeoutMs) {
  if (Ethernet.hardwareStatus() != EthernetW5500 && Ethernet.hardwareStatus() != EthernetW5200) {
    return true;
  }

  uint32_t deadline = millis() + timeoutMs;
  while (millis() < deadline) {
    if (Ethernet.linkStatus() == LinkON) return true;
    wdt_reset();
    delay(10);
  }
  return Ethernet.linkStatus() == LinkON;
}

bool pingIp(const IPAddress& addr, uint16_t timeoutMs) {
  if (Ethernet.hardwareStatus() != EthernetW5500 && Ethernet.hardwareStatus() != EthernetW5200) {
    return true;
  }

  const uint8_t s = NET_GATEWAY_PING_SOCKET;
  uint8_t packet[ICMP_ECHO_PACKET_LEN];
  const uint16_t id = 0x5758; // "WX"

  packet[0] = ICMP_ECHOREQ;
  packet[1] = 0;
  packet[2] = 0;
  packet[3] = 0;
  packet[4] = (uint8_t)(id >> 8);
  packet[5] = (uint8_t)(id & 0xFF);
  packet[6] = 0;
  packet[7] = wxPingSeq;
  for (uint8_t i = 0; i < ICMP_ECHO_DATA_LEN; i++) {
    packet[8 + i] = (uint8_t)(0x10 + i);
  }
  uint16_t checksum = wxPingChecksum(packet, ICMP_ECHO_PACKET_LEN);
  packet[2] = (uint8_t)(checksum >> 8);
  packet[3] = (uint8_t)(checksum & 0xFF);

  SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
  if (!wxPingOpenSocket(s)) {
    SPI.endTransaction();
    return false;
  }

  bool ok = false;
  if (wxPingSend(s, addr, packet, ICMP_ECHO_PACKET_LEN)) {
    ok = wxPingReceive(s, id, wxPingSeq, timeoutMs);
  }
  wxPingCloseSocket(s);
  SPI.endTransaction();

  wxPingSeq++;
  return ok;
}

bool pingGateway(uint16_t timeoutMs) {
  Serial.print(F("[NET] ping gateway "));
  Serial.print(gateway[0], DEC);
  Serial.print(F("."));
  Serial.print(gateway[1], DEC);
  Serial.print(F("."));
  Serial.print(gateway[2], DEC);
  Serial.print(F("."));
  Serial.println(gateway[3], DEC);

  bool ok = pingIp(gateway, timeoutMs);
  wxLogTag(F("NET"), ok ? F("gateway ping ok") : F("gateway ping failed"));
  return ok;
}

#endif  // NET_CONNECTIVITY_CHECKS
