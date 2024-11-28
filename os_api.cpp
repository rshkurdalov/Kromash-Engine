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
#pragma comment (lib, "ws2_32.lib")
#endif
#include <wincodec.h>
#include <wincodecsdk.h>
#pragma comment(lib, "WindowsCodecs.lib")
#include <cstddef>
#include <cstdint>
#ifdef _MSC_VER
#pragma comment(lib, "uuid.lib")
#endif
#include <algorithm>
#include <assert.h>
#include <tuple>
#include <memory>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#ifdef _WIN32

struct window_data_win32
{
	HWND hwnd;
	HBITMAP bmp;
	HDC dc;
	uint8 *bits;
	window *wnd;

	window_data_win32() {}

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
IWICImagingFactory *wic_factory = nullptr;
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
		postaction = triggered_timer->callback();
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
			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);
			wnd->update();
			EndPaint(hwnd, &ps);
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
	MSG msg;
	while(true)
	{
		while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT) return;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		for(uint64 i = 0; i < windows.size; i++)
			windows[i].wnd->update();

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
		graphics_displayer gd;
		gd.push_transform(translating_matrix(-lx, -ly));
		gd.render(data->path, &data->bmp);
		gd.pop_transform();
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

bool os_delete_file(file *f, u16string &filename)
{
#ifdef _WIN32
	char16 *str_win32 = create_u16sz(filename);
	if(f->status == filestatus::opened) f->close();
	BOOL deleted = DeleteFileW((wchar_t *)(str_win32));
	delete[] str_win32;
	if(deleted != 0) return true;
	else return false;
#endif
}

bool os_move_file(file *f, u16string &new_path)
{
#ifdef _WIN32
	bool reopen;
	if(f->status == filestatus::opened)
	{
		reopen = true;
		f->close();
	}
	else reopen = false;
	wchar *str1_win32 = create_wsz(f->filename),
		*str2_win32 = create_wsz(new_path);
	BOOL result = MoveFile(str1_win32, str2_win32);
	delete[] str1_win32;
	delete[] str2_win32;
	if(result != 0)
	{
		f->filename <<= new_path;
		if(reopen) f->open();
		return true;
	}
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

bool os_load_image(u16string &filename, bitmap *bmp)
{
	if(wic_factory == nullptr)
	{
		CoInitialize(nullptr);
		CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID *)(&wic_factory));
	}
	HRESULT hr;
	IWICBitmapDecoder *decoder = nullptr;
	wstring wfilename;
	wfilename << create_wsz(filename) << L'\0';
	hr = wic_factory->CreateDecoderFromFilename(wfilename.addr, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
	if(hr != S_OK) return false;
	IWICBitmapFrameDecode *frame_decoder = nullptr;
	decoder->GetFrame(0, &frame_decoder);
	frame_decoder->GetSize(&bmp->width, &bmp->height);
	bmp->resize(bmp->width, bmp->height);
	IWICFormatConverter *format_converter = nullptr;
    wic_factory->CreateFormatConverter(&format_converter);
    format_converter->Initialize(frame_decoder, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
    hr = format_converter->CopyPixels(nullptr, bmp->width * 4, bmp->width * bmp->height * 4, (BYTE *)(bmp->data));
	return true;
}

enum WIC_LOADER_FLAGS : uint32
{
    WIC_LOADER_DEFAULT = 0,
    WIC_LOADER_FORCE_SRGB = 0x1,
    WIC_LOADER_IGNORE_SRGB = 0x2,
    WIC_LOADER_SRGB_DEFAULT = 0x4,
    WIC_LOADER_FIT_POW2 = 0x20,
    WIC_LOADER_MAKE_SQUARE = 0x40,
    WIC_LOADER_FORCE_RGBA32 = 0x80,
};

inline DXGI_FORMAT MakeSRGB(_In_ DXGI_FORMAT format) noexcept
{
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    case DXGI_FORMAT_BC1_UNORM:
        return DXGI_FORMAT_BC1_UNORM_SRGB;

    case DXGI_FORMAT_BC2_UNORM:
        return DXGI_FORMAT_BC2_UNORM_SRGB;

    case DXGI_FORMAT_BC3_UNORM:
        return DXGI_FORMAT_BC3_UNORM_SRGB;

    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

    case DXGI_FORMAT_B8G8R8X8_UNORM:
        return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

    case DXGI_FORMAT_BC7_UNORM:
        return DXGI_FORMAT_BC7_UNORM_SRGB;

    default:
        return format;
    }
}

void FitPowerOf2(UINT origx, UINT origy, _Inout_ UINT& targetx, _Inout_ UINT& targety, size_t maxsize)
{
    const float origAR = float(origx) / float(origy);

    if (origx > origy)
    {
        size_t x;
        for (x = maxsize; x > 1; x >>= 1) { if (x <= targetx) break; }
        targetx = UINT(x);

        float bestScore = FLT_MAX;
        for (size_t y = maxsize; y > 0; y >>= 1)
        {
            const float score = fabsf((float(x) / float(y)) - origAR);
            if (score < bestScore)
            {
                bestScore = score;
                targety = UINT(y);
            }
        }
    }
    else
    {
        size_t y;
        for (y = maxsize; y > 1; y >>= 1) { if (y <= targety) break; }
        targety = UINT(y);

        float bestScore = FLT_MAX;
        for (size_t x = maxsize; x > 0; x >>= 1)
        {
            const float score = fabsf((float(x) / float(y)) - origAR);
            if (score < bestScore)
            {
                bestScore = score;
                targetx = UINT(x);
            }
        }
    }
}

namespace
{
    struct WICTranslate
    {
        const GUID&         wic;
        DXGI_FORMAT         format;

        constexpr WICTranslate(const GUID& wg, DXGI_FORMAT fmt) noexcept :
            wic(wg),
            format(fmt) {}
    };

    constexpr WICTranslate g_WICFormats[] =
    {
        { GUID_WICPixelFormat128bppRGBAFloat,       DXGI_FORMAT_R32G32B32A32_FLOAT },

        { GUID_WICPixelFormat64bppRGBAHalf,         DXGI_FORMAT_R16G16B16A16_FLOAT },
        { GUID_WICPixelFormat64bppRGBA,             DXGI_FORMAT_R16G16B16A16_UNORM },

        { GUID_WICPixelFormat32bppRGBA,             DXGI_FORMAT_R8G8B8A8_UNORM },
        { GUID_WICPixelFormat32bppBGRA,             DXGI_FORMAT_B8G8R8A8_UNORM }, // DXGI 1.1
        { GUID_WICPixelFormat32bppBGR,              DXGI_FORMAT_B8G8R8X8_UNORM }, // DXGI 1.1

        { GUID_WICPixelFormat32bppRGBA1010102XR,    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM }, // DXGI 1.1
        { GUID_WICPixelFormat32bppRGBA1010102,      DXGI_FORMAT_R10G10B10A2_UNORM },

        { GUID_WICPixelFormat16bppBGRA5551,         DXGI_FORMAT_B5G5R5A1_UNORM },
        { GUID_WICPixelFormat16bppBGR565,           DXGI_FORMAT_B5G6R5_UNORM },

        { GUID_WICPixelFormat32bppGrayFloat,        DXGI_FORMAT_R32_FLOAT },
        { GUID_WICPixelFormat16bppGrayHalf,         DXGI_FORMAT_R16_FLOAT },
        { GUID_WICPixelFormat16bppGray,             DXGI_FORMAT_R16_UNORM },
        { GUID_WICPixelFormat8bppGray,              DXGI_FORMAT_R8_UNORM },

        { GUID_WICPixelFormat8bppAlpha,             DXGI_FORMAT_A8_UNORM },
    };

    //-------------------------------------------------------------------------------------
    // WIC Pixel Format nearest conversion table
    //-------------------------------------------------------------------------------------
    struct WICConvert
    {
        const GUID& source;
        const GUID& target;

        constexpr WICConvert(const GUID& src, const GUID& tgt) noexcept :
            source(src),
            target(tgt) {}
    };

