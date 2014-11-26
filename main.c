#include <windows.h>
#include <CommCtrl.h>
#include "resource.h"

#pragma optimize("gsy", on)
#pragma comment(linker, \
    "\"/manifestdependency:type='Win32' "\
    "name='Microsoft.Windows.Common-Controls' "\
    "version='6.0.0.0' "\
    "processorArchitecture='*' "\
    "publicKeyToken='6595b64144ccf1df' "\
    "language='*'\"")

#pragma comment(lib, "ComCtl32.lib")

#define BUFSIZ 255U
#define ID_TRAY_APP_ICON 5000
#define WM_TRAYICON (WM_USER + 1)
#define MUTEX_NAME L"MagicWorkerMutex"
#define DEF_TIMEOUT L"30"

struct {
    PNOTIFYICONDATA notifyIconData;

    struct {
        WCHAR appName[BUFSIZ];
        WCHAR appHint[BUFSIZ];

        WCHAR err[BUFSIZ];
        WCHAR errWinCreat[BUFSIZ];
        WCHAR errNoMem[BUFSIZ];
    } Strings;
} App;

struct {
    HANDLE thread;
    POINT cursor;
    RECT desktop;
    DWORD timeout;
} Worker;

UINT WM_TASKBARCREATED = 0;

DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    DWORD tick = 0;
    WCHAR buf[BUFSIZ];

    GetWindowRect(GetDesktopWindow(), &Worker.desktop);
    srand((unsigned) GetTickCount());

    while (Worker.timeout) {
        wsprintf(buf, L"%02d:%02d:%02d", tick / 3600, (tick / 60) % 60, tick % 60);
        SetDlgItemText(App.notifyIconData->hWnd, IDC_TIME, buf);

        if (tick > 0 && tick % 60 == 0) {
            wsprintf(App.notifyIconData->szInfo, L"Elapsed time: %s", buf);
            Shell_NotifyIcon(NIM_MODIFY, App.notifyIconData);
        }

        if (tick++ % Worker.timeout == 0) {
            Worker.cursor.x = (LONG) ((DOUBLE) rand() / RAND_MAX * Worker.desktop.right);
            Worker.cursor.y = (LONG) ((DOUBLE) rand() / RAND_MAX * Worker.desktop.bottom);
            SetCursorPos(Worker.cursor.x, Worker.cursor.y);
        }

        Sleep(1000);
    }

    return 0;
}

INT WorkerStart()
{
    Worker.thread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);

    return 0;
}

INT WorkerStop()
{
    return TerminateThread(Worker.thread, 0);
}

VOID WorkerMinimize()
{
    Shell_NotifyIcon(NIM_ADD, App.notifyIconData);
    ShowWindow(App.notifyIconData->hWnd, SW_HIDE);
}

VOID WorkerRestore()
{
    Shell_NotifyIcon(NIM_DELETE, App.notifyIconData);
    ShowWindow(App.notifyIconData->hWnd, SW_SHOW);
}

LRESULT CALLBACK LowLevelKeyboardProc(INT nCode, WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT *pKeyBoard = (KBDLLHOOKSTRUCT *) lParam;

    switch (wParam) {
    case WM_KEYUP:
        if (pKeyBoard->vkCode == VK_ESCAPE) {
            WorkerStop();
            WorkerRestore();
            break;
        }
    default:
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    return 0;
}

INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND hButton = GetDlgItem(hDlg, IDC_START);
    WCHAR buf[BUFSIZ];

    HICON hIcon;

    if (msg == WM_TASKBARCREATED && !IsWindowVisible(hDlg)) {
        WorkerMinimize();
        return 0;
    }

    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_START:
            WorkerMinimize();
            WorkerStart();
            break;

        case IDC_TIMEOUT:
            if (HIWORD(wParam) == EN_CHANGE) {
                GetDlgItemText(hDlg, IDC_TIMEOUT, buf, BUFSIZ);
                EnableWindow(hButton, (Worker.timeout = _wtoi(buf)) > 0);
            }
        }
        break;

    case WM_INITDIALOG:
        SetWindowText(hDlg, App.Strings.appName);

        hIcon = (HICON) LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 64, 64, 0);
        SendDlgItemMessage(hDlg, IDC_START, BM_SETIMAGE, IMAGE_ICON, (LPARAM) hIcon);

        SendDlgItemMessage(hDlg, IDC_TIMEOUT, EM_LIMITTEXT, 5, 0);
        SetDlgItemText(hDlg, IDC_TIMEOUT, DEF_TIMEOUT);

        return TRUE;

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return TRUE;

    case WM_DESTROY:
        PostQuitMessage(0);
        return TRUE;

    case WM_TRAYICON:
        switch (lParam) {
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            WorkerStop();
            WorkerRestore();
        }
    }

    return FALSE;
}

