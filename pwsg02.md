# Analiza kodu aplikacji Win32 (app_ractangles)

Poniżej znajduje się szczegółowy podział Twojego kodu na logiczne sekcje. Każda część zawiera paragraf z opisem działania oraz kod ze szczegółowymi komentarzami dla każdej linijki.

---

## 1. Nazwa klasy okna

Ten fragment definiuje unikalną nazwę, która będzie identyfikować klasę Twojego okna w systemie Windows. Jest ona wymagana podczas rejestracji oraz tworzenia okna.

```cpp
// Definiuje stałą, statyczną zmienną przechowującą szeroki łańcuch znaków (wstring) z nazwą klasy.
std::wstring const app_ractangles::s_class_name{ L"2048 Window" };
```

---

## 2. Rejestracja klasy okna (`register_class`)

Funkcja ta wypełnia strukturę opisującą właściwości okna (takie jak kolor tła, kursor, wskaźnik do funkcji obsługującej komunikaty) i rejestruje ją w systemie operacyjnym, aby później można było stworzyć okno na jej podstawie.

```cpp
bool app_ractangles::register_class() {
	WNDCLASSEX desc{}; // Tworzy i zeruje strukturę konfiguracji klasy okna (WNDCLASSEX).
	
	// Sprawdza, czy klasa o tej nazwie została już zarejestrowana. Jeśli tak, zwraca true.
	if (GetClassInfoExW(m_instance, s_class_name.c_str(), &desc) != 0) return true;	
	
	// Wypełnia strukturę konfiguracji używając nowoczesnej składni inicjalizatora:
	desc = { 
		.cbSize = sizeof(WNDCLASSEXW), // Podaje rozmiar struktury (wymagane przez API Windows).
		.lpfnWndProc = window_proc_static, // Przypisuje statyczną funkcję obsługi komunikatów okna.
		.hInstance = m_instance, // Przypisuje uchwyt instancji aplikacji (z konstruktora).
		.hCursor = LoadCursorW(nullptr, L"IDC_ARROW"), // Ładuje domyślny kursor systemowy w kształcie strzałki.
		.hbrBackground = CreateSolidBrush(RGB(30, 50, 90)), // Tworzy pędzel dla tła okna (ciemnoniebieski kolor).
		.lpszClassName = s_class_name.c_str() // Przypisuje wcześniej zdefiniowaną nazwę klasy okna.
	};	
	
	// Rejestruje klasę w systemie i zwraca true, jeśli się powiodło (wynik inny niż 0).
	return RegisterClassExW(&desc) != 0;
}
```

---

## 3. Tworzenie okna (`create_window`)

Ta metoda fizycznie tworzy okno systemowe. Oblicza ona dokładny rozmiar całego okna w taki sposób, aby sam obszar roboczy (wnętrze, po którym rysujesz) wynosił dokładnie 800 na 600 pikseli, dodając miejsce na ramki i paski tytułowe.

```cpp
HWND app_ractangles::create_window(DWORD style, HWND parent) {
	RECT size{ 0, 0, 800 ,600 }; // Definiuje strukturę prostokąta dla pożądanego rozmiaru obszaru roboczego (800x600).
	
	// Dostosowuje wymiary prostokąta tak, aby uwzględniał grubość ramek i paska tytułowego wg podanego stylu.
	AdjustWindowRectEx(&size, style, false, 0);	
	
	// Tworzy główne okno na podstawie zarejestrowanej klasy i parametrów:
	HWND window = CreateWindowExW(
		0, // Rozszerzone style okna (brak).
		s_class_name.c_str(), // Nazwa zarejestrowanej klasy.
		L"MSPAINT", // Tytuł okna, który wyświetli się na górnym pasku.
		style, // Zwykłe style okna (przekazane jako argument).
		CW_USEDEFAULT, 0, // Domyślna pozycja okna X i Y (wybierana przez system).
		size.right - size.left, // Ostateczna szerokość okna z uwzględnieniem ramek.
		size.bottom - size.top, // Ostateczna wysokość okna z uwzględnieniem paska tytułowego.
		parent, // Uchwyt okna rodzica (tutaj nullptr, bo to okno główne).
		nullptr, // Uchwyt do menu okna (brak).
		m_instance, // Uchwyt instancji naszej aplikacji.
		this // WAŻNE: Przekazuje wskaźnik na bieżący obiekt (this), by można było z niego korzystać w window_proc_static.
	);	
	
	// Zwraca uchwyt do nowo utworzonego okna.
	return window;
}
```

