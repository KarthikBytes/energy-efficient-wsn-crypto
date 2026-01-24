#include "ascon_crypto.h"
#include <algorithm>
#include <random>

AsconCrypto::AsconCrypto() : packetsEncrypted(0), packetsDecrypted(0), decryptionFailures(0) {
    memset(state, 0, sizeof(state));
}

void AsconCrypto::Permutation(uint64_t* s, int rounds) {
    for (int r = 0; r < rounds; r++) {
        s[2] ^= ((0x0F - r) << 4) | r;
        
        uint64_t x0 = s[0];
        uint64_t x1 = s[1];
        uint64_t x2 = s[2];
        uint64_t x3 = s[3];
        uint64_t x4 = s[4];
        
        s[0] = x4 ^ x1 ^ ((x2 & ~x1) << 1);
        s[1] = x0 ^ x2 ^ ((x3 & ~x2) << 1);
        s[2] = x1 ^ x3 ^ ((x4 & ~x3) << 1);
        s[3] = x2 ^ x4 ^ ((x0 & ~x4) << 1);
        s[4] = x3 ^ x0 ^ ((x1 & ~x0) << 1);
        
        s[0] ^= ((s[0] >> 19) | (s[0] << (64 - 19))) ^ ((s[0] >> 28) | (s[0] << (64 - 28)));
        s[1] ^= ((s[1] >> 61) | (s[1] << (64 - 61))) ^ ((s[1] >> 39) | (s[1] << (64 - 39)));
        s[2] ^= ((s[2] >> 1)  | (s[2] << (64 - 1)))  ^ ((s[2] >> 6)  | (s[2] << (64 - 6)));
        s[3] ^= ((s[3] >> 10) | (s[3] << (64 - 10))) ^ ((s[3] >> 17) | (s[3] << (64 - 17)));
        s[4] ^= ((s[4] >> 7)  | (s[4] << (64 - 7)))  ^ ((s[4] >> 41) | (s[4] << (64 - 41)));
    }
}

void AsconCrypto::Initialize(const uint8_t* key, const uint8_t* nonce) {
    std::cout << "\033[1;32m" << "=" << std::string(60, '=') << "=" << "\033[0m" << std::endl;
    std::cout << "\033[1;32m" << "  ASCON-128 CRYPTOGRAPHY INITIALIZATION  " << "\033[0m" << std::endl;
    std::cout << "\033[1;32m" << "=" << std::string(60, '=') << "=" << "\033[0m" << std::endl;
    
    state[0] = ((uint64_t)key[0] << 56) | ((uint64_t)key[1] << 48) | ((uint64_t)key[2] << 40) | ((uint64_t)key[3] << 32) |
              ((uint64_t)key[4] << 24) | ((uint64_t)key[5] << 16) | ((uint64_t)key[6] << 8) | key[7];
    state[1] = ((uint64_t)key[8] << 56) | ((uint64_t)key[9] << 48) | ((uint64_t)key[10] << 40) | ((uint64_t)key[11] << 32) |
              ((uint64_t)key[12] << 24) | ((uint64_t)key[13] << 16) | ((uint64_t)key[14] << 8) | key[15];
    state[2] = ((uint64_t)nonce[0] << 56) | ((uint64_t)nonce[1] << 48) | ((uint64_t)nonce[2] << 40) | ((uint64_t)nonce[3] << 32) |
              ((uint64_t)nonce[4] << 24) | ((uint64_t)nonce[5] << 16) | ((uint64_t)nonce[6] << 8) | nonce[7];
    state[3] = ((uint64_t)nonce[8] << 56) | ((uint64_t)nonce[9] << 48) | ((uint64_t)nonce[10] << 40) | ((uint64_t)nonce[11] << 32) |
              ((uint64_t)nonce[12] << 24) | ((uint64_t)nonce[13] << 16) | ((uint64_t)nonce[14] << 8) | nonce[15];
    state[4] = 0x0000000000000080ULL;
    
    Permutation(state, ASCON_a);
    
    state[3] ^= ((uint64_t)key[0] << 56) | ((uint64_t)key[1] << 48) | ((uint64_t)key[2] << 40) | ((uint64_t)key[3] << 32) |
               ((uint64_t)key[4] << 24) | ((uint64_t)key[5] << 16) | ((uint64_t)key[6] << 8) | key[7];
    state[4] ^= ((uint64_t)key[8] << 56) | ((uint64_t)key[9] << 48) | ((uint64_t)key[10] << 40) | ((uint64_t)key[11] << 32) |
               ((uint64_t)key[12] << 24) | ((uint64_t)key[13] << 16) | ((uint64_t)key[14] << 8) | key[15];
    
    std::cout << "✓ ASCON-128 Initialized Successfully\n" << std::endl;
}

std::vector<uint8_t> AsconCrypto::Encrypt(const std::vector<uint8_t>& plaintext, 
                                         uint32_t packetId, uint32_t nodeId) {
    packetsEncrypted++;
    
    uint64_t currentState[5];
    memcpy(currentState, state, sizeof(state));
    
    std::vector<uint8_t> ciphertext(plaintext.size());
    
    for (size_t i = 0; i < plaintext.size(); i += ASCON_RATE/8) {
        size_t blockSize = std::min<size_t>(ASCON_RATE/8, plaintext.size() - i);
        
        for (size_t j = 0; j < blockSize; j++) {
            uint8_t byte = plaintext[i + j];
            uint8_t stateByte = (currentState[j/8] >> (56 - 8*(j%8))) & 0xFF;
            ciphertext[i + j] = byte ^ stateByte;
            currentState[j/8] ^= ((uint64_t)byte << (56 - 8*(j%8)));
        }
        
        if (i + blockSize < plaintext.size()) {
            Permutation(currentState, ASCON_b);
        }
    }
    
    currentState[4] ^= 0x01;
    Permutation(currentState, ASCON_a);
    
    std::vector<uint8_t> tag(16);
    for (int i = 0; i < 16; i++) {
        tag[i] = (currentState[i/8] >> (56 - 8*(i%8))) & 0xFF;
    }
    
    ciphertext.insert(ciphertext.end(), tag.begin(), tag.end());
    return ciphertext;
}

std::vector<uint8_t> AsconCrypto::Decrypt(const std::vector<uint8_t>& ciphertext, 
                                         uint32_t packetId, uint32_t nodeId) {
    if (ciphertext.size() < 16) {
        decryptionFailures++;
        return {};
    }
    
    uint64_t currentState[5];
    memcpy(currentState, state, sizeof(state));
    
    size_t dataSize = ciphertext.size() - 16;
    std::vector<uint8_t> plaintext(dataSize);
    
    for (size_t i = 0; i < dataSize; i += ASCON_RATE/8) {
        size_t blockSize = std::min<size_t>(ASCON_RATE/8, dataSize - i);
        
        for (size_t j = 0; j < blockSize; j++) {
            uint8_t cByte = ciphertext[i + j];
            uint8_t stateByte = (currentState[j/8] >> (56 - 8*(j%8))) & 0xFF;
            plaintext[i + j] = cByte ^ stateByte;
            currentState[j/8] ^= ((uint64_t)plaintext[i + j] << (56 - 8*(j%8)));
        }
        
        if (i + blockSize < dataSize) {
            Permutation(currentState, ASCON_b);
        }
    }
    
    currentState[4] ^= 0x01;
    Permutation(currentState, ASCON_a);
    
    bool verificationSuccess = true;
    for (int i = 0; i < 16; i++) {
        uint8_t expectedTag = (currentState[i/8] >> (56 - 8*(i%8))) & 0xFF;
        if (expectedTag != ciphertext[dataSize + i]) {
            verificationSuccess = false;
            break;
        }
    }
    
    if (verificationSuccess) {
        packetsDecrypted++;
        return plaintext;
    } else {
        decryptionFailures++;
        return {};
    }
}

void AsconCrypto::PrintCryptoMetrics() const {
    double successRate = (packetsEncrypted > 0) ? 
        (double)packetsDecrypted / packetsEncrypted * 100 : 0.0;
    
    std::cout << "\033[1;34m" << std::string(60, '-') << "\033[0m" << std::endl;
    std::cout << "\033[1;34m" << "ASCON-128 CRYPTOGRAPHY METRICS" << "\033[0m" << std::endl;
    std::cout << "\033[1;34m" << std::string(60, '-') << "\033[0m" << std::endl;
    std::cout << "Algorithm: ASCON-128 (NIST Lightweight Standard)" << std::endl;
    std::cout << "Key Size: 128 bits" << std::endl;
    std::cout << "State: 320 bits (5×64-bit words)" << std::endl;
    std::cout << "Packets Encrypted: " << packetsEncrypted << std::endl;
    std::cout << "Packets Decrypted: " << packetsDecrypted << std::endl;
    std::cout << "Decryption Failures: " << decryptionFailures << std::endl;
    std::cout << "Success Rate: " << std::fixed << std::setprecision(2) 
              << successRate << "%" << std::endl;
    std::cout << "\033[1;34m" << std::string(60, '-') << "\033[0m" << std::endl;
}