PNOTIFYICONDATA WorkerNotifyIconData(HINSTANCE hInstance, HWND hWnd)
{
    PNOTIFYICONDATA notifyIconData;
    size_t size = sizeof(NOTIFYICONDATA);

    notifyIconData = (PNOTIFYICONDATA) HeapAlloc(GetProcessHeap(), 0, size);
    if (notifyIconData == NULL)
        return NULL;

    memset(notifyIconData, 0, size);

    notifyIconData->cbSize = size;
    notifyIconData->hWnd = hWnd;
    notifyIconData->uID = ID_TRAY_APP_ICON;
    notifyIconData->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    notifyIconData->dwInfoFlags = NIIF_INFO;
    notifyIconData->uCallbackMessage = WM_TRAYICON;
    notifyIconData->hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));

    lstrcpy(notifyIconData->szTip, App.Strings.appName);
    lstrcpy(notifyIconData->szInfoTitle, App.Strings.appName);
    lstrcpy(notifyIconData->szInfo, App.Strings.appHint);

    return notifyIconData;
}

VOID WorkerLoadResources(HINSTANCE hInstance)
{
    LoadString(hInstance, IDS_APP_NAME, App.Strings.appName, BUFSIZ);
    LoadString(hInstance, IDS_APP_HINT, App.Strings.appHint, BUFSIZ);

    LoadString(hInstance, IDS_ERR, App.Strings.err, BUFSIZ);
    LoadString(hInstance, IDS_ERR_WINCREAT, App.Strings.errWinCreat, BUFSIZ);
    LoadString(hInstance, IDS_ERR_NOMEM, App.Strings.errNoMem, BUFSIZ);
}

VOID WorkerError(LPCWSTR msg)
{
    MessageBox(NULL, msg, App.Strings.err, MB_ICONEXCLAMATION | MB_OK);
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
    HANDLE hMutex;
    HWND hDlg;
    MSG msg;

    if ((hMutex = OpenMutex(MUTEX_ALL_ACCESS, TRUE, MUTEX_NAME)) == NULL) {
        CreateMutex(NULL, TRUE, MUTEX_NAME);
    } else {
        ReleaseMutex(hMutex);
        return EXIT_SUCCESS;
    }

    InitCommonControls();
    WorkerLoadResources(hInstance);

    WM_TASKBARCREATED = RegisterWindowMessageA("TaskbarCreated") ;

    if ((hDlg = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_MAIN), 0, DlgProc, 0)) == NULL) {
        WorkerError(App.Strings.errWinCreat);
        return EXIT_FAILURE;
    }

    if ((App.notifyIconData = WorkerNotifyIconData(hInstance, hDlg)) == NULL) {
        WorkerError(App.Strings.errNoMem);
        return EXIT_FAILURE;
    }

    ShowWindow(hDlg, nCmdShow);

    SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (!IsWindowVisible(hDlg)) {
        Shell_NotifyIcon(NIM_DELETE, App.notifyIconData);
    }

    HeapFree(GetProcessHeap(), 0, App.notifyIconData);

    return msg.wParam;
}