---

## 4. Statyczna procedura okna (`window_proc_static`)

Interfejs Windows API wymaga funkcji C (nie powiązanej z konkretnym obiektem C++) do obsługi komunikatów. Ta statyczna metoda działa jako "most". Przechwytuje wskaźnik obiektu `this` (przekazany podczas tworzenia okna) i zapisuje go w wewnętrznej pamięci okna, dzięki czemu może przekierowywać komunikaty systemowe do odpowiedniej metody klasy `app_ractangles`.

```cpp
LRESULT app_ractangles::window_proc_static(
	HWND window, // Uchwyt do okna otrzymującego komunikat.
	UINT message, // Identyfikator komunikatu (np. kliknięcie, narysowanie).
	WPARAM wparam, // Dodatkowy parametr komunikatu.
	LPARAM lparam // Dodatkowy parametr komunikatu.
){
	app_ractangles* app = nullptr; // Wskaźnik, w którym przechowamy instancję naszej klasy.
	
	// Sprawdza, czy to jest komunikat o tworzeniu okna (wysyłany bardzo wcześnie).
	if (message == WM_NCCREATE)	{
		auto p = reinterpret_cast<LPCREATESTRUCTW>(lparam); // Rzutuje lparam na strukturę zawierającą dane z CreateWindowEx.
		app = static_cast<app_ractangles*>(p->lpCreateParams); // Wyciąga wskaźnik "this" przekazany w funkcji create_window.
		
		// Zapisuje wskaźnik do obiektu w specjalnej pamięci okna powiązanej z systemem (GWLP_USERDATA).
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
	}
	else {
		// Dla każdego innego komunikatu pobiera wskaźnik z pamięci okna.
		app = reinterpret_cast<app_ractangles*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	}
	
	// Jeśli wskaźnik do obiektu jest poprawny (został już przypisany)...
	if (app != nullptr)	{
		// ...wywołuje niestatyczną metodę "window_proc" bezpośrednio na tym obiekcie.
		return app->window_proc(window, message, wparam, lparam);
	}
	
	// Jeśli obiekt jeszcze nie istnieje, pozwala systemowi Windows obsłużyć komunikat domyślnie.
	return DefWindowProcW(window, message, wparam, lparam);
}
```

---

## 5. Główna procedura okna (`window_proc`)

To "serce" Twojej aplikacji. Funkcja ta podejmuje decyzje na podstawie tego, co dzieje się z oknem. W Twoim przypadku odpowiada ona za logikę rysowania prostokątów myszką. Prostokąty tworzone są jako małe, puste okienka typu `STATIC`, które kolorujesz przechwytując komunikat malowania, a ich rozmiar zmieniasz podczas ruchu kursorem.

