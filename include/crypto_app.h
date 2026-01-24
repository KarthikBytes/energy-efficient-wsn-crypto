#ifndef CRYPTO_APP_H
#define CRYPTO_APP_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "memostp_protocol.h"
#include <vector>
#include <cstdint>

class CryptoTestApplication : public ns3::Application {
public:
    CryptoTestApplication();
    
    static ns3::TypeId GetTypeId();
    
    void Setup(ns3::Ptr<ns3::Socket> socket, ns3::Address address, uint16_t port, 
               uint32_t packetSize, EnhancedMEMOSTPProtocol* protocol, 
               bool isReceiver, uint32_t nodeId);
    
    void StartApplication() override;
    void StopApplication() override;
    
private:
    void SendPacket();
    void HandleRead(ns3::Ptr<ns3::Socket> socket);
    
    ns3::Ptr<ns3::Socket> m_socket;
    ns3::Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize;
    EnhancedMEMOSTPProtocol* m_protocol;
    bool m_isReceiver;
    uint32_t m_nodeId;
    uint32_t m_packetCounter;
    ns3::EventId m_sendEvent;
};

#endif // CRYPTO_APP_H