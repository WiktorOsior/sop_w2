#pragma once
#include<Windows.h>
#include<string>
#include "board.h"

class app_2048
{
private:
	bool register_class();
	static std::wstring const s_class_name;
	static LRESULT CALLBACK window_proc_static(
		HWND window, UINT messege, WPARAM wparam, LPARAM lparam);
	LRESULT window_proc(
		HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	HWND create_window(DWORD style, HWND parent = nullptr);
	HINSTANCE m_instance;
	HWND m_main, m_popup;
	board m_board;
	HBRUSH m_field_brush;
	POINT m_screen_size;
	void on_window_move(HWND window, LPWINDOWPOS params);
public:
	app_2048(HINSTANCE instance);
	int run(int show_command);
};