```cpp
LRESULT app_ractangles::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam){
	switch (message) { // Rozpoczyna sprawdzanie rodzaju otrzymanego komunikatu.
	case WM_CLOSE: // Użytkownik kliknął "X" aby zamknąć okno.
		DestroyWindow(window); // Niszczy okno (generuje za chwilę komunikat WM_DESTROY).
		return 0; // Informuje system, że komunikat został obsłużony.
		
	case WM_DESTROY: // Okno jest fizycznie usuwane z pamięci.
		if (window == m_main) // Jeśli niszczone okno jest oknem głównym aplikacji...
			PostQuitMessage(EXIT_SUCCESS); // ...wyślij komunikat o zakończeniu działania programu (do pętli run).
		return 0; // Komunikat obsłużony.
		
	case WM_LBUTTONDOWN: // Użytkownik Wcisnął lewy przycisk myszy.
		drawing = 1; // Ustawia flagę rysowania na aktywną.
		x = (short)LOWORD(lparam); // Pobiera z lparam koordynatę X myszki (początek rysowania).
		y = (short)HIWORD(lparam); // Pobiera z lparam koordynatę Y myszki (początek rysowania).
		
		// Tworzy nowe okno typu "STATIC" pełniące rolę prostokąta, początkowo z wymiarami 0x0.
		last = CreateWindowExW(
			0, // Brak specjalnych stylów rozszerzonych.
			L"STATIC", // Typ kontrolki (zwykły statyczny obszar).
			nullptr, // Brak tekstu na kontrolce.
			WS_CHILD | WS_VISIBLE, // Styl: jest to okno-dziecko i jest widoczne.
			x, y, // Pozycja X, Y kontrolki.
			0, 0, // Szerokość i wysokość (na razie 0).
			window, // Oknem-rodzicem jest główne okno aplikacji.
			nullptr, // Brak menu/ID.
			m_instance, // Instancja aplikacji.
			nullptr); // Brak dodatkowych parametrów.
			
		if (recs.empty()) { // Jeśli wektor prostokątów jest pusty...
			recs = { last }; // ...inicjalizuje go tym pierwszym nowym prostokątem.
		}
		else {
			recs.push_back(last); // ...w przeciwnym razie dopisuje go na koniec listy.
		}
		SetCapture(window); // Skupia uwagę myszy na oknie głównym (nawet jak wyjdzie za krawędź podczas przeciągania).
		return 0;
		
	case WM_MOUSEMOVE: // Myszka poruszyła się w obszarze okna.
		if (!recs.empty()) { // Upewnia się, że istnieje jakikolwiek stworzony prostokąt.
			if (drawing == 0) return 0; // Jeśli użytkownik nie trzyma wciśniętego guzika, pomiń.
			
			currx = (short)LOWORD(lparam); // Pobiera aktualną pozycję X kursora.
			curry = (short)HIWORD(lparam); // Pobiera aktualną pozycję Y kursora.
			
			// Aktualizuje pozycję i rozmiar aktualnie rysowanego (ostatniego) prostokąta.
			// Używa min(), aby ustalić lewy górny róg, i abs() do obliczenia ostatecznej szerokości i wysokości.
			SetWindowPos(recs.back(), nullptr, min(currx, x), min(curry, y), abs(currx - x), abs(curry - y), SWP_NOZORDER);
		}
		return 0;
		
	case WM_LBUTTONUP: // Użytkownik puścił lewy przycisk myszy.
		ReleaseCapture(); // Zwalnia przejęcie myszy przez okno (przywraca normalne zachowanie systemu).
		drawing = 0; // Wyłącza tryb rysowania.
		return 0;
		
	case WM_CTLCOLORSTATIC: // Komunikat wywoływany przez kontrolki STATIC, pytający jakiego koloru tła użyć.
		return reinterpret_cast<INT_PTR>(m_field_brush); // Zwraca pędzel (m_field_brush), malując nim wszystkie prostokąty na jednolity kolor.
		return 0; // (Nieużywane: powyżej jest już return).
	}
	
	// Jeśli pętla case nie obsłużyła komunikatu, oddaj go domyślnej procedurze systemu Windows.
	return DefWindowProcW(window, message, wparam, lparam);
}
```

---

## 6. Konstruktor

Miejsce inicjalizacji obiektu. Tworzy pędzel do malowania, wywołuje rejestrację klasy, a na końcu tworzy główne okno określając, jakie atrybuty ma posiadać.

```cpp
app_ractangles::app_ractangles(HINSTANCE instance)
	: m_instance{ instance }, // Inicjalizuje zmienną członkowską m_instance uchwytem programu.
	m_main{}, // Zeruje zmienną na okno główne.
	m_field_brush{ CreateSolidBrush(RGB(170, 70, 80)) } // Inicjalizuje pędzel kolorujący prostokąty (kolor buraczkowy/czerwony).
{
	register_class(); // Wywołuje funkcję rejestrującą klasę okna.
	
	// Definiuje styl głównego okna: posiada ramkę, menu systemowe, pasek z tytułem, przycisk minimalizacji oraz odcina odświeżanie tła pod kontrolkami.
	DWORD main_style = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
		
	m_main = create_window(main_style); // Tworzy główne okno i przypisuje jego uchwyt do zmiennej m_main.
}
```

---

## 7. Pętla komunikatów (`run`)

Metoda odpowiedzialna za uruchomienie działania programu po konfiguracji. Okno zostaje wyświetlone, a program wpada w pętlę nieskończoną przechwytującą ruchy użytkownika i komunikaty systemowe aż do zamknięcia.

