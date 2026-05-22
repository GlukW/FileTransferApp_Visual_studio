#undef min
#undef max

#include "network.h"
#include "checksum.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

NetworkManager::NetworkManager()
    : serverSocket(INVALID_SOCKET), clientSocket(INVALID_SOCKET), running(false) {

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

NetworkManager::~NetworkManager() {
    stop();
    WSACleanup();
}

bool NetworkManager::startServer(int port, const std::string& receiveDir,
    LogCallback logCb, ProgressCallback progressCb, FinishedCallback finishedCb) {
    m_receiveDir = receiveDir;
    m_serverLogCb = logCb;
    m_serverProgressCb = progressCb;
    m_serverFinishedCb = finishedCb;

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        logCb("Error creating socket");
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        logCb("Bind error: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket);
        return false;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        logCb("Listen error");
        closesocket(serverSocket);
        return false;
    }

    running = true;
    serverThread = std::thread(&NetworkManager::acceptConnection, this);
    return true;
}

bool NetworkManager::connectToServer(const std::string& ip, int port, LogCallback logCb) {
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        logCb("Error creating socket");
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    addr.sin_port = htons(port);

    if (::connect(clientSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        logCb("Connection error");
        closesocket(clientSocket);
        return false;
    }

    logCb("Connected to " + ip + ":" + std::to_string(port));
    return true;
}

void NetworkManager::acceptConnection() {
    while (running) {
        sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET client = accept(serverSocket, (sockaddr*)&clientAddr, &addrLen);

        if (client != INVALID_SOCKET && running) {
            clientSocket = client;
            m_serverLogCb("Client connected");
            receiveFile(m_receiveDir, m_serverProgressCb, m_serverFinishedCb, m_serverLogCb);
        }
    }
}

void NetworkManager::sendFile(const std::string& filePath, ProgressCallback progressCb,
    FinishedCallback finishedCb, LogCallback logCb) {
    if (clientSocket == INVALID_SOCKET) {
        finishedCb(false, "Not connected");
        return;
    }

    std::thread([this, filePath, progressCb, finishedCb, logCb]() {
        try {
            std::ifstream file(filePath, std::ios::binary | std::ios::ate);
            if (!file) {
                finishedCb(false, "Cannot open file");
                return;
            }

            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::string checksum = Checksum::calculateMD5(filePath);
            std::string filename = std::filesystem::path(filePath).filename().string();

            FileHeader header;
            header.type = "file_meta";
            header.filename = filename;
            header.size = size;
            header.checksum = checksum;

            if (!sendHeader(header)) {
                finishedCb(false, "Error sending header");
                return;
            }

            logCb("[SENDER] Checksum: " + checksum + " Size: " + std::to_string(size));

            char buffer[65536];
            long long totalSent = 0;

            while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
                std::streamsize bytesRead = file.gcount();
                int sent = send(clientSocket, buffer, (int)bytesRead, 0);

                if (sent == SOCKET_ERROR) {
                    finishedCb(false, "Error sending data");
                    return;
                }

                totalSent += sent;
                int percent = (int)((totalSent * 100) / size);
                progressCb(percent);
            }

            logCb("File sent successfully");
            finishedCb(true, "Sent");
        }
        catch (const std::exception& e) {
            finishedCb(false, std::string("Error: ") + e.what());
        }
        }).detach();
}

void NetworkManager::receiveFile(const std::string& saveDir, ProgressCallback progressCb,
    FinishedCallback finishedCb, LogCallback logCb) {
    std::thread([this, saveDir, progressCb, finishedCb, logCb]() {
        try {
            FileHeader header;
            if (!receiveHeader(header)) {
                finishedCb(false, "Error receiving header");
                return;
            }

            std::string savePath = saveDir + "\\" + header.filename;
            std::ofstream file(savePath, std::ios::binary);
            if (!file) {
                finishedCb(false, "Cannot create file");
                return;
            }

            logCb("[RECEIVER] Expected: " + header.checksum + " Size: " + std::to_string(header.size));

            // Читаеn тело файла
            char buffer[65536];
            long long totalReceived = 0;

            while (totalReceived < header.size) {
                long long remaining = header.size - totalReceived;
                long long toRead = (sizeof(buffer) < remaining) ? sizeof(buffer) : remaining;
                int bytesToRead = static_cast<int>(toRead);

                int received = recv(clientSocket, buffer, bytesToRead, 0);

                if (received <= 0) break;

                file.write(buffer, received);
                totalReceived += received;

                int percent = (int)((totalReceived * 100) / header.size);
                progressCb(percent);
            }

            file.close();

            std::string actualChecksum = Checksum::calculateMD5(savePath);
            logCb("[RECEIVER] Actual: " + actualChecksum);

            if (_stricmp(actualChecksum.c_str(), header.checksum.c_str()) == 0) {
                logCb("File received and verified");
                finishedCb(true, "Received");
            }
            else {
                std::filesystem::remove(savePath);
                logCb("Checksum mismatch");
                finishedCb(false, "Checksum mismatch");
            }
        }
        catch (const std::exception& e) {
            finishedCb(false, std::string("Error: ") + e.what());
        }
        }).detach();
}

bool NetworkManager::sendHeader(const FileHeader& header) {
    std::string json = serializeJSON(header) + "\n";
    int sent = send(clientSocket, json.c_str(), (int)json.length(), 0);
    return sent != SOCKET_ERROR;
}

bool NetworkManager::receiveHeader(FileHeader& header) {
    std::string json;
    char c;

    while (true) {
        int received = recv(clientSocket, &c, 1, 0);
        if (received <= 0) return false;
        if (c == '\n') break;
        json += c;
    }

    return parseJSON(json, header);
}

std::string NetworkManager::serializeJSON(const FileHeader& header) {
    std::stringstream ss;
    ss << "{\"type\":\"" << header.type << "\","
        << "\"filename\":\"" << header.filename << "\","
        << "\"size\":" << header.size << ","
        << "\"checksum\":\"" << header.checksum << "\"}";
    return ss.str();
}

bool NetworkManager::parseJSON(const std::string& json, FileHeader& header) {
    size_t pos;

    if ((pos = json.find("\"filename\":\"")) != std::string::npos) {
        size_t start = pos + 12;
        size_t end = json.find("\"", start);
        header.filename = json.substr(start, end - start);
    }

    if ((pos = json.find("\"size\":")) != std::string::npos) {
        size_t start = pos + 7;
        size_t end = json.find(",", start);
        header.size = std::stoll(json.substr(start, end - start));
    }

    if ((pos = json.find("\"checksum\":\"")) != std::string::npos) {
        size_t start = pos + 12;
        size_t end = json.find("\"", start);
        header.checksum = json.substr(start, end - start);
    }

    return true;
}

void NetworkManager::stop() {
    running = false;
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
    if (serverThread.joinable()) {
        serverThread.join();
    }
}