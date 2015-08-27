// MultiDownloader.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Urlmon.h>
#include <atlstr.h>
#include <WinIoCtl.h>
#include <assert.h>
#include "MultiDownloader.h"
#include "icseapi.h"

#pragma comment(lib, "Ws2_32.lib")

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
HWND g_hWndMain = NULL;
HWND g_hWndURL = NULL;
HWND g_hWndFile = NULL;
WNDPROC g_oldURLProc;
WNDPROC g_oldFileProc;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	OpenURLDlgProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    ControlWindowsProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
HRESULT             OpenURL(HWND);
void                ThreadProc(void* param);
void                OnDownloadProgress(HWND, WPARAM, LPARAM);

#define MAX_URL_LENGTH 512
#define DEFAULT_BUFFER_LENGTH 1024

WCHAR g_wcFile[MAX_URL_LENGTH] = L"C:\\Users\\test\\Desktop\\download.jpg";
WCHAR g_wcURL[MAX_URL_LENGTH] = L"http://www.test.com/img/1.jpg";
char g_acURL[MAX_URL_LENGTH] = {0};
char g_acDomain[MAX_URL_LENGTH] = {0};
int g_factor = 10;
__int64 g_segmentMatrix[10][3] = {{0}};

#define WM_DOWNLOAD_PROGRESS WM_USER + 1

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

    WSADATA wsaData;
    int iResult = 0;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        ICSE_OUTPUTDEBUGSTRING_W(L"WSAStartup failed: %d", iResult);
        return E_FAIL;
    }

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_MULTIDOWNLOADER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MULTIDOWNLOADER));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

    WSACleanup();

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MULTIDOWNLOADER));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_MULTIDOWNLOADER);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 800, 600, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   g_hWndMain = hWnd;
   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
        case IDM_OPENURL:
            OpenURL(hWnd);
            break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
        TextOut(hdc, 5, 0, L"Open URL:", 9);
        TextOut(hdc, 90, 0, g_wcURL, wcslen(g_wcURL));
        TextOut(hdc, 5, 20, L"Save to:", 8);
        TextOut(hdc, 90, 20, g_wcFile, wcslen(g_wcFile));
        for (int i = 0; i < g_factor; i++)
        {
            CString str(L"");
            str.Format(L"[%02d]: %I64d -- %I64d bytes", i+1, g_segmentMatrix[i][1]-g_segmentMatrix[i][0]+1, g_segmentMatrix[i][2]);
            TextOut(hdc, 20, 50 + i * 20, str, str.GetLength());
        }
        EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
    case WM_DOWNLOAD_PROGRESS:
        OnDownloadProgress(hWnd, wParam, lParam);
        break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK OpenURLDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        SetDlgItemText(hDlg, IDC_URL, g_wcURL);
        SetDlgItemText(hDlg, IDC_FILE, g_wcFile);

        g_hWndURL = GetDlgItem(hDlg, IDC_URL);
        g_hWndFile = GetDlgItem(hDlg, IDC_FILE);
        SetFocus(g_hWndURL);
        g_oldURLProc = (WNDPROC)GetWindowLong(g_hWndURL, GWL_WNDPROC);
        SetWindowLong(g_hWndURL, GWL_WNDPROC, (LONG)&ControlWindowsProc);
        g_oldFileProc = (WNDPROC)GetWindowLong(g_hWndFile, GWL_WNDPROC);
        SetWindowLong(g_hWndFile, GWL_WNDPROC, (LONG)&ControlWindowsProc);
        return (INT_PTR)FALSE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            GetDlgItemText(hDlg, IDC_URL, g_wcURL, MAX_URL_LENGTH);
            GetDlgItemText(hDlg, IDC_FILE, g_wcFile, MAX_URL_LENGTH);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)FALSE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