```cpp
int app_ractangles::run(int show_command){
	ShowWindow(m_main, show_command); // Pokazuje utworzone okno na ekranie (w trybie zdefiniowanym przez system, np. zminimalizowane/normalne).
	
	MSG msg{}; // Tworzy i zeruje strukturę MSG przechowującą informacje o aktualnym komunikacie.
	BOOL result = TRUE; // Zmienna trzymająca wynik operacji pobierania komunikatu.
	
	// Główna pętla komunikatów. GetMessageW wyciąga komunikaty z kolejki systemu Windows, a pętla trwa dopóki system nie wyśle komunikatu zamknięcia.
	while ((result = GetMessageW(&msg, nullptr, 0, 0)) != 0)
	{
		if (result == -1) // Jeśli GetMessage zwróci -1, oznacza to błąd krytyczny.
			return EXIT_FAILURE; // Zamyka program z kodem błędu.
			
		TranslateMessage(&msg); // Tłumaczy kody wciśnięć klawiszy na czytelne znaki (np. generuje WM_CHAR).
		DispatchMessageW(&msg); // Rozsyła komunikat do odpowiedniej procedury okna (w naszym przypadku wywołuje window_proc_static).
	}
	
	// Zwraca kod sukcesu po opuszczeniu pętli (gdy pobrano komunikat WM_QUIT).
	return EXIT_SUCCESS;
}
```

---

## 8. Czysty Boilerplate (Szablon) C++ Win32 API

Ten kod możesz skopiować do całkowicie czystego pliku `.cpp`. Zawiera w pełni gotową i elegancką bazę pod każdą nową aplikację opartą na Win32 API (w tym poprawny punkt wejścia `wWinMain`).

```cpp
#include <windows.h>
#include <string>

// --- Klasa Aplikacji ---
class Application {
public:
	Application(HINSTANCE hInstance);
	~Application();
	
	int Run(int showCommand);

private:
	bool RegisterWindowClass();
	HWND CreateMainWindow(DWORD style, HWND parent = nullptr);
	
	static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	HINSTANCE m_hInstance;
	HWND m_hMainWindow;
	static const std::wstring s_ClassName;
};

// Definicja stałej nazwy klasy
const std::wstring Application::s_ClassName = L"Win32BaseAppClass";

// --- Implementacja Aplikacji ---

Application::Application(HINSTANCE hInstance) : m_hInstance(hInstance), m_hMainWindow(nullptr) {
	RegisterWindowClass();
	m_hMainWindow = CreateMainWindow(WS_OVERLAPPEDWINDOW); // Okno ze wszystkimi standardowymi przyciskami i możliwością zmiany rozmiaru
}

Application::~Application() {
	// Miejsce na zwalnianie zasobów (GDI, etc.)
}

bool Application::RegisterWindowClass() {
	WNDCLASSEXW wc = {0};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = StaticWindowProc;
	wc.hInstance = m_hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Domyślne białe tło systemu
	wc.lpszClassName = s_ClassName.c_str();

	return RegisterClassExW(&wc) != 0;
}

HWND Application::CreateMainWindow(DWORD style, HWND parent) {
	HWND hwnd = CreateWindowExW(
		0,
		s_ClassName.c_str(),
		L"Moja Aplikacja Win32", // Tytuł okna
		style,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, // Pozycja i rozmiar
		parent,
		nullptr,
		m_hInstance,
		this // Przekazanie wskaźnika this
	);
	return hwnd;
}

LRESULT CALLBACK Application::StaticWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	Application* pThis = nullptr;

	if (uMsg == WM_NCCREATE) {
		CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
		pThis = (Application*)pCreate->lpCreateParams;
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
	} else {
		pThis = (Application*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	}

	if (pThis) {
		return pThis->WindowProc(hwnd, uMsg, wParam, lParam);
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Application::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
			
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			// Tutaj odbywa się własne rysowanie GDI
			EndPaint(hwnd, &ps);
			return 0;
		}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int Application::Run(int showCommand) {
	if (!m_hMainWindow) return EXIT_FAILURE;

	ShowWindow(m_hMainWindow, showCommand);
	UpdateWindow(m_hMainWindow);

	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

// --- Główny punkt wejścia Windows ---
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	Application app(hInstance);
	return app.Run(nCmdShow);
}
```

# Kompendium WinAPI na Laboratoria: Kontrolki, Menu i Dialogi

Poniższy dokument to kompletna ściąga z tworzenia interfejsu użytkownika w czystym WinAPI w C++. Skupia się na trzech głównych elementach: Kontrolkach (przyciski, pola tekstowe), Menu (z zasobów i z kodu) oraz Oknach Dialogowych.

---

## CZĘŚĆ 1: KONTROLKI (Przyciski i Pola tekstowe)

Kontrolki to tak naprawdę zwykłe okna, ale o z góry określonych klasach (np. `"BUTTON"`, `"EDIT"`). Tworzymy je funkcją `CreateWindowExW` w momencie, gdy powstaje okno główne (komunikat `WM_CREATE`).

