#include "os_api.h"
#include "real.h"
#include "array.h"
#include "hardware.h"
#include "timer.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <WinSock2.h>
#pragma comment (lib, "ws2_32.lib")
#endif

#ifdef _WIN32

struct window_data_win32
{
	HWND hwnd;
	HBITMAP bmp;
	HDC dc;
	uint8 *bits;
	window *wnd;

	window_data_win32(HWND hwnd, HBITMAP bmp, HDC dc, uint8 *bits, window *wnd)
		: hwnd(hwnd), bmp(bmp), dc(dc), bits(bits), wnd(wnd) {}
};

template<> struct key<window_data_win32>
{
	HWND key_value;

	key(const window_data_win32 &value)
		: key_value(value.hwnd) {}

	key(HWND value)
		: key_value(value) {}

	bool operator<(const key &value) const
	{
		return key_value < value.key_value;
	}
};

array<window_data_win32> windows;
bool network_initialized = false;
WSADATA wsadata;
array<network_server *> network_servers;
UINT_PTR timer_id_win32 = 0;

void __stdcall timer_proc_win32(HWND hwnd, UINT msg, UINT_PTR id, DWORD sys_time)
{
	set_node<timer *> *node;
	timer *triggered_timer;
	timestamp time = now();
	timer_trigger_postaction postaction;
	while(true)
	{
		node = timers()->begin();
		if(node == nullptr || node->value->trigger_time > time) break;
		triggered_timer = node->value;
		postaction = triggered_timer->callback(triggered_timer->data);
		timers()->remove(key<timer *>(triggered_timer));
		if(postaction == timer_trigger_postaction::repeat)
		{
			triggered_timer->trigger_time += triggered_timer->period;
			timers()->insert(triggered_timer);
		}
		else if(postaction == timer_trigger_postaction::reactivate)
		{
			triggered_timer->trigger_time = time + triggered_timer->period;
			timers()->insert(triggered_timer);
		}
		else triggered_timer->state = timer_state::inactive;
	}
	os_update_internal_timer();
}

LRESULT CALLBACK wnd_proc_win32(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	uint64 idx = windows.binary_search(key<window_data_win32>(hwnd));
	if(idx == windows.size)
		return DefWindowProc(hwnd, msg, wParam, lParam);
	window *wnd = windows[idx].wnd;
    switch(msg)
    {
		case WM_MOUSEMOVE:
		{
			POINT point;
			GetCursorPos(&point);
			mouse()->prev_position = mouse()->position;
			mouse()->position = vector<int32, 2>(
				int32(point.x), int32(GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1) - int32(point.y));
			mouse()->mouse_move.trigger();
			wnd->fm.mouse_move.trigger(indefinite<frame>(wnd));
			wnd->update();
			break;
		}
		case WM_LBUTTONDOWN:
		{
			nanoseconds time = now();
			if(mouse()->double_click)
				mouse()->double_click = false;
			else if(time.value < mouse()->last_left_clicked_time.value + mouse()->double_click_time.value)
				mouse()->double_click = true;
			mouse()->last_left_clicked_time = time;
			mouse()->last_clicked = mouse_button::left;
			mouse()->left_pressed = true;
			mouse()->mouse_click.trigger();
			wnd->fm.mouse_click.trigger(indefinite<frame>(wnd));
			wnd->update();
			break;
		}
		case WM_LBUTTONUP:
		{
			mouse()->last_released = mouse_button::left;
			mouse()->left_pressed = false;
			mouse()->mouse_release.trigger();
			wnd->fm.mouse_release.trigger(indefinite<frame>(wnd));
			wnd->update();
			break;
		}
		case WM_RBUTTONDOWN:
		{
			mouse()->last_clicked = mouse_button::right;
			mouse()->right_pressed = true;
			mouse()->mouse_click.trigger();
			wnd->fm.mouse_click.trigger(indefinite<frame>(wnd));
			wnd->update();
			break;
		}
		case WM_RBUTTONUP:
		{
			mouse()->last_released = mouse_button::right;
			mouse()->right_pressed = false;
			mouse()->mouse_release.trigger();
			wnd->fm.mouse_release.trigger(indefinite<frame>(wnd));
			wnd->update();
			break;
		}
		case WM_KEYDOWN:
		{
			keyboard()->last_pressed = key_code(wParam);
			if(!keyboard()->key_pressed[uint8(keyboard()->last_pressed)])
			{
				keyboard()->key_pressed[uint8(keyboard()->last_pressed)] = true;
				keyboard()->pressed_count++;
			}
			keyboard()->key_press.trigger();
			wnd->fm.key_press.trigger(indefinite<frame>(wnd));
			wnd->update();
			break;
		}
		case WM_KEYUP:
		{
			keyboard()->last_released = key_code(wParam);
			if(keyboard()->key_pressed[uint8(keyboard()->last_released)])
			{
				keyboard()->key_pressed[uint8(keyboard()->last_released)] = false;
				keyboard()->pressed_count--;
			}
			keyboard()->key_release.trigger();
			wnd->fm.key_release.trigger(indefinite<frame>(wnd));
			wnd->update();
			break;
		}
		case WM_CHAR:
		{ 
			keyboard()->char_code = char32(wParam);
			keyboard()->char_input.trigger();
			wnd->fm.char_input.trigger(indefinite<frame>(wnd));
			wnd->update();
			break;
		}
		case WM_MOUSEWHEEL:
		{
			mouse()->wheel_forward = GET_WHEEL_DELTA_WPARAM(wParam) > 0;
			mouse()->mouse_wheel_rotate.trigger();
			wnd->fm.mouse_wheel_rotate.trigger(indefinite<frame>(wnd));
			wnd->update();
			break;
		}
		case WM_MOVE:
		{
			wnd->update();
			break;
		}
		case WM_PAINT:
		{
			wnd->update();
			break;
		}
		case WM_SIZE:
		{
			wnd->update();
			break;
		}
        case WM_CLOSE:
		{
            DestroyWindow(hwnd);
			break;
		}
        case WM_DESTROY:
		{
            PostQuitMessage(0);
			break;
		}
        default:
		{
            return DefWindowProc(hwnd, msg, wParam, lParam);
		}
    }
    return 0;
}

