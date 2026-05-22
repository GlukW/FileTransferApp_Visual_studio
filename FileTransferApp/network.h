#pragma once
#ifndef NETWORK_H
#define NETWORK_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

struct FileHeader {
    std::string type;
    std::string filename;
    long long size = 0;
    std::string checksum;
};

class NetworkManager {
public:
    using ProgressCallback = std::function<void(int percent)>;
    using LogCallback = std::function<void(const std::string&)>;
    using FinishedCallback = std::function<void(bool success, const std::string& message)>;

    NetworkManager();
    ~NetworkManager();

    bool startServer(int port, const std::string& receiveDir,
        LogCallback logCb, ProgressCallback progressCb, FinishedCallback finishedCb);

    bool connectToServer(const std::string& ip, int port, LogCallback logCb);
    void sendFile(const std::string& filePath, ProgressCallback progressCb,
        FinishedCallback finishedCb, LogCallback logCb);
    void stop();
    bool isConnected() const { return clientSocket != INVALID_SOCKET; }

private:
    SOCKET serverSocket;
    SOCKET clientSocket;
    std::atomic<bool> running;
    std::thread serverThread;

    std::string m_receiveDir;
    LogCallback m_serverLogCb;
    ProgressCallback m_serverProgressCb;
    FinishedCallback m_serverFinishedCb;

    void acceptConnection();
    void receiveFile(const std::string& saveDir, ProgressCallback progressCb,
        FinishedCallback finishedCb, LogCallback logCb);

    bool sendHeader(const FileHeader& header);
    bool receiveHeader(FileHeader& header);
    std::string serializeJSON(const FileHeader& header);
    bool parseJSON(const std::string& json, FileHeader& header);
};

#endif