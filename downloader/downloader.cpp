#include <stdint.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

// #define NDEBUG
#include <assert.h>

#pragma comment(lib, "Ws2_32.lib")

WCHAR g_wcFile[URL_LENGTH_MAX] =
    L"Z:\\tmp\\ubuntu-16.04-desktop-amd64__.iso";
WCHAR g_wcURL[URL_LENGTH_MAX] =
    L"http://releases.ubuntu.com/16.04/ubuntu-16.04-desktop-amd64.iso";
const int8_t* g_acProxy[] =
    "Proxy-Connection: keep-alive\r\n" \
    "Proxy-Authorization: Basic %s\r\n";
    //"Proxy-Authorization: Basic eHVxaW5nOnZpZXdFNzFm\r\n";
const int8_t* g_acWithoutProxy[] = "Connection: keep-alive\r\n";
const int8_t* g_acRangeFormat[] = "Range: bytes=%I64d-%I64d\r\n";
const int8_t* g_acRequestFormat =
    "%s %s HTTP/1.1\r\n" \
    "Host: %s\r\n" \
    "%s" \
    "%s" \
    "Accept: */*\r\n" \
    "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.107 Safari/537.36\r\n\r\n";
char g_acURL[URL_LENGTH_MAX] = {0};
char g_acDomain[URL_LENGTH_MAX] = {0};
int g_factor = 10;
__int64 g_segmentMatrix[10][3] = {{0}};

typedef enum ResultCode_t {
    R_FAIL = -1,
    R_OK = 1,
} ResultCode;

typedef struct SliceInfo_t {
    int64_t begin;
    int64_t size;
    int64_t filled;
} SliceInfo;

class CDownloader() {
public:
    CDownloader(const char* file, const char* host, const char* path);
    ~CDownloader();
    int32_t Start(int32_t slice=10);
    int32_t Stop();
    int32_t SetProxy(const char* host, const char* port, const char* base64);
private:
    char* MakeUpRequest(const char* method, const char* append=nullptr);
    char* AppendRange(const char* request, int64_t begin, int64_t end);
    int32_t OpenSocket();
    int32_t CloseSocket(int32_t sock);
    int32_t GetResponse(const char* request, string& response);
    int32_t GetContentLength(int64_t* psize);
    int32_t Process(int32_t slice);
    int32_t SliceProc(int32_t index);
    int32_t OpenFile(const char* file);
    int32_t CloseFile();
    int32_t WriteFile();
    static const uint32_t BUFFER_SIZE_MAX = 2048u;
    static const char* REQUEST_MAIN = "%s %s HTTP/1.1\r\n" \
                                      "Host: %s\r\n" \
                                      "Accept: */*\r\n" \
                                      "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.107 Safari/537.36\r\n";
    static const char* REQUEST_CONNECTION = "Connection: keep-alive\r\n";
    static const char* REQUEST_CONNECTION_PROXY = "Proxy-Connection: keep-alive\r\n";
    static const char* REQUEST_CONNECTION_PROXY_PW = "Proxy-Connection: keep-alive\r\n" \
                                                     "Proxy-Authorization: Basic %s\r\n";
    static const char* REQUEST_RANGE = "Range: bytes=%lld-%lld\r\n";
    char* m_localFile;
    char* m_urlHost;
    char* m_urlPath;
    char* m_base64;
    int32_t m_fileHandler;
    std::mutex* m_filemtx;
    struct SliceInfo* m_sliceList;
};

#define INIT_TYPE_ARRAY(ptr, n, type) \
    ptr = new type[n]; \
    assert(ptr != nullptr); \
    memset(ptr, 0, n * sizeof(type))
#define DEINIT_ARRAY(ptr) \
    if (ptr != nullptr) { \
        delete[] ptr; \
        ptr = nullptr; \
    }

CDownloader::CDownloader(const char* file, const char* host, const char* path, const char* port="80")
: m_localFile(nullptr), m_urlHost(nullptr), m_urlPath(nullptr), m_urlPort(nullptr)
, m_proxyHost(nullptr), m_proxyPort(nullptr), m_base64(nullptr)
, m_fileHandler(0), m_filemtx(nullptr), m_sliceList(nullptr) {
    if (file == nullptr || host == nullptr || path == nullptr) {
        throw "construct parameter error";
    }
    size_t len = wcslen(file);
    INIT_TYPE_ARRAY(m_localFile, len+1, char);
    memcpy(m_localFile, file, len)；
    
    len = strlen(host);
    INIT_TYPE_ARRAY(m_urlHost, len+1, char);
    memcpy(m_urlHost, host, len);
    
    len = strlen(path);
    INIT_TYPE_ARRAY(m_urlPath, len+1, char);
    memcpy(m_urlPath, path, len);

    len = strlen(port);
    INIT_TYPE_ARRAY(m_urlPort, len+1, char);
    memcpy(m_urlPort, port, len);

    m_filemtx = new std::mutex();
}

