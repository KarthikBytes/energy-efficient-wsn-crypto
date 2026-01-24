#ifndef MEMOSTP_PROTOCOL_H
#define MEMOSTP_PROTOCOL_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ascon_crypto.h"
#include "snake_optimizer.h"
#include <vector>
#include <random>

class EnhancedMEMOSTPProtocol {
private:
    ns3::NodeContainer nodes;
    EnhancedSnakeOptimizer optimizer;
    std::vector<double> optimizedParams;
    int optimization_iterations;
    AsconCrypto cryptoEngine;
    bool cryptoEnabled;
    uint8_t cryptoKey[16];
    uint8_t cryptoNonce[16];
    uint32_t packetsEncrypted;
    uint32_t packetsDecrypted;
    uint32_t packetsReceived;

public:
    EnhancedMEMOSTPProtocol(ns3::NodeContainer &nodeContainer, int opt_iters = 10);
    
    void initializeProtocol();
    
    std::vector<uint8_t> encryptPacket(const std::vector<uint8_t>& plaintext, 
                                      uint32_t nodeId, uint32_t packetId);
    std::vector<uint8_t> decryptPacket(const std::vector<uint8_t>& ciphertext, 
                                      uint32_t nodeId, uint32_t packetId);
    
    double getEnergyWeight() const;
    double getPowerControl() const;
    double getSleepRatio() const;
    
    uint32_t getPacketsEncrypted() const { return packetsEncrypted; }
    uint32_t getPacketsDecrypted() const { return packetsDecrypted; }
    uint32_t getPacketsReceived() const { return packetsReceived; }
    
    void printCryptoStats() const;
    void printProtocolStats() const;
    
    void setCryptoEnabled(bool enabled) { cryptoEnabled = enabled; }
    bool isCryptoEnabled() const { return cryptoEnabled; }

private:
    void generateCryptoKeys();
};

#endif // MEMOSTP_PROTOCOL_H