    constexpr WICConvert g_WICConvert[] =
    {
        // Note target GUID in this conversion table must be one of those directly supported formats (above).

        { GUID_WICPixelFormatBlackWhite,            GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

        { GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

        { GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM
        { GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

        { GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf }, // DXGI_FORMAT_R16_FLOAT
        { GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat }, // DXGI_FORMAT_R32_FLOAT

        { GUID_WICPixelFormat16bppBGR555,           GUID_WICPixelFormat16bppBGRA5551 }, // DXGI_FORMAT_B5G5R5A1_UNORM

        { GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102 }, // DXGI_FORMAT_R10G10B10A2_UNORM

        { GUID_WICPixelFormat24bppBGR,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

        { GUID_WICPixelFormat48bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat48bppBGR,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppBGRA,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPRGBA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPBGRA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

        { GUID_WICPixelFormat48bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat48bppBGRFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppBGRAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat48bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT

        { GUID_WICPixelFormat128bppPRGBAFloat,      GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBFloat,        GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBAFixedPoint,  GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBFixedPoint,   GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat32bppRGBE,             GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT

        { GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        { GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat64bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPRGBAHalf,        GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
    #endif

        // We don't support n-channel formats
    };

    bool g_WIC2 = false;

    BOOL WINAPI InitializeWICFactory(PINIT_ONCE, PVOID, PVOID *ifactory) noexcept
    {
    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory2,
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory2),
            ifactory
        );

        if (SUCCEEDED(hr))
        {
            // WIC2 is available on Windows 10, Windows 8.x, and Windows 7 SP1 with KB 2670838 installed
            g_WIC2 = true;
            return TRUE;
        }
        else
        {
            hr = CoCreateInstance(
                CLSID_WICImagingFactory1,
                nullptr,
                CLSCTX_INPROC_SERVER,
                __uuidof(IWICImagingFactory),
                ifactory
            );
            return SUCCEEDED(hr) ? TRUE : FALSE;
        }
    #else
        return SUCCEEDED(CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory),
            ifactory)) ? TRUE : FALSE;
    #endif
    }
}

IWICImagingFactory *GetWIC()
{
    if(wic_factory == nullptr)
    {
        CoInitialize(nullptr);
        CoCreateInstance(
            CLSID_WICImagingFactory,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&wic_factory));
    }
    return wic_factory;
}

namespace
{
    //---------------------------------------------------------------------------------
    DXGI_FORMAT WICToDXGI(const GUID& guid) noexcept
    {
        for (size_t i = 0; i < std::size(g_WICFormats); ++i)
        {
            if (memcmp(&g_WICFormats[i].wic, &guid, sizeof(GUID)) == 0)
                return g_WICFormats[i].format;
        }

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
        if (g_WIC2)
        {
            if (memcmp(&GUID_WICPixelFormat96bppRGBFloat, &guid, sizeof(GUID)) == 0)
                return DXGI_FORMAT_R32G32B32_FLOAT;
        }
    #endif

        return DXGI_FORMAT_UNKNOWN;
    }

    //---------------------------------------------------------------------------------
    size_t WICBitsPerPixel(REFGUID targetGuid) noexcept
    {
        auto pWIC = GetWIC();
        if (!pWIC)
            return 0;

        ComPtr<IWICComponentInfo> cinfo;
        if (FAILED(pWIC->CreateComponentInfo(targetGuid, cinfo.GetAddressOf())))
            return 0;

        WICComponentType type;
        if (FAILED(cinfo->GetComponentType(&type)))
            return 0;

        if (type != WICPixelFormat)
            return 0;

        ComPtr<IWICPixelFormatInfo> pfinfo;
        if (FAILED(cinfo.As(&pfinfo)))
            return 0;

        UINT bpp;
        if (FAILED(pfinfo->GetBitsPerPixel(&bpp)))
            return 0;

        return bpp;
    }
}

 HRESULT CreateTextureFromWIC(
    _In_ ID3D11Device* d3dDevice,
    _In_opt_ ID3D11DeviceContext* d3dContext,
#if defined(_XBOX_ONE) && defined(_TITLE)
    _In_opt_ ID3D11DeviceX* d3dDeviceX,
    _In_opt_ ID3D11DeviceContextX* d3dContextX,
#endif
    _In_ IWICBitmapFrameDecode *frame,
    _In_ size_t maxsize,
    _In_ D3D11_USAGE usage,
    _In_ unsigned int bindFlags,
    _In_ unsigned int cpuAccessFlags,
    _In_ unsigned int miscFlags,
    _In_ WIC_LOADER_FLAGS loadFlags,
    _Outptr_opt_ ID3D11Resource** texture,
    _Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept
{
    UINT width, height;
    HRESULT hr = frame->GetSize(&width, &height);
    if (FAILED(hr))
        return hr;

    if (maxsize > UINT32_MAX)
        return E_INVALIDARG;

    assert(width > 0 && height > 0);

    if (!maxsize)
    {
        // This is a bit conservative because the hardware could support larger textures than
        // the Feature Level defined minimums, but doing it this way is much easier and more
        // performant for WIC than the 'fail and retry' model used by DDSTextureLoader

        switch (d3dDevice->GetFeatureLevel())
        {
        case D3D_FEATURE_LEVEL_9_1:
        case D3D_FEATURE_LEVEL_9_2:
            maxsize = 2048u /*D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
            break;

        case D3D_FEATURE_LEVEL_9_3:
            maxsize = 4096u /*D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
            break;

        case D3D_FEATURE_LEVEL_10_0:
        case D3D_FEATURE_LEVEL_10_1:
            maxsize = 8192u /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
            break;

        default:
            maxsize = size_t(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION);
            break;
        }
    }

    assert(maxsize > 0);

    UINT twidth = width;
    UINT theight = height;
    if (loadFlags & WIC_LOADER_FIT_POW2)
    {
        FitPowerOf2(width, height, twidth, theight, maxsize);
    }
    else if (width > maxsize || height > maxsize)
    {
        const float ar = static_cast<float>(height) / static_cast<float>(width);
        if (width > height)
        {
            twidth = static_cast<UINT>(maxsize);
            theight = std::max<UINT>(1, static_cast<UINT>(static_cast<float>(maxsize) * ar));
        }
        else
        {
            theight = static_cast<UINT>(maxsize);
            twidth = std::max<UINT>(1, static_cast<UINT>(static_cast<float>(maxsize) / ar));
        }
        assert(twidth <= maxsize && theight <= maxsize);
    }

    if (loadFlags & WIC_LOADER_MAKE_SQUARE)
    {
        twidth = std::max<UINT>(twidth, theight);
        theight = twidth;
    }

    // Determine format
    WICPixelFormatGUID pixelFormat;
    hr = frame->GetPixelFormat(&pixelFormat);
    if (FAILED(hr))
        return hr;

    WICPixelFormatGUID convertGUID;
    memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &pixelFormat, sizeof(GUID));

    size_t bpp = 0;

    DXGI_FORMAT format = WICToDXGI(pixelFormat);
    if (format == DXGI_FORMAT_UNKNOWN)
    {
        if (memcmp(&GUID_WICPixelFormat96bppRGBFixedPoint, &pixelFormat, sizeof(WICPixelFormatGUID)) == 0)
        {
        #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
            if (g_WIC2)
            {
                memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat96bppRGBFloat, sizeof(GUID));
                format = DXGI_FORMAT_R32G32B32_FLOAT;
                bpp = 96;
            }
            else
            #endif
            {
                memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat128bppRGBAFloat, sizeof(GUID));
                format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                bpp = 128;
            }
        }
        else
        {
            for (size_t i = 0; i < std::size(g_WICConvert); ++i)
            {
                if (memcmp(&g_WICConvert[i].source, &pixelFormat, sizeof(WICPixelFormatGUID)) == 0)
                {
                    memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &g_WICConvert[i].target, sizeof(GUID));

                    format = WICToDXGI(g_WICConvert[i].target);
                    assert(format != DXGI_FORMAT_UNKNOWN);
                    bpp = WICBitsPerPixel(convertGUID);
                    break;
                }
            }
        }

        if (format == DXGI_FORMAT_UNKNOWN)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }
    }
    else
    {
        bpp = WICBitsPerPixel(pixelFormat);
    }

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
    if ((format == DXGI_FORMAT_R32G32B32_FLOAT) && d3dContext && textureView)
    {
        // Special case test for optional device support for autogen mipchains for R32G32B32_FLOAT
        UINT fmtSupport = 0;
        hr = d3dDevice->CheckFormatSupport(DXGI_FORMAT_R32G32B32_FLOAT, &fmtSupport);
        if (FAILED(hr) || !(fmtSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN))
        {
            // Use R32G32B32A32_FLOAT instead which is required for Feature Level 10.0 and up
            memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat128bppRGBAFloat, sizeof(GUID));
            format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            bpp = 128;
        }
    }
#endif

    if (loadFlags & WIC_LOADER_FORCE_RGBA32)
    {
        memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat32bppRGBA, sizeof(GUID));
        format = DXGI_FORMAT_R8G8B8A8_UNORM;
        bpp = 32;
    }

    if (!bpp)
        return E_FAIL;

    // Handle sRGB formats
    if (loadFlags & WIC_LOADER_FORCE_SRGB)
    {
        format = MakeSRGB(format);
    }
    else if (!(loadFlags & WIC_LOADER_IGNORE_SRGB))
    {
        ComPtr<IWICMetadataQueryReader> metareader;
        if (SUCCEEDED(frame->GetMetadataQueryReader(metareader.GetAddressOf())))
        {
            GUID containerFormat;
            if (SUCCEEDED(metareader->GetContainerFormat(&containerFormat)))
            {
                bool sRGB = false;

                PROPVARIANT value;
                PropVariantInit(&value);

                // Check for colorspace chunks
                if (memcmp(&containerFormat, &GUID_ContainerFormatPng, sizeof(GUID)) == 0)
                {
                    // Check for sRGB chunk
                    if (SUCCEEDED(metareader->GetMetadataByName(L"/sRGB/RenderingIntent", &value)) && value.vt == VT_UI1)
                    {
                        sRGB = true;
                    }
                    else if (SUCCEEDED(metareader->GetMetadataByName(L"/gAMA/ImageGamma", &value)) && value.vt == VT_UI4)
                    {
                        sRGB = (value.uintVal == 45455);
                    }
                    else
                    {
                        sRGB = (loadFlags & WIC_LOADER_SRGB_DEFAULT) != 0;
                    }
                }
            #if defined(_XBOX_ONE) && defined(_TITLE)
                else if (memcmp(&containerFormat, &GUID_ContainerFormatJpeg, sizeof(GUID)) == 0)
                {
                    if (SUCCEEDED(metareader->GetMetadataByName(L"/app1/ifd/exif/{ushort=40961}", &value)) && value.vt == VT_UI2)
                    {
                        sRGB = (value.uiVal == 1);
                    }
                    else
                    {
                        sRGB = (loadFlags & WIC_LOADER_SRGB_DEFAULT) != 0;
                    }
                }
                else if (memcmp(&containerFormat, &GUID_ContainerFormatTiff, sizeof(GUID)) == 0)
                {
                    if (SUCCEEDED(metareader->GetMetadataByName(L"/ifd/exif/{ushort=40961}", &value)) && value.vt == VT_UI2)
                    {
                        sRGB = (value.uiVal == 1);
                    }
                    else
                    {
                        sRGB = (loadFlags & WIC_LOADER_SRGB_DEFAULT) != 0;
                    }
                }
            #else
                else if (SUCCEEDED(metareader->GetMetadataByName(L"System.Image.ColorSpace", &value)) && value.vt == VT_UI2)
                {
                    sRGB = (value.uiVal == 1);
                }
                else
                {
                    sRGB = (loadFlags & WIC_LOADER_SRGB_DEFAULT) != 0;
                }
            #endif

                std::ignore = PropVariantClear(&value);

                if (sRGB)
                    format = MakeSRGB(format);
            }
        }
    }

    // Verify our target format is supported by the current device
    // (handles WDDM 1.0 or WDDM 1.1 device driver cases as well as DirectX 11.0 Runtime without 16bpp format support)
    UINT support = 0;
    hr = d3dDevice->CheckFormatSupport(format, &support);
    if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_TEXTURE2D))
    {
        // Fallback to RGBA 32-bit format which is supported by all devices
        memcpy_s(&convertGUID, sizeof(WICPixelFormatGUID), &GUID_WICPixelFormat32bppRGBA, sizeof(GUID));
        format = DXGI_FORMAT_R8G8B8A8_UNORM;
        bpp = 32;
    }

    // Allocate temporary memory for image
    const uint64_t rowBytes = (uint64_t(twidth) * uint64_t(bpp) + 7u) / 8u;
    const uint64_t numBytes = rowBytes * uint64_t(theight);

    if (rowBytes > UINT32_MAX || numBytes > UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

    auto const rowPitch = static_cast<size_t>(rowBytes);
    auto const imageSize = static_cast<size_t>(numBytes);

    std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[imageSize]);
    if (!temp)
        return E_OUTOFMEMORY;

    // Load image data
    if (memcmp(&convertGUID, &pixelFormat, sizeof(GUID)) == 0
        && twidth == width
        && theight == height)
    {
        // No format conversion or resize needed
        hr = frame->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
        if (FAILED(hr))
            return hr;
    }
    else if (twidth != width || theight != height)
    {
        // Resize
        auto pWIC = GetWIC();
        if (!pWIC)
            return E_NOINTERFACE;

        ComPtr<IWICBitmapScaler> scaler;
        hr = pWIC->CreateBitmapScaler(scaler.GetAddressOf());
        if (FAILED(hr))
            return hr;

        hr = scaler->Initialize(frame, twidth, theight, WICBitmapInterpolationModeFant);
        if (FAILED(hr))
            return hr;

        WICPixelFormatGUID pfScaler;
        hr = scaler->GetPixelFormat(&pfScaler);
        if (FAILED(hr))
            return hr;

        if (memcmp(&convertGUID, &pfScaler, sizeof(GUID)) == 0)
        {
            // No format conversion needed
            hr = scaler->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
            if (FAILED(hr))
                return hr;
        }
        else
        {
            ComPtr<IWICFormatConverter> FC;
            hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
            if (FAILED(hr))
                return hr;

            BOOL canConvert = FALSE;
            hr = FC->CanConvert(pfScaler, convertGUID, &canConvert);
            if (FAILED(hr) || !canConvert)
            {
                return E_UNEXPECTED;
            }

            hr = FC->Initialize(scaler.Get(), convertGUID, WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut);
            if (FAILED(hr))
                return hr;

            hr = FC->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
            if (FAILED(hr))
                return hr;
        }
    }
    else
    {
        // Format conversion but no resize
        auto pWIC = GetWIC();
        if (!pWIC)
            return E_NOINTERFACE;

        ComPtr<IWICFormatConverter> FC;
        hr = pWIC->CreateFormatConverter(FC.GetAddressOf());
        if (FAILED(hr))
            return hr;

        BOOL canConvert = FALSE;
        hr = FC->CanConvert(pixelFormat, convertGUID, &canConvert);
        if (FAILED(hr) || !canConvert)
        {
            return E_UNEXPECTED;
        }

        hr = FC->Initialize(frame, convertGUID, WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut);
        if (FAILED(hr))
            return hr;

        hr = FC->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), temp.get());
        if (FAILED(hr))
            return hr;
    }

    // See if format is supported for auto-gen mipmaps (varies by feature level)
    bool autogen = false;
    if (d3dContext && textureView) // Must have context and shader-view to auto generate mipmaps
    {
        UINT fmtSupport = 0;
        hr = d3dDevice->CheckFormatSupport(format, &fmtSupport);
        if (SUCCEEDED(hr) && (fmtSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN))
        {
            autogen = true;
        #if defined(_XBOX_ONE) && defined(_TITLE)
            if (!d3dDeviceX || !d3dContextX)
                return E_INVALIDARG;
        #endif
        }
    }

    // Create texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = twidth;
    desc.Height = theight;
    desc.MipLevels = (autogen) ? 0u : 1u;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = usage;
    desc.CPUAccessFlags = cpuAccessFlags;

    if (autogen)
    {
        desc.BindFlags = bindFlags | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = miscFlags | D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }
    else
    {
        desc.BindFlags = bindFlags;
        desc.MiscFlags = miscFlags;
    }

    D3D11_SUBRESOURCE_DATA initData = { temp.get(), static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize) };

    ID3D11Texture2D* tex = nullptr;
    hr = d3dDevice->CreateTexture2D(&desc, (autogen) ? nullptr : &initData, &tex);
    if (SUCCEEDED(hr) && tex)
    {
        if (textureView)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.Format = desc.Format;

            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = (autogen) ? unsigned(-1) : 1u;

            hr = d3dDevice->CreateShaderResourceView(tex, &SRVDesc, textureView);
            if (FAILED(hr))
            {
                tex->Release();
                return hr;
            }

            if (autogen)
            {
                assert(d3dContext != nullptr);

            #if defined(_XBOX_ONE) && defined(_TITLE)
                ID3D11Texture2D *pStaging = nullptr;
                CD3D11_TEXTURE2D_DESC stagingDesc(format, twidth, theight, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, 1, 0, 0);
                initData.pSysMem = temp.get();
                initData.SysMemPitch = static_cast<UINT>(rowPitch);
                initData.SysMemSlicePitch = static_cast<UINT>(imageSize);

                hr = d3dDevice->CreateTexture2D(&stagingDesc, &initData, &pStaging);
                if (SUCCEEDED(hr))
                {
                    d3dContext->CopySubresourceRegion(tex, 0, 0, 0, 0, pStaging, 0, nullptr);

                    UINT64 copyFence = d3dContextX->InsertFence(0);
                    while (d3dDeviceX->IsFencePending(copyFence)) { SwitchToThread(); }
                    pStaging->Release();
                }
            #else
                d3dContext->UpdateSubresource(tex, 0, nullptr, temp.get(), static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize));
            #endif
                d3dContext->GenerateMips(*textureView);
            }
        }

        if (texture)
        {
            *texture = tex;
        }
        else
        {
            tex->Release();
        }
    }

    return hr;
}

HRESULT CreateWICTextureFromMemoryEx(
    ID3D11Device* d3dDevice,
    const uint8_t* wicData,
    size_t wicDataSize,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    WIC_LOADER_FLAGS loadFlags,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView **textureView)
{
	if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }

    if (!d3dDevice || !wicData || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    if (!wicDataSize)
        return E_FAIL;

    if (wicDataSize > UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);

    auto pWIC = GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    // Create input stream for memory
    ComPtr<IWICStream> stream;
    HRESULT hr = pWIC->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = stream->InitializeFromMemory(const_cast<uint8_t*>(wicData), static_cast<DWORD>(wicDataSize));
    if (FAILED(hr))
        return hr;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    hr = pWIC->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = CreateTextureFromWIC(d3dDevice, nullptr,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        nullptr, nullptr,
    #endif
        frame.Get(), maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);
    if(FAILED(hr))
        return hr;

    return hr;
}

HRESULT CreateWICTextureFromFileEx(
    ID3D11Device *d3dDevice,
    const wchar_t *szFileName,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    WIC_LOADER_FLAGS loadFlags,
    ID3D11Resource **texture,
    ID3D11ShaderResourceView **textureView)
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }

    if (!d3dDevice || !szFileName || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    auto pWIC = GetWIC();
    if (!pWIC)
        return E_NOINTERFACE;

    // Initialize WIC
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = pWIC->CreateDecoderFromFilename(
        szFileName,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = CreateTextureFromWIC(d3dDevice, nullptr,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        nullptr, nullptr,
    #endif
        frame.Get(),
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);

    return hr;
}

void os_create_texture_from_memory(
    ID3D11Device *d3dDevice,
    const uint8 *wicData,
    size_t wicDataSize,
    ID3D11Resource **texture,
    ID3D11ShaderResourceView **textureView)
{
	CreateWICTextureFromMemoryEx(
		d3dDevice,
        wicData,
		wicDataSize,
        0,
        D3D11_USAGE_DEFAULT,
		D3D11_BIND_SHADER_RESOURCE,
		0,
		0,
        WIC_LOADER_DEFAULT,
        texture,
		textureView);
}

void os_create_texture_from_file(
    ID3D11Device *d3dDevice,
    const wchar *szFileName,
    ID3D11Resource **texture,
    ID3D11ShaderResourceView **textureView)
{
	CreateWICTextureFromFileEx(
		d3dDevice,
        szFileName,
        0,
        D3D11_USAGE_DEFAULT,
		D3D11_BIND_SHADER_RESOURCE,
		0,
		0,
        WIC_LOADER_DEFAULT,
        texture,
		textureView);
}

 constexpr uint32_t DDS_MAGIC = 0x20534444;

struct DDS_PIXELFORMAT
{
    uint32_t    size;
    uint32_t    flags;
    uint32_t    fourCC;
    uint32_t    RGBBitCount;
    uint32_t    RBitMask;
    uint32_t    GBitMask;
    uint32_t    BBitMask;
    uint32_t    ABitMask;
};

struct DDS_HEADER
{
    uint32_t        size;
    uint32_t        flags;
    uint32_t        height;
    uint32_t        width;
    uint32_t        pitchOrLinearSize;
    uint32_t        depth;
    uint32_t        mipMapCount;
    uint32_t        reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t        caps;
    uint32_t        caps2;
    uint32_t        caps3;
    uint32_t        caps4;
    uint32_t        reserved2;
};

struct DDS_HEADER_DXT10
{
    DXGI_FORMAT     dxgiFormat;
    uint32_t        resourceDimension;
    uint32_t        miscFlag; // see D3D11_RESOURCE_MISC_FLAG
    uint32_t        arraySize;
    uint32_t        miscFlags2; // see DDS_MISC_FLAGS2
};

constexpr size_t DDS_MIN_HEADER_SIZE = sizeof(uint32_t) + sizeof(DDS_HEADER);
constexpr size_t DDS_DX10_HEADER_SIZE = sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10);

enum DDS_RESOURCE_MISC_FLAG : uint32_t
{
    DDS_RESOURCE_MISC_TEXTURECUBE = 0x4L,
};

enum DDS_MISC_FLAGS2 : uint32_t
{
    DDS_MISC_FLAGS2_ALPHA_MODE_MASK = 0x7L,
};

#define DDS_FOURCC        0x00000004  // DDPF_FOURCC
#define DDS_RGB           0x00000040  // DDPF_RGB
#define DDS_RGBA          0x00000041  // DDPF_RGB | DDPF_ALPHAPIXELS
#define DDS_LUMINANCE     0x00020000  // DDPF_LUMINANCE
#define DDS_LUMINANCEA    0x00020001  // DDPF_LUMINANCE | DDPF_ALPHAPIXELS
#define DDS_ALPHAPIXELS   0x00000001  // DDPF_ALPHAPIXELS
#define DDS_ALPHA         0x00000002  // DDPF_ALPHA
#define DDS_PAL8          0x00000020  // DDPF_PALETTEINDEXED8
#define DDS_PAL8A         0x00000021  // DDPF_PALETTEINDEXED8 | DDPF_ALPHAPIXELS
#define DDS_BUMPLUMINANCE 0x00040000  // DDPF_BUMPLUMINANCE
#define DDS_BUMPDUDV      0x00080000  // DDPF_BUMPDUDV
#define DDS_BUMPDUDVA     0x00080001  // DDPF_BUMPDUDV | DDPF_ALPHAPIXELS

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
                (static_cast<uint32_t>(static_cast<uint8_t>(ch0)) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))
#endif

#ifndef DDSGLOBALCONST
#if defined(__GNUC__) && !defined(__MINGW32__)
#define DDSGLOBALCONST extern const __attribute__((weak))
#else
#define DDSGLOBALCONST extern const __declspec(selectany)
#endif
#endif /* DDSGLOBALCONST */

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DXT1 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','1'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DXT2 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','2'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DXT3 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','3'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DXT4 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','4'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DXT5 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','5'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_BC4_UNORM =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','4','U'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_BC4_SNORM =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','4','S'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_BC5_UNORM =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','5','U'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_BC5_SNORM =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('B','C','5','S'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_R8G8_B8G8 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('R','G','B','G'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_G8R8_G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('G','R','G','B'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_YUY2 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('Y','U','Y','2'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_UYVY =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('U','Y','V','Y'), 0, 0, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_X8R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8B8G8R8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_X8B8G8R8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_G16R16 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB,  0, 32, 0x0000ffff, 0xffff0000, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_R5G6B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A1R5G5B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x7c00, 0x03e0, 0x001f, 0x8000 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_X1R5G5B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x7c00, 0x03e0, 0x001f, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A4R4G4B4 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x0f00, 0x00f0, 0x000f, 0xf000 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_X4R4G4B4 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x0f00, 0x00f0, 0x000f, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 24, 0xff0000, 0x00ff00, 0x0000ff, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8R3G3B2 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00e0, 0x001c, 0x0003, 0xff00 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_R3G3B2 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 8, 0xe0, 0x1c, 0x03, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A4L4 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 8, 0x0f, 0, 0, 0xf0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_L8 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE, 0, 8, 0xff, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_L16 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE, 0, 16, 0xffff, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8L8 =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 16, 0x00ff, 0, 0, 0xff00 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8L8_ALT =
    { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 8, 0x00ff, 0, 0, 0xff00 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_L8_NVTT1 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 8, 0xff, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_L16_NVTT1 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0xffff, 0, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8L8_NVTT1 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00ff, 0, 0, 0xff00 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A8 =
    { sizeof(DDS_PIXELFORMAT), DDS_ALPHA, 0, 8, 0, 0, 0, 0xff };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_V8U8 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 16, 0x00ff, 0xff00, 0, 0 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_Q8W8V8U8 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_V16U16 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDV, 0, 32, 0x0000ffff, 0xffff0000, 0, 0 };

// D3DFMT_A2R10G10B10/D3DFMT_A2B10G10R10 should be written using DX10 extension to avoid D3DX 10:10:10:2 reversal issue
    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A2R10G10B10 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000 };
    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A2B10G10R10 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 };

// The following legacy Direct3D 9 formats use 'mixed' signed & unsigned channels so requires special handling
    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_A2W10V10U10 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPDUDVA, 0, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 };
    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_L6V5U5 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPLUMINANCE, 0, 16, 0x001f, 0x03e0, 0xfc00, 0 };
    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_X8L8V8U8 =
    { sizeof(DDS_PIXELFORMAT), DDS_BUMPLUMINANCE, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0 };

// This indicates the DDS_HEADER_DXT10 extension is present (the format is in dxgiFormat)
    DDSGLOBALCONST DDS_PIXELFORMAT DDSPF_DX10 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','1','0'), 0, 0, 0, 0, 0 };

#define DDS_HEADER_FLAGS_TEXTURE        0x00001007  // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
#define DDS_HEADER_FLAGS_MIPMAP         0x00020000  // DDSD_MIPMAPCOUNT
#define DDS_HEADER_FLAGS_VOLUME         0x00800000  // DDSD_DEPTH
#define DDS_HEADER_FLAGS_PITCH          0x00000008  // DDSD_PITCH
#define DDS_HEADER_FLAGS_LINEARSIZE     0x00080000  // DDSD_LINEARSIZE

#define DDS_HEIGHT 0x00000002 // DDSD_HEIGHT
#define DDS_WIDTH  0x00000004 // DDSD_WIDTH

#define DDS_SURFACE_FLAGS_TEXTURE 0x00001000 // DDSCAPS_TEXTURE
#define DDS_SURFACE_FLAGS_MIPMAP  0x00400008 // DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
#define DDS_SURFACE_FLAGS_CUBEMAP 0x00000008 // DDSCAPS_COMPLEX

#define DDS_CUBEMAP_POSITIVEX 0x00000600 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
#define DDS_CUBEMAP_POSITIVEY 0x00001200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
#define DDS_CUBEMAP_NEGATIVEY 0x00002200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
#define DDS_CUBEMAP_POSITIVEZ 0x00004200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

#define DDS_CUBEMAP_ALLFACES ( DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |\
                               DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |\
                               DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ )

#define DDS_CUBEMAP 0x00000200 // DDSCAPS2_CUBEMAP

#define DDS_FLAGS_VOLUME 0x00200000 // DDSCAPS2_VOLUME

 enum DDS_ALPHA_MODE : uint32_t
{
    DDS_ALPHA_MODE_UNKNOWN = 0,
    DDS_ALPHA_MODE_STRAIGHT = 1,
    DDS_ALPHA_MODE_PREMULTIPLIED = 2,
    DDS_ALPHA_MODE_OPAQUE = 3,
    DDS_ALPHA_MODE_CUSTOM = 4,
};

enum DDS_LOADER_FLAGS : uint32_t
{
    DDS_LOADER_DEFAULT = 0,
    DDS_LOADER_FORCE_SRGB = 0x1,
    DDS_LOADER_IGNORE_SRGB = 0x2,
};

size_t BitsPerPixel(_In_ DXGI_FORMAT fmt) noexcept
{
    switch (fmt)
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 96;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        return 64;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_YUY2:
    #if (defined(_XBOX_ONE) && defined(_TITLE)) || defined(_GAMING_XBOX)
    case DXGI_FORMAT_R10G10B10_7E3_A2_FLOAT:
    case DXGI_FORMAT_R10G10B10_6E4_A2_FLOAT:
    case DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM:
    #endif
        return 32;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN10)
    case DXGI_FORMAT_V408:
    #endif
    #if (defined(_XBOX_ONE) && defined(_TITLE)) || defined(_GAMING_XBOX)
    case DXGI_FORMAT_D16_UNORM_S8_UINT:
    case DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X16_TYPELESS_G8_UINT:
    #endif
        return 24;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN10)
    case DXGI_FORMAT_P208:
    case DXGI_FORMAT_V208:
    #endif
        return 16;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
        return 12;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    #if (defined(_XBOX_ONE) && defined(_TITLE)) || defined(_GAMING_XBOX)
    case DXGI_FORMAT_R4G4_UNORM:
    #endif
        return 8;

    case DXGI_FORMAT_R1_UNORM:
        return 1;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 4;

    case DXGI_FORMAT_UNKNOWN:
    case DXGI_FORMAT_FORCE_UINT:
    default:
        return 0;
    }
}

inline HRESULT GetSurfaceInfo(
    _In_ size_t width,
    _In_ size_t height,
    _In_ DXGI_FORMAT fmt,
    _Out_opt_ size_t* outNumBytes,
    _Out_opt_ size_t* outRowBytes,
    _Out_opt_ size_t* outNumRows) noexcept
{
    uint64_t numBytes = 0;
    uint64_t rowBytes = 0;
    uint64_t numRows = 0;

    bool bc = false;
    bool packed = false;
    bool planar = false;
    size_t bpe = 0;
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        bc = true;
        bpe = 8;
        break;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        bc = true;
        bpe = 16;
        break;

    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_YUY2:
        packed = true;
        bpe = 4;
        break;

    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        packed = true;
        bpe = 8;
        break;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
        if ((height % 2) != 0)
        {
            // Requires a height alignment of 2.
            return E_INVALIDARG;
        }
        planar = true;
        bpe = 2;
        break;

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN10)

    case DXGI_FORMAT_P208:
        planar = true;
        bpe = 2;
        break;

    #endif

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        if ((height % 2) != 0)
        {
            // Requires a height alignment of 2.
            return E_INVALIDARG;
        }
        planar = true;
        bpe = 4;
        break;

    #if (defined(_XBOX_ONE) && defined(_TITLE)) || defined(_GAMING_XBOX)

    case DXGI_FORMAT_D16_UNORM_S8_UINT:
    case DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X16_TYPELESS_G8_UINT:
        planar = true;
        bpe = 4;
        break;

    #endif

    default:
        break;
    }

    if (bc)
    {
        uint64_t numBlocksWide = 0;
        if (width > 0)
        {
            numBlocksWide = std::max<uint64_t>(1u, (uint64_t(width) + 3u) / 4u);
        }
        uint64_t numBlocksHigh = 0;
        if (height > 0)
        {
            numBlocksHigh = std::max<uint64_t>(1u, (uint64_t(height) + 3u) / 4u);
        }
        rowBytes = numBlocksWide * bpe;
        numRows = numBlocksHigh;
        numBytes = rowBytes * numBlocksHigh;
    }
    else if (packed)
    {
        rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
        numRows = uint64_t(height);
        numBytes = rowBytes * height;
    }
    else if (fmt == DXGI_FORMAT_NV11)
    {
        rowBytes = ((uint64_t(width) + 3u) >> 2) * 4u;
        numRows = uint64_t(height) * 2u; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
        numBytes = rowBytes * numRows;
    }
    else if (planar)
    {
        rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
        numBytes = (rowBytes * uint64_t(height)) + ((rowBytes * uint64_t(height) + 1u) >> 1);
        numRows = height + ((uint64_t(height) + 1u) >> 1);
    }
    else
    {
        const size_t bpp = BitsPerPixel(fmt);
        if (!bpp)
            return E_INVALIDARG;

        rowBytes = (uint64_t(width) * bpp + 7u) / 8u; // round up to nearest byte
        numRows = uint64_t(height);
        numBytes = rowBytes * height;
    }

#if defined(_M_IX86) || defined(_M_ARM) || defined(_M_HYBRID_X86_ARM64)
    static_assert(sizeof(size_t) == 4, "Not a 32-bit platform!");
    if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX || numRows > UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
#else
    static_assert(sizeof(size_t) == 8, "Not a 64-bit platform!");
#endif

    if (outNumBytes)
    {
        *outNumBytes = static_cast<size_t>(numBytes);
    }
    if (outRowBytes)
    {
        *outRowBytes = static_cast<size_t>(rowBytes);
    }
    if (outNumRows)
    {
        *outNumRows = static_cast<size_t>(numRows);
    }

    return S_OK;
}

#define ISBITMASK( r,g,b,a ) ( ddpf.RBitMask == r && ddpf.GBitMask == g && ddpf.BBitMask == b && ddpf.ABitMask == a )

DXGI_FORMAT GetDXGIFormat(const DDS_PIXELFORMAT& ddpf) noexcept
{
    if (ddpf.flags & DDS_RGB)
    {
        // Note that sRGB formats are written using the "DX10" extended header

        switch (ddpf.RGBBitCount)
        {
        case 32:
            if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
            {
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            }

            if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
            {
                return DXGI_FORMAT_B8G8R8A8_UNORM;
            }

            if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0))
            {
                return DXGI_FORMAT_B8G8R8X8_UNORM;
            }

            // No DXGI format maps to ISBITMASK(0x000000ff,0x0000ff00,0x00ff0000,0) aka D3DFMT_X8B8G8R8

            // Note that many common DDS reader/writers (including D3DX) swap the
            // the RED/BLUE masks for 10:10:10:2 formats. We assume
            // below that the 'backwards' header mask is being used since it is most
            // likely written by D3DX. The more robust solution is to use the 'DX10'
            // header extension and specify the DXGI_FORMAT_R10G10B10A2_UNORM format directly

            // For 'correct' writers, this should be 0x000003ff,0x000ffc00,0x3ff00000 for RGB data
            if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
            {
                return DXGI_FORMAT_R10G10B10A2_UNORM;
            }

            // No DXGI format maps to ISBITMASK(0x000003ff,0x000ffc00,0x3ff00000,0xc0000000) aka D3DFMT_A2R10G10B10

            if (ISBITMASK(0x0000ffff, 0xffff0000, 0, 0))
            {
                return DXGI_FORMAT_R16G16_UNORM;
            }

            if (ISBITMASK(0xffffffff, 0, 0, 0))
            {
                // Only 32-bit color channel format in D3D9 was R32F
                return DXGI_FORMAT_R32_FLOAT; // D3DX writes this out as a FourCC of 114
            }
            break;

        case 24:
            // No 24bpp DXGI formats aka D3DFMT_R8G8B8
            break;

        case 16:
            if (ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x8000))
            {
                return DXGI_FORMAT_B5G5R5A1_UNORM;
            }
            if (ISBITMASK(0xf800, 0x07e0, 0x001f, 0))
            {
                return DXGI_FORMAT_B5G6R5_UNORM;
            }

            // No DXGI format maps to ISBITMASK(0x7c00,0x03e0,0x001f,0) aka D3DFMT_X1R5G5B5

            if (ISBITMASK(0x0f00, 0x00f0, 0x000f, 0xf000))
            {
                return DXGI_FORMAT_B4G4R4A4_UNORM;
            }

            // NVTT versions 1.x wrote this as RGB instead of LUMINANCE
            if (ISBITMASK(0x00ff, 0, 0, 0xff00))
            {
                return DXGI_FORMAT_R8G8_UNORM;
            }
            if (ISBITMASK(0xffff, 0, 0, 0))
            {
                return DXGI_FORMAT_R16_UNORM;
            }

            // No DXGI format maps to ISBITMASK(0x0f00,0x00f0,0x000f,0) aka D3DFMT_X4R4G4B4

            // No 3:3:2:8 or paletted DXGI formats aka D3DFMT_A8R3G3B2, D3DFMT_A8P8, etc.
            break;

        case 8:
            // NVTT versions 1.x wrote this as RGB instead of LUMINANCE
            if (ISBITMASK(0xff, 0, 0, 0))
            {
                return DXGI_FORMAT_R8_UNORM;
            }

            // No 3:3:2 or paletted DXGI formats aka D3DFMT_R3G3B2, D3DFMT_P8
            break;

        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else if (ddpf.flags & DDS_LUMINANCE)
    {
        switch (ddpf.RGBBitCount)
        {
        case 16:
            if (ISBITMASK(0xffff, 0, 0, 0))
            {
                return DXGI_FORMAT_R16_UNORM; // D3DX10/11 writes this out as DX10 extension
            }
            if (ISBITMASK(0x00ff, 0, 0, 0xff00))
            {
                return DXGI_FORMAT_R8G8_UNORM; // D3DX10/11 writes this out as DX10 extension
            }
            break;

        case 8:
            if (ISBITMASK(0xff, 0, 0, 0))
            {
                return DXGI_FORMAT_R8_UNORM; // D3DX10/11 writes this out as DX10 extension
            }

            // No DXGI format maps to ISBITMASK(0x0f,0,0,0xf0) aka D3DFMT_A4L4

            if (ISBITMASK(0x00ff, 0, 0, 0xff00))
            {
                return DXGI_FORMAT_R8G8_UNORM; // Some DDS writers assume the bitcount should be 8 instead of 16
            }
            break;

        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }
    else if (ddpf.flags & DDS_ALPHA)
    {
        if (8 == ddpf.RGBBitCount)
        {
            return DXGI_FORMAT_A8_UNORM;
        }
    }
    else if (ddpf.flags & DDS_BUMPDUDV)
    {
        switch (ddpf.RGBBitCount)
        {
        case 32:
            if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
            {
                return DXGI_FORMAT_R8G8B8A8_SNORM; // D3DX10/11 writes this out as DX10 extension
            }
            if (ISBITMASK(0x0000ffff, 0xffff0000, 0, 0))
            {
                return DXGI_FORMAT_R16G16_SNORM; // D3DX10/11 writes this out as DX10 extension
            }

            // No DXGI format maps to ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000) aka D3DFMT_A2W10V10U10
            break;

        case 16:
            if (ISBITMASK(0x00ff, 0xff00, 0, 0))
            {
                return DXGI_FORMAT_R8G8_SNORM; // D3DX10/11 writes this out as DX10 extension
            }
            break;

        default:
            return DXGI_FORMAT_UNKNOWN;
        }

        // No DXGI format maps to DDPF_BUMPLUMINANCE aka D3DFMT_L6V5U5, D3DFMT_X8L8V8U8
    }
    else if (ddpf.flags & DDS_FOURCC)
    {
        if (MAKEFOURCC('D', 'X', 'T', '1') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC1_UNORM;
        }
        if (MAKEFOURCC('D', 'X', 'T', '3') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC2_UNORM;
        }
        if (MAKEFOURCC('D', 'X', 'T', '5') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC3_UNORM;
        }

        // While pre-multiplied alpha isn't directly supported by the DXGI formats,
        // they are basically the same as these BC formats so they can be mapped
        if (MAKEFOURCC('D', 'X', 'T', '2') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC2_UNORM;
        }
        if (MAKEFOURCC('D', 'X', 'T', '4') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC3_UNORM;
        }

        if (MAKEFOURCC('A', 'T', 'I', '1') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC4_UNORM;
        }
        if (MAKEFOURCC('B', 'C', '4', 'U') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC4_UNORM;
        }
        if (MAKEFOURCC('B', 'C', '4', 'S') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC4_SNORM;
        }

        if (MAKEFOURCC('A', 'T', 'I', '2') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC5_UNORM;
        }
        if (MAKEFOURCC('B', 'C', '5', 'U') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC5_UNORM;
        }
        if (MAKEFOURCC('B', 'C', '5', 'S') == ddpf.fourCC)
        {
            return DXGI_FORMAT_BC5_SNORM;
        }

        // BC6H and BC7 are written using the "DX10" extended header

        if (MAKEFOURCC('R', 'G', 'B', 'G') == ddpf.fourCC)
        {
            return DXGI_FORMAT_R8G8_B8G8_UNORM;
        }
        if (MAKEFOURCC('G', 'R', 'G', 'B') == ddpf.fourCC)
        {
            return DXGI_FORMAT_G8R8_G8B8_UNORM;
        }

        if (MAKEFOURCC('Y', 'U', 'Y', '2') == ddpf.fourCC)
        {
            return DXGI_FORMAT_YUY2;
        }

        // Check for D3DFORMAT enums being set here
        switch (ddpf.fourCC)
        {
        case 36: // D3DFMT_A16B16G16R16
            return DXGI_FORMAT_R16G16B16A16_UNORM;

        case 110: // D3DFMT_Q16W16V16U16
            return DXGI_FORMAT_R16G16B16A16_SNORM;

        case 111: // D3DFMT_R16F
            return DXGI_FORMAT_R16_FLOAT;

        case 112: // D3DFMT_G16R16F
            return DXGI_FORMAT_R16G16_FLOAT;

        case 113: // D3DFMT_A16B16G16R16F
            return DXGI_FORMAT_R16G16B16A16_FLOAT;

        case 114: // D3DFMT_R32F
            return DXGI_FORMAT_R32_FLOAT;

        case 115: // D3DFMT_G32R32F
            return DXGI_FORMAT_R32G32_FLOAT;

        case 116: // D3DFMT_A32B32G32R32F
            return DXGI_FORMAT_R32G32B32A32_FLOAT;

        // No DXGI format maps to D3DFMT_CxV8U8

        default:
            return DXGI_FORMAT_UNKNOWN;
        }
    }

    return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT MakeLinear(_In_ DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case DXGI_FORMAT_BC1_UNORM_SRGB:
        return DXGI_FORMAT_BC1_UNORM;

    case DXGI_FORMAT_BC2_UNORM_SRGB:
        return DXGI_FORMAT_BC2_UNORM;

    case DXGI_FORMAT_BC3_UNORM_SRGB:
        return DXGI_FORMAT_BC3_UNORM;

    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM;

    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8X8_UNORM;

    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return DXGI_FORMAT_BC7_UNORM;

    default:
        return format;
    }
}

HRESULT CreateD3DResources(
    _In_ ID3D11Device* d3dDevice,
    _In_ uint32_t resDim,
    _In_ size_t width,
    _In_ size_t height,
    _In_ size_t depth,
    _In_ size_t mipCount,
    _In_ size_t arraySize,
    _In_ DXGI_FORMAT format,
    _In_ D3D11_USAGE usage,
    _In_ unsigned int bindFlags,
    _In_ unsigned int cpuAccessFlags,
    _In_ unsigned int miscFlags,
    _In_ DDS_LOADER_FLAGS loadFlags,
    _In_ bool isCubeMap,
    _In_reads_opt_(mipCount*arraySize) const D3D11_SUBRESOURCE_DATA* initData,
    _Outptr_opt_ ID3D11Resource** texture,
    _Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept
{
    if (!d3dDevice)
        return E_POINTER;

    HRESULT hr = E_FAIL;

    if (loadFlags & DDS_LOADER_FORCE_SRGB)
    {
        format = MakeSRGB(format);
    }
    else if (loadFlags & DDS_LOADER_IGNORE_SRGB)
    {
        format = MakeLinear(format);
    }

    switch (resDim)
    {
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        {
            D3D11_TEXTURE1D_DESC desc = {};
            desc.Width = static_cast<UINT>(width);
            desc.MipLevels = static_cast<UINT>(mipCount);
            desc.ArraySize = static_cast<UINT>(arraySize);
            desc.Format = format;
            desc.Usage = usage;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = cpuAccessFlags;
            desc.MiscFlags = miscFlags & ~static_cast<unsigned int>(D3D11_RESOURCE_MISC_TEXTURECUBE);

            ID3D11Texture1D* tex = nullptr;
            hr = d3dDevice->CreateTexture1D(&desc,
                initData,
                &tex
            );
            if (SUCCEEDED(hr) && tex)
            {
                if (textureView)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                    SRVDesc.Format = format;

                    if (arraySize > 1)
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
                        SRVDesc.Texture1DArray.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                        SRVDesc.Texture1DArray.ArraySize = static_cast<UINT>(arraySize);
                    }
                    else
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                        SRVDesc.Texture1D.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                    }

                    hr = d3dDevice->CreateShaderResourceView(tex,
                        &SRVDesc,
                        textureView
                    );
                    if (FAILED(hr))
                    {
                        tex->Release();
                        return hr;
                    }
                }

                if (texture)
                {
                    *texture = tex;
                }
                else
                {
                    tex->Release();
                }
            }
        }
        break;

    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        {
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = static_cast<UINT>(width);
            desc.Height = static_cast<UINT>(height);
            desc.MipLevels = static_cast<UINT>(mipCount);
            desc.ArraySize = static_cast<UINT>(arraySize);
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = usage;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = cpuAccessFlags;
            if (isCubeMap)
            {
                desc.MiscFlags = miscFlags | D3D11_RESOURCE_MISC_TEXTURECUBE;
            }
            else
            {
                desc.MiscFlags = miscFlags & ~static_cast<unsigned int>(D3D11_RESOURCE_MISC_TEXTURECUBE);
            }

            ID3D11Texture2D* tex = nullptr;
            hr = d3dDevice->CreateTexture2D(&desc,
                initData,
                &tex
            );
            if (SUCCEEDED(hr) && tex)
            {
                if (textureView)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                    SRVDesc.Format = format;

                    if (isCubeMap)
                    {
                        if (arraySize > 6)
                        {
                            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                            SRVDesc.TextureCubeArray.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;

                            // Earlier we set arraySize to (NumCubes * 6)
                            SRVDesc.TextureCubeArray.NumCubes = static_cast<UINT>(arraySize / 6);
                        }
                        else
                        {
                            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                            SRVDesc.TextureCube.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                        }
                    }
                    else if (arraySize > 1)
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                        SRVDesc.Texture2DArray.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                        SRVDesc.Texture2DArray.ArraySize = static_cast<UINT>(arraySize);
                    }
                    else
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                        SRVDesc.Texture2D.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                    }

                    hr = d3dDevice->CreateShaderResourceView(tex,
                        &SRVDesc,
                        textureView
                    );
                    if (FAILED(hr))
                    {
                        tex->Release();
                        return hr;
                    }
                }

                if (texture)
                {
                    *texture = tex;
                }
                else
                {
                    tex->Release();
                }
            }
        }
        break;

    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        {
            D3D11_TEXTURE3D_DESC desc = {};
            desc.Width = static_cast<UINT>(width);
            desc.Height = static_cast<UINT>(height);
            desc.Depth = static_cast<UINT>(depth);
            desc.MipLevels = static_cast<UINT>(mipCount);
            desc.Format = format;
            desc.Usage = usage;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = cpuAccessFlags;
            desc.MiscFlags = miscFlags & ~UINT(D3D11_RESOURCE_MISC_TEXTURECUBE);

            ID3D11Texture3D* tex = nullptr;
            hr = d3dDevice->CreateTexture3D(&desc,
                initData,
                &tex
            );
            if (SUCCEEDED(hr) && tex)
            {
                if (textureView)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                    SRVDesc.Format = format;

                    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                    SRVDesc.Texture3D.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;

                    hr = d3dDevice->CreateShaderResourceView(tex,
                        &SRVDesc,
                        textureView
                    );
                    if (FAILED(hr))
                    {
                        tex->Release();
                        return hr;
                    }
                }

                if (texture)
                {
                    *texture = tex;
                }
                else
                {
                    tex->Release();
                }
            }
        }
        break;

    default:
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    return hr;
}

