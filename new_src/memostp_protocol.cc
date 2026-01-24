#include "memostp_protocol.h"
#include "event_emitter.h"
#include <iostream>
#include <iomanip>

EnhancedMEMOSTPProtocol::EnhancedMEMOSTPProtocol(ns3::NodeContainer &nodeContainer, int opt_iters)
    : nodes(nodeContainer), 
      optimization_iterations(opt_iters),
      cryptoEnabled(true), 
      packetsEncrypted(0), 
      packetsDecrypted(0), 
      packetsReceived(0) {
    
    generateCryptoKeys();
}

void EnhancedMEMOSTPProtocol::generateCryptoKeys() {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    
    for (int i = 0; i < 16; i++) {
        cryptoKey[i] = dist(rng);
        cryptoNonce[i] = dist(rng);
    }
}

void EnhancedMEMOSTPProtocol::initializeProtocol() {
    EventEmitter& emitter = EventEmitter::Instance();
    emitter.EmitEvent("protocol_init", 0);
    
    std::cout << "\033[1;32m╔══════════════════════════════════════════════════════╗\033[0m" << std::endl;
    std::cout << "\033[1;32m║     ENHANCED MEMOSTP PROTOCOL INITIALIZATION        ║\033[0m" << std::endl;
    std::cout << "\033[1;32m╚══════════════════════════════════════════════════════╝\033[0m" << std::endl;
    
    if (cryptoEnabled) {
        cryptoEngine.Initialize(cryptoKey, cryptoNonce);
        cryptoEngine.PrintCryptoMetrics();
    }
    
    std::cout << "\n\033[1;33m🚀 Starting Parameter Optimization...\033[0m" << std::endl;
    optimizedParams = optimizer.optimize(optimization_iterations);
    
    std::cout << "\n\033[1;32m✨ MEMOSTP PROTOCOL CONFIGURED:\033[0m" << std::endl;
    std::cout << "├─ Cryptography: " << (cryptoEnabled ? "ASCON-128" : "Disabled") << std::endl;
    std::cout << "├─ Optimization: " << optimization_iterations << " iterations" << std::endl;
    std::cout << "├─ Nodes: " << nodes.GetN() << std::endl;
    std::cout << "└─ Parameters optimized successfully" << std::endl;
}

std::vector<uint8_t> EnhancedMEMOSTPProtocol::encryptPacket(const std::vector<uint8_t>& plaintext, 
                                                           uint32_t nodeId, uint32_t packetId) {
    if (!cryptoEnabled) return plaintext;
    
    packetsEncrypted++;
    
    std::vector<uint8_t> dataToEncrypt = plaintext;
    uint32_t seqNum = packetsEncrypted;
    dataToEncrypt.insert(dataToEncrypt.begin(), 
                        reinterpret_cast<uint8_t*>(&seqNum), 
                        reinterpret_cast<uint8_t*>(&seqNum) + 4);
    
    auto ciphertext = cryptoEngine.Encrypt(dataToEncrypt, packetId, nodeId);
    
    // Log first few encryptions
    if (packetsEncrypted <= 3) {
        std::cout << "\033[36m🔒 Encrypted Packet #" << packetsEncrypted 
                  << " (Node " << nodeId << ", " << plaintext.size() << " bytes)\033[0m" << std::endl;
    }
    
    return ciphertext;
}

std::vector<uint8_t> EnhancedMEMOSTPProtocol::decryptPacket(const std::vector<uint8_t>& ciphertext, 
                                                           uint32_t nodeId, uint32_t packetId) {
    if (!cryptoEnabled) return ciphertext;
    
    packetsReceived++;
    
    auto plaintext = cryptoEngine.Decrypt(ciphertext, packetId, nodeId);
    
    if (!plaintext.empty()) {
        packetsDecrypted++;
        
        if (plaintext.size() >= 4) {
            uint32_t seqNum;
            memcpy(&seqNum, plaintext.data(), 4);
            
            if (packetsDecrypted <= 3) {
                std::cout << "\033[32m🔓 Decrypted Packet #" << seqNum 
                          << " (Node " << nodeId << ", " << plaintext.size()-4 << " bytes)\033[0m" << std::endl;
            }
            
            plaintext.erase(plaintext.begin(), plaintext.begin() + 4);
        }
    }
    
    return plaintext;
}

double EnhancedMEMOSTPProtocol::getEnergyWeight() const { 
    return optimizedParams.size() > 0 ? optimizer.getBestEnergyWeight(optimizedParams) : 0.6; 
}

double EnhancedMEMOSTPProtocol::getPowerControl() const { 
    return optimizedParams.size() > 1 ? optimizer.getBestPowerControl(optimizedParams) : 0.7; 
}

double EnhancedMEMOSTPProtocol::getSleepRatio() const { 
    return optimizedParams.size() > 2 ? optimizer.getBestSleepRatio(optimizedParams) : 0.3; 
}

void EnhancedMEMOSTPProtocol::printCryptoStats() const {
    double successRate = (packetsReceived > 0) ? 
        (double)packetsDecrypted / packetsReceived * 100 : 0.0;
    
    std::cout << "\033[1;35m" << std::string(50, '=') << "\033[0m" << std::endl;
    std::cout << "\033[1;35m" << "   CRYPTOGRAPHY STATISTICS   " << "\033[0m" << std::endl;
    std::cout << "\033[1;35m" << std::string(50, '=') << "\033[0m" << std::endl;
    std::cout << "Packets Encrypted: " << packetsEncrypted << std::endl;
    std::cout << "Packets Received:  " << packetsReceived << std::endl;
    std::cout << "Packets Decrypted: " << packetsDecrypted << std::endl;
    std::cout << "Crypto Success Rate: " << std::fixed << std::setprecision(2) 
              << successRate << "%" << std::endl;
    std::cout << "\033[1;35m" << std::string(50, '=') << "\033[0m" << std::endl;
}

void EnhancedMEMOSTPProtocol::printProtocolStats() const {
    std::cout << "\033[1;36m" << std::string(50, '=') << "\033[0m" << std::endl;
    std::cout << "\033[1;36m" << "   MEMOSTP PROTOCOL STATISTICS   " << "\033[0m" << std::endl;
    std::cout << "\033[1;36m" << std::string(50, '=') << "\033[0m" << std::endl;
    std::cout << "Crypto Enabled: " << (cryptoEnabled ? "Yes" : "No") << std::endl;
    std::cout << "Optimized Parameters: " << std::endl;
    std::cout << "  - Energy Weight: " << std::fixed << std::setprecision(4) 
              << getEnergyWeight() << std::endl;
    std::cout << "  - Power Control: " << getPowerControl() << std::endl;
    std::cout << "  - Sleep Ratio:   " << getSleepRatio() << std::endl;
    std::cout << "\033[1;36m" << std::string(50, '=') << "\033[0m" << std::endl;
}