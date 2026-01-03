#include "pch.h"
#include "framework.h"
#include "Mfc_Youtube.h"
#include "Mfc_YoutubeDlg.h"
#include "afxdialogex.h"

#include <cmath>
#include <random>
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CMfcYoutubeDlg::CMfcYoutubeDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_MFC_YOUTUBE_DIALOG, pParent)
{
}

void CMfcYoutubeDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMfcYoutubeDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_BN_CLICKED(IDC_BTN_RESET, &CMfcYoutubeDlg::OnBnClickedBtnReset)
    ON_BN_CLICKED(IDC_BTN_MOVE, &CMfcYoutubeDlg::OnBnClickedBtnMove)
    ON_MESSAGE(WM_APP_RANDOMMOVE, &CMfcYoutubeDlg::OnAppRandomMove)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CMfcYoutubeDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    
    ModifyStyle(0, WS_CLIPCHILDREN);

    // 기본값
    SetDlgItemInt(IDC_RADIUS, 12, FALSE); // 클릭 지점 원 반지름
    SetDlgItemInt(IDC_THICK, 3, FALSE);   // 정원 테두리 두께

    ResetAll();

    // 입력 전 "반지름", "두께" 표시
    ::SendMessage(GetDlgItem(IDC_RADIUS)->GetSafeHwnd(), EM_SETCUEBANNER, TRUE, (LPARAM)L"반지름");
    ::SendMessage(GetDlgItem(IDC_THICK)->GetSafeHwnd(), EM_SETCUEBANNER, TRUE, (LPARAM)L"두께");

    // 초기 값 비우기
    SetDlgItemText(IDC_RADIUS, L"");
    SetDlgItemText(IDC_THICK, L"");

    GetDlgItem(IDC_RADIUS)->SetFocus();
    return FALSE;

}

void CMfcYoutubeDlg::OnDestroy()
{
    // 스레드 정리
    m_randomRunning = false;
    if (m_pMoveThread)
    {
        // Sleep 중이어도 최대 0.5초 내 종료됨
        WaitForSingleObject(m_pMoveThread->m_hThread, 2000);
        m_pMoveThread = nullptr;
    }

    if (!m_canvas.IsNull())
        m_canvas.Destroy();

    CDialogEx::OnDestroy();
}

BOOL CMfcYoutubeDlg::OnEraseBkgnd(CDC* /*pDC*/)
{
    // 깜빡임 방지
    return TRUE;
}

void CMfcYoutubeDlg::OnPaint()
{
    CPaintDC dc(this);

    EnsureCanvasSize();
    RenderScene();
    BlitCanvas(dc);
}

void CMfcYoutubeDlg::EnsureCanvasSize()
{
    CRect rc;
    GetClientRect(&rc);
    int w = rc.Width();
    int h = rc.Height();
    if (w <= 0 || h <= 0) return;

    if (m_canvas.IsNull() || w != m_cw || h != m_ch)
    {
        if (!m_canvas.IsNull()) m_canvas.Destroy();
        m_canvas.Create(w, h, 32);
        m_cw = w;
        m_ch = h;
    }
}

void CMfcYoutubeDlg::BlitCanvas(CDC& dc)
{
    if (!m_canvas.IsNull())
        m_canvas.Draw(dc, 0, 0);
}

int CMfcYoutubeDlg::GetPointRadius()
{
    CString s;
    GetDlgItemText(IDC_RADIUS, s);
    s.Trim();
    if (s.IsEmpty()) return 12; // 기본 반지름

    int r = _wtoi(s);
    if (r < 1) r = 1;
    return r;
}

int CMfcYoutubeDlg::GetGardenThickness()
{
    CString s;
    GetDlgItemText(IDC_THICK, s);
    s.Trim();
    if (s.IsEmpty()) return 3; // 기본 두께

    int t = _wtoi(s);
    if (t < 1) t = 1;
    return t;
}


