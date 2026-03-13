#include <windows.h>
#include <cmath>
#include <string>

// --- ZMIENNE GLOBALNE ---
HWND hDigits[12];
bool isDragging = false;
bool isAnimating = false;

double currentOffsetAngle = 0.0; // Aktualny obrót tarczy
double startMouseAngle = 0.0;    // Kąt chwytu myszki
double velocity = 0.0;           // Prędkość obrotu (dla fizyki sprężyny)

WNDPROC OldStaticProc;

// --- FUNKCJA AKTUALIZUJĄCA POZYCJE ---
void UpdatePositions(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int cx = (rc.right - rc.left) / 2;
    int cy = (rc.bottom - rc.top) / 2;
    int radius = 150;

    for (int i = 0; i < 12; ++i) {
        int faceNumber = (i == 0) ? 12 : i;
        // Kąt bazowy (od -90 stopni)
        double baseAngle = -1.5707963268 + (i * 0.5235987756); 
        
        // Dodajemy nasz aktualny, globalny obrót
        double currentAngle = baseAngle + currentOffsetAngle;

        int x = cx + (int)(radius * cos(currentAngle)) - 20;
        int y = cy + (int)(radius * sin(currentAngle)) - 20;

        SetWindowPos(hDigits[i], nullptr, x, y, 40, 40, SWP_NOZORDER);
    }
}

// --- SUBCLASSING (Przekazywanie kliknięć do głównego okna) ---
LRESULT CALLBACK DigitSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_LBUTTONDOWN) {
        HWND hParent = GetParent(hwnd);
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        MapWindowPoints(hwnd, hParent, &pt, 1);
        SendMessageW(hParent, uMsg, wParam, MAKELPARAM(pt.x, pt.y));
        return 0;
    }
    return CallWindowProc(OldStaticProc, hwnd, uMsg, wParam, lParam);
}

// --- GŁÓWNA PROCEDURA OKNA ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        for (int i = 0; i < 12; ++i) {
            int faceNumber = (i == 0) ? 12 : i;
            std::wstring text = std::to_wstring(faceNumber);
            
            hDigits[i] = CreateWindowExW(
                0, L"STATIC", text.c_str(),
                WS_CHILD | WS_VISIBLE | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
                0, 0, 40, 40, hwnd, nullptr, GetModuleHandle(nullptr), nullptr
            );
            
            SendMessageW(hDigits[i], WM_SETFONT, (WPARAM)hFont, FALSE);
            OldStaticProc = (WNDPROC)SetWindowLongPtrW(hDigits[i], GWLP_WNDPROC, (LONG_PTR)DigitSubclassProc);
        }
        
        UpdatePositions(hwnd);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        
        HWND hClicked = ChildWindowFromPoint(hwnd, pt);
        bool isDigit = false;
        for (int i = 0; i < 12; ++i) {
            if (hClicked == hDigits[i]) isDigit = true;
        }

        if (isDigit) {
            // Zatrzymujemy ewentualną animację, jeśli złapiemy w locie
            if (isAnimating) {
                KillTimer(hwnd, 1);
                isAnimating = false;
                velocity = 0.0;
            }

            isDragging = true;
            RECT rc; GetClientRect(hwnd, &rc);
            int cx = (rc.right - rc.left) / 2;
            int cy = (rc.bottom - rc.top) / 2;
            
            // Odejmujemy currentOffsetAngle, by tarcza nie "skoczyła" pod kursor
            startMouseAngle = atan2(pt.y - cy, pt.x - cx) - currentOffsetAngle;
            SetCapture(hwnd);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (isDragging) {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rc; GetClientRect(hwnd, &rc);
            int cx = (rc.right - rc.left) / 2;
            int cy = (rc.bottom - rc.top) / 2;
            
            double currentMouseAngle = atan2(pt.y - cy, pt.x - cx);
            
            // Sztywno podpinamy wychylenie pod ruch myszy
            currentOffsetAngle = currentMouseAngle - startMouseAngle;
            UpdatePositions(hwnd);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (isDragging) {
            isDragging = false;
            ReleaseCapture();
            
            // Zamiast zerować obrót, STARTUJEMY ANIMACJĘ SPRĘŻYNY
            isAnimating = true;
            SetTimer(hwnd, 1, 16, nullptr); // ~60 FPS
        }
        return 0;
    }

    case WM_TIMER: {
        if (isAnimating) {
            // FIZYKA SPRĘŻYNY
            // 1. Siła sprężyny ciągnąca w stronę zera (kąt bazowy)
            velocity += (0.0 - currentOffsetAngle) * 0.1; 
            
            // 2. Tarcie, żeby sprężyna zwalniała
            velocity *= 0.85; 
            
            // 3. Dodanie prędkości do pozycji
            currentOffsetAngle += velocity; 

            UpdatePositions(hwnd);

            // Jeśli tarcza już prawie stoi i jest w punkcie 0, wyłączamy timer
            if (abs(currentOffsetAngle) < 0.001 && abs(velocity) < 0.001) {
                currentOffsetAngle = 0.0;
                velocity = 0.0;
                isAnimating = false;
                KillTimer(hwnd, 1);
                UpdatePositions(hwnd); // Ostateczne wyrównanie do zera
            }
        }
        return 0;
    }

    case WM_DESTROY:
        if (isAnimating) KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// --- PUNKT WEJŚCIA (Main) ---
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"SprezystaTarczaClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Obrotowe Okienka (Sprezyna)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 600,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr) return 0;
    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
