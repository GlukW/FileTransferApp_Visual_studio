#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#include "resource.h"
#include "network.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

HINSTANCE g_hInst;
HWND g_hWnd;
NetworkManager g_network;
std::string g_selectedFile;

void AddLog(const std::string& message) {
    HWND hList = GetDlgItem(g_hWnd, IDC_LIST_LOG);
    std::string timestamp = std::to_string(GetTickCount() / 1000) + "s - ";
    SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)(timestamp + message).c_str());
    SendMessage(hList, WM_VSCROLL, SB_BOTTOM, 0);
}

void UpdateProgress(int percent) {
    HWND hProgress = GetDlgItem(g_hWnd, IDC_PROGRESS);
    SendMessage(hProgress, PBM_SETPOS, percent, 0);
}

INT_PTR CALLBACK DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        g_hWnd = hWnd;
        InitCommonControls();

        SetDlgItemTextA(hWnd, IDC_EDIT_IP, "127.0.0.1");
        SetDlgItemTextA(hWnd, IDC_EDIT_PORT, "12345");

        char path[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, path);
        std::string recvFolder = std::string(path) + "\\ReceivedFiles";
        CreateDirectoryA(recvFolder.c_str(), nullptr);
        SetDlgItemTextA(hWnd, IDC_EDIT_FOLDER, recvFolder.c_str());

        return TRUE;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BTN_START_SERVER: {
            int port = GetDlgItemInt(hWnd, IDC_EDIT_PORT, nullptr, FALSE);

            char recvPath[MAX_PATH];
            GetDlgItemTextA(hWnd, IDC_EDIT_FOLDER, recvPath, MAX_PATH);

            if (g_network.startServer(port, recvPath, AddLog, UpdateProgress,
                [hWnd](bool success, const std::string& msg) {
                    MessageBoxA(hWnd, msg.c_str(), success ? "Server Result" : "Server Error",
                        success ? MB_ICONINFORMATION : MB_ICONERROR);
                })) {
                AddLog("Server started on port " + std::to_string(port));
                EnableWindow(GetDlgItem(hWnd, IDC_BTN_START_SERVER), FALSE);
            }
            break;
        }

        case IDC_BTN_CONNECT: {
            char ip[64];
            GetDlgItemTextA(hWnd, IDC_EDIT_IP, ip, sizeof(ip));
            int port = GetDlgItemInt(hWnd, IDC_EDIT_PORT, nullptr, FALSE);

            if (g_network.connectToServer(ip, port, AddLog)) {
                EnableWindow(GetDlgItem(hWnd, IDC_BTN_CONNECT), FALSE);
            }
            break;
        }

        case IDC_BTN_SELECT_FILE: {
            OPENFILENAMEA ofn = {};
            char file[MAX_PATH] = "";

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = file;
            ofn.nMaxFile = sizeof(file);
            ofn.lpstrFilter = "All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameA(&ofn)) {
                g_selectedFile = file;
                AddLog("Selected file: " + std::string(file));
            }
            break;
        }

        case IDC_BTN_SEND: {
            if (g_selectedFile.empty()) {
                MessageBoxA(hWnd, "Select a file first!", "Error", MB_ICONWARNING);
                return TRUE;
            }

            if (!g_network.isConnected()) {
                MessageBoxA(hWnd, "Not connected!", "Error", MB_ICONWARNING);
                return TRUE;
            }

            g_network.sendFile(g_selectedFile, UpdateProgress,
                [hWnd](bool success, const std::string& msg) {
                    MessageBoxA(hWnd, msg.c_str(), success ? "Success" : "Error",
                        success ? MB_ICONINFORMATION : MB_ICONERROR);
                },
                AddLog
            );
            break;
        }
        }
        return TRUE;
    }

    case WM_CLOSE: {
        g_network.stop();
        DestroyWindow(hWnd);
        return TRUE;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        return TRUE;
    }
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);

    g_hInst = hInstance;

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    HWND hDialog = CreateDialogParamA(hInstance, MAKEINTRESOURCEA(IDD_MAIN_DIALOG),
        nullptr, DialogProc, 0);
    if (!hDialog) {
        MessageBoxA(nullptr, "Error creating window", "Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hDialog, nCmdShow);
    UpdateWindow(hDialog);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(hDialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}