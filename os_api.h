#pragma once
#include "global_types.h"
#include "text_layout.h"
#include "window.h"
#include "file.h"
#include "network.h"
#include <d3d11_1.h>

void os_create_window(window *wnd);
void os_destroy_window(window *wnd);
void os_open_window(window *wnd);
void os_resize_window(window *wnd, uint32 width, uint32 height);
void os_update_window_size(window *wnd);
vector<int32, 2> os_window_content_position(window *wnd);
vector<uint32, 2> os_window_content_size(window *wnd);
vector<uint32, 2> os_screen_size();
void os_message_loop();
void os_window_render_buffer(window *wnd, void **bits);
void os_render_window(window *wnd);
bool os_load_glyph(glyph_data *data);
timestamp os_current_timestamp();
void os_copy_text_to_clipboard(u16string &text);
void os_copy_text_from_clipboard(u16string *text);
void os_update_internal_timer();
void os_update_windows();
bool os_filename_exists(u16string &filename);
void os_open_file(file *f);
void os_close_file(file *f);
void os_resize_file(uint64 size, file *f);
uint64 os_read_file(file *f, uint64 size, void *addr);
uint64 os_write_file(file *f, void *addr, uint64 size);
bool os_delete_file(file *f, u16string &filename);
bool os_move_file(file *f, u16string &new_path);
void os_regiser_web_server(network_server *ws);
void os_unregister_web_server(network_server *ws);
bool os_load_image(u16string &filename, bitmap *bmp);
void os_create_texture_from_memory(
    ID3D11Device *d3dDevice,
    const uint8 *wicData,
    size_t wicDataSize,
    ID3D11Resource **texture,
    ID3D11ShaderResourceView **textureView);
void os_create_texture_from_file(
    ID3D11Device *d3dDevice,
    const wchar *szFileName,
    ID3D11Resource **texture,
    ID3D11ShaderResourceView **textureView);
void os_create_dds_texture_from_memory(
    ID3D11Device *d3dDevice,
    const uint8 *ddsData,
    size_t ddsDataSize,
    ID3D11Resource **texture,
    ID3D11ShaderResourceView **textureView);
void os_create_dds_texture_from_file(
    ID3D11Device *d3dDevice,
    const wchar *szFileName,
    ID3D11Resource **texture,
    ID3D11ShaderResourceView **textureView);