### 1.1 Tworzenie kontrolek (`WM_CREATE`)

Na górze pliku zdefiniuj unikalne identyfikatory:
```cpp
#define ID_MY_BUTTON 1001
#define ID_MY_TEXTBOX 1002
```

W funkcji `window_proc` wewnątrz instrukcji `switch(message)`:
```cpp
case WM_CREATE: 
{
    // 1. TWORZENIE PRZYCISKU (Button)
    CreateWindowExW(
        0, 
        L"BUTTON", // Klasa "BUTTON" - wbudowana w Windows
        L"Zapisz dane", // Napis wyświetlany na przycisku
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, // Style: widoczny, dziecko okna głównego, standardowy przycisk
        50, 50, // Pozycja X, Y w oknie
        120, 30, // Szerokość, Wysokość
        window, // Uchwyt (HWND) okna rodzica
        (HMENU)ID_MY_BUTTON, // NAJWAŻNIEJSZE: Rzutujemy nasze ID na HMENU. Dzięki temu rozpoznamy ten przycisk.
        m_instance, nullptr
    );

    // 2. TWORZENIE POLA TEKSTOWEGO (Edit)
    CreateWindowExW(
        WS_EX_CLIENTEDGE, // Styl rozszerzony: wklęsła ramka 3D
        L"EDIT", // Klasa "EDIT" - pole do wpisywania tekstu
        L"", // Pusty tekst na start
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
        50, 100, 200, 25, // Pozycja X, Y, Szerokość, Wysokość
        window, // Rodzic
        (HMENU)ID_MY_TEXTBOX, // ID naszego pola tekstowego
        m_instance, nullptr
    );
    return 0;
}
```

### 1.2 Obsługa zdarzeń kontrolek (`WM_COMMAND`)

Kiedy użytkownik klika przycisk, system wysyła do okna komunikat `WM_COMMAND`.

```cpp
case WM_COMMAND:
{
    // LOWORD(wparam) ZAWSZE zawiera ID kontrolki, która wywołała akcję.
    int control_id = LOWORD(wparam); 

    switch(control_id) 
    {
        case ID_MY_BUTTON: // Użytkownik kliknął nasz przycisk (1001)
        {
            wchar_t buffer[256]; 
            // Pobieramy wpisany tekst z pola tekstowego. GetDlgItem znajduje kontrolkę.
            GetWindowTextW(GetDlgItem(window, ID_MY_TEXTBOX), buffer, 256); 

            // Wyświetlamy pobrany tekst w okienku Popup
            MessageBoxW(window, buffer, L"Wpisałeś to:", MB_OK | MB_ICONINFORMATION); 
            break;
        }
    }
    return 0;
}
```

---

## CZĘŚĆ 2: MENU GÓRNE (Pasek Menu)

Menu można stworzyć na dwa sposoby. Sposób A jest zazwyczaj wymagany na zajęciach, Sposób B to dobra alternatywa "ratunkowa".

### SPOSÓB A: Z użyciem Plików Zasobów (.rc) [WYMAGANE PRZEZ INSTRUKCJĘ]

Projektujesz menu myszką w środowisku Visual Studio, a w kodzie tylko "podpinasz" gotowy efekt.

**Krok 1: Praca w Visual Studio (Edytor wizualny)**
1. W **Solution Explorer** kliknij dwukrotnie `Resource.rc` lub dodaj nowy (Prawy przycisk myszy -> *Add* -> *Resource* -> *Menu*).
2. Otworzy się edytor wizualny. Kliknij *Type Here* i wpisz np. `Game` (Dodając `&` przed literą, np. `&Game`, umożliwiasz skrót `Alt+G`).
3. We właściwościach (*Properties Window*) zmień ID całego menu na `ID_MAINMENU`.
4. Dodaj opcję wewnątrz (np. `New Game` ze skrótem `&New Game\tCtrl+N`) i zmień jej ID na `ID_NEWGAME`.

**Krok 2: Modyfikacja C++ - Podpięcie Menu**
W funkcji rejestrującej klasę (`register_class`) podepnij identyfikator zdefiniowany w zasobach:
```cpp
bool app_ractangles::register_class() {
    WNDCLASSEX desc{};
    if (GetClassInfoExW(m_instance, s_class_name.c_str(), &desc) != 0) return true;
    
    desc = { 
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = window_proc_static,
        .hInstance = m_instance,
        .hCursor = LoadCursorW(nullptr, L"IDC_ARROW"),
        .hbrBackground = CreateSolidBrush(RGB(30, 50, 90)),
        // TĘ LINIJKĘ NALEŻY DODAĆ (Podpięcie Menu):
        .lpszMenuName = MAKEINTRESOURCEW(ID_MAINMENU), 
        .lpszClassName = s_class_name.c_str() 
    };
    return RegisterClassExW(&desc) != 0;
}
```