#endif

void os_create_window(window *wnd)
{
#ifdef _WIN32
	WNDCLASSW wc = {};
	wc.lpfnWndProc = wnd_proc_win32;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpszClassName = L"win32 window";
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClassW(&wc);
	HWND hwnd = CreateWindowExW(
		0,
		wc.lpszClassName,
		L"Window",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		(LONG)(wnd->fm.width_desc.value),
		(LONG)(wnd->fm.height_desc.value),
		nullptr,  
		nullptr,
		GetModuleHandleW(nullptr),
		nullptr);
	RECT rect;
	GetClientRect(hwnd, &rect);
	BITMAPINFO bitmap_info;
	bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info.bmiHeader.biWidth = rect.right - rect.left;
	bitmap_info.bmiHeader.biHeight = rect.bottom - rect.top;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biCompression = BI_RGB;
	bitmap_info.bmiHeader.biSizeImage
		= bitmap_info.bmiHeader.biWidth * bitmap_info.bmiHeader.biHeight * 4;
	bitmap_info.bmiHeader.biClrUsed = 0;
	bitmap_info.bmiHeader.biClrImportant = 0;
	HDC hdc = CreateCompatibleDC(GetDC(hwnd));
	uint8 *bits;
	HBITMAP hbitmap = CreateDIBSection(hdc, &bitmap_info, DIB_RGB_COLORS, (void **)(&bits), NULL, NULL);
	wnd->handler = (void *)(hwnd);
	windows.binary_insert(window_data_win32(hwnd, hbitmap, hdc, bits, wnd));
#endif
}

void os_destroy_window(window *wnd)
{
#ifdef _WIN32
	window_data_win32 *data = &windows[windows.binary_search(key<window_data_win32>(HWND(wnd->handler)))];
	DeleteObject(data->bmp);
	DeleteDC(data->dc);
	DestroyWindow(HWND(wnd->handler));
	windows.binary_remove(key<window_data_win32>(HWND(wnd->handler)));
#endif
}

void os_open_window(window *wnd)
{
#ifdef _WIN32
	ShowWindow(HWND(wnd->handler), SW_SHOW);
	SetForegroundWindow(HWND(wnd->handler));
	SetFocus(HWND(wnd->handler));
#endif
}

void os_resize_window(window *wnd, uint32 width, uint32 height)
{
#ifdef _WIN32
	SetWindowPos(
		HWND(wnd->handler),
		nullptr,
		0,
		0,
		width,
		height,
		SWP_NOMOVE | SWP_NOREDRAW | SWP_NOZORDER);
#endif
}