void CMfcYoutubeDlg::UpdatePointStatics()
{
    auto fmt = [](const CPoint& p) -> CString
        {
            CString s;
            s.Format(L"(%d, %d)", p.x, p.y);
            return s;
        };

    if (m_ptCount >= 1) SetDlgItemText(IDC_STATIC_P1, fmt(m_pts[0]));
    else SetDlgItemText(IDC_STATIC_P1, L"-");

    if (m_ptCount >= 2) SetDlgItemText(IDC_STATIC_P2, fmt(m_pts[1]));
    else SetDlgItemText(IDC_STATIC_P2, L"-");

    if (m_ptCount >= 3) SetDlgItemText(IDC_STATIC_P3, fmt(m_pts[2]));
    else SetDlgItemText(IDC_STATIC_P3, L"-");
}

void CMfcYoutubeDlg::ResetAll()
{
    m_ptCount = 0;
    m_hasGarden = false;
    m_dragging = false;
    m_dragIndex = -1;

    // 초기화
    SetDlgItemText(IDC_RADIUS, L"");
    SetDlgItemText(IDC_THICK, L"");

    UpdatePointStatics();
    Invalidate(FALSE);
}

void CMfcYoutubeDlg::OnBnClickedBtnReset()
{
    ResetAll();
}

void CMfcYoutubeDlg::OnBnClickedBtnMove()
{
    // 정원이 그려진 상태(= 점 3개가 존재)에서만 동작
    if (m_ptCount < 3) return;

    if (m_randomRunning.load())
        return; // 이미 실행 중이면 무시(중복 방지)

    m_randomRunning = true;
    m_pMoveThread = AfxBeginThread(MoveThreadProc, this, THREAD_PRIORITY_NORMAL, 0, 0, nullptr);
}

UINT CMfcYoutubeDlg::MoveThreadProc(LPVOID pParam)
{
    auto* self = reinterpret_cast<CMfcYoutubeDlg*>(pParam);
    if (!self) return 0;

    // 랜덤 엔진
    std::mt19937 rng{ std::random_device{}() };

    for (int i = 0; i < 10; ++i)
    {
        if (!self->m_randomRunning.load())
            break;

        // 2Hz => 500ms
        ::Sleep(500);

        if (!self->m_randomRunning.load())
            break;

        int margin = self->GetPointRadius() + 5;

        // payload 생성 (UI 스레드에서 delete)
        auto* payload = new RandomMovePayload();
        payload->pts[0] = self->RandomPointInClient(margin);
        payload->pts[1] = self->RandomPointInClient(margin);
        payload->pts[2] = self->RandomPointInClient(margin);

        // UI 스레드에 반영 요청
        self->PostMessage(WM_APP_RANDOMMOVE, 0, reinterpret_cast<LPARAM>(payload));
    }

    self->m_randomRunning = false;
    return 0;
}

CPoint CMfcYoutubeDlg::RandomPointInClient(int margin)
{
    CRect rc;
    GetClientRect(&rc);

    int minX = rc.left + margin;
    int maxX = rc.right - margin;
    int minY = rc.top + margin;
    int maxY = rc.bottom - margin;

    if (maxX <= minX) { minX = rc.left; maxX = rc.right; }
    if (maxY <= minY) { minY = rc.top;  maxY = rc.bottom; }

    static thread_local std::mt19937 rng{ std::random_device{}() };

    std::uniform_int_distribution<int> dx(minX, max(minX, maxX - 1));
    std::uniform_int_distribution<int> dy(minY, max(minY, maxY - 1));

    return CPoint(dx(rng), dy(rng));
}

LRESULT CMfcYoutubeDlg::OnAppRandomMove(WPARAM, LPARAM lParam)
{
    auto* payload = reinterpret_cast<RandomMovePayload*>(lParam);
    if (!payload) return 0;

    // 점 3개 갱신
    m_pts[0] = payload->pts[0];
    m_pts[1] = payload->pts[1];
    m_pts[2] = payload->pts[2];
    m_ptCount = 3;

    delete payload;

    UpdatePointStatics();
    m_hasGarden = ComputeCircumcircle();
    Invalidate(FALSE);
    return 0;
}

