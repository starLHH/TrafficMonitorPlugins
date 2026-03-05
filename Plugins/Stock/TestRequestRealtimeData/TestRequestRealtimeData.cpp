/**
 * RequestRealtimeData 测试 Demo
 * 模拟 DataManager::RequestRealtimeData() 的请求逻辑，并输出调用参数和接口信息
 *
 * 编译（VS 开发者命令提示符）:
 *   cl /EHsc /std:c++17 /Fe:TestRealtime.exe TestRequestRealtimeData.cpp winhttp.lib
 * 或直接打开 TestRequestRealtimeData.vcxproj 在 VS 中生成并运行
 */
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>

#pragma comment(lib, "winhttp.lib")

// 与 StockDef.cpp _DATA_LEN_HF 一致
static const size_t DATA_LEN_HF = 14;

// 与 DataManager 中一致的 User-Agent
static const wchar_t* WEB_USERAGENT = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36 Edg/135.0.0.0";
// 接口基地址（新浪行情）
static const wchar_t* API_BASE = L"https://hq.sinajs.cn/?";
static const wchar_t* REFERER = L"Referer: https://finance.sina.com.cn";

// 生成随机数，与 DataManager::generateRandomDouble 逻辑一致
static double generateRandomDouble()
{
    srand((unsigned)time(nullptr));
    return (double)rand() / RAND_MAX;
}

// 拼接 list 参数：多个股票代码用逗号连接
static std::wstring JoinCodes(const std::vector<std::wstring>& codes, const std::wstring& sep)
{
    std::wstring out;
    for (size_t i = 0; i < codes.size(); i++)
    {
        if (i > 0) out += sep;
        out += codes[i];
    }
    return out;
}

