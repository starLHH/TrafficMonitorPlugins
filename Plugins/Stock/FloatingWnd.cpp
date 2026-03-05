#include "pch.h"
#include "FloatingWnd.h"
#include <afxinet.h>
#include <cmath>
#include <memory>
#include "Common.h"
#include "DataManager.h"
#include <Stock.h>

BEGIN_MESSAGE_MAP(CFloatingWnd, CWnd)
ON_WM_PAINT()
ON_WM_ERASEBKGND()
ON_WM_LBUTTONDOWN()
ON_WM_CREATE()
ON_MESSAGE(FWND_MSG_UPDATE_STATUS, OnUpdateStatus)
ON_MESSAGE(FWND_MSG_REQUEST_DATA, OnRequestData)
END_MESSAGE_MAP()

int CFloatingWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    PostMessage(FWND_MSG_REQUEST_DATA, time(nullptr), 0);
    return 0;
}

// 处理消息
LRESULT CFloatingWnd::OnUpdateStatus(WPARAM wParam, LPARAM lParam)
{
    Invalidate();
    return 0;
}

LRESULT CFloatingWnd::OnRequestData(WPARAM wParam, LPARAM lParam)
{
    if (wParam)
    {
        RequestData();
        loading_state_txt += L".";
        Invalidate();
    }
    return 0;
}

CFloatingWnd::CFloatingWnd() : m_isDestroying(FALSE)
{
}

CFloatingWnd::~CFloatingWnd()
{
    // 标记窗口正在销毁
    m_isDestroying = TRUE;
    if (m_CTransparentWnd.GetSafeHwnd())
        m_CTransparentWnd.DestroyWindow();
}

BOOL CFloatingWnd::Create(CFont *font, CPoint pt, std::wstring stock_id)
{
    m_stock_id = stock_id;
    // 注册窗口类
    WNDCLASS wndcls;
    HINSTANCE hInst = AfxGetInstanceHandle();
    if (!(::GetClassInfo(hInst, L"CTransparentWnd", &wndcls)))
    {
        wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wndcls.lpfnWndProc = ::DefWindowProc;
        wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
        wndcls.hInstance = hInst;
        wndcls.hIcon = NULL;
        wndcls.hCursor = LoadCursor(NULL, IDC_ARROW);
        // wndcls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wndcls.hbrBackground = NULL; // 重要：设置为NULL
        wndcls.lpszMenuName = NULL;
        wndcls.lpszClassName = L"CTransparentWnd";
        if (!AfxRegisterClass(&wndcls))
            return FALSE;
    }

    // 设置父窗口指针
    m_CTransparentWnd.SetParent(this);

    m_pfont = font;

    // 获取包含鼠标点的显示器
    HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(MONITORINFO)};
    GetMonitorInfo(hMonitor, &mi);
    CRect screenRect = mi.rcWork; // 工作区域

    // 创建透明全屏窗口
    if (!m_CTransparentWnd.CreateEx(WS_EX_TOOLWINDOW /* | WS_EX_LAYERED */ /* | WS_EX_TRANSPARENT */,
                                    L"CTransparentWnd", L"", WS_POPUP | WS_VISIBLE,
                                    screenRect, NULL, 0, NULL))
    {
        TRACE(L"Failed to create transparent window\n");
        return FALSE;
    }

    const int WIDTH = g_data.RDPI(g_data.m_setting_data.m_kline_width);
    const int HEIGHT = g_data.RDPI(g_data.m_setting_data.m_kline_height);
    int x = pt.x;
    int y = pt.y;

    // 调整位置
    if (x + WIDTH > screenRect.right)
        x = x - WIDTH;
    if (y + HEIGHT > screenRect.bottom)
        y = y - HEIGHT;
    x = max(screenRect.left, x);
    y = max(screenRect.top, y);

    CRect rect(x, y, x + WIDTH, y + HEIGHT);

    // 创建实际的浮动窗口
    if (!CreateEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                  AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW),
                  L"", WS_POPUP | WS_VISIBLE | WS_BORDER,
                  rect, &m_CTransparentWnd, 0))
    {
        TRACE(L"Failed to create floating window\n");
        m_CTransparentWnd.DestroyWindow();
        return FALSE;
    }

    // 确保浮动窗口在最顶层
    BringWindowToTop();
    SetForegroundWindow();

    // 设置完全透明
    m_CTransparentWnd.SetLayeredWindowAttributes(0, 0, LWA_ALPHA);
    m_CTransparentWnd.ShowWindow(SW_SHOW);

    TRACE(L"Windows created successfully\n");
    return TRUE;
}

