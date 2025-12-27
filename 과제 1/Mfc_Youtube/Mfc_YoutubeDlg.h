#pragma once
#include <atomic>

#define WM_APP_RANDOMMOVE (WM_APP + 101)

struct RandomMovePayload
{
    CPoint pts[3];
};

class CMfcYoutubeDlg : public CDialogEx
{
public:
    CMfcYoutubeDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_MFCYOUTUBE_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnBnClickedBtnReset();
    afx_msg void OnBnClickedBtnMove();
    afx_msg LRESULT OnAppRandomMove(WPARAM wParam, LPARAM lParam);
    afx_msg void OnDestroy();

    DECLARE_MESSAGE_MAP()

private:
    // ===== 캔버스 =====
    CImage m_canvas;
    int m_cw = 0;
    int m_ch = 0;

    // ===== 데이터 =====
    CPoint m_pts[3]{};
    int m_ptCount = 0;              // 저장된 점 (0~3)
	bool m_hasGarden = false;       // 원 사용 여부
	double m_gx = 0.0;              // 원 중심 x
	double m_gy = 0.0;              // 원 중심 y
	double m_gr = 0.0;              // 원 중심 반지름

    // ===== 드래그 =====
    bool m_dragging = false;
    int m_dragIndex = -1;

    // ===== 랜덤 이동 =====
    std::atomic_bool m_randomRunning{ false };
    CWinThread* m_pMoveThread = nullptr;

private:
    // ===== 헬퍼 함수 =====
	int GetPointRadius();       // IDC_RADIUS
    int GetGardenThickness();   // IDC_THICK
    void UpdatePointStatics();
    void ResetAll();

    // 렌더링
    void EnsureCanvasSize();
    void ClearCanvas(COLORREF bg);
    void RenderScene();
    void BlitCanvas(CDC& dc);

    // 원 그리기
    void PutPixel32(int x, int y, COLORREF c);
    void Plot8(int cx, int cy, int x, int y, COLORREF c);
    void DrawCircleMidpoint(int cx, int cy, int r, COLORREF c);
    void DrawCircleThick(int cx, int cy, int r, int thickness, COLORREF c);
    void DrawFilledCircle(int cx, int cy, int r, COLORREF c);


    // 기하
    bool ComputeCircumcircle(); // m_pts[0~2]에서 -> m_gx,m_gy,m_gr 계산
    bool HitTestPointCircle(int idx, CPoint mousePt, int pointR);

    // 랜덤
    static UINT MoveThreadProc(LPVOID pParam);
    CPoint RandomPointInClient(int margin);
};
