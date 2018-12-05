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

#define BUFSIZ       255U
#define KEY_TIME     1000
#define DEF_TIMEOUT  L"30"
#define MUTEX_NAME   L"MagicWorkerMutex"

#define WM_TRAYICON  (WM_USER + 1)
#define WM_INVISIBLE (WM_TRAYICON + 1)

struct {
    PNOTIFYICONDATA notifyIconData;

    struct {
        WCHAR appName[BUFSIZ];
        WCHAR appHint[BUFSIZ];

        WCHAR err[BUFSIZ];
        WCHAR errWinCreat[BUFSIZ];
        WCHAR errNoMem[BUFSIZ];

        WCHAR cfgInvisible[BUFSIZ];
    } Strings;
} App;

struct {
    HANDLE hThread;
    DWORD dwTimeout;
    DWORD dwKeyTime;
    UINT prevScreenSaver;
    EXECUTION_STATE prevExecState;
    BOOL isInvisible;
	BOOL isShiftDown;
} Worker;

UINT WM_TASKBARCREATED = 0;

DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    INPUT input;
    DWORD tick = 0;
    WCHAR buf[BUFSIZ];

    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.mouseData = 0;
    input.mi.dy = 0;

    while (Worker.dwTimeout) {
        wsprintf(buf, L"%02d:%02d:%02d", tick / 3600, (tick / 60) % 60, tick % 60);
        SetDlgItemText(App.notifyIconData->hWnd, IDC_TIME, buf);

        if (tick > 0 && tick % 60 == 0) {
            wsprintf(App.notifyIconData->szInfo, L"Elapsed time: %s", buf);
            Shell_NotifyIcon(NIM_MODIFY, App.notifyIconData);
        }

        if (tick++ % Worker.dwTimeout == 0) {
            input.mi.dx = 1;
            SendInput(1, &input, sizeof(input));

            input.mi.dx = -1;
            SendInput(1, &input, sizeof(input));
        }

        Sleep(1000);
    }

    return 0;
}

HANDLE WorkerStart()
{
    Worker.prevExecState = SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
    SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &Worker.prevScreenSaver, 0);
    SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, FALSE, NULL, 0);

    return (Worker.hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL));
}

INT WorkerStop()
{
    SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, Worker.prevScreenSaver, NULL, 0);
    SetThreadExecutionState(Worker.prevExecState);

    return TerminateThread(Worker.hThread, 0);
}

VOID WorkerMinimize()
{
    if (!Worker.isInvisible) {
        Shell_NotifyIcon(NIM_ADD, App.notifyIconData);
    }

    ShowWindow(App.notifyIconData->hWnd, SW_HIDE);
}

VOID WorkerRestore()
{
    if (!Worker.isInvisible) {
        Shell_NotifyIcon(NIM_DELETE, App.notifyIconData);
    }

    ShowWindow(App.notifyIconData->hWnd, SW_SHOW);
}

LRESULT CALLBACK LowLevelKeyboardProc(INT nCode, WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT *pKbd = (KBDLLHOOKSTRUCT *) lParam;

    switch (wParam) {
	case WM_KEYDOWN:
		if (pKbd->vkCode == VK_LSHIFT) {
			Worker.isShiftDown = TRUE;
		}
		break;
    case WM_KEYUP:
		if (pKbd->vkCode == VK_LSHIFT) {
			Worker.isShiftDown = FALSE;
		} else if (Worker.isShiftDown && pKbd->vkCode == VK_ESCAPE) {
            if (pKbd->time - Worker.dwKeyTime < KEY_TIME) {
                WorkerStop();
                WorkerRestore();
				return 0;
            }

            Worker.dwKeyTime = pKbd->time;
        }
    }

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND hButton = GetDlgItem(hDlg, IDC_START);
    HMENU hMenu = GetSystemMenu(hDlg, FALSE);
    HICON hBigIcon, hSmallIcon;
    WCHAR buf[BUFSIZ];

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
                EnableWindow(hButton, (Worker.dwTimeout = _wtoi(buf)) > 0);
            }
        }
        break;

    case WM_SYSCOMMAND:
        switch(LOWORD(wParam)) {
        case WM_INVISIBLE:
            if (Worker.isInvisible = !(GetMenuState(hMenu, WM_INVISIBLE, MF_BYCOMMAND) & MF_CHECKED)) {
                CheckMenuItem(hMenu, WM_INVISIBLE, MF_BYCOMMAND | MF_CHECKED);
            } else {
                CheckMenuItem(hMenu, WM_INVISIBLE, MF_BYCOMMAND | MF_UNCHECKED);
            }
        }
        break;

    case WM_INITDIALOG:
        SetWindowText(hDlg, App.Strings.appName);

        hSmallIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
        SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM) hSmallIcon);

        hBigIcon = (HICON) LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 64, 64, 0);
        SendDlgItemMessage(hDlg, IDC_START, BM_SETIMAGE, IMAGE_ICON, (LPARAM) hBigIcon);

        SendDlgItemMessage(hDlg, IDC_TIMEOUT, EM_LIMITTEXT, 5, 0);
        SetDlgItemText(hDlg, IDC_TIMEOUT, DEF_TIMEOUT);

        DeleteMenu(hMenu, SC_MAXIMIZE, MF_BYCOMMAND);
        DeleteMenu(hMenu, SC_RESTORE, MF_BYCOMMAND);
        DeleteMenu(hMenu, SC_SIZE, MF_BYCOMMAND);

        InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING | MF_CHECKED, WM_INVISIBLE, App.Strings.cfgInvisible);
        InsertMenu(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, 0);

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
        return FALSE;
    }

    return FALSE;
}

PNOTIFYICONDATA WorkerNotifyIconData(HINSTANCE hInstance, HWND hWnd)
{
    PNOTIFYICONDATA notifyIconData;
    DWORD size = sizeof(NOTIFYICONDATA);

    notifyIconData = (PNOTIFYICONDATA) HeapAlloc(GetProcessHeap(), 0, size);
    if (notifyIconData == NULL)
        return NULL;

    memset(notifyIconData, 0, size);

    notifyIconData->cbSize = size;
    notifyIconData->hWnd = hWnd;
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

    LoadString(hInstance, IDS_CFG_INVISIBLE, App.Strings.cfgInvisible, BUFSIZ);
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

    Worker.dwKeyTime = 0;
    Worker.isInvisible = TRUE;

    WM_TASKBARCREATED = RegisterWindowMessageA("TaskbarCreated");

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

    return (INT) msg.wParam;
}