CPoint CFloatingWnd::Stock2Point(int x, int y, int w, int h, float unitY, const STOCK::TimelinePoint &item, const STOCK::Price prevClosePrice)
{
    CPoint p = CPoint();
    std::vector<std::string> time_arr = CCommon::split(item.time, ":");
    if (time_arr.size() == 3)
    {
        // 9:30 10:00 11:30 13:00 14:00 15:00
        static int before12ClockOffset = 570; // 9.5 * 60;
        static int after12ClockOffset = 660;  // 9.5 * 60 + 1.5 * 60;
        static float totalMinutes = 240.0;    // 4 * 60;

        int hour = _ttoi(CString(time_arr[0].c_str()));
        int minute = _ttoi(CString(time_arr[1].c_str()));

        int countX = hour * 60 + minute;

        if (hour < 12)
        {
            countX -= before12ClockOffset;
        }
        else if (hour >= 13)
        {
            countX -= after12ClockOffset;
        }
        p.x = static_cast<LONG>(w / totalMinutes * countX);
    }
    p.y = static_cast<LONG>((item.price - prevClosePrice) * unitY * 100);
    return p;
}

void CFloatingWnd::OnPaint()
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);

    // 双缓冲绘制
    CDC memDC;
    CBitmap memBitmap;
    memDC.CreateCompatibleDC(&dc);
    if (m_pfont)
    {
        memDC.SelectObject(m_pfont);
    }
    memBitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height());
    CBitmap *pOldBitmap = memDC.SelectObject(&memBitmap);

    // 绘制背景
    memDC.FillSolidRect(rect, RGB(255, 255, 255));

    memDC.SetBkMode(TRANSPARENT);

    int x = rect.left, y = rect.top, h = rect.Height(), w = rect.Width();

    CPen pGrid(PS_DOT, 1, RGB(240, 240, 240));
    CPen *pOldPen = memDC.SelectObject(&pGrid);
    memDC.MoveTo(0, h / 4);
    memDC.LineTo(w, h / 4);
    memDC.MoveTo(0, h / 4 * 3);
    memDC.LineTo(w, h / 4 * 3);

    memDC.MoveTo(w / 4, 0);
    memDC.LineTo(w / 4, h);
    memDC.MoveTo(w / 2, 0);
    memDC.LineTo(w / 2, h);
    memDC.MoveTo(w / 4 * 3, 0);
    memDC.LineTo(w / 4 * 3, h);

    CPen pMiddleLine(PS_DASHDOT, 1, RGB(140, 140, 140));
    memDC.SelectObject(&pMiddleLine);
    memDC.MoveTo(0, h / 2);
    memDC.LineTo(w, h / 2);

    CPen pKLine(PS_SOLID, 1, RGB(70, 113, 152));
    memDC.SelectObject(&pKLine);

    STOCK::RealTimeData realtimeData;
    std::vector<STOCK::TimelinePoint> timelinePoint;
    {
        std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
        auto stockData = g_data.GetStockData(m_stock_id);
        realtimeData = stockData->realTimeData;
        timelinePoint = stockData->getTimelineData()->data;
    }

    if (timelinePoint.size() > 0)
    {
        float halfH = static_cast<float>(h / 2.0);

        // 期货(nf_)可能先打开 K 线再拉实时，或实时接口无数据，用分时数据推算参考价与涨跌幅度
        STOCK::Price drawPrevClose = realtimeData.prevClosePrice;
        STOCK::Price drawPriceLimit = realtimeData.priceLimit;
        if (drawPriceLimit <= 0.0 || drawPrevClose <= 0.0)
        {
            drawPrevClose = timelinePoint[0].price;
            STOCK::Price range = 0.01;
            for (const auto &pt : timelinePoint)
            {
                STOCK::Price d = static_cast<STOCK::Price>(std::fabs(pt.price - drawPrevClose));
                if (d > range)
                    range = d;
            }
            drawPriceLimit = range;
        }
        float unitY = drawPriceLimit != 0 ? static_cast<float>(halfH / (drawPriceLimit * 100)) : 0.0f;

        memDC.SetTextColor(RGB(179, 64, 65));
        float upperLimitPrice = static_cast<float>(drawPrevClose + drawPriceLimit);
        CString upperLimitTxt;
        upperLimitTxt.Format(_T("%.2f"), upperLimitPrice);
        CRect upperLimitTxtRect{rect};
        upperLimitTxtRect.right = upperLimitTxtRect.left + memDC.GetTextExtent(upperLimitTxt).cx;
        memDC.DrawText(upperLimitTxt, upperLimitTxtRect, DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        CString upperLimitRateTxt;
        upperLimitRateTxt.Format(_T("%.2f%%"), drawPrevClose > 0 ? (drawPriceLimit * 100.0 / drawPrevClose) : 0.0);
        CRect upperLimitRateTxtRect{rect};
        upperLimitRateTxtRect.left = w - (upperLimitRateTxtRect.left + memDC.GetTextExtent(upperLimitRateTxt).cx);
        memDC.DrawText(upperLimitRateTxt, upperLimitRateTxtRect, DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        memDC.SetTextColor(RGB(44, 144, 51));
        float lowerLimitPrice = static_cast<float>(drawPrevClose - drawPriceLimit);
        CString lowerLimitTxt;
        lowerLimitTxt.Format(_T("%.2f"), lowerLimitPrice);
        CRect lowerLimitTxtRect{rect};
        lowerLimitTxtRect.right = lowerLimitTxtRect.left + memDC.GetTextExtent(lowerLimitTxt).cx;
        memDC.DrawText(lowerLimitTxt, lowerLimitTxtRect, DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);

        CString lowerLimitRateTxt;
        lowerLimitRateTxt.Format(_T("-%.2f%%"), drawPrevClose > 0 ? (drawPriceLimit * 100.0 / drawPrevClose) : 0.0);
        CRect lowerLimitRateTxtRect{rect};
        lowerLimitRateTxtRect.left = w - (lowerLimitRateTxtRect.left + memDC.GetTextExtent(lowerLimitRateTxt).cx);
        memDC.DrawText(lowerLimitRateTxt, lowerLimitRateTxtRect, DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);

        memDC.SetTextColor(RGB(154, 151, 157));
        CString middleTxt;
        middleTxt.Format(_T("%.2f"), drawPrevClose);
        CRect middleTxtRect{rect};
        middleTxtRect.right = middleTxtRect.left + memDC.GetTextExtent(middleTxt).cx;
        memDC.DrawText(middleTxt, middleTxtRect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        std::vector<CPoint> dataPoints;
        // nf_ 等时间为 "YYYY-MM-DD HH:MM:SS"，不能用 A 股当日时分秒算 X，改用索引均匀分布
        bool useIndexX = (timelinePoint.size() > 0 && timelinePoint[0].time.find(' ') != std::string::npos);
        for (size_t i = 0; i < timelinePoint.size(); i++)
        {
            const STOCK::TimelinePoint &item = timelinePoint[i];
            CPoint p = Stock2Point(x, y, w, h, unitY, item, drawPrevClose);
            if (useIndexX)
                p.x = (timelinePoint.size() <= 1) ? x : (x + (int)(w * (double)i / (timelinePoint.size() - 1)));
            dataPoints.push_back(p);
        }

        STOCK::Price startPrice = (realtimeData.openPrice > 0 && realtimeData.prevClosePrice > 0) ? realtimeData.openPrice : timelinePoint[0].price;
        int startY = static_cast<int>(halfH - (startPrice - drawPrevClose) * unitY * 100);
        memDC.MoveTo(x, startY);
        for (size_t i = 0; i < dataPoints.size(); i++)
        {
            int pX = dataPoints[i].x;
            int pY = static_cast<int>(halfH - dataPoints[i].y);
            memDC.LineTo(pX, pY);
        }
    }
    else
    {
        memDC.SelectObject(&pMiddleLine);
        memDC.SetTextColor(RGB(154, 151, 157));
        memDC.TextOut((w - memDC.GetTextExtent(loading_state_txt).cx) / 2, g_data.RDPI(10), loading_state_txt);
    }

    memDC.SelectObject(pOldPen);

    // 复制到屏幕
    dc.BitBlt(0, 0, rect.Width(), rect.Height(), &memDC, 0, 0, SRCCOPY);

    memDC.SelectObject(pOldBitmap);
}

BOOL CFloatingWnd::OnEraseBkgnd(CDC *pDC)
{
    return TRUE; // 不擦除背景
}

void CFloatingWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
    // DestroyWindow();
}

void CFloatingWnd::RequestData()
{
    if (!m_is_thread_running)
    {
        loading_state_txt = g_data.StringRes(IDS_LOADING).GetString();
        AfxBeginThread(NetworkThreadProc, this);
    }
}

UINT CFloatingWnd::NetworkThreadProc(LPVOID pParam)
{
    CFloatingWnd *pFW = (CFloatingWnd *)pParam;

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CFlagLocker flag_locker(pFW->m_is_thread_running);

    if (pFW->m_stock_id.empty())
    {
        return 0;
    }

    g_data.RequestTimelineData(pFW->m_stock_id);

    return 0;
}