bool CMfcYoutubeDlg::HitTestPointCircle(int idx, CPoint mousePt, int pointR)
{
    if (idx < 0 || idx >= m_ptCount) return false;

    long long dx = (long long)mousePt.x - (long long)m_pts[idx].x;
    long long dy = (long long)mousePt.y - (long long)m_pts[idx].y;
    long long rr = (long long)pointR * (long long)pointR;
    return (dx * dx + dy * dy) <= rr;
}

void CMfcYoutubeDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    CDialogEx::OnLButtonDown(nFlags, point);

    int pr = GetPointRadius();

    // 점 3개가 이미 있으면: 드래그 시작 시도(4번째 클릭부터는 점 추가 금지)
    if (m_ptCount >= 3)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (HitTestPointCircle(i, point, pr))
            {
                m_dragging = true;
                m_dragIndex = i;
                SetCapture();
                return;
            }
        }
        return; // 4번째 클릭부터는 클릭 지점 원을 그리지 않음(= 입력 무시)
    }

    // 1~3번째 클릭: 점 추가
    m_pts[m_ptCount] = point;
    m_ptCount++;

    UpdatePointStatics();

    if (m_ptCount == 3)
        m_hasGarden = ComputeCircumcircle();

    Invalidate(FALSE);
}

void CMfcYoutubeDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    CDialogEx::OnMouseMove(nFlags, point);

    if (!m_dragging || m_dragIndex < 0 || m_dragIndex >= 3) return;

    // 드래그 중 해당 점을 커서 위치로 이동
    m_pts[m_dragIndex] = point;

    UpdatePointStatics();
    m_hasGarden = ComputeCircumcircle();

    // 드래그 중 계속 그리기
    Invalidate(FALSE);
}

void CMfcYoutubeDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    CDialogEx::OnLButtonUp(nFlags, point);

    if (m_dragging)
    {
        m_dragging = false;
        m_dragIndex = -1;
        ReleaseCapture();
    }
}

// ====================== Geometry: circumcircle ======================
bool CMfcYoutubeDlg::ComputeCircumcircle()
{
    if (m_ptCount < 3) return false;

    const double x1 = (double)m_pts[0].x, y1 = (double)m_pts[0].y;
    const double x2 = (double)m_pts[1].x, y2 = (double)m_pts[1].y;
    const double x3 = (double)m_pts[2].x, y3 = (double)m_pts[2].y;

    const double d = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
    if (std::fabs(d) < 1e-6)
    {
        // 공선(또는 거의 공선): 외접원 정의 불가
        return false;
    }

    const double a1 = x1 * x1 + y1 * y1;
    const double a2 = x2 * x2 + y2 * y2;
    const double a3 = x3 * x3 + y3 * y3;

    const double ux =
        (a1 * (y2 - y3) + a2 * (y3 - y1) + a3 * (y1 - y2)) / d;

    const double uy =
        (a1 * (x3 - x2) + a2 * (x1 - x3) + a3 * (x2 - x1)) / d;

    const double rr = std::sqrt((ux - x1) * (ux - x1) + (uy - y1) * (uy - y1));

    m_gx = ux;
    m_gy = uy;
    m_gr = rr;
    return (rr > 0.5); // 반지름이 너무 작으면 의미 없음
}

// ====================== Rendering ======================
void CMfcYoutubeDlg::RenderScene()
{
    // 캔버스 없으면 return
    if (m_canvas.IsNull()) return;

    // 배경 흰색으로 덮기
    ClearCanvas(RGB(255, 255, 255));

    // 현재 입력값(반지름, 외접원 두께) 반영
    const int pr = GetPointRadius();
    const int thick = GetGardenThickness();

    // 클릭 지점 원(최대 3개)
    // 4번째 클릭부터는 추가 입력을 막았으므로, 기존 3개는 계속 그려짐
    const COLORREF pointCols[3] = { RGB(0, 120, 215), RGB(0, 180, 80), RGB(220, 60, 60) };

    for (int i = 0; i < m_ptCount && i < 3; ++i)
    {
        DrawFilledCircle(m_pts[i].x, m_pts[i].y, pr, RGB(0, 0, 0));
    }

    // 정원(외접원): 테두리만 두께 적용
    if (m_ptCount == 3 && m_hasGarden)
    {
        int cx = (int)std::lround(m_gx);
        int cy = (int)std::lround(m_gy);
        int r = (int)std::lround(m_gr);

        // 너무 큰 반지름(캔버스 밖으로 한참 나감)은 제외.
        const int maxR = (std::max)(m_cw, m_ch) * 3;
        r = (std::min)(r, maxR);

        DrawCircleThick(cx, cy, r, thick, RGB(20, 20, 20));
    }
}

