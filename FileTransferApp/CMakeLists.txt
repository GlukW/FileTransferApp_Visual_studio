cmake_minimum_required(VERSION 3.15)
project(FileTransferApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I\"C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/shared\"")

# WIN32 важно для GUI-приложений (убирает консольное окно)
add_executable(${PROJECT_NAME} WIN32
    main.cpp
    network.cpp network.h
    checksum.cpp checksum.h
    app.rc
    resource.h
)

# Системные библиотеки Windows
target_link_libraries(${PROJECT_NAME} PRIVATE
    ws2_32      # Winsock2
    crypt32     # CryptoAPI (для MD5)
    comctl32    # Common Controls (Progress Bar)
)

# Подключаем заголовочные директории
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})