void os_update_window_size(window *wnd)
{
#ifdef _WIN32
	RECT rect;
	GetClientRect(HWND(wnd->handler), &rect);
	window_data_win32 *data = &windows[windows.binary_search(key<window_data_win32>(HWND(wnd->handler)))];
	DeleteObject(data->bmp);
	BITMAPINFO bitmap_info;
	bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info.bmiHeader.biWidth = rect.right - rect.left;
	bitmap_info.bmiHeader.biHeight = rect.bottom - rect.top;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biCompression = BI_RGB;
	bitmap_info.bmiHeader.biSizeImage
		= bitmap_info.bmiHeader.biWidth * bitmap_info.bmiHeader.biHeight * 4;
	bitmap_info.bmiHeader.biClrUsed = 0;
	bitmap_info.bmiHeader.biClrImportant = 0;
	data->bmp = CreateDIBSection(data->dc, &bitmap_info, DIB_RGB_COLORS, (void **)(&data->bits), NULL, NULL);
#endif
}

vector<int32, 2> os_window_content_position(window *wnd)
{
	RECT rect;
	GetClientRect((HWND)(wnd->handler), &rect);
	POINT point;
	point.x = rect.left;
	point.y = rect.bottom;
	ClientToScreen((HWND)(wnd->handler), &point);
	return vector<int32, 2>(int32(point.x), int32(GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1 - point.y));
}

vector<uint32, 2> os_window_content_size(window *wnd)
{
#ifdef _WIN32
	RECT rect;
	GetClientRect(HWND(wnd->handler), &rect);
	return vector<uint32, 2>(
		uint32(rect.right - rect.left),
		uint32(rect.bottom - rect.top));
#endif
}

vector<uint32, 2> os_screen_size()
{
	return vector<uint32, 2>(
		uint32(GetSystemMetrics(SM_CXVIRTUALSCREEN)),
		uint32(GetSystemMetrics(SM_CYVIRTUALSCREEN)));
}

void os_message_loop()
{
#ifdef _WIN32
	if(!network_initialized)
	{ 
		WSAStartup(MAKEWORD(2, 0), &wsadata);
		network_initialized = true;
	}
	SOCKET client;
	byte *input, *output;
	uint64 size = 5000, input_size, output_size;
	input = new byte[size];
	MSG msg = {};
	while(true)
	{
		while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT) return;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		/*while(true)
		{
			client = accept(SOCKET(network_servers.addr[0]->handler), NULL, NULL);
			if(client == INVALID_SOCKET) break;
			input_size = (recv(client, (char *)(input), int(size), 0));
			if(input_size != SOCKET_ERROR)
			{
				if(network_servers.addr[0]->process_request(input, input_size, &output, &output_size))
				{
					send(client, (char *)(output), int(output_size), 0);
				}
			}
			closesocket(client);
		}*/

		//Sleep(5);
	}
#endif
}

void os_window_render_buffer(window *wnd, void **bits)
{
#ifdef _WIN32
	uint64 idx = windows.binary_search(key<window_data_win32>((HWND)wnd->handler));
	window_data_win32 *wnd_data = &windows[idx];
	*bits = wnd_data->bits;
#endif
}

void os_render_window(window *wnd)
{
#ifdef _WIN32
	uint64 idx = windows.binary_search(key<window_data_win32>((HWND)wnd->handler));
	window_data_win32 *wnd_data = &windows[idx];
	uint8 *bits = wnd_data->bits;
	vector<int32, 2> position = os_window_content_position(wnd);
	vector<uint32, 2> size = os_window_content_size(wnd),
		screen_size = os_screen_size();
	int32 bmp_idx;
	for(int32 ys = position.y; ys < position.y + int32(size.y); ys++)
		for(int32 xs = position.x; xs < position.x + int32(size.x); xs++)
		{
			bmp_idx = (wnd->bmp.height - 1 - ys) * int32(wnd->bmp.width) + xs;
			if(bmp_idx < 0 || bmp_idx >= int32(screen_size.x * screen_size.y))
			{
				bits += 4;
				continue;
			}
			*bits = wnd->bmp.data[bmp_idx].b;
			bits++;
			*bits = wnd->bmp.data[bmp_idx].g;
			bits++;
			*bits = wnd->bmp.data[bmp_idx].r;
			bits += 2;
		}
	SelectObject(wnd_data->dc, wnd_data->bmp);
	BitBlt(GetDC(wnd_data->hwnd), 0, 0,
		(int)wnd->fm.width, (int)wnd->fm.height,
		wnd_data->dc, 0, 0, SRCCOPY);
#endif
}

