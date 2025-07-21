#define UNICODE
#define _UNICODE

#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <mmsystem.h>
#include "resource.h"
#pragma comment(lib, "winmm.lib")

HWND hwndMain, hwndEdit, hwndAttachBtn, hwndHotkeyBtn, hwndHotkeyLabel, hwndStatus, hwndMuteBtn;
HFONT hFont;
DWORD targetPID = 0;
bool isPaused = false;
bool hotkeyCaptureMode = false;
bool isMuted = false;
UINT currentHotkey = VK_F9;
int hotkeyID = 1;

std::wstring currentProcessName = L"";

COLORREF bgColor = RGB(30, 30, 30);
COLORREF textColor = RGB(255, 255, 255);

DWORD GetPIDByName(const std::wstring& name) {
    PROCESSENTRY32W pe32 = { sizeof(PROCESSENTRY32W) };
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD pid = 0;

    if (Process32FirstW(hSnap, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, name.c_str()) == 0) {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return pid;
}

std::vector<DWORD> GetThreadIDs(DWORD pid) {
    std::vector<DWORD> ids;
    THREADENTRY32 te32 = { sizeof(te32) };
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (Thread32First(hSnap, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid)
                ids.push_back(te32.th32ThreadID);
        } while (Thread32Next(hSnap, &te32));
    }
    CloseHandle(hSnap);
    return ids;
}

void SuspendProcess(DWORD pid) {
    for (auto tid : GetThreadIDs(pid)) {
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME, FALSE, tid);
        if (h) {
            SuspendThread(h);
            CloseHandle(h);
        }
    }
}

void ResumeProcess(DWORD pid) {
    for (auto tid : GetThreadIDs(pid)) {
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME, FALSE, tid);
        if (h) {
            ResumeThread(h);
            CloseHandle(h);
        }
    }
}

void UpdateStatus() {
    SetWindowTextW(hwndStatus, isPaused ? L"Status: Paused" : L"Status: Running");
}

void SetFontAndTheme(HWND hwnd) {
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(bgColor));
}

void PlaySoundFromResource(int resID) {
    if (isMuted) return;

    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCE(resID), L"WAVE");
    if (!hRes) return;

    HGLOBAL hResLoad = LoadResource(NULL, hRes);
    if (!hResLoad) return;

    void* pResData = LockResource(hResLoad);
    if (!pResData) return;

    PlaySoundW(reinterpret_cast<LPCWSTR>(pResData), NULL, SND_MEMORY | SND_ASYNC);
}

std::wstring VkToString(UINT vk) {
    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    wchar_t name[128] = { 0 };

    LONG lParam = scanCode << 16;
    if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN ||
        vk == VK_PRIOR || vk == VK_NEXT || vk == VK_END || vk == VK_HOME ||
        vk == VK_INSERT || vk == VK_DELETE) {
        lParam |= (1 << 24); // extended key
    }

    if (GetKeyNameTextW(lParam, name, 128) > 0)
        return name;
    else
        return L"Unknown";
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, textColor);
        SetBkColor(hdc, bgColor);
        return (INT_PTR)CreateSolidBrush(bgColor);
    }

    case WM_CREATE: {
        hFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        hwndEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            20, 20, 250, 25, hwnd, NULL, NULL, NULL);
        hwndAttachBtn = CreateWindowW(L"BUTTON", L"Attach", WS_CHILD | WS_VISIBLE,
            280, 20, 80, 25, hwnd, (HMENU)1, NULL, NULL);
        hwndHotkeyLabel = CreateWindowW(L"STATIC", L"Hotkey: F9", WS_CHILD | WS_VISIBLE,
            20, 60, 150, 25, hwnd, NULL, NULL, NULL);
        hwndHotkeyBtn = CreateWindowW(L"BUTTON", L"Change Hotkey", WS_CHILD | WS_VISIBLE,
            180, 60, 120, 25, hwnd, (HMENU)2, NULL, NULL);
        hwndMuteBtn = CreateWindowW(L"BUTTON", L"Mute Audio", WS_CHILD | WS_VISIBLE,
            310, 60, 80, 25, hwnd, (HMENU)3, NULL, NULL);
        hwndStatus = CreateWindowW(L"STATIC", L"Status: Not Attached", WS_CHILD | WS_VISIBLE,
            20, 100, 200, 25, hwnd, NULL, NULL, NULL);

        SetFontAndTheme(hwndEdit);
        SetFontAndTheme(hwndAttachBtn);
        SetFontAndTheme(hwndHotkeyLabel);
        SetFontAndTheme(hwndHotkeyBtn);
        SetFontAndTheme(hwndMuteBtn);
        SetFontAndTheme(hwndStatus);
        break;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case 1: {
            wchar_t buffer[256];
            GetWindowTextW(hwndEdit, buffer, 255);
            currentProcessName = buffer;
            targetPID = GetPIDByName(currentProcessName);
            if (targetPID) {
                SetWindowTextW(hwndStatus, L"Status: Running");
                RegisterHotKey(hwnd, hotkeyID, 0, currentHotkey);
            } else {
                SetWindowTextW(hwndStatus, L"Status: Process Not Found");
            }
            break;
        }
        case 2: {
            hotkeyCaptureMode = true;
            SetWindowTextW(hwndHotkeyLabel, L"Press any key...");
            SetFocus(hwnd); // ensure main window receives key
            break;
        }
        case 3: {
            isMuted = !isMuted;
            SetWindowTextW(hwndMuteBtn, isMuted ? L"Unmute Audio" : L"Mute Audio");
            break;
        }
        }
        break;
    }

    case WM_HOTKEY: {
        if (wParam == hotkeyID && targetPID) {
            isPaused = !isPaused;
            if (isPaused) {
                SuspendProcess(targetPID);
                PlaySoundFromResource(IDR_WAV_PAUSE);
            } else {
                ResumeProcess(targetPID);
                PlaySoundFromResource(IDR_WAV_RESUME);
            }
            UpdateStatus();
        }
        break;
    }

    case WM_KEYDOWN: {
        if (hotkeyCaptureMode) {
            hotkeyCaptureMode = false;
            currentHotkey = (UINT)wParam;

            UnregisterHotKey(hwnd, hotkeyID);
            RegisterHotKey(hwnd, hotkeyID, 0, currentHotkey);

            std::wstring label = L"Hotkey: " + VkToString(currentHotkey);
            SetWindowTextW(hwndHotkeyLabel, label.c_str());
        }
        break;
    }

    case WM_DESTROY:
        UnregisterHotKey(hwnd, hotkeyID);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"EWCMainWindow";
    wc.hbrBackground = CreateSolidBrush(bgColor);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EWC_ICON));

    RegisterClassW(&wc);

    hwndMain = CreateWindowW(wc.lpszClassName, L"Watch Clip by @8.7_.",
        WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 180,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
