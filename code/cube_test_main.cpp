#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>
#include <stdint.h>

#include <gj/gj_base.h>
#include <gj/gj_math.h>
#include <gj/win32_platform.h>
#define STB_IMAGE_IMPLEMENTATION
#include <libs/stb/stb_image.h>

#include "cube_test_main.h"
#include "cube_test_parse_obj.h"

#include "vs.h"
#include "ps.h"
#include "skybox_vs.h"
#include "skybox_ps.h"

static void
show_error_and_exit(const char* error)
{
    MessageBoxA(NULL, error, "Error", NULL);
    __debugbreak();
    ExitProcess(1);
}

static void
get_wnd_dimensions(HWND wnd, uint32_t* width, uint32_t* height)
{
    RECT rect;
    GetClientRect(wnd, &rect);
    *width  = rect.right - rect.left;
    *height = rect.bottom - rect.top;
}

static void
d3d11_resize(HWND wnd, D3D11CubeTest* d3d11_cube_test, uint32_t width, uint32_t height)
{
    HRESULT hr;

    if (d3d11_cube_test->render_target_view) d3d11_cube_test->render_target_view->Release();
    if (d3d11_cube_test->depth_buffer) d3d11_cube_test->depth_buffer->Release();
    if (d3d11_cube_test->depth_buffer_view) d3d11_cube_test->depth_buffer_view->Release();
    
    d3d11_cube_test->swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* backbuffer;
    hr = d3d11_cube_test->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer);
    if (FAILED(hr)) { show_error_and_exit("Failed to get backbuffer!"); }
    hr = d3d11_cube_test->device->CreateRenderTargetView(backbuffer, NULL, &d3d11_cube_test->render_target_view);
    if (FAILED(hr)) { show_error_and_exit("Failed to create render target view!"); }
    D3D11_TEXTURE2D_DESC depth_buffer_desc; gj_ZeroStruct(&depth_buffer_desc);
    depth_buffer_desc.Width      = width;
    depth_buffer_desc.Height     = height;
    depth_buffer_desc.MipLevels  = 1;
    depth_buffer_desc.ArraySize  = 1;
    depth_buffer_desc.Format     = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_buffer_desc.SampleDesc = {1, 0};
    depth_buffer_desc.Usage      = D3D11_USAGE_DEFAULT;
    depth_buffer_desc.BindFlags  = D3D11_BIND_DEPTH_STENCIL;
    d3d11_cube_test->device->CreateTexture2D(&depth_buffer_desc, NULL, &d3d11_cube_test->depth_buffer);
    d3d11_cube_test->device->CreateDepthStencilView(d3d11_cube_test->depth_buffer, NULL, &d3d11_cube_test->depth_buffer_view);
    backbuffer->Release();

    d3d11_cube_test->projection_matrix = M4x4_projection_matrix(70.0f, (float)width/(float)height, 0.01f, 100.0f);
}

static ID3D11Buffer*
d3d11_create_vertex_buffer(ID3D11Device* device, void* data, size_t data_size, size_t stride)
{
    ID3D11Buffer* result;
    D3D11_BUFFER_DESC buffer_desc; gj_ZeroStruct(&buffer_desc);
    buffer_desc.ByteWidth = data_size;
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buffer_desc.StructureByteStride = stride;
    D3D11_SUBRESOURCE_DATA _data; gj_ZeroStruct(&_data); _data.pSysMem = data;
    HRESULT hr = device->CreateBuffer(&buffer_desc, &_data, &result);
    if (FAILED(hr)) show_error_and_exit("Failed to create vertex buffer!");
    return result;
}

static ID3D11Buffer*
d3d11_create_index_buffer(ID3D11Device* device, s32* data, const u32 count)
{
    ID3D11Buffer* result;
    D3D11_BUFFER_DESC buffer_desc; gj_ZeroStruct(&buffer_desc);
    buffer_desc.ByteWidth = sizeof(s32) * count;
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    buffer_desc.StructureByteStride = sizeof(s32);
    D3D11_SUBRESOURCE_DATA _data; gj_ZeroStruct(&_data); _data.pSysMem = data;
    HRESULT hr = device->CreateBuffer(&buffer_desc, &_data, &result);
    if (FAILED(hr)) show_error_and_exit("Failed to create index buffer!");
    return result;
}

