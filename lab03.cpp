#include <windows.h>
#include <cmath>
#include <string>

// --- ZMIENNE GLOBALNE ---
HWND hDigits[12];
bool isDragging = false;
double startMouseAngle = 0.0;
WNDPROC OldStaticProc;

// --- FUNKCJA AKTUALIZUJĄCA POZYCJE (Rysuje kółko) ---
// offsetAngle = 0.0 to stan bazowy.
void UpdatePositions(HWND hwnd, double offsetAngle) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int cx = (rc.right - rc.left) / 2;
    int cy = (rc.bottom - rc.top) / 2;
    int radius = 150; // Promień kółka

    for (int i = 0; i < 12; ++i) {
        // Cyfra na tarczy (12 na samej górze, potem 1, 2, 3...)
        int faceNumber = (i == 0) ? 12 : i;
        
        // Obliczamy stały kąt bazowy dla danego okienka (zacznij od -90 stopni dla godziny 12)
        double baseAngle = -1.5707963268 + (i * 0.5235987756); // -PI/2 + i*(PI/6)
        
        // Dodajemy przesunięcie wynikające z ciągnięcia myszką
        double currentAngle = baseAngle + offsetAngle;

        // Wyliczamy współrzędne X i Y (odejmujemy 20, by wyśrodkować okienko 40x40)
        int x = cx + (int)(radius * cos(currentAngle)) - 20;
        int y = cy + (int)(radius * sin(currentAngle)) - 20;

        SetWindowPos(hDigits[i], nullptr, x, y, 40, 40, SWP_NOZORDER);
    }
}

// --- SUBCLASSING OKIENEK DZIECI ---
// To sprawia, że kliknięcie w małe okienko (STATIC) wysyła sygnał do okna głównego
LRESULT CALLBACK DigitSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_LBUTTONDOWN) {
        HWND hParent = GetParent(hwnd);
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        MapWindowPoints(hwnd, hParent, &pt, 1); // Tłumaczymy kordynaty myszy na okno główne
        SendMessageW(hParent, uMsg, wParam, MAKELPARAM(pt.x, pt.y)); // Przekazujemy kliknięcie
        return 0; // Blokujemy domyślne zachowanie kontrolki
    }
    return CallWindowProc(OldStaticProc, hwnd, uMsg, wParam, lParam);
}

// --- GŁÓWNA PROCEDURA OKNA ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT); // Standardowa czcionka systemu

        for (int i = 0; i < 12; ++i) {
            int faceNumber = (i == 0) ? 12 : i;
            std::wstring text = std::to_wstring(faceNumber);
            
            // Tworzymy okienka z ramką (WS_BORDER)
            hDigits[i] = CreateWindowExW(
                0, L"STATIC", text.c_str(),
                WS_CHILD | WS_VISIBLE | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
                0, 0, 40, 40,
                hwnd, nullptr, GetModuleHandle(nullptr), nullptr
            );
            
            SendMessageW(hDigits[i], WM_SETFONT, (WPARAM)hFont, FALSE);

            // Podpinamy nasz własny system przechwytywania myszy pod to okienko
            OldStaticProc = (WNDPROC)SetWindowLongPtrW(hDigits[i], GWLP_WNDPROC, (LONG_PTR)DigitSubclassProc);
        }
        
        // Ustawiamy okienka na pozycjach startowych (obrót = 0.0)
        UpdatePositions(hwnd, 0.0);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        
        // Sprawdzamy czy kliknięto faktycznie na jedno z okienek
        HWND hClicked = ChildWindowFromPoint(hwnd, pt);
        bool isDigit = false;
        for (int i = 0; i < 12; ++i) {
            if (hClicked == hDigits[i]) isDigit = true;
        }

        // Jeśli tak, rozpoczynamy obrót
        if (isDigit) {
            isDragging = true;
            RECT rc; GetClientRect(hwnd, &rc);
            int cx = (rc.right - rc.left) / 2;
            int cy = (rc.bottom - rc.top) / 2;
            
            // Zapisujemy startowy kąt, pod jakim znajduje się myszka
            startMouseAngle = atan2(pt.y - cy, pt.x - cx);
            
            // Rezerwujemy myszkę dla okna głównego (nie zgubi śledzenia po wyjechaniu za krawędź)
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
            
            // Wyliczamy obecny kąt myszki
            double currentMouseAngle = atan2(pt.y - cy, pt.x - cx);
            
            // Obracamy kółko o tyle, o ile przesunęła się myszka
            UpdatePositions(hwnd, currentMouseAngle - startMouseAngle);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (isDragging) {
            isDragging = false;
            ReleaseCapture(); // Zwalniamy śledzenie myszy
            
            // PO PUSZCZENIU: wymuszamy powrót kółka do stanu bazowego (obrót = 0.0)
            UpdatePositions(hwnd, 0.0);
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// --- PUNKT WEJŚCIA (Main) ---
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"ObrotoweKoloClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Obrotowe Okienka",
        WS_OVERLAPPEDWINDOW, // Zwykłe okno
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 600, // Rozmiar
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