**Krok 3: Poprawka rozmiaru okna**
Ponieważ menu zajmuje miejsce, zmień trzeci parametr `AdjustWindowRectEx` w funkcji `create_window` z `false` na `true`.
```cpp
HWND app_ractangles::create_window(DWORD style, HWND parent) {
    RECT size{ 0, 0, 800 ,600 };
    // ZMIANA TUTAJ: Trzeci parametr na 'true' (okno posiada menu)
    AdjustWindowRectEx(&size, style, true, 0); 
    // ... reszta kodu
}
```

**Krok 4: Obsługa kliknięć**
Działa tak samo jak dla przycisków:
```cpp
case WM_COMMAND:
{
    int cmdID = LOWORD(wparam); 
    switch (cmdID)
    {
        case ID_NEWGAME: // Wyklikanie "New Game" w Menu
            // Tutaj logika resetu (np. wyczyszczenie pola)
            break;
    }
    return 0;
}
```

### SPOSÓB B: W czystym kodzie C++ (Bez plików zasobów)

Jeśli wolisz napisać wszystko w kodzie, robisz to w `WM_CREATE`.

```cpp
#define ID_MENU_NOWY 2001
#define ID_MENU_ZAKONCZ 2002

case WM_CREATE:
{
    HMENU hMenu = CreateMenu(); // Główny pasek poziomy
    HMENU hSubMenu = CreatePopupMenu(); // Lista rozwijana

    AppendMenuW(hSubMenu, MF_STRING, ID_MENU_NOWY, L"Nowy rysunek"); 
    AppendMenuW(hSubMenu, MF_SEPARATOR, 0, nullptr); // Kreska oddzielająca
    AppendMenuW(hSubMenu, MF_STRING, ID_MENU_ZAKONCZ, L"Zakończ"); 

    // Podpięcie listy pod pasek główny z nazwą "Plik"
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, L"Plik"); 

    // Ustawienie menu dla aktualnego okna
    SetMenu(window, hMenu); 
    return 0;
}
```

---

## CZĘŚĆ 3: OKNA DIALOGOWE (Dialogi)

Okno dialogowe (np. "O programie") rysuje się w edytorze zasobów (Prawy przycisk na projekt -> *Add* -> *Resource* -> *Dialog*). Dostanie ono własne ID, np. `IDD_DIALOG1`. 

W kodzie musisz napisać dla niego tzw. **Procedurę Dialogową** oraz wywołać ją funkcją **`DialogBoxW`**.

### 3.1 Procedura Okna Dialogowego
Należy ją umieścić jako osobną funkcję. Zwraca `INT_PTR` zamiast `LRESULT`.

```cpp
INT_PTR CALLBACK MojaProceduraDialogu(HWND hDlg, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG: 
        // Okno się otwiera (Odpowiednik WM_CREATE)
        return (INT_PTR)TRUE; // Wszystko gotowe do wyświetlenia

    case WM_COMMAND: 
        // Ktoś kliknął przycisk W ŚRODKU okna dialogowego
        if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL) 
        {
            // WAŻNE: Dialogi zamyka się przez EndDialog! (Drugi parametr to zwracana wartość)
            EndDialog(hDlg, LOWORD(wparam)); 
            return (INT_PTR)TRUE; 
        }
        break;
    }
    
    // Dla każdej innej wiadomości zwracamy FALSE (system sam się tym zajmie)
    return (INT_PTR)FALSE; 
}
```

### 3.2 Wyświetlanie Okna Dialogowego
Kiedy chcesz pokazać dialog (np. po kliknięciu przycisku), wywołaj:

```cpp
// Tworzy okno modalne (zablokuje główne okno, dopóki dialog nie zniknie)
DialogBoxW(
    m_instance,                    // Instancja Twojego programu
    MAKEINTRESOURCEW(IDD_DIALOG1), // ID okienka z pliku .rc
    window,                        // HWND okna głównego (rodzic)
    MojaProceduraDialogu           // Wskaźnik na funkcję obsługującą dialog
);
```