HRESULT FillInitData(
    _In_ size_t width,
    _In_ size_t height,
    _In_ size_t depth,
    _In_ size_t mipCount,
    _In_ size_t arraySize,
    _In_ DXGI_FORMAT format,
    _In_ size_t maxsize,
    _In_ size_t bitSize,
    _In_reads_bytes_(bitSize) const uint8_t* bitData,
    _Out_ size_t& twidth,
    _Out_ size_t& theight,
    _Out_ size_t& tdepth,
    _Out_ size_t& skipMip,
    _Out_writes_(mipCount*arraySize) D3D11_SUBRESOURCE_DATA* initData) noexcept
{
    if (!bitData || !initData)
    {
        return E_POINTER;
    }

    skipMip = 0;
    twidth = 0;
    theight = 0;
    tdepth = 0;

    size_t NumBytes = 0;
    size_t RowBytes = 0;
    const uint8_t* pSrcBits = bitData;
    const uint8_t* pEndBits = bitData + bitSize;

    size_t index = 0;
    for (size_t j = 0; j < arraySize; j++)
    {
        size_t w = width;
        size_t h = height;
        size_t d = depth;
        for (size_t i = 0; i < mipCount; i++)
        {
            HRESULT hr = GetSurfaceInfo(w, h, format, &NumBytes, &RowBytes, nullptr);
            if (FAILED(hr))
                return hr;

            if (NumBytes > UINT32_MAX || RowBytes > UINT32_MAX)
                return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

            if ((mipCount <= 1) || !maxsize || (w <= maxsize && h <= maxsize && d <= maxsize))
            {
                if (!twidth)
                {
                    twidth = w;
                    theight = h;
                    tdepth = d;
                }

                assert(index < mipCount * arraySize);
                _Analysis_assume_(index < mipCount * arraySize);
                initData[index].pSysMem = pSrcBits;
                initData[index].SysMemPitch = static_cast<UINT>(RowBytes);
                initData[index].SysMemSlicePitch = static_cast<UINT>(NumBytes);
                ++index;
            }
            else if (!j)
            {
                // Count number of skipped mipmaps (first item only)
                ++skipMip;
            }

            if (pSrcBits + (NumBytes*d) > pEndBits)
            {
                return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
            }

            pSrcBits += NumBytes * d;

            w = w >> 1;
            h = h >> 1;
            d = d >> 1;
            if (w == 0)
            {
                w = 1;
            }
            if (h == 0)
            {
                h = 1;
            }
            if (d == 0)
            {
                d = 1;
            }
        }
    }

    return (index > 0) ? S_OK : E_FAIL;
}