bool os_load_glyph(glyph_data *data)
{
#ifdef _WIN32
	char16 *str_win32 = create_u16sz(data->font_name);
	HFONT hfont = CreateFont(
		data->size,
		0,
		0,
		0,
		data->weight,
		(data->italic ? TRUE : FALSE),
		FALSE,
		FALSE,
		DEFAULT_CHARSET,
		OUT_OUTLINE_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		DEFAULT_PITCH,
		(wchar_t *)(str_win32));
	delete[] str_win32;
	if(hfont == nullptr) return false;
	HDC hdc = CreateCompatibleDC(nullptr);
	SelectObject(hdc, hfont);
	TEXTMETRIC tm;
	GetTextMetrics(hdc, &tm);
	LPOUTLINETEXTMETRICW otm;
	uint32 bufferSize = GetOutlineTextMetrics(hdc, 0, nullptr);
	otm = LPOUTLINETEXTMETRIC(new uint8[bufferSize]);
	GetOutlineTextMetrics(hdc, bufferSize, otm);
	data->ascent = uint32(tm.tmAscent);
	data->descent = uint32(tm.tmDescent);
	data->internal_leading = uint32(tm.tmInternalLeading);
	data->underline_offset = otm->otmsUnderscorePosition;
	data->underline_size = uint32(otm->otmsUnderscoreSize);
	data->strikethrough_offset = otm->otmsStrikeoutPosition;
	data->strikethrough_size = uint32(otm->otmsStrikeoutSize);
	delete[] otm;
	if(data->code == U'\t')
	{
		data->advance = vector<int32, 2>(2 * int32(data->size), 0);
		DeleteDC(hdc);
		return true;
	}
	if(data->code == U'\n')
	{
		data->advance = vector<int32, 2>(0, 0);
		DeleteDC(hdc);
		return true;
	}
	GLYPHMETRICS glyph_metrics;
	auto fixed_to_float32 = [](FIXED value) -> float32
	{
		return float32(value.value) + float32(value.fract) / 65535.0f;
	};
	SelectObject(hdc, hfont);
	MAT2 transform = { 0, 1, 0, 0, 0, 0, 0, 1 };
	DWORD size = GetGlyphOutline(
		hdc,
		data->code,
		GGO_NATIVE,
		&glyph_metrics,
		0,
		nullptr,
		&transform);
	array<uint8> outline;
	outline.insert_default(0, size);
	if(GetGlyphOutline(
		hdc,
		data->code,
		GGO_NATIVE,
		&glyph_metrics,
		size,
		outline.addr,
		&transform) == GDI_ERROR)
	{
		DeleteDC(hdc);
		return false;
	}
	TTPOLYGONHEADER *polygon = (TTPOLYGONHEADER *)outline.addr;
	TTPOLYCURVE *curve;
	uint8 *contour_end;
	vector<float32, 2> last_move;
	while((uint8 *)polygon < outline.addr + outline.size)
	{
		contour_end = (uint8 *)(polygon) + polygon->cb;
		data->path.move(vector<float32, 2>(
			fixed_to_float32(polygon->pfxStart.x),
			fixed_to_float32(polygon->pfxStart.y)));
		last_move = data->path.data[data->path.data.size - 1].p1;
		polygon++;
		curve = (TTPOLYCURVE *)(polygon);
		while((uint8 *)(curve) < contour_end)
		{
			if(curve->wType == TT_PRIM_LINE)
			{
				for(uint32 iter = 0; iter < curve->cpfx; iter++)
					data->path.push_line(vector<float32, 2>(
						fixed_to_float32(curve->apfx[iter].x),
						fixed_to_float32(curve->apfx[iter].y)));
			}
			else
			{
				for(uint32 iter = 0; int32(iter) < curve->cpfx - 2; iter++)
				{
					data->path.push_quadratic_arc(
						vector<float32, 2>(
							fixed_to_float32(curve->apfx[iter].x),
							fixed_to_float32(curve->apfx[iter].y)),
						vector<float32, 2>(
							0.5f * (fixed_to_float32(curve->apfx[iter].x) + fixed_to_float32(curve->apfx[iter + 1].x)),
							0.5f * (fixed_to_float32(curve->apfx[iter].y) + fixed_to_float32(curve->apfx[iter + 1].y))));
				}
				data->path.push_quadratic_arc(
					vector<float32, 2>(
						fixed_to_float32(curve->apfx[curve->cpfx - 2].x),
						fixed_to_float32(curve->apfx[curve->cpfx - 2].y)),
					vector<float32, 2>(
						fixed_to_float32(curve->apfx[curve->cpfx - 1].x),
						fixed_to_float32(curve->apfx[curve->cpfx - 1].y)));
			}
			curve = (TTPOLYCURVE *)(&curve->apfx[curve->cpfx]);
		}
		data->path.push_line(last_move);
		polygon = (TTPOLYGONHEADER *)(curve);
	}
	data->path.orientation = face_orientation::clockwise;
	if(data->path.data.size != 0)
	{
		float32 lx = 1000000.0f, hx = -1000000.0f, ly = 1000000.0f, hy = -1000000.0f;
		for(uint64 i = 0; i < data->path.data.size; i++)
		{
			lx = min(lx, data->path.data[i].p1.x);
			hx = max(hx, data->path.data[i].p1.x);
			ly = min(ly, data->path.data[i].p1.y);
			hy = max(hy, data->path.data[i].p1.y);
			if(data->path.data[i].type == geometry_path_unit::quadratic_arc)
			{
				lx = min(lx, data->path.data[i].p2.x);
				hx = max(hx, data->path.data[i].p2.x);
				ly = min(ly, data->path.data[i].p2.y);
				hy = max(hy, data->path.data[i].p2.y);
			}
		}
		lx = floor(lx);
		hx = ceil(hx);
		ly = floor(ly);
		hy = ceil(hy);
		data->bmp.resize(uint32(hx - lx), uint32(hy - ly));
		for(uint32 i = 0; i < data->bmp.width * data->bmp.height; i++)
			data->bmp.data[i] = alpha_color(0, 0, 0, 0);
		bitmap_processor bp;
		bp.transform = translating_matrix(-lx, -ly);
		bp.render(data->path, &data->bmp);
		data->bmp_offset = vector<float32, 2>(lx, ly);
	}
	data->advance.x = int32(glyph_metrics.gmCellIncX);
	data->advance.y = int32(glyph_metrics.gmCellIncY);
	DeleteDC(hdc);
	return true;
#endif
}