CDownloader::~CDownloader() {
    DEINIT_ARRAY(m_localFile);
    DEINIT_ARRAY(m_urlHost);
    DEINIT_ARRAY(m_urlPath);
    DEINIT_ARRAY(m_urlPort);
    DEINIT_ARRAY(m_proxyHost);
    DEINIT_ARRAY(m_proxyPort);
    DEINIT_ARRAY(m_base64);
    delete m_filemtx;
    DEINIT_ARRAY(m_sliceList);
}

int32_t CDownloader::Start(int32_t slice=10) {
    WSADATA wsaData;
    int ret = -1;
    ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0) {
        printf("WSAStartup failed: %d\n", ret);
        return R_FAIL;
    }

    int64_t len = 0ll;
    ret = GetContentLength(&len);
    if (ret != R_OK || len <= 0ll) {
        WSACleanup();
        return R_FAIL;
    }

    OpenFile();
    Process(slice);
    CloseFile();
    return R_OK;
}

int32_t CDownloader::Stop() {
    WSACleanup();
    return R_OK;
}

int32_t CDownloader::SetProxy(const char* host, const char* port, const char* base64=nullptr) {
    if (host == nullptr || port == nullptr) {
        throw "proxy parameter error";
    }
    size_t len = strlen(host);
    INIT_TYPE_ARRAY(m_proxyHost, len+1, char);
    memcpy(m_proxyHost, host, len);

    len = strlen(port);
    INIT_TYPE_ARRAY(m_proxyPort, len+1, char);
    memcpy(m_proxyPort, port, len);

    if (base64 != nullptr) {
        len = strlen(base64);
        INIT_TYPE_ARRAY(m_base64, len+1, char);
        memcpy(m_base64, base64, len);
    }
    return R_OK;
}

#define STRCAT(a, b) (a b)
char* CDownloader::MakeUpRequest(const char* method, const char* append=nullptr) {
    size_t len = strlen(method) + strlen(m_urlPath) + strlen(m_urlHost) + strlen(REQUEST_MAIN) - 2*3 + strlen("\r\n");
    if (m_proxyHost != nullptr && m_base64 != nullptr) {
        len += strlen(REQUEST_CONNECTION_PROXY_PW) + strlen(m_base64) - 2;
    } else if (m_proxyHost != nullptr && m_base64 == nullptr) {
        len += strlen(REQUEST_CONNECTION_PROXY);
    } else {
        len += strlen(REQUEST_CONNECTION);
    }
    if (append != nullptr) {
        len += strlen(append);
    }

    char* request = nullptr;
    INIT_TYPE_ARRAY(request, len+1, char);
    if (m_proxyHost != nullptr && m_base64 != nullptr) {
        snprintf(request, len+1, STRCAT(REQUEST_MAIN, REQUEST_CONNECTION_PROXY_PW), method, m_urlPath, m_urlHost, m_base64);
    } else if (m_proxyHost != nullptr && m_base64 == nullptr) {
        snprintf(request, len+1, STRCAT(REQUEST_MAIN, REQUEST_CONNECTION_PROXY), method, m_urlPath, m_urlHost)
    } else {
        snprintf(request, len+1, STRCAT(REQUEST_MAIN, REQUEST_CONNECTION), method, m_urlPath, m_urlHost);
    }
    if (append != nullptr) {
        strncat(request, append, strlen(append));
    }
    strncat(request, "\r\n", strlen("\r\n"));
    return request;
}

char* AppendRange(const char* request, int64_t begin, int64_t end) {
    assert(request != nullptr);
    char range[256] = {0};
    snprintf(range, 256, REQUEST_RANGE, begin, begin + len - 1);
    size_t len = strlen(request) + strlen(range);
    char* newReq = nullptr;
    INIT_TYPE_ARRAY(newReq, len+1, char);
    strncat(newReq, request, strlen(request) - strlen("\r\n"));
    strncat(newReq, range, strlen(range));
    strncat(newReq, "\r\n", strlen("\r\n"));
    return newReq;
}