HRESULT CreateTextureFromDDS(
    _In_ ID3D11Device* d3dDevice,
    _In_opt_ ID3D11DeviceContext* d3dContext,
#if defined(_XBOX_ONE) && defined(_TITLE)
    _In_opt_ ID3D11DeviceX* d3dDeviceX,
    _In_opt_ ID3D11DeviceContextX* d3dContextX,
#endif
    _In_ const DDS_HEADER* header,
    _In_reads_bytes_(bitSize) const uint8_t* bitData,
    _In_ size_t bitSize,
    _In_ size_t maxsize,
    _In_ D3D11_USAGE usage,
    _In_ unsigned int bindFlags,
    _In_ unsigned int cpuAccessFlags,
    _In_ unsigned int miscFlags,
    _In_ DDS_LOADER_FLAGS loadFlags,
    _Outptr_opt_ ID3D11Resource** texture,
    _Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept
{
    HRESULT hr = S_OK;

    const UINT width = header->width;
    UINT height = header->height;
    UINT depth = header->depth;

    uint32_t resDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    UINT arraySize = 1;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool isCubeMap = false;

    size_t mipCount = header->mipMapCount;
    if (0 == mipCount)
    {
        mipCount = 1;
    }

    if ((header->ddspf.flags & DDS_FOURCC) &&
        (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
    {
        auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10*>(reinterpret_cast<const char*>(header) + sizeof(DDS_HEADER));

        arraySize = d3d10ext->arraySize;
        if (arraySize == 0)
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

        switch (d3d10ext->dxgiFormat)
        {
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_420_OPAQUE:
            if ((d3d10ext->resourceDimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
                || (width % 2) != 0 || (height % 2) != 0)
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        case DXGI_FORMAT_YUY2:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
        case DXGI_FORMAT_P208:
            if ((width % 2) != 0)
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        case DXGI_FORMAT_NV11:
            if ((width % 4) != 0)
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
        case DXGI_FORMAT_A8P8:
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

        default:
            if (BitsPerPixel(d3d10ext->dxgiFormat) == 0)
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
        }

        format = d3d10ext->dxgiFormat;

        switch (d3d10ext->resourceDimension)
        {
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            // D3DX writes 1D textures with a fixed Height of 1
            if ((header->flags & DDS_HEIGHT) && height != 1)
            {
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            }
            height = depth = 1;
            break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            if (d3d10ext->miscFlag & D3D11_RESOURCE_MISC_TEXTURECUBE)
            {
                arraySize *= 6;
                isCubeMap = true;
            }
            depth = 1;
            break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            if (!(header->flags & DDS_HEADER_FLAGS_VOLUME))
            {
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            }

            if (arraySize > 1)
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        case D3D11_RESOURCE_DIMENSION_BUFFER:
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

        case D3D11_RESOURCE_DIMENSION_UNKNOWN:
        default:
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        resDim = d3d10ext->resourceDimension;
    }
    else
    {
        format = GetDXGIFormat(header->ddspf);

        if (format == DXGI_FORMAT_UNKNOWN)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        if (header->flags & DDS_HEADER_FLAGS_VOLUME)
        {
            resDim = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
        }
        else
        {
            if (header->caps2 & DDS_CUBEMAP)
            {
                // We require all six faces to be defined
                if ((header->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES)
                {
                    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                }

                arraySize = 6;
                isCubeMap = true;
            }

            depth = 1;
            resDim = D3D11_RESOURCE_DIMENSION_TEXTURE2D;

            // Note there's no way for a legacy Direct3D 9 DDS to express a '1D' texture
        }

        assert(BitsPerPixel(format) != 0);
    }

    if ((miscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
        && (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        && ((arraySize % 6) == 0))
    {
        isCubeMap = true;
    }

    // Bound sizes (for security purposes we don't trust DDS file metadata larger than the Direct3D hardware requirements)
    if (mipCount > D3D11_REQ_MIP_LEVELS)
    {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    switch (resDim)
    {
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        if ((arraySize > D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION) ||
            (width > D3D11_REQ_TEXTURE1D_U_DIMENSION))
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }
        break;

    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        if (isCubeMap)
        {
            // This is the right bound because we set arraySize to (NumCubes*6) above
            if ((arraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
                (width > D3D11_REQ_TEXTURECUBE_DIMENSION) ||
                (height > D3D11_REQ_TEXTURECUBE_DIMENSION))
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
        }
        else if ((arraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
            (width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) ||
            (height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION))
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }
        break;

    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        if ((arraySize > 1) ||
            (width > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
            (height > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
            (depth > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION))
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }
        break;

    case D3D11_RESOURCE_DIMENSION_BUFFER:
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    case D3D11_RESOURCE_DIMENSION_UNKNOWN:
    default:
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    bool autogen = false;
    if (mipCount == 1 && d3dContext && textureView) // Must have context and shader-view to auto generate mipmaps
    {
        // See if format is supported for auto-gen mipmaps (varies by feature level)
        UINT fmtSupport = 0;
        hr = d3dDevice->CheckFormatSupport(format, &fmtSupport);
        if (SUCCEEDED(hr) && (fmtSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN))
        {
            // 10level9 feature levels do not support auto-gen mipgen for volume textures
            if ((resDim != D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                || (d3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_10_0))
            {
                autogen = true;
            #if defined(_XBOX_ONE) && defined(_TITLE)
                if (!d3dDeviceX || !d3dContextX)
                    return E_INVALIDARG;
            #endif
            }
        }
    }

    if (autogen)
    {
        // Create texture with auto-generated mipmaps
        ID3D11Resource* tex = nullptr;
        hr = CreateD3DResources(d3dDevice,
            resDim, width, height, depth, 0, arraySize,
            format,
            usage,
            bindFlags | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            cpuAccessFlags,
            miscFlags | D3D11_RESOURCE_MISC_GENERATE_MIPS, loadFlags,
            isCubeMap,
            nullptr,
            &tex, textureView);
        if (SUCCEEDED(hr))
        {
            size_t numBytes = 0;
            size_t rowBytes = 0;
            hr = GetSurfaceInfo(width, height, format, &numBytes, &rowBytes, nullptr);
            if (FAILED(hr))
                return hr;

            if (numBytes > bitSize)
            {
                (*textureView)->Release();
                *textureView = nullptr;
                tex->Release();
                return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
            }

            if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX)
                return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

            D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
            (*textureView)->GetDesc(&desc);

            UINT mipLevels = 1;

            switch (desc.ViewDimension)
            {
            case D3D_SRV_DIMENSION_TEXTURE1D:       mipLevels = desc.Texture1D.MipLevels; break;
            case D3D_SRV_DIMENSION_TEXTURE1DARRAY:  mipLevels = desc.Texture1DArray.MipLevels; break;
            case D3D_SRV_DIMENSION_TEXTURE2D:       mipLevels = desc.Texture2D.MipLevels; break;
            case D3D_SRV_DIMENSION_TEXTURE2DARRAY:  mipLevels = desc.Texture2DArray.MipLevels; break;
            case D3D_SRV_DIMENSION_TEXTURECUBE:     mipLevels = desc.TextureCube.MipLevels; break;
            case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:mipLevels = desc.TextureCubeArray.MipLevels; break;
            case D3D_SRV_DIMENSION_TEXTURE3D:       mipLevels = desc.Texture3D.MipLevels; break;
            default:
                (*textureView)->Release();
                *textureView = nullptr;
                tex->Release();
                return E_UNEXPECTED;
            }

        #if defined(_XBOX_ONE) && defined(_TITLE)

            std::unique_ptr<D3D11_SUBRESOURCE_DATA[]> initData(new (std::nothrow) D3D11_SUBRESOURCE_DATA[arraySize]);
            if (!initData)
            {
                return E_OUTOFMEMORY;
            }

            const uint8_t* pSrcBits = bitData;
            const uint8_t* pEndBits = bitData + bitSize;
            for (UINT item = 0; item < arraySize; ++item)
            {
                if ((pSrcBits + numBytes) > pEndBits)
                {
                    (*textureView)->Release();
                    *textureView = nullptr;
                    tex->Release();
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                }

                initData[item].pSysMem = pSrcBits;
                initData[item].SysMemPitch = static_cast<UINT>(rowBytes);
                initData[item].SysMemSlicePitch = static_cast<UINT>(numBytes);
                pSrcBits += numBytes;
            }

            ID3D11Resource* pStaging = nullptr;
            switch (resDim)
            {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                {
                    ID3D11Texture1D *temp = nullptr;
                    CD3D11_TEXTURE1D_DESC stagingDesc(format, width, arraySize, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
                    hr = d3dDevice->CreateTexture1D(&stagingDesc, initData.get(), &temp);
                    if (SUCCEEDED(hr))
                        pStaging = temp;
                }
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                {
                    ID3D11Texture2D *temp = nullptr;
                    CD3D11_TEXTURE2D_DESC stagingDesc(format, width, height, arraySize, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, 1, 0, isCubeMap ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0);
                    hr = d3dDevice->CreateTexture2D(&stagingDesc, initData.get(), &temp);
                    if (SUCCEEDED(hr))
                        pStaging = temp;
                }
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                {
                    ID3D11Texture3D *temp = nullptr;
                    CD3D11_TEXTURE3D_DESC stagingDesc(format, width, height, depth, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
                    hr = d3dDevice->CreateTexture3D(&stagingDesc, initData.get(), &temp);
                    if (SUCCEEDED(hr))
                        pStaging = temp;
                }
                break;
            };

            if (SUCCEEDED(hr))
            {
                for (UINT item = 0; item < arraySize; ++item)
                {
                    UINT res = D3D11CalcSubresource(0, item, mipLevels);
                    d3dContext->CopySubresourceRegion(tex, res, 0, 0, 0, pStaging, item, nullptr);
                }

                UINT64 copyFence = d3dContextX->InsertFence(0);
                while (d3dDeviceX->IsFencePending(copyFence)) { SwitchToThread(); }
                pStaging->Release();
            }
        #else
            if (arraySize > 1)
            {
                const uint8_t* pSrcBits = bitData;
                const uint8_t* pEndBits = bitData + bitSize;
                for (UINT item = 0; item < arraySize; ++item)
                {
                    if ((pSrcBits + numBytes) > pEndBits)
                    {
                        (*textureView)->Release();
                        *textureView = nullptr;
                        tex->Release();
                        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                    }

                    const UINT res = D3D11CalcSubresource(0, item, mipLevels);
                    d3dContext->UpdateSubresource(tex, res, nullptr, pSrcBits, static_cast<UINT>(rowBytes), static_cast<UINT>(numBytes));
                    pSrcBits += numBytes;
                }
            }
            else
            {
                d3dContext->UpdateSubresource(tex, 0, nullptr, bitData, static_cast<UINT>(rowBytes), static_cast<UINT>(numBytes));
            }
        #endif

            d3dContext->GenerateMips(*textureView);

            if (texture)
            {
                *texture = tex;
            }
            else
            {
                tex->Release();
            }
        }
    }
    else
    {
        // Create the texture
        std::unique_ptr<D3D11_SUBRESOURCE_DATA[]> initData(new (std::nothrow) D3D11_SUBRESOURCE_DATA[mipCount * arraySize]);
        if (!initData)
        {
            return E_OUTOFMEMORY;
        }

        size_t skipMip = 0;
        size_t twidth = 0;
        size_t theight = 0;
        size_t tdepth = 0;
        hr = FillInitData(width, height, depth, mipCount, arraySize, format,
            maxsize, bitSize, bitData,
            twidth, theight, tdepth, skipMip, initData.get());

        if (SUCCEEDED(hr))
        {
            hr = CreateD3DResources(d3dDevice,
                resDim, twidth, theight, tdepth, mipCount - skipMip, arraySize,
                format,
                usage, bindFlags, cpuAccessFlags, miscFlags,
                loadFlags,
                isCubeMap,
                initData.get(),
                texture, textureView);

            if (FAILED(hr) && !maxsize && (mipCount > 1))
            {
                // Retry with a maxsize determined by feature level
                switch (d3dDevice->GetFeatureLevel())
                {
                case D3D_FEATURE_LEVEL_9_1:
                case D3D_FEATURE_LEVEL_9_2:
                    if (isCubeMap)
                    {
                        maxsize = 512u /*D3D_FL9_1_REQ_TEXTURECUBE_DIMENSION*/;
                    }
                    else
                    {
                        maxsize = (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                            ? 256u /*D3D_FL9_1_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/
                            : 2048u /*D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                    }
                    break;

                case D3D_FEATURE_LEVEL_9_3:
                    maxsize = (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                        ? 256u /*D3D_FL9_1_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/
                        : 4096u /*D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                    break;

                default: // D3D_FEATURE_LEVEL_10_0 & D3D_FEATURE_LEVEL_10_1
                    maxsize = (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                        ? 2048u /*D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/
                        : 8192u /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                    break;
                }

                hr = FillInitData(width, height, depth, mipCount, arraySize, format,
                    maxsize, bitSize, bitData,
                    twidth, theight, tdepth, skipMip, initData.get());
                if (SUCCEEDED(hr))
                {
                    hr = CreateD3DResources(d3dDevice,
                        resDim, twidth, theight, tdepth, mipCount - skipMip, arraySize,
                        format,
                        usage, bindFlags, cpuAccessFlags, miscFlags,
                        loadFlags,
                        isCubeMap,
                        initData.get(),
                        texture, textureView);
                }
            }
        }
    }

    return hr;
}

DDS_ALPHA_MODE GetAlphaMode(_In_ const DDS_HEADER* header) noexcept
{
    if (header->ddspf.flags & DDS_FOURCC)
    {
        if (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC)
        {
            auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10*>(reinterpret_cast<const uint8_t*>(header) + sizeof(DDS_HEADER));
            auto const mode = static_cast<DDS_ALPHA_MODE>(d3d10ext->miscFlags2 & DDS_MISC_FLAGS2_ALPHA_MODE_MASK);
            switch (mode)
            {
            case DDS_ALPHA_MODE_STRAIGHT:
            case DDS_ALPHA_MODE_PREMULTIPLIED:
            case DDS_ALPHA_MODE_OPAQUE:
            case DDS_ALPHA_MODE_CUSTOM:
                return mode;

            case DDS_ALPHA_MODE_UNKNOWN:
            default:
                break;
            }
        }
        else if ((MAKEFOURCC('D', 'X', 'T', '2') == header->ddspf.fourCC)
            || (MAKEFOURCC('D', 'X', 'T', '4') == header->ddspf.fourCC))
        {
            return DDS_ALPHA_MODE_PREMULTIPLIED;
        }
    }

    return DDS_ALPHA_MODE_UNKNOWN;
}

HRESULT LoadTextureDataFromMemory(
    const uint8_t *ddsData,
    size_t ddsDataSize,
    const DDS_HEADER **header,
    const uint8_t **bitData,
    size_t *bitSize)
{
    if (!header || !bitData || !bitSize)
    {
        return E_POINTER;
    }

    *bitSize = 0;

    if (ddsDataSize > UINT32_MAX)
    {
        return E_FAIL;
    }

    if (ddsDataSize < DDS_MIN_HEADER_SIZE)
    {
        return E_FAIL;
    }

    // DDS files always start with the same magic number ("DDS ")
    auto const dwMagicNumber = *reinterpret_cast<const uint32_t*>(ddsData);
    if (dwMagicNumber != DDS_MAGIC)
    {
        return E_FAIL;
    }

    auto hdr = reinterpret_cast<const DDS_HEADER*>(ddsData + sizeof(uint32_t));

    // Verify header to validate DDS file
    if (hdr->size != sizeof(DDS_HEADER) ||
        hdr->ddspf.size != sizeof(DDS_PIXELFORMAT))
    {
        return E_FAIL;
    }

    // Check for DX10 extension
    bool bDXT10Header = false;
    if ((hdr->ddspf.flags & DDS_FOURCC) &&
        (MAKEFOURCC('D', 'X', '1', '0') == hdr->ddspf.fourCC))
    {
        // Must be long enough for both headers and magic value
        if (ddsDataSize < DDS_DX10_HEADER_SIZE)
        {
            return E_FAIL;
        }

        bDXT10Header = true;
    }

    // setup the pointers in the process request
    *header = hdr;
    auto offset = DDS_MIN_HEADER_SIZE
        + (bDXT10Header ? sizeof(DDS_HEADER_DXT10) : 0u);
    *bitData = ddsData + offset;
    *bitSize = ddsDataSize - offset;

    return S_OK;
}

HRESULT CreateDDSTextureFromMemoryEx(
    ID3D11Device* d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    DDS_LOADER_FLAGS loadFlags,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }
    if (alphaMode)
    {
        *alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    }

    if (!d3dDevice || !ddsData || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    // Validate DDS file in memory
    const DDS_HEADER* header = nullptr;
    const uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    HRESULT hr = LoadTextureDataFromMemory(ddsData, ddsDataSize,
        &header,
        &bitData,
        &bitSize
    );
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CreateTextureFromDDS(d3dDevice, nullptr,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        nullptr, nullptr,
    #endif
        header, bitData, bitSize,
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);
    if (SUCCEEDED(hr))
    {
        if (alphaMode)
            *alphaMode = GetAlphaMode(header);
    }

    return hr;
}

struct handle_closer { void operator()(HANDLE h) noexcept { if (h) CloseHandle(h); } };
using ScopedHandle = std::unique_ptr<void, handle_closer>;
inline HANDLE safe_handle(HANDLE h) noexcept { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

HRESULT LoadTextureDataFromFile(
    _In_z_ const wchar_t* fileName,
    std::unique_ptr<uint8_t[]>& ddsData,
    const DDS_HEADER** header,
    const uint8_t** bitData,
    size_t* bitSize) noexcept
{
    if (!header || !bitData || !bitSize)
    {
        return E_POINTER;
    }

    *bitSize = 0;

    // open the file
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(
        fileName,
        GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
        nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(
        fileName,
        GENERIC_READ, FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
        nullptr)));
#endif

    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Get the file size
    FILE_STANDARD_INFO fileInfo;
    if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // File is too big for 32-bit allocation, so reject read
    if (fileInfo.EndOfFile.HighPart > 0)
    {
        return E_FAIL;
    }

    // Need at least enough data to fill the header and magic number to be a valid DDS
    if (fileInfo.EndOfFile.LowPart < DDS_MIN_HEADER_SIZE)
    {
        return E_FAIL;
    }

    // create enough space for the file data
    ddsData.reset(new (std::nothrow) uint8_t[fileInfo.EndOfFile.LowPart]);
    if (!ddsData)
    {
        return E_OUTOFMEMORY;
    }

    // read the data in
    DWORD bytesRead = 0;
    if (!ReadFile(hFile.get(),
        ddsData.get(),
        fileInfo.EndOfFile.LowPart,
        &bytesRead,
        nullptr
    ))
    {
        ddsData.reset();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (bytesRead < fileInfo.EndOfFile.LowPart)
    {
        ddsData.reset();
        return E_FAIL;
    }

    // DDS files always start with the same magic number ("DDS ")
    auto const dwMagicNumber = *reinterpret_cast<const uint32_t*>(ddsData.get());
    if (dwMagicNumber != DDS_MAGIC)
    {
        ddsData.reset();
        return E_FAIL;
    }

    auto hdr = reinterpret_cast<const DDS_HEADER*>(ddsData.get() + sizeof(uint32_t));

    // Verify header to validate DDS file
    if (hdr->size != sizeof(DDS_HEADER) ||
        hdr->ddspf.size != sizeof(DDS_PIXELFORMAT))
    {
        ddsData.reset();
        return E_FAIL;
    }

    // Check for DX10 extension
    bool bDXT10Header = false;
    if ((hdr->ddspf.flags & DDS_FOURCC) &&
        (MAKEFOURCC('D', 'X', '1', '0') == hdr->ddspf.fourCC))
    {
        // Must be long enough for both headers and magic value
        if (fileInfo.EndOfFile.LowPart < DDS_DX10_HEADER_SIZE)
        {
            ddsData.reset();
            return E_FAIL;
        }

        bDXT10Header = true;
    }

    // setup the pointers in the process request
    *header = hdr;
    auto offset = DDS_MIN_HEADER_SIZE
        + (bDXT10Header ? sizeof(DDS_HEADER_DXT10) : 0u);
    *bitData = ddsData.get() + offset;
    *bitSize = fileInfo.EndOfFile.LowPart - offset;

    return S_OK;
}

HRESULT CreateDDSTextureFromFileEx(
    ID3D11Device* d3dDevice,
    const wchar_t* fileName,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    DDS_LOADER_FLAGS loadFlags,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }
    if (alphaMode)
    {
        *alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    }

    if (!d3dDevice || !fileName || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    const DDS_HEADER* header = nullptr;
    const uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    std::unique_ptr<uint8_t[]> ddsData;
    HRESULT hr = LoadTextureDataFromFile(fileName,
        ddsData,
        &header,
        &bitData,
        &bitSize
    );
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CreateTextureFromDDS(d3dDevice, nullptr,
    #if defined(_XBOX_ONE) && defined(_TITLE)
        nullptr, nullptr,
    #endif
        header, bitData, bitSize,
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        loadFlags,
        texture, textureView);

    if (SUCCEEDED(hr))
    {
        if (alphaMode)
            *alphaMode = GetAlphaMode(header);
    }

    return hr;
}

void os_create_dds_texture_from_memory(
    ID3D11Device *d3dDevice,
    const uint8 *ddsData,
    size_t ddsDataSize,
    ID3D11Resource **texture,
    ID3D11ShaderResourceView **textureView)
{
    CreateDDSTextureFromMemoryEx(
        d3dDevice,
        ddsData,
        ddsDataSize,
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        DDS_LOADER_DEFAULT,
        texture,
        textureView,
        nullptr);
}

void os_create_dds_texture_from_file(
    ID3D11Device *d3dDevice,
    const wchar *szFileName,
    ID3D11Resource **texture,
    ID3D11ShaderResourceView **textureView)
{
    CreateDDSTextureFromFileEx(
        d3dDevice,
        szFileName,
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        DDS_LOADER_DEFAULT,
        texture,
        textureView,
        nullptr);
}