void CMfcYoutubeDlg::ClearCanvas(COLORREF bg)
{
    BYTE* bits = (BYTE*)m_canvas.GetBits();
    int pitch = m_canvas.GetPitch();
    int w = m_canvas.GetWidth();
    int h = m_canvas.GetHeight();
    if (!bits || w <= 0 || h <= 0) return;

    BYTE r = GetRValue(bg), g = GetGValue(bg), b = GetBValue(bg);

    // 32bpp: BGRA
    for (int y = 0; y < h; ++y)
    {
        BYTE* row = bits + y * pitch;
        for (int x = 0; x < w; ++x)
        {
            DWORD* px = (DWORD*)(row + x * 4);
            *px = (DWORD)(b | (g << 8) | (r << 16) | (0xFFu << 24));
        }
    }
}

void CMfcYoutubeDlg::PutPixel32(int x, int y, COLORREF c)
{
    if (m_canvas.IsNull()) return;
    if (x < 0 || y < 0 || x >= m_canvas.GetWidth() || y >= m_canvas.GetHeight()) return;

    BYTE* bits = (BYTE*)m_canvas.GetBits();
    int pitch = m_canvas.GetPitch();

    BYTE* row = bits + y * pitch;
    DWORD* px = (DWORD*)(row + x * 4);

    BYTE r = GetRValue(c);
    BYTE g = GetGValue(c);
    BYTE b = GetBValue(c);

    *px = (DWORD)(b | (g << 8) | (r << 16) | (0xFFu << 24));
}

void CMfcYoutubeDlg::Plot8(int cx, int cy, int x, int y, COLORREF c)
{
    PutPixel32(cx + x, cy + y, c);
    PutPixel32(cx - x, cy + y, c);
    PutPixel32(cx + x, cy - y, c);
    PutPixel32(cx - x, cy - y, c);
    PutPixel32(cx + y, cy + x, c);
    PutPixel32(cx - y, cy + x, c);
    PutPixel32(cx + y, cy - x, c);
    PutPixel32(cx - y, cy - x, c);
}

void CMfcYoutubeDlg::DrawCircleMidpoint(int cx, int cy, int r, COLORREF c)
{
    if (r <= 0) return;

    int x = 0;
    int y = r;
    int d = 1 - r;

    Plot8(cx, cy, x, y, c);
    while (x < y)
    {
        ++x;
        if (d < 0)
        {
            d += 2 * x + 1;
        }
        else
        {
            --y;
            d += 2 * (x - y) + 1;
        }
        Plot8(cx, cy, x, y, c);
    }
}

void CMfcYoutubeDlg::DrawCircleThick(int cx, int cy, int r, int thickness, COLORREF c)
{
    if (thickness < 1) thickness = 1;
    if (r <= 0) return;

    // 두께 구현: 동심원 여러 개를 그려서 테두리 두께 확보
    for (int t = 0; t < thickness; ++t)
        DrawCircleMidpoint(cx, cy, r + t, c);
}

void CMfcYoutubeDlg::DrawFilledCircle(int cx, int cy, int r, COLORREF c)
{
    if (r <= 0) return;

    const int rr = r * r;

    // 채우기 구현: y를 기준으로 내부 x 범위 채우기
    for (int dy = -r; dy <= r; ++dy)
    {
        int y = cy + dy;
        int dxLimit = (int)std::sqrt((double)(rr - dy * dy));

        for (int dx = -dxLimit; dx <= dxLimit; ++dx)
        {
            int x = cx + dx;
            PutPixel32(x, y, c);
        }
    }
}