// 使用 WinHTTP 请求 URL，返回响应 body（窄字符，便于打印）
static bool HttpGet(const std::wstring& url, const std::wstring& referer, std::string& outBody)
{
    URL_COMPONENTS uc = { 0 };
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = { 0 }, path[2048] = { 0 };
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc))
    {
        wprintf(L"WinHttpCrackUrl failed, error=%lu\n", GetLastError());
        return false;
    }

    HINTERNET hSession = WinHttpOpen(WEB_USERAGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { wprintf(L"WinHttpOpen failed\n"); return false; }

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    std::wstring header = std::wstring(L"Referer: ") + (referer[0] ? referer : L"https://finance.sina.com.cn");
    WinHttpAddRequestHeaders(hRequest, header.c_str(), (DWORD)header.size(), WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
    {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }
    if (!WinHttpReceiveResponse(hRequest, NULL))
    {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    outBody.clear();
    DWORD read = 0;
    char buf[4096];
    do
    {
        if (!WinHttpReadData(hRequest, buf, sizeof(buf) - 1, &read) || read == 0) break;
        buf[read] = '\0';
        outBody += buf;
    } while (read > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

// 按分隔符拆分字符串
static void Split(const std::string& s, char delim, std::vector<std::string>& out)
{
    out.clear();
    std::istringstream iss(s);
    std::string item;
    while (std::getline(iss, item, delim))
        out.push_back(item);
}

/**
 * 解析单行 hf_ 响应，逻辑与 StockDef.cpp LoadHF + 显示计算一致。
 * 若解析成功则打印并返回 true。
 */
static bool ParseAndPrintHF(const std::string& line)
{
    const char* prefix = "var hq_str_hf_";
    size_t pos = line.find(prefix);
    if (pos == std::string::npos) return false;
    std::string rest = line.substr(pos + strlen(prefix));
    size_t eq = rest.find('=');
    if (eq == std::string::npos) return false;
    std::string key = rest.substr(0, eq);
    if (key.compare(0, 3, "hf_") != 0) return false;
    std::string dataStr = rest.substr(eq + 1);
    while (!dataStr.empty() && (dataStr.back() == '"' || dataStr.back() == ';' || dataStr.back() == '\r')) dataStr.pop_back();
    if (!dataStr.empty() && dataStr.front() == '"') dataStr = dataStr.substr(1);
    std::vector<std::string> dataArr;
    Split(dataStr, ',', dataArr);
    if (dataArr.size() < DATA_LEN_HF) return false;
    double currentPrice = atof(dataArr[0].c_str());
    double prevClosePrice = atof(dataArr[1].c_str());
    if (prevClosePrice <= 0.0) return false;
    char displayPrice[32], displayFluctuation[32];
    sprintf_s(displayPrice, "%.2f", currentPrice);
    sprintf_s(displayFluctuation, "%.2f%%", (currentPrice - prevClosePrice) / prevClosePrice * 100.0);
    const std::string& displayName = dataArr[13];
    printf("\n[HF 解析测试] (与插件 LoadHF 逻辑一致)\n");
    printf("  key                = %s\n", key.c_str());
    printf("  displayName        = %s\n", displayName.c_str());
    printf("  displayPrice       = %s\n", displayPrice);
    printf("  displayFluctuation = %s\n", displayFluctuation);
    printf("  -> 插件中「其他 + hf_XAU」会按此结果显示。\n");
    return true;
}

int wmain(int argc, wchar_t* argv[])
{
    // 默认含 hf_XAU，用于测试「选择其他 + 输入 hf_XAU」的解析
    std::vector<std::wstring> codes = { L"hf_XAU" };
    if (argc > 1)
    {
        codes.clear();
        for (int i = 1; i < argc; i++)
            codes.push_back(argv[i]);
    }

    // ---------- 1. 构造参数（与 RequestRealtimeData 一致）----------
    double randVal = generateRandomDouble();
    std::wstring listParam = JoinCodes(codes, L",");

    std::wstring url = API_BASE;
    url += L"_=";
    wchar_t randBuf[64];
    swprintf_s(randBuf, L"%.16f", randVal);
    url += randBuf;
    url += L"&list=";
    url += listParam;

    // ---------- 2. 输出：接口与调用参数 ----------
    printf("========== RequestRealtimeData 测试 ==========\n\n");
    printf("[接口名称] 新浪股票实时行情\n");
    printf("[接口地址] %ls\n", API_BASE);
    printf("[请求方式] GET\n\n");
    printf("[调用参数]\n");
    printf("  _    = %.16f   (防缓存随机数)\n", randVal);
    printf("  list = %ls   (股票代码，逗号分隔)\n\n", listParam.c_str());
    printf("[完整 URL]\n  %ls\n\n", url.c_str());
    printf("[请求头]\n  User-Agent: %ls\n  Referer: https://finance.sina.com.cn\n\n", WEB_USERAGENT);

    // ---------- 3. 发起请求 ----------
    printf("[请求结果]\n");
    std::string body;
    if (!HttpGet(url, L"https://finance.sina.com.cn", body))
    {
        printf("  HTTP 请求失败\n");
        return 1;
    }
    printf("  状态: 成功\n");
    printf("  响应长度: %zu 字节\n\n", body.size());

    // 输出响应内容前 800 字符（新浪返回 GBK，控制台为 UTF-8 时可能乱码，仅作长度与格式参考）
    printf("[响应内容预览]\n");
    size_t preview = (body.size() > 800) ? 800 : body.size();
    for (size_t i = 0; i < preview; i++)
        putchar(body[i] >= 32 && body[i] < 127 ? body[i] : '.');
    if (body.size() > 800)
        printf("\n  ... (已截断，共 %zu 字节)", body.size());
    printf("\n");

    // ---------- 4. HF 解析测试（与 StockDef.cpp LoadHF 一致）----------
    std::vector<std::string> lines;
    Split(body, ';', lines);
    bool hfParsed = false;
    for (const std::string& line : lines)
    {
        if (ParseAndPrintHF(line)) hfParsed = true;
    }
    if (!hfParsed && body.find("hq_str_hf_") != std::string::npos)
        printf("\n[HF 解析测试] 响应中含 hf_ 但解析未通过（请检查列数或格式）。\n");
    else if (!hfParsed)
        printf("\n[HF 解析测试] 本次 list 未包含 hf_ 代码，跳过。可传入 hf_XAU 测试。\n");

    return 0;
}
