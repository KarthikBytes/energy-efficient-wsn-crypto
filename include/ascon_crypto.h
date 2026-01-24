#ifndef ASCON_CRYPTO_H
#define ASCON_CRYPTO_H

#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <iostream>
#include <iomanip>

class AsconCrypto {
private:
    static const int ASCON_128_KEY_SIZE = 16;
    static const int ASCON_128_IV_SIZE = 16;
    static const int ASCON_RATE = 64;
    static const int ASCON_a = 12;
    static const int ASCON_b = 6;
    
    uint64_t state[5];
    
    void Permutation(uint64_t* s, int rounds);
    
public:
    AsconCrypto();
    
    void Initialize(const uint8_t* key, const uint8_t* nonce);
    std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& plaintext, 
                                 uint32_t packetId, uint32_t nodeId);
    std::vector<uint8_t> Decrypt(const std::vector<uint8_t>& ciphertext, 
                                 uint32_t packetId, uint32_t nodeId);
    
    void PrintCryptoMetrics() const;
    
    // For testing
    static void TestCrypto();
    
private:
    uint32_t packetsEncrypted;
    uint32_t packetsDecrypted;
    uint32_t decryptionFailures;
};

#endif // ASCON_CRYPTO_H