LRESULT CALLBACK ControlWindowsProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if ((message == WM_KEYDOWN) && (wParam == 0x41/*A*/) && ((GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0x8000))
    {
        SendMessage(hWnd, EM_SETSEL, 0, -1);
    }
    if (hWnd == g_hWndURL)
    {
        return g_oldURLProc(hWnd, message, wParam, lParam);
    }
    else if (hWnd == g_hWndFile)
    {
        return g_oldFileProc(hWnd, message, wParam, lParam);
    }
    else
    {
        assert(FALSE);
    }
}

HRESULT OpenURL(HWND hWnd)
{
    HRESULT hr = S_OK;
    INT_PTR ret = DialogBox(hInst, MAKEINTRESOURCE(IDD_OPENURL), hWnd, OpenURLDlgProc);
    if (LOWORD(ret) == IDCANCEL)
    {
        return E_FAIL;
    }
    InvalidateRect(hWnd, NULL, TRUE);

    WCHAR wcDomain[MAX_URL_LENGTH] = {0};
    DWORD cchDecodeUrl = 0;
    CoInternetParseUrl(g_wcURL, PARSE_DOMAIN, 0, wcDomain, MAX_URL_LENGTH, &cchDecodeUrl, 0);

    //int nbytes = WideCharToMultiByte(0, 0, g_szURL, nLength, NULL, 0, NULL, NULL);
    ZeroMemory(g_acURL, MAX_URL_LENGTH);
    WideCharToMultiByte(0, 0, g_wcURL, wcslen(g_wcURL), g_acURL, wcslen(g_wcURL), NULL, NULL);
    
    ZeroMemory(g_acDomain, MAX_URL_LENGTH);
    //nbytes = WideCharToMultiByte(0, 0, szDomain, wcslen(szDomain), NULL, 0, NULL, NULL);
    WideCharToMultiByte(0, 0, wcDomain, wcslen(wcDomain), g_acDomain, wcslen(wcDomain), NULL, NULL);
    
    char sendBuff[DEFAULT_BUFFER_LENGTH] = {0};
    sprintf_s(sendBuff, DEFAULT_BUFFER_LENGTH,
        "HEAD %s HTTP/1.1\r\n" \
        "Host: %s\r\n" \
        "Proxy-Connection: keep-alive\r\n" \
        "Proxy-Authorization: Basic ***********\r\n" \
        "Accept: */*\r\n" \
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.107 Safari/537.36\r\n\r\n",
        strstr(g_acURL, g_acDomain), g_acDomain);

    int iResult = 0;
    ADDRINFOW* result = NULL;
    ADDRINFOW* ptr = NULL;
    ADDRINFOW hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    WCHAR* strProxyURL = L"test.proxy.com";
    WCHAR* strProxyPort = L"8080";

    iResult = GetAddrInfo(strProxyURL, strProxyPort, &hints, &result);
    if (iResult != 0)
    {
        ICSE_OUTPUTDEBUGSTRING_W(L"getaddrinfo failed: %d", iResult);
        return E_FAIL;
    }

    SOCKET connectSocket = INVALID_SOCKET;
    ptr = result;
    connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (connectSocket == INVALID_SOCKET)
    {
        ICSE_OUTPUTDEBUGSTRING_W(L"socket failed: %ld\n", WSAGetLastError());
        FreeAddrInfo(result);
        return E_FAIL;
    }

    int value = 0;
    int size = sizeof(value);
    iResult = getsockopt(connectSocket, SOL_SOCKET, SO_RCVBUF, (char*)&value, &size);

    iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    FreeAddrInfo(result);
    if (iResult == SOCKET_ERROR)
    {
        ICSE_OUTPUTDEBUGSTRING_W(L"connect failed: %d", iResult);
        closesocket(connectSocket);
        return E_FAIL;
    }


    char recvBuff[DEFAULT_BUFFER_LENGTH] = {0};
    iResult = send(connectSocket, sendBuff, (int)strlen(sendBuff), 0);
    if (iResult == SOCKET_ERROR)
    {
        ICSE_OUTPUTDEBUGSTRING_W(L"send failed: %d", WSAGetLastError());
        closesocket(connectSocket);
        return E_FAIL;
    }

    iResult = shutdown(connectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR)
    {
        ICSE_OUTPUTDEBUGSTRING_W(L"shutdown failed %d", WSAGetLastError());
        closesocket(connectSocket);
        return E_FAIL;
    }

    CStringA strHeader("");
    do
    {
        iResult = recv(connectSocket, recvBuff, DEFAULT_BUFFER_LENGTH, 0);
        if (iResult > 0)
        {
            strHeader += recvBuff;
        }
        else if (iResult == 0)
        {
            ICSE_OUTPUTDEBUGSTRING_W(L"Connection closed");
        }
        else
        {
            ICSE_OUTPUTDEBUGSTRING_W(L"recv failed %d", WSAGetLastError());
        }
    } while (iResult > 0);

    if (strHeader.GetLength() > 0)
    {
        int begin = strHeader.Find("Content-Length: ");
        int end = strHeader.Find("\r\n", begin);
        CStringA strLength = strHeader.Mid(begin + 16, end - begin - 16);
        __int64 length = _atoi64(strLength);
        ICSE_OUTPUTDEBUGSTRING_W(L"content length: %I64d", length);

        HANDLE hFile = ::CreateFile(g_wcFile, GENERIC_READ|GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        DWORD dwTemp = 0;
        ::DeviceIoControl(hFile, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &dwTemp, NULL);
        CloseHandle(hFile);

        __int64 segmentLength = length / g_factor;
        for (int i = 0; i < g_factor; i++)
        {
            g_segmentMatrix[i][0] = i * segmentLength;
            g_segmentMatrix[i][1] = g_segmentMatrix[i][0] + segmentLength - 1;
            length -= segmentLength;
        }
        g_segmentMatrix[g_factor-1][1] += length;

        for (int i = 0; i < g_factor; i++)
        {
            _beginthread(ThreadProc, 0, (void*)i);
        }
    }

    closesocket(connectSocket);
    return hr;
}

void ThreadProc(void* param)
{
    int index = (int)param;
    ICSE_OUTPUTDEBUGSTRING_W(L"[%d][%I64d:%I64d]", index, g_segmentMatrix[index][0], g_segmentMatrix[index][1]);
    __int64 begin = 0;
    __int64 end = g_segmentMatrix[index][0] - 1;
    __int64 backup = end;

LOOP:
    __int64 writeSize = 0LL;
    int iResult = 0;
    ADDRINFOW* result = NULL;
    ADDRINFOW* ptr = NULL;
    ADDRINFOW hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    WCHAR szDomain[MAX_URL_LENGTH] = {0};
    DWORD cchDecodeUrl = 0;
    CoInternetParseUrl(g_wcURL, PARSE_DOMAIN, 0, szDomain, MAX_URL_LENGTH, &cchDecodeUrl, 0);

    WCHAR* strProxyURL = L"test.proxy.com";
    WCHAR* strProxyPort = L"8080";
    iResult = GetAddrInfo(strProxyURL, strProxyPort, &hints, &result);
    if (iResult != 0)
    {
        ICSE_OUTPUTDEBUGSTRING_W(L"[%d]getaddrinfo failed: %d", index, iResult);
    }

    SOCKET connectSocket = INVALID_SOCKET;
    ptr = result;
    connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (connectSocket == INVALID_SOCKET)
    {
        ICSE_OUTPUTDEBUGSTRING_W(L"[%d]socket failed: %ld\n", index, WSAGetLastError());
        FreeAddrInfo(result);
    }

    int value = 0;
    int size = sizeof(value);
    iResult = getsockopt(connectSocket, SOL_SOCKET, SO_RCVBUF, (char*)&value, &size);

    iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    FreeAddrInfo(result);
    if (iResult == SOCKET_ERROR)
    {
        ICSE_OUTPUTDEBUGSTRING_W(L"[%d]connect failed: %d", index, iResult);
        closesocket(connectSocket);
    }

    do
    {
        begin = end + 1;
        end = ((begin + 32767) > g_segmentMatrix[index][1] ? g_segmentMatrix[index][1] : (begin + 32767));
        char sendBuff[DEFAULT_BUFFER_LENGTH] = {0};
        sprintf_s(sendBuff, DEFAULT_BUFFER_LENGTH,
            "GET %s HTTP/1.1\r\n" \
            "Host: %s\r\n" \
            "Proxy-Connection: keep-alive\r\n" \
            "Proxy-Authorization: Basic ***********\r\n" \
            "Accept: */*\r\n" \
            "Range: bytes=%I64d-%I64d\r\n"
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.107 Safari/537.36\r\n\r\n",
            strstr(g_acURL, g_acDomain), g_acDomain, begin, end);
        iResult = send(connectSocket, sendBuff, (int)strlen(sendBuff), 0);
        if (iResult == SOCKET_ERROR)
        {
            ICSE_OUTPUTDEBUGSTRING_W(L"[%d]send failed: %d", index, WSAGetLastError());
            closesocket(connectSocket);
        }

        char recvBuff[DEFAULT_BUFFER_LENGTH * 33] = {0};
        int recvLength = 0;
        char* dataAddress = NULL;
        bool bRecvSucess = false;
        do
        {
            iResult = recv(connectSocket, recvBuff + recvLength, DEFAULT_BUFFER_LENGTH * 33 - recvLength, 0);
            if (iResult > 0)
            {
                recvLength += iResult;
                ICSE_OUTPUTDEBUGSTRING_W(L"[%d]recv length %d", index, iResult);
                dataAddress = StrStrA(recvBuff, "\r\n\r\n") + 4;
                if ((recvLength - (dataAddress - recvBuff)) == (int)(end - begin + 1))
                {
                    bRecvSucess = true;
                    break;
                }
            }
            else if (iResult == 0)
            {
                ICSE_OUTPUTDEBUGSTRING_W(L"[%d]Connection closed", index);
            }
            else
            {
                ICSE_OUTPUTDEBUGSTRING_W(L"[%d]recv failed %d", index, WSAGetLastError());
            }
        } while (iResult > 0);

        if (bRecvSucess)
        {
            HANDLE hFile = CreateFile(g_wcFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == NULL)
            {
                ICSE_OUTPUTDEBUGSTRING_W(L"[%d]file open failed %d", index, GetLastError());
                break;
            }
            LONG llow = (LONG)(begin & 0x0FFFFFFFF);
            LONG lhigh = (LONG)((begin & 0xFFFFFFFF00000000)>>32);
            DWORD dwPrt = SetFilePointer(hFile, llow, &lhigh, FILE_BEGIN);
            if (dwPrt == INVALID_SET_FILE_POINTER)
            {
                ICSE_OUTPUTDEBUGSTRING_W(L"[%d]move pointer failed %d", index, GetLastError());
                break;
            }
            DWORD dwSize = 0;
            WriteFile(hFile, dataAddress, (int)(end - begin + 1), &dwSize, NULL);
            CloseHandle(hFile);

            backup = end;
            writeSize += (__int64)dwSize;
            g_segmentMatrix[index][2] =writeSize;
            InvalidateRect(g_hWndMain, NULL, TRUE);
        }
        else
        {
            ICSE_OUTPUTDEBUGSTRING_W(L"[%d]recv error! %d", index, recvLength);
            closesocket(connectSocket);
            end = backup;
            goto LOOP;
        }
    } while (end < g_segmentMatrix[index][1]);

    ICSE_OUTPUTDEBUGSTRING_W(L"[%d]recv finish", index);
    closesocket(connectSocket);
}

void OnDownloadProgress(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    // TODO.
    //int index = (int)wParam;
    //__int64 length = (__int64)lParam;
    InvalidateRect(hWnd, NULL, TRUE);
    return;
}