int32_t CDownloader::OpenSocket() {
    struct addrinfo* servAddr = nullptr;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    char* host = (m_proxyHost != nullptr) ? m_proxyHost : m_urlHost;
    char* port = (m_proxyPort != nullptr) ? m_proxyPort : m_urlPort;

    DWORD result = getaddrinfo(host, port, &hints, &servAddr);
    if (result != 0) {
        printf("getaddrinfo failed with error: %d\n", result);
        return INVALID_SOCKET;
    }

    SOCKET sock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
    if (sock == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(servAddr);
        return INVALID_SOCKET;
    }

    int ret = connect(sock, servAddr->ai_addr, (int)result->ai_addrlen);
    if (ret == SOCKET_ERROR) {
        printf("connect failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(servAddr);
        closesocket(sock);
        return INVALID_SOCKET;
    }
    freeaddrinfo(servAddr);
    return sock;
}

int32_t CDownloader::CloseSocket(int32_t sock) {
    closesocket(sock);
}

int32_t CDownloader::GetResponse(const char* request, string& response) {
    response = "";
    int ret = send(sock, request, (int)strlen(request), 0);
    if (ret == SOCKET_ERROR) {
        printf("send failed with error: %ld\n", WSAGetLastError());
        return R_FAIL;
    }
    // ret = shutdown(sock, SD_SEND);
    // if (ret == SOCKET_ERROR) {
    //     printf("shutdown failed with error: %ld\n", WSAGetLastError());
    //     return R_FAIL;
    // }

    char buf[BUFFER_SIZE_MAX] = {0};
    int filled = 0;
    int left = BUFFER_SIZE_MAX;
    do {
        ret = recv(sock, buf+filled, left, 0);
        if (ret > 0 && ret < left) {
            filled += ret;
            left -= ret;
        } else if (ret > 0 && ret == left) {
            printf("request header size may be larger than buffer size!\n");
            filled = BUFFER_SIZE_MAX;
            left = 0;
            break;
        } else if (ret == 0) {
            printf("recv is finish: %d\n", filled);
        } else {
            printf("recv failed with error: %d, %ld\n", ret, WSAGetLastError());
        }
    } while (ret > 0);
    const char* end = strstr(buf, "\r\n\r\n");
    if (end != nullptr) {
        response.assign(buff, end - buff + sizeof("\r\n\r\n"));
    }
    if (response.length() > strlen("HTTP/1.1 ")) {
        int status = atoi(response.substr(strlen("HTTP/1.1 ", 3)));
        printf("status code: %d\n", status);
    }
    return R_OK;
}

int32_t CDownloader::GetContentLength(int64_t* psize) {
    SOCKET sock = OpenSocket();
    if (sock == INVALID_SOCKET) {
        return R_FAIL;
    }
    string response;
    char* request = MakeUpRequest("HEAD");  // allocate memory.
    int32_t ret = GetResponse(request, response);
    DEINIT_ARRAY(request);                  // release memory.
    CloseSocket(sock);

    if (response.length() > 0) {
        const char* str = "Content-Length: ";
        size_t begin = response.find(str);
        size_t end = response.Find("\r\n", begin);
        string strlen = response.substr(begin + strlen(str), end - begin - strlen(str));
        *psize = atoll(strlen.c_str());
        return R_OK;
    } else {
        printf("empty response\n");
    }
    return R_FAIL;
}

int32_t CDownloader::Process(int32_t slice) {
    assert(m_sliceList == nullptr);
    INIT_TYPE_ARRAY(m_sliceList, slice, SliceInfo);
    int64_t sliceSize = len / slice; 
    for (int32_t i = 0; i < slice; i++) {
        m_sliceList[i].begin = i * sliceSize;
        m_sliceList[i].size = sliceSize;
        m_sliceList[i].filled = 0;
    }
    m_sliceList[slice-1].size += len % slice;

    std::vector<std::thread> threads;
    for (int32_t i = 0; i < slice; i++) {
        threads.push_back(std::thread(&CDownloader::SliceProc, this, i);
    }
    for (auto& th : threads) {
        th.join();
    }

    DEINIT_ARRAY(m_sliceList);
    return R_OK;
}

int32_t CDownloader::SliceProc(int32_t index) {
    if (m_sliceList[index].filled == m_sliceList[index].size) {
        return R_OK;
    }

    int32_t cell = 1024 * 1024;
    const SliceInfo* pSlice = m_sliceList + index;
    SOCKET sock = OpenSocket();
    if (sock == INVALID_SOCKET) {
        return R_FAIL;
    }

    int64_t begin = 0ll;
    int64_t len = 0ll;
    char* request = MakeUpRequest("GET");  // allocate memory.
    do {
        begin = pSlice->begin + pSlice->filled;
        len = (cell < (pSlice->size - pSlice->filled)) ? cell : (pSlice->size - pSlice->filled);
        char* newReq = AppendRange(request, begin, begin + len -1); // allocate memory.
        string response;
        int32_t ret = GetResponse(request, response);
        DEINIT_ARRAY(newReq);                                       // release memory.
    } while (true);

    DEINIT_ARRAY(request);                  // release memory.
    CloseSocket(sock);

    do
    {
        // begin = end + 1;
        // end = ((begin + 32767) > g_segmentMatrix[index][1] ? g_segmentMatrix[index][1] : (begin + 32767));
        char rangeBuff[URL_LENGTH_MAX] = { 0 };
        sprintf_s(rangeBuff, URL_LENGTH_MAX, g_acRangeFormat, begin, end);
        char sendBuff[BUFFER_SIZE_MAX] = {0};
        sprintf_s(sendBuff, BUFFER_SIZE_MAX, g_acRequestFormat, strstr(g_acURL, g_acDomain)+strlen(g_acDomain), g_acDomain, rangeBuff, (g_bUseProxy ? g_acProxy : g_acWithoutProxy));
        iResult = send(connectSocket, sendBuff, (int)strlen(sendBuff), 0);
        if (iResult == SOCKET_ERROR)
        {
            ICSE_OUTPUTDEBUGSTRING_W(L"[%d]send failed: %d", index, WSAGetLastError());
            closesocket(connectSocket);
        }

        char recvBuff[BUFFER_SIZE_MAX * 33] = {0};
        int recvLength = 0;
        char* dataAddress = NULL;
        bool bRecvSucess = false;
        do
        {
            iResult = recv(connectSocket, recvBuff + recvLength, BUFFER_SIZE_MAX * 33 - recvLength, 0);
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
            HANDLE hFile = NULL;
            unsigned char loop = 0;
            do
            {
                Sleep(1);
                hFile = CreateFile(g_wcFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                loop++;
            } while ((hFile == INVALID_HANDLE_VALUE) && (GetLastError() == ERROR_SHARING_VIOLATION) && (loop <= 10));
            if (hFile == INVALID_HANDLE_VALUE)
            {
                ICSE_OUTPUTDEBUGSTRING_W(L"[%d]file open failed %d", index, GetLastError());
                break;
            }
            LONG llow = (LONG)(begin & 0x0FFFFFFFF);
            LONG lhigh = (LONG)((begin & 0xFFFFFFFF00000000)>>32);
            DWORD dwPrt = SetFilePointer(hFile, llow, &lhigh, FILE_BEGIN);
            if (dwPrt == INVALID_SET_FILE_POINTER)
            {
                ICSE_OUTPUTDEBUGSTRING_W(L"[%d]move pointer failed %d, %I64d", index, GetLastError(), begin);
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

        //if (strHeader.GetLength() > 0)
        //{
        //    int begin = strHeader.Find("Content-Length: ");
        //    int end = strHeader.Find("\r\n", begin);
        //    CStringA strLength = strHeader.Mid(begin + 16, end - begin - 16);
        //    __int64 length = _atoi64(strLength);
        //}
    } while (end < g_segmentMatrix[index][1]);

    ICSE_OUTPUTDEBUGSTRING_W(L"[%d]recv finish", index);
    closesocket(connectSocket);
}

int32_t CDownloader::OpenFile(const char* file) {
    std::lock_guard<std::mutex> lck(*m_filemtx);
    assert(m_fileHandler == 0);
    FILE* pf = fopen(file, "wb");
    if (pf == nullptr) {
        throw "open file failed";
    }
    m_fileHandler = (int32_t)pf;
    return R_OK;
}

int32_t CDownloader::CloseFile() {
    std::lock_guard<std::mutex> lck(*m_filemtx);
    if (m_fileHandler != 0) {
        fclose((FILE*)m_fileHandler);
        m_fileHandler = 0;
    }
    return R_OK;
}

int32_t CDownloader::WriteFile(int64_t offset, int8_t* buffer, int32_t size) {
    std::lock_guard<std::mutex> lck(*m_filemtx);
    if (m_fileHandler != 0) {
#ifdef __linux__
        // #define _FILE_OFFSET_BITS 64
        fseeko((FILE*)m_fileHandler, offset, SEEK_SET);
#endif // __linux__
        size_t ret = fwrite(buffer, sizeof(int8_t), size, (FILE*)m_fileHandler);
    }
    return R_OK;
}