static D3D11CubeTest
d3d11_init(HWND wnd)
{
    HRESULT hr;
    
    D3D11CubeTest result; gj_ZeroStruct(&result);

    DXGI_SWAP_CHAIN_DESC swap_chain_desc;
    gj_ZeroStruct(&swap_chain_desc);
    swap_chain_desc.BufferDesc.RefreshRate = {1, 60};
    swap_chain_desc.BufferDesc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.SampleDesc             = {1, 0};
    swap_chain_desc.BufferUsage            = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount            = 2;
    swap_chain_desc.OutputWindow           = wnd;
    swap_chain_desc.Windowed               = TRUE;
    swap_chain_desc.SwapEffect             = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT device_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if defined(CUBE_TEST_DEBUG)
    device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        device_flags,
        NULL, 0,
        D3D11_SDK_VERSION,
        &swap_chain_desc,
        &result.swap_chain,
        &result.device,
        NULL,
        &result.device_context);
    if (FAILED(hr)) { show_error_and_exit("Failed to create device and swapchain!"); }

    ID3D11InfoQueue* d3d11_info_queue;
    hr = result.device->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3d11_info_queue);
    if (FAILED(hr)) show_error_and_exit("Failed to query info queue!");
    d3d11_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
    d3d11_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    d3d11_info_queue->Release();

    {
        D3D11_RASTERIZER_DESC rasterizer_desc; gj_ZeroStruct(&rasterizer_desc);
        rasterizer_desc.FillMode = D3D11_FILL_SOLID;
        rasterizer_desc.CullMode = D3D11_CULL_BACK;
        result.device->CreateRasterizerState(&rasterizer_desc, &result.rasterizer_state);
    }
    
    D3D11_DEPTH_STENCIL_DESC depth_stencil_desc; gj_ZeroStruct(&depth_stencil_desc);
    depth_stencil_desc.DepthEnable    = true;
    depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depth_stencil_desc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    result.device->CreateDepthStencilState(&depth_stencil_desc, &result.depth_stencil_state);
        
    hr = result.device->CreateVertexShader(VS_BYTES, sizeof(VS_BYTES), NULL, &result.vertex_shader);
    if (FAILED(hr)) show_error_and_exit("Failed to create vertex shader!");
    hr = result.device->CreatePixelShader(PS_BYTES, sizeof(PS_BYTES), NULL, &result.pixel_shader);
    if (FAILED(hr)) show_error_and_exit("Failed to create pixel shader!");
    
    get_wnd_dimensions(wnd, &result.width, &result.height);
    d3d11_resize(wnd, &result, result.width, result.height);

    D3D11_BUFFER_DESC constant_buffer_desc; gj_ZeroStruct(&constant_buffer_desc);
    constant_buffer_desc.ByteWidth      = sizeof(M4x4) * 2;
    constant_buffer_desc.Usage          = D3D11_USAGE_DYNAMIC;
    constant_buffer_desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result.device->CreateBuffer(&constant_buffer_desc, NULL, &result.constant_buffer);

    // Sphere
    {
        D3D11_INPUT_ELEMENT_DESC input_elem_descs[] = {
            {"POS",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        result.device->CreateInputLayout(input_elem_descs, 3, VS_BYTES, sizeof(VS_BYTES), &result.input_layout);

        u32 position_count = 0;
        u32 normal_count   = 0;
        u32 uv_count       = 0;
        u32 index_count    = 0;
        V3f* positions        = (V3f*)g_platform_api.allocate_memory(2 * 482 * sizeof(V3f));
        s32* position_indices = (s32*)g_platform_api.allocate_memory(8 * 512 * sizeof(s32));
        V3f* normals          = (V3f*)g_platform_api.allocate_memory(2 * 512 * sizeof(V3f));
        s32* normal_indices   = (s32*)g_platform_api.allocate_memory(8 * 512 * sizeof(s32));
        V2f* uvs              = (V2f*)g_platform_api.allocate_memory(2 * 559 * sizeof(V2f));
        s32* uv_indices       = (s32*)g_platform_api.allocate_memory(8 * 512 * sizeof(s32));
        ctpo_parse("obj/sphere.obj",
                   positions, &position_count, position_indices,
                   normals,   &normal_count,   normal_indices,
                   uvs,       &uv_count,       uv_indices,
                   &index_count);

        const u32 vertices_size = sizeof(SphereVertex) * index_count;
        SphereVertex* vertices = (SphereVertex*)g_platform_api.allocate_memory(vertices_size);
        for (u32 index = 0;
             index < index_count;
             index++)
        {
            vertices[index].pos    = positions[position_indices[index]];
            vertices[index].normal = normals[normal_indices[index]];
            vertices[index].uv     = uvs[uv_indices[index]];
        }    
        result.sphere_vertex_buffer = d3d11_create_vertex_buffer(result.device, vertices, vertices_size, sizeof(SphereVertex));
        result.sphere_vertex_count = index_count;
        
        g_platform_api.deallocate_memory(positions);
        g_platform_api.deallocate_memory(position_indices);
        g_platform_api.deallocate_memory(normals);
        g_platform_api.deallocate_memory(normal_indices);
        g_platform_api.deallocate_memory(uvs);
        g_platform_api.deallocate_memory(uv_indices);
    }
    
    // Skybox
    {
        result.device->CreateVertexShader(VSSkybox_BYTES, sizeof(VSSkybox_BYTES), NULL, &result.skybox_vertex_shader);
        result.device->CreatePixelShader(PSSkybox_BYTES, sizeof(PSSkybox_BYTES), NULL, &result.skybox_pixel_shader);

        D3D11_SAMPLER_DESC cube_sampler_desc = {};
        cube_sampler_desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
        cube_sampler_desc.AddressU       = D3D11_TEXTURE_ADDRESS_BORDER;
        cube_sampler_desc.AddressV       = D3D11_TEXTURE_ADDRESS_BORDER;
        cube_sampler_desc.AddressW       = D3D11_TEXTURE_ADDRESS_BORDER;
        cube_sampler_desc.BorderColor[0] = 1.0f;
        cube_sampler_desc.BorderColor[1] = 1.0f;
        cube_sampler_desc.BorderColor[2] = 1.0f;
        cube_sampler_desc.BorderColor[3] = 1.0f;
        cube_sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        result.device->CreateSamplerState(&cube_sampler_desc, &result.skybox_sampler_state);

        s32 skybox_png_width;
        s32 skybox_png_height;
        s32 skybox_png_channels;
        stbi_set_flip_vertically_on_load(true);
        D3D11_SUBRESOURCE_DATA xneg, xpos, yneg, ypos, zneg, zpos;
        xneg.pSysMem = stbi_load("skybox/xneg.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        xpos.pSysMem = stbi_load("skybox/xpos.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        yneg.pSysMem = stbi_load("skybox/yneg.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        ypos.pSysMem = stbi_load("skybox/ypos.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        zneg.pSysMem = stbi_load("skybox/zneg.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        zpos.pSysMem = stbi_load("skybox/zpos.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        gj_Assert(skybox_png_channels == 4);
        xneg.SysMemPitch = xpos.SysMemPitch = yneg.SysMemPitch = ypos.SysMemPitch = zneg.SysMemPitch = zpos.SysMemPitch = skybox_png_width * sizeof(byte) * skybox_png_channels;
    
        D3D11_TEXTURE2D_DESC cube_map_desc;
        cube_map_desc.Width              = skybox_png_width;
        cube_map_desc.Height             = skybox_png_height;
        cube_map_desc.MipLevels          = 1;
        cube_map_desc.ArraySize          = 6;
        cube_map_desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
        cube_map_desc.CPUAccessFlags     = 0;
        cube_map_desc.SampleDesc.Count   = 1;
        cube_map_desc.SampleDesc.Quality = 0;
        cube_map_desc.Usage              = D3D11_USAGE_DEFAULT;
        cube_map_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
        cube_map_desc.CPUAccessFlags     = 0;
        cube_map_desc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE;
        D3D11_SUBRESOURCE_DATA skybox_texture_data[] = {xneg, xpos, yneg, ypos, zneg, zpos};
        result.device->CreateTexture2D(&cube_map_desc, skybox_texture_data, &result.skybox_texture);
    
        D3D11_SHADER_RESOURCE_VIEW_DESC skybox_texture_view_desc;
        skybox_texture_view_desc.Format                      = cube_map_desc.Format;
        skybox_texture_view_desc.ViewDimension               = D3D11_SRV_DIMENSION_TEXTURECUBE;
        skybox_texture_view_desc.TextureCube.MipLevels       = cube_map_desc.MipLevels;
        skybox_texture_view_desc.TextureCube.MostDetailedMip = 0;
        result.device->CreateShaderResourceView(result.skybox_texture, &skybox_texture_view_desc, &result.skybox_texture_view);
    
        stbi_image_free((void*)xneg.pSysMem);
        stbi_image_free((void*)xpos.pSysMem);
        stbi_image_free((void*)yneg.pSysMem);
        stbi_image_free((void*)ypos.pSysMem);
        stbi_image_free((void*)zneg.pSysMem);
        stbi_image_free((void*)zpos.pSysMem);

        D3D11_INPUT_ELEMENT_DESC input_elem_descs[] = {
            {"POS",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        result.device->CreateInputLayout(input_elem_descs, 1, VSSkybox_BYTES, sizeof(VSSkybox_BYTES), &result.skybox_input_layout);

        float skybox_vertices[] = {
            -1.0f, -1.0f, -1.0f, // 0
             1.0f, -1.0f, -1.0f, // 1
            -1.0f,  1.0f, -1.0f, // 2
             1.0f,  1.0f, -1.0f, // 3
            -1.0f, -1.0f,  1.0f, // 4
             1.0f, -1.0f,  1.0f, // 5
            -1.0f,  1.0f,  1.0f, // 6
             1.0f,  1.0f,  1.0f, // 7
        };
        result.skybox_vertex_buffer = d3d11_create_vertex_buffer(result.device, skybox_vertices, sizeof(skybox_vertices), sizeof(V3f));

        s32 skybox_indices[] = {
            0, 3, 2,
            0, 1, 3,

            0, 2, 6,
            0, 6, 4,

            0, 5, 1,
            0, 4, 5,

            7, 1, 5,
            7, 3, 1,

            7, 4, 6,
            7, 5, 4,

            7, 2, 3,
            7, 6, 2,
        };
        result.skybox_index_count = gj_ArrayCount(skybox_indices);
        result.skybox_index_buffer = d3d11_create_index_buffer(result.device, skybox_indices, result.skybox_index_count);
    }
    return result;
}

static void
d3d11_render(HWND wnd, D3D11CubeTest* d3d11_cube_test)
{
    uint32_t width, height;
    get_wnd_dimensions(wnd, &width, &height);
    if (width != d3d11_cube_test->width || height != d3d11_cube_test->height)
    {
        d3d11_resize(wnd, d3d11_cube_test, width, height);
    }
    
    D3D11_VIEWPORT d3d11_viewport;
    d3d11_viewport.TopLeftX = 0.0f;
    d3d11_viewport.TopLeftY = 0.0f;
    d3d11_viewport.Width    = width;
    d3d11_viewport.Height   = height;
    d3d11_viewport.MinDepth = 0.0f;
    d3d11_viewport.MaxDepth = 1.0f;
        
    d3d11_cube_test->device_context->RSSetViewports(1, &d3d11_viewport);
    float clear_color[] = {1.0f, 0.0f, 0.0f, 1.0f};
    d3d11_cube_test->device_context->ClearRenderTargetView(d3d11_cube_test->render_target_view, clear_color);
    d3d11_cube_test->device_context->ClearDepthStencilView(d3d11_cube_test->depth_buffer_view, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0.0f);
    d3d11_cube_test->device_context->OMSetRenderTargets(1, &d3d11_cube_test->render_target_view, d3d11_cube_test->depth_buffer_view);
    d3d11_cube_test->device_context->OMSetDepthStencilState(d3d11_cube_test->depth_stencil_state, 0);
    d3d11_cube_test->device_context->RSSetState(d3d11_cube_test->rasterizer_state);
    
    D3D11_MAPPED_SUBRESOURCE constant_buffer_data; gj_ZeroStruct(&constant_buffer_data);
    d3d11_cube_test->device_context->Map(d3d11_cube_test->constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constant_buffer_data);
    memcpy(constant_buffer_data.pData,                       d3d11_cube_test->projection_matrix.a, sizeof(M4x4));
    memcpy((byte*)constant_buffer_data.pData + sizeof(M4x4), d3d11_cube_test->model_view_matrix.a, sizeof(M4x4));
    d3d11_cube_test->device_context->Unmap(d3d11_cube_test->constant_buffer, 0);
    
    // Sphere
    {
        ID3D11Buffer* vertex_buffers[] = {d3d11_cube_test->sphere_vertex_buffer};
        UINT strides[] = {sizeof(SphereVertex)};
        UINT offsets[] = {0};
        
        d3d11_cube_test->device_context->IASetVertexBuffers(0, 1, vertex_buffers, strides, offsets);
        d3d11_cube_test->device_context->IASetInputLayout(d3d11_cube_test->input_layout);
        d3d11_cube_test->device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        d3d11_cube_test->device_context->VSSetShader(d3d11_cube_test->vertex_shader, NULL, 0);
        d3d11_cube_test->device_context->PSSetShader(d3d11_cube_test->pixel_shader, NULL, 0);
        d3d11_cube_test->device_context->VSSetConstantBuffers(0, 1, &d3d11_cube_test->constant_buffer);

        d3d11_cube_test->device_context->Draw(d3d11_cube_test->sphere_vertex_count, 0);
    }
    
    // Skybox
    {
        ID3D11Buffer* vertex_buffers[] = {d3d11_cube_test->skybox_vertex_buffer};
        UINT strides[] = {sizeof(V3f)};
        UINT offsets[] = {0};

        d3d11_cube_test->device_context->IASetVertexBuffers(0, 1, vertex_buffers, strides, offsets);
        d3d11_cube_test->device_context->IASetIndexBuffer(d3d11_cube_test->skybox_index_buffer, DXGI_FORMAT_R32_UINT, 0);
        d3d11_cube_test->device_context->IASetInputLayout(d3d11_cube_test->skybox_input_layout);
        d3d11_cube_test->device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        d3d11_cube_test->device_context->VSSetShader(d3d11_cube_test->skybox_vertex_shader, NULL, 0);
        d3d11_cube_test->device_context->PSSetShader(d3d11_cube_test->skybox_pixel_shader, NULL, 0);
        d3d11_cube_test->device_context->VSSetConstantBuffers(0, 1, &d3d11_cube_test->constant_buffer);
        d3d11_cube_test->device_context->PSSetSamplers(0, 1, &d3d11_cube_test->skybox_sampler_state);
        d3d11_cube_test->device_context->PSSetShaderResources(0, 1, &d3d11_cube_test->skybox_texture_view);

        d3d11_cube_test->device_context->DrawIndexed(d3d11_cube_test->skybox_index_count, 0, 0);
    }
    
    d3d11_cube_test->swap_chain->Present(1, 0);
}

static void
center_and_get_mouse_pos(HWND wnd, uint32_t* x, uint32_t* y)
{
    POINT mouse_pos;
    GetCursorPos(&mouse_pos);
    RECT wnd_rect;
    GetWindowRect(wnd, &wnd_rect);
    RECT client_rect;
    GetClientRect(wnd, &client_rect);
    *x = client_rect.right / 2 + wnd_rect.left;
    *y = client_rect.bottom / 2 + wnd_rect.top;
    SetCursorPos(*x, *y);
}

DWORD WINAPI
main_thread_proc(LPVOID _param)
{
    HRESULT hr;
    
    HWND wnd = (HWND)_param;
    ShowWindow(wnd, SW_SHOWNORMAL);
    SetActiveWindow(wnd);
    SetForegroundWindow(wnd);

    win32_init_platform_api();
    
    D3D11CubeTest d3d11_cube_test = d3d11_init(wnd);

    uint32_t width, height;
    get_wnd_dimensions(wnd, &width, &height);
    while (true)
    {
        float move_delta_x = 0.0f;
        float move_delta_y = 0.0f;
        
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            switch (msg.message)
            {
                case WM_QUIT:
                {
                    d3d11_cube_test.device_context->Flush();
                    ExitProcess(0);
                    return 0;
                } break;

                case WM_KEYDOWN:
                {
                    switch (msg.wParam)
                    {
                        case 'W': move_delta_y += 0.1f; break;
                        case 'A': move_delta_x -= 0.1f; break;
                        case 'S': move_delta_y -= 0.1f; break;
                        case 'D': move_delta_x += 0.1f; break;
                    }
                } break;
            }
        }
        
        uint32_t new_mouse_x, new_mouse_y;
        POINT mouse_pos; GetCursorPos(&mouse_pos);
        new_mouse_x = mouse_pos.x;
        new_mouse_y = mouse_pos.y;
        uint32_t mouse_x, mouse_y;
        center_and_get_mouse_pos(wnd, &mouse_x, &mouse_y);
       
        d3d11_cube_test.yaw   -= (float)new_mouse_x - (float)mouse_x;
        d3d11_cube_test.pitch -= (float)new_mouse_y - (float)mouse_y;
        d3d11_cube_test.pitch = clamp(-89.0f, d3d11_cube_test.pitch, 89.0f);
        V3f fps_forward = V3f_fps_forward_vector(d3d11_cube_test.yaw * DEG_TO_RAD, d3d11_cube_test.pitch * DEG_TO_RAD);
        
        V3f forward_movement = fps_forward;
        forward_movement = V3_normalize(forward_movement);
        forward_movement = V3_mul(move_delta_y, forward_movement);
        V3f left_movement = V3_cross(V3_normalize(fps_forward), {0.0f, 1.0f, 0.0f});
        left_movement.y = 0.0f;
        left_movement = V3_mul(-move_delta_x, left_movement);
        d3d11_cube_test.camera_pos = V3_add(d3d11_cube_test.camera_pos, V3_add(forward_movement, left_movement));
        
        d3d11_cube_test.model_view_matrix = M4x4_model_view_matrix(
            d3d11_cube_test.camera_pos,
            fps_forward,
            {0.0f, 1.0f, 0.0f});
        
        d3d11_render(wnd, &d3d11_cube_test);
    }
    
    return 0;
}

LRESULT CALLBACK
wndproc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT result = DefWindowProcA(wnd, msg, wp, lp);
    switch (msg)
    {
        case WM_CLOSE:
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;

        case WM_ACTIVATEAPP:
        {
            if (wp == TRUE)
            {
                // g_has_focus = true;
                SetLayeredWindowAttributes(wnd, RGB(0, 0, 0), 255, LWA_ALPHA);
            }
            else
            {
                // g_has_focus = false;
                SetLayeredWindowAttributes(wnd, RGB(0, 0, 0), 64, LWA_ALPHA);
            }
        }
        break;
    }
    return result;
}

INT WINAPI
WinMain(HINSTANCE _inst, HINSTANCE, PSTR, INT)
{
    const char* wndclass_name = "CubeTestClass";
    WNDCLASSEXA wndclass;
    gj_ZeroStruct(&wndclass);
    wndclass.cbSize        = sizeof(WNDCLASSEXA);
    wndclass.lpfnWndProc   = wndproc;
    wndclass.hInstance     = _inst;
    wndclass.hCursor       = LoadCursorA(0, IDC_ARROW);
    wndclass.lpszClassName = wndclass_name;
    RegisterClassExA(&wndclass);
    HWND wnd = CreateWindowExA(
        0,
        wndclass_name,
        "CubeTest",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, _inst, NULL);
    if (!wnd) show_error_and_exit("Failed to create window!");
    
    DWORD main_thread_id;
    HANDLE main_thread_handle = CreateThread(NULL, 0, main_thread_proc, wnd, 0, &main_thread_id);

    while (true)
    {
        MSG msg;
        GetMessage(&msg, NULL, 0, 0);
        TranslateMessage(&msg);
        switch (msg.message)
        {
            case WM_QUIT:
            case WM_KEYDOWN:
            {
                PostThreadMessageA(main_thread_id, msg.message, msg.wParam, msg.lParam);
            } break;

            default:
            {
                DispatchMessage(&msg);
            } break;
        }
    }
    
    return 0;
}
