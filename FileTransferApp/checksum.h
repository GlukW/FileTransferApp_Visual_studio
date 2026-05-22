#pragma once
#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <string>
#include <windows.h>
#include <wincrypt.h>

class Checksum {
public:
    static std::string calculateMD5(const std::string& filePath);
    static bool verifyMD5(const std::string& filePath, const std::string& expectedHex);
};

#endif