timestamp os_current_timestamp()
{
#ifdef _WIN32
	int64 freq, counter;
	QueryPerformanceFrequency((LARGE_INTEGER *)(&freq));
	QueryPerformanceCounter((LARGE_INTEGER *)(&counter));
	return timestamp((counter / freq) * 1000000000 + (counter % freq) * 1000000000 / freq);
#endif
}

void os_copy_text_to_clipboard(u16string &text)
{
#ifdef _WIN32
	if(OpenClipboard(nullptr) == 0) return;
	HGLOBAL hglbCopy = GlobalAlloc(
		GMEM_MOVEABLE,
		(text.size + 1) * sizeof(char16));
	char16 *str_win32 = (char16 *)(GlobalLock(hglbCopy));
	for(uint64 i = 0; i < text.size; i++)
		str_win32[i] = char16(text.addr[i]);
	str_win32[text.size] = u'\0';
	GlobalUnlock(hglbCopy);
	SetClipboardData(CF_UNICODETEXT, hglbCopy);
	CloseClipboard();
#endif
}

void os_copy_text_from_clipboard(u16string *text)
{
#ifdef _WIN32
	if(IsClipboardFormatAvailable(CF_UNICODETEXT) == FALSE
		|| OpenClipboard(nullptr) == 0) return;
	HGLOBAL hglb = GetClipboardData(CF_UNICODETEXT);
	if(hglb != nullptr)
	{
		char16 *buffer = (char16 *)(GlobalLock(hglb));
		if(buffer != nullptr)
			*text << buffer;
		GlobalUnlock(hglb);
	}
	CloseClipboard();
#endif
}

void os_update_internal_timer()
{
#ifdef _WIN32
	if(timers()->size == 0) return;
	milliseconds ms = 0;
	ms << nanoseconds(timers()->begin()->value->trigger_time - now());
	if(ms.value < 0) ms.value = 0;
	timer_id_win32 = SetTimer(NULL, timer_id_win32, (UINT)ms.value, timer_proc_win32);
#endif
}

void os_update_windows()
{
	for(uint64 i = 0; i < windows.size; i++)
		SendMessage(HWND(windows[i].hwnd), WM_PAINT, 0, 0);
}

bool os_filename_exists(u16string &filename)
{
#ifdef _WIN32
	char16 *str_win32 = create_u16sz(filename);
	BOOL exists = PathFileExistsW((wchar_t *)(str_win32));
	delete[] str_win32;
	if(exists == TRUE) return true;
	else return false;
#endif
}

void os_open_file(file *f)
{
#ifdef _WIN32
	char16 *str_win32 = create_u16sz(f->filename);
	DWORD access = 0;
	if(f->read_access) access |= GENERIC_READ;
	if(f->write_access) access |= GENERIC_WRITE;
	HANDLE handler = CreateFileW(
		(wchar_t *)(str_win32),
		access,
		0,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	delete[] str_win32;
	if(handler != INVALID_HANDLE_VALUE)
	{
		f->status = filestatus::opened;
		f->position = 0;
		GetFileSizeEx(handler, (LARGE_INTEGER *)(&f->size));
		f->handler = handler;
	}
#endif
}

void os_close_file(file *f)
{
#ifdef _WIN32
	CloseHandle(f->handler);
	f->status = filestatus::closed;
#endif
}

void os_resize_file(uint64 size, file *f)
{
#ifdef _WIN32
	LARGE_INTEGER li;
	li.QuadPart = LONGLONG(size);
	SetFilePointerEx(HANDLE(f->handler), li, NULL, FILE_BEGIN);
	SetEndOfFile(HANDLE(f->handler));
	GetFileSizeEx(HANDLE(f->handler), (LARGE_INTEGER *)(&f->size));
	if(f->position > f->size) f->position = f->size;
#endif
}

uint64 os_read_file(file *f, uint64 size, void *addr)
{
#ifdef _WIN32
	DWORD bytes_read;
	LARGE_INTEGER li;
	li.QuadPart = LONGLONG(f->position);
	SetFilePointerEx(HANDLE(f->handler), li, NULL, FILE_BEGIN);
	ReadFile(
		HANDLE(f->handler),
		addr,
		DWORD(size),
		&bytes_read,
		NULL);
	f->position += uint64(bytes_read);
	return uint64(bytes_read);
#endif
}

uint64 os_write_file(file *f, void *addr, uint64 size)
{
#ifdef _WIN32
	DWORD bytes_written;
	LARGE_INTEGER li;
	li.QuadPart = LONGLONG(f->position);
	SetFilePointerEx(HANDLE(f->handler), li, NULL, FILE_BEGIN);
	WriteFile(
		HANDLE(f->handler),
		addr,
		DWORD(size),
		&bytes_written,
		NULL);
	f->position += uint64(bytes_written);
	GetFileSizeEx(HANDLE(f->handler), (LARGE_INTEGER *)(&f->size));
	return uint64(bytes_written);
#endif
}

bool os_delete_file(u16string &filename)
{
#ifdef _WIN32
	char16 *str_win32 = create_u16sz(filename);
	BOOL deleted = DeleteFileW((wchar_t *)(str_win32));
	delete[] str_win32;
	if(deleted != 0) return true;
	else return false;
#endif
}

void os_regiser_web_server(network_server *ws)
{
#ifdef _WIN32
	if(!network_initialized)
	{ 
		WSAStartup(MAKEWORD(2, 0), &wsadata);
		network_initialized = true;
	}
	ws->handler = (void *)(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	sockaddr_in socket_addr;
	socket_addr.sin_family = AF_INET;
	socket_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	socket_addr.sin_port = htons(ws->port);
	u_long blocking_mode = 1;
	ioctlsocket(SOCKET(ws->handler), FIONBIO, &blocking_mode);
	bind(SOCKET(ws->handler), (sockaddr *)(&socket_addr), sizeof(socket_addr));
	listen(SOCKET(ws->handler), 1);
	ws->state = network_server_state::running;
	network_servers.push(ws);
#endif
}

void os_unregister_web_server(network_server *ws)
{
#ifdef _WIN32
	ws->state = network_server_state::inactive;
	for(uint64 i = 0; i < network_servers.size; i++)
		if(network_servers[i] == ws)
		{
			network_servers.remove(i);
			return;
		}
#endif
}


