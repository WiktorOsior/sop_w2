#include "app_2048.h"
#include <stdexcept>

std::wstring const app_2048::s_class_name{ L"2048 Window" };
bool app_2048::register_class() {
	WNDCLASSEX desc{};
	if (GetClassInfoExW(m_instance, s_class_name.c_str(), &desc) != 0) return true;
	desc = { .cbSize = sizeof(WNDCLASSEXW),
		.lpfnWndProc = window_proc_static,
		.hInstance = m_instance,
		.hCursor = LoadCursorW(nullptr, L"IDC_ARROW"),
		.hbrBackground = CreateSolidBrush(RGB(250, 247, 238)),
		.lpszClassName = s_class_name.c_str() };
	return RegisterClassExW(&desc) != 0;
}
HWND app_2048::create_window(DWORD style, HWND parent) {
	RECT size{ 0, 0, board::width, board::height };
	AdjustWindowRectEx(&size, style, false, 0);
	HWND window =  CreateWindowExW(
		0,
		s_class_name.c_str(),
		L"2048",
		style,
		CW_USEDEFAULT, 0,
		size.right - size.left, size.bottom - size.top,
		parent,
		nullptr,
		m_instance,
		this);
	for (auto& f : m_board.fields())
		CreateWindowExW(
			0,
			L"STATIC",
			nullptr,
			WS_CHILD | WS_VISIBLE | SS_CENTER,
			f.position.left, f.position.top,
			f.position.right - f.position.left,
			f.position.bottom - f.position.top,
			window,
			nullptr,
			m_instance,
			nullptr);
	return window;
}

LRESULT app_2048::window_proc_static(
	HWND window,
	UINT message,
	WPARAM wparam,
	LPARAM lparam)
{
	app_2048* app = nullptr;
	if (message == WM_NCCREATE)
	{
		auto p = reinterpret_cast<LPCREATESTRUCTW>(lparam);
		app = static_cast<app_2048*>(p->lpCreateParams);
		SetWindowLongPtrW(window, GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(app));
	}
	else
	{
		app = reinterpret_cast<app_2048*>(
			GetWindowLongPtrW(window, GWLP_USERDATA));
	}
	if (app != nullptr)
	{
		return app->window_proc(window, message,
			wparam, lparam);
	}
	return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT app_2048::window_proc(
	HWND window, UINT message,
	WPARAM wparam, LPARAM lparam)
{
	switch (message) {
	case WM_CLOSE:
		DestroyWindow(window);
		return 0;
	case WM_DESTROY:
		if (window == m_main)
			PostQuitMessage(EXIT_SUCCESS);
		return 0;
	case WM_CTLCOLORSTATIC:
		return reinterpret_cast<INT_PTR>(m_field_brush);
	case WM_WINDOWPOSCHANGED:
		on_window_move(window,
			reinterpret_cast<LPWINDOWPOS>(lparam));
		return 0;
	}
	return DefWindowProcW(window, message, wparam, lparam);
}

app_2048::app_2048(HINSTANCE instance)
	: m_instance{ instance }, m_main{}, m_popup{},
	m_field_brush{ CreateSolidBrush(RGB(204, 192, 174)) },
	m_screen_size{ GetSystemMetrics(SM_CXSCREEN),
	GetSystemMetrics(SM_CYSCREEN) }
{
	register_class();
	DWORD main_style = WS_OVERLAPPED | WS_SYSMENU |
		WS_CAPTION | WS_MINIMIZEBOX;
	DWORD popup_style = WS_OVERLAPPED | WS_CAPTION;
	m_main = create_window(main_style);
	m_popup = create_window(popup_style, m_main);
}

int app_2048::run(int show_command)
{
	ShowWindow(m_main, show_command);
	ShowWindow(m_popup, SW_SHOWNA);
	MSG msg{};
	BOOL result = TRUE;
	while ((result = GetMessageW(&msg, nullptr, 0, 0)) != 0)
	{
		if (result == -1)
			return EXIT_FAILURE;
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return EXIT_SUCCESS;
}
void app_2048::on_window_move(
	HWND window,
	LPWINDOWPOS params)
{
	HWND other = (window == m_main) ? m_popup : m_main;

	// --- KROK 5A: Znajdź dokładny środek planszy poruszanego okna (Cw) ---
	// Najpierw bierzemy środek planszy względem samego pola roboczego (obszaru bez ramek)
	POINT Cw{ board::width / 2, board::height / 2 };

	// Następnie prosimy Windows, aby przeliczył ten lokalny punkt 
	// na globalne współrzędne całego monitora
	ClientToScreen(window, &Cw);

	// --- KROK 4: Oblicz symetryczny punkt docelowy dla drugiego okna (Co) ---
	// Odwracamy pozycję względem środka ekranu.
	// Wzór: Co = Ss - Cw  (gdzie Ss to rozmiar ekranu, a Cw to środek pierwszej planszy)
	POINT Co{
		m_screen_size.x - Cw.x,
		m_screen_size.y - Cw.y
	};

	// --- KROK 5B: Oblicz nową pozycję dla drugiego okna (Po) ---
	// Pobieramy aktualną pozycję całej ramki drugiego okna
	RECT other_rc;
	GetWindowRect(other, &other_rc);

	// Aby poprawnie ustawić drugie okno, musimy wiedzieć, jaka jest 
	// odległość (offset) między jego lewym górnym rogiem a środkiem jego planszy.
	POINT other_board_center{ board::width / 2, board::height / 2 };
	ClientToScreen(other, &other_board_center); // Globalne kordynaty środka planszy drugiego okna

	// Wektor przesunięcia: o ile pikseli w prawo i w dół od lewego górnego rogu 
	// znajduje się środek planszy.
	long offset_x = other_board_center.x - other_rc.left;
	long offset_y = other_board_center.y - other_rc.top;

	// Odejmujemy ten offset od naszego docelowego środka, aby uzyskać 
	// współrzędne lewego górnego rogu ramki (których wymaga funkcja SetWindowPos)
	POINT new_pos{
		Co.x - offset_x,
		Co.y - offset_y
	};

	// --- ZABEZPIECZENIE: Przerwij, jeśli okno już jest na właściwym miejscu ---
	if (new_pos.x == other_rc.left && new_pos.y == other_rc.top)
		return;

	// Fizyczne przesunięcie drugiego okna
	SetWindowPos(other, nullptr, new_pos.x, new_pos.y,
		0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
}
