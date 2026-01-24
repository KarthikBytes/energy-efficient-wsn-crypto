#include "crypto_app.h"
#include "event_emitter.h"
#include "ns3/random-variable-stream.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include <iostream>

ns3::TypeId CryptoTestApplication::GetTypeId() {
    static ns3::TypeId tid = ns3::TypeId("CryptoTestApplication")
        .SetParent<ns3::Application>()
        .AddConstructor<CryptoTestApplication>();
    return tid;
}

CryptoTestApplication::CryptoTestApplication() 
    : m_socket(0), m_peerPort(0), m_packetSize(512), 
      m_isReceiver(false), m_nodeId(0), m_packetCounter(0) {}

void CryptoTestApplication::Setup(ns3::Ptr<ns3::Socket> socket, ns3::Address address, uint16_t port, 
                                 uint32_t packetSize, EnhancedMEMOSTPProtocol* protocol, 
                                 bool isReceiver, uint32_t nodeId) {
    m_socket = socket;
    m_peerAddress = address;
    m_peerPort = port;
    m_packetSize = packetSize;
    m_protocol = protocol;
    m_isReceiver = isReceiver;
    m_nodeId = nodeId;
}

void CryptoTestApplication::StartApplication() {
    if (m_isReceiver) {
        m_socket->Bind(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), m_peerPort));
        m_socket->SetRecvCallback(ns3::MakeCallback(&CryptoTestApplication::HandleRead, this));
        
        EventEmitter::Instance().EmitNodeEvent(m_nodeId, "receiver_started");
    } else {
        m_socket->Bind();
        m_socket->Connect(m_peerAddress);
        
        EventEmitter::Instance().EmitNodeEvent(m_nodeId, "sender_started");
        
        m_sendEvent = ns3::Simulator::Schedule(ns3::Seconds(0.1), &CryptoTestApplication::SendPacket, this);
    }
}

void CryptoTestApplication::StopApplication() {
    if (m_sendEvent.IsRunning()) {
        ns3::Simulator::Cancel(m_sendEvent);
    }
    
    if (m_socket) {
        m_socket->Close();
    }
    
    EventEmitter::Instance().EmitNodeEvent(m_nodeId, "app_stopped");
}

void CryptoTestApplication::SendPacket() {
    std::vector<uint8_t> data(m_packetSize);
    ns3::Ptr<ns3::UniformRandomVariable> uv = ns3::CreateObject<ns3::UniformRandomVariable>();
    
    for (size_t i = 0; i < m_packetSize; i++) {
        data[i] = uv->GetInteger(0, 255);
    }
    
    uint32_t packetId = ++m_packetCounter;
    
    ns3::InetSocketAddress destAddr = ns3::InetSocketAddress::ConvertFrom(m_peerAddress);
    uint32_t destNode = destAddr.GetIpv4().Get();
    
    EventEmitter::Instance().EmitEvent("packet_tx", packetId, m_nodeId, destNode);
    
    auto encryptedData = m_protocol->encryptPacket(data, m_nodeId, packetId);
    
    if (!encryptedData.empty()) {
        ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(encryptedData.data(), encryptedData.size());
        m_socket->Send(packet);
        
        EventEmitter::Instance().EmitMetric("packet_size", encryptedData.size(), "bytes");
    }
    
    m_sendEvent = ns3::Simulator::Schedule(ns3::Seconds(0.5), &CryptoTestApplication::SendPacket, this);
}

void CryptoTestApplication::HandleRead(ns3::Ptr<ns3::Socket> socket) {
    ns3::Ptr<ns3::Packet> packet;
    ns3::Address from;
    
    while ((packet = socket->RecvFrom(from))) {
        ns3::InetSocketAddress srcAddr = ns3::InetSocketAddress::ConvertFrom(from);
        uint32_t srcNode = srcAddr.GetIpv4().Get();
        uint32_t packetId = ++m_packetCounter;
        
        EventEmitter::Instance().EmitEvent("packet_rx", packetId, srcNode, m_nodeId);
        
        uint32_t size = packet->GetSize();
        std::vector<uint8_t> buffer(size);
        packet->CopyData(buffer.data(), size);
        
        auto decryptedData = m_protocol->decryptPacket(buffer, m_nodeId, packetId);
        
        EventEmitter::Instance().EmitMetric("packet_latency", 
                                           ns3::Simulator::Now().GetSeconds(), "s");
    }
}