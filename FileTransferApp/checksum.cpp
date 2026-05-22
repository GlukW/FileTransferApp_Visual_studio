#include "checksum.h"
#include <fstream>
#include <sstream>
#include <iomanip>

std::string Checksum::calculateMD5(const std::string& filePath) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    char buffer[65536];
    while (file.read(buffer, sizeof(buffer))) {
        CryptHashData(hHash, reinterpret_cast<const BYTE*>(buffer), static_cast<DWORD>(file.gcount()), 0);
    }
    if (file.gcount() > 0) {
        CryptHashData(hHash, reinterpret_cast<const BYTE*>(buffer), static_cast<DWORD>(file.gcount()), 0);
    }

    BYTE hashBytes[16];
    DWORD hashLen = sizeof(hashBytes);
    CryptGetHashParam(hHash, HP_HASHVAL, hashBytes, &hashLen, 0);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) {
        ss << std::setw(2) << static_cast<int>(hashBytes[i]);
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return ss.str();
}

bool Checksum::verifyMD5(const std::string& filePath, const std::string& expectedHex) {
    std::string actual = calculateMD5(filePath);
    // Case-insensitive comparison
    return _stricmp(actual.c_str(), expectedHex.c_str()) == 0;
}