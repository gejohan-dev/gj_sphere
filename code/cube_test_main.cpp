#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>
#include <stdint.h>

#include <gj/gj_base.h>
#include <gj/gj_math.h>
#include <gj/win32_platform.h>
#define STB_IMAGE_IMPLEMENTATION
#include <libs/stb/stb_image.h>
#if USE_IMGUI
#include <libs/imgui/imgui.h>
#include <libs/imgui/backends/imgui_impl_win32.h>
#include <libs/imgui/backends/imgui_impl_dx11.h>
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#include "cube_test_main.h"
#include "cube_test_parse_obj.h"

#include "vs.h"
#include "ps.h"
#include "shadow_map_vs.h"
#include "shadow_map_ps.h"
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

    // Backbuffer color/depth/stencil
    {
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
    }

    // Shadow map
    {
        D3D11_TEXTURE2D_DESC shadow_map_buffer_desc; gj_ZeroStruct(&shadow_map_buffer_desc);
        shadow_map_buffer_desc.Width      = width;
        shadow_map_buffer_desc.Height     = height;
        shadow_map_buffer_desc.MipLevels  = 1;
        shadow_map_buffer_desc.ArraySize  = 1;
        shadow_map_buffer_desc.Format     = DXGI_FORMAT_R32_TYPELESS;
        shadow_map_buffer_desc.SampleDesc = {1, 0};
        shadow_map_buffer_desc.Usage      = D3D11_USAGE_DEFAULT;
        shadow_map_buffer_desc.BindFlags  = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        d3d11_cube_test->device->CreateTexture2D(&shadow_map_buffer_desc, NULL, &d3d11_cube_test->shadow_map_render_target);

        D3D11_DEPTH_STENCIL_VIEW_DESC shadow_map_view_desc; gj_ZeroStruct(&shadow_map_view_desc);
        shadow_map_view_desc.Format        = DXGI_FORMAT_D32_FLOAT;
        shadow_map_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        d3d11_cube_test->device->CreateDepthStencilView(d3d11_cube_test->shadow_map_render_target, &shadow_map_view_desc, &d3d11_cube_test->shadow_map_render_target_view);

        D3D11_SHADER_RESOURCE_VIEW_DESC shadow_map_texture_view_desc;
        shadow_map_texture_view_desc.Format                      = DXGI_FORMAT_R32_FLOAT;
        shadow_map_texture_view_desc.ViewDimension               = D3D11_SRV_DIMENSION_TEXTURE2D;
        shadow_map_texture_view_desc.TextureCube.MipLevels       = shadow_map_buffer_desc.MipLevels;
        shadow_map_texture_view_desc.TextureCube.MostDetailedMip = 0;
        d3d11_cube_test->device->CreateShaderResourceView(d3d11_cube_test->shadow_map_render_target, &shadow_map_texture_view_desc, &d3d11_cube_test->shadow_map_texture_view);
    }
    
    d3d11_cube_test->width = width;
    d3d11_cube_test->height = height;

    f32 aspect_w_over_h = (float)width/(float)height;
    d3d11_cube_test->world_projection_matrix = M4x4_projection_matrix(70.0f, aspect_w_over_h, 0.01f, 100.0f);
    // d3d11_cube_test->light_projection_matrix = M4x4_projection_matrix(70.0f, (float)width/(float)height, 0.01f, 100.0f);
    // d3d11_cube_test->light_projection_matrix = M4x4_orthographic_matrix(-20.0f, 20.0f, 20.0f, -20.0f, 1.0f, 50.0f);
    d3d11_cube_test->light_projection_matrix = M4x4_orthographic_matrix(aspect_w_over_h, 1.0f, 50.0f);
}

static ID3D11Buffer*
d3d11_create_vertex_buffer(ID3D11Device* device, void* data, size_t data_size, size_t stride)
{
    ID3D11Buffer* result;
    D3D11_BUFFER_DESC buffer_desc; gj_ZeroStruct(&buffer_desc);
    buffer_desc.ByteWidth           = data_size;
    buffer_desc.Usage               = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
    buffer_desc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
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

static void
d3d11_load_texture(ID3D11Device* device, const char* texture_filename,
                   ID3D11Texture2D** texture, ID3D11ShaderResourceView** texture_view)
{
    s32 texture_width;
    s32 texture_height;
    s32 texture_channels;
    D3D11_SUBRESOURCE_DATA texture_data;
    texture_data.pSysMem = stbi_load(texture_filename, &texture_width, &texture_height, &texture_channels, 4);
    gj_Assert(texture_channels == 4);
    texture_data.SysMemPitch = texture_width * texture_channels;

    D3D11_TEXTURE2D_DESC texture_desc;
    texture_desc.Width              = texture_width;
    texture_desc.Height             = texture_height;
    texture_desc.MipLevels          = 1;
    texture_desc.ArraySize          = 1;
    texture_desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_desc.CPUAccessFlags     = 0;
    texture_desc.SampleDesc.Count   = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Usage              = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    texture_desc.CPUAccessFlags     = 0;
    texture_desc.MiscFlags          = 0;
    device->CreateTexture2D(&texture_desc, &texture_data, texture);
    
    D3D11_SHADER_RESOURCE_VIEW_DESC texture_view_desc;
    texture_view_desc.Format                      = texture_desc.Format;
    texture_view_desc.ViewDimension               = D3D11_SRV_DIMENSION_TEXTURE2D;
    texture_view_desc.TextureCube.MipLevels       = texture_desc.MipLevels;
    texture_view_desc.TextureCube.MostDetailedMip = 0;
    device->CreateShaderResourceView(*texture, &texture_view_desc, texture_view);

    stbi_image_free((void*)texture_data.pSysMem);
}

static void
d3d11_load_mesh(D3D11CubeTest* d3d11_cube_test,
                const char* obj_filename, const char* color_texture_filename, const char* normal_texture_filename,
                ID3D11Buffer** vertex_buffer, uint32_t* vertex_count,
                ID3D11Texture2D** color_texture, ID3D11ShaderResourceView** color_texture_view,
                ID3D11Texture2D** normal_texture, ID3D11ShaderResourceView** normal_texture_view)
{
    const uint32_t buffer_size = 600000;
    u32 position_count = 0;
    u32 normal_count   = 0;
    u32 uv_count       = 0;
    u32 index_count    = 0;
    V3f* positions        = (V3f*)g_platform_api.allocate_memory(buffer_size * sizeof(V3f));
    s32* position_indices = (s32*)g_platform_api.allocate_memory(buffer_size * sizeof(s32));
    V3f* normals          = (V3f*)g_platform_api.allocate_memory(buffer_size * sizeof(V3f));
    s32* normal_indices   = (s32*)g_platform_api.allocate_memory(buffer_size * sizeof(s32));
    V2f* uvs              = (V2f*)g_platform_api.allocate_memory(buffer_size * sizeof(V2f));
    s32* uv_indices       = (s32*)g_platform_api.allocate_memory(buffer_size * sizeof(s32));
    ctpo_parse(obj_filename,
               positions, &position_count, position_indices,
               normals,   &normal_count,   normal_indices,
               uvs,       &uv_count,       uv_indices,
               &index_count);

    const u32 vertices_size = sizeof(MeshVertex) * index_count;
    MeshVertex* vertices = (MeshVertex*)g_platform_api.allocate_memory(vertices_size);
    for (u32 index = 0;
         index < index_count;
         index++)
    {
        vertices[index].pos    = positions[position_indices[index]];
        vertices[index].normal = normals[normal_indices[index]];
        vertices[index].uv     = uvs[uv_indices[index]];
    }    
    *vertex_buffer = d3d11_create_vertex_buffer(d3d11_cube_test->device, vertices, vertices_size, sizeof(MeshVertex));
    *vertex_count = index_count;
        
    g_platform_api.deallocate_memory(positions);
    g_platform_api.deallocate_memory(position_indices);
    g_platform_api.deallocate_memory(normals);
    g_platform_api.deallocate_memory(normal_indices);
    g_platform_api.deallocate_memory(uvs);
    g_platform_api.deallocate_memory(uv_indices);

    d3d11_load_texture(d3d11_cube_test->device, color_texture_filename,  color_texture, color_texture_view);
    d3d11_load_texture(d3d11_cube_test->device, normal_texture_filename, normal_texture, normal_texture_view);
}

static void
d3d11_init(HWND wnd, D3D11CubeTest* d3d11_cube_test)
{
    HRESULT hr;

    // SwapChain, Device, DeviceContext
    {
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
            &d3d11_cube_test->swap_chain,
            &d3d11_cube_test->device,
            NULL,
            &d3d11_cube_test->device_context);
        if (FAILED(hr)) { show_error_and_exit("Failed to create device and swapchain!"); }
    }

#if defined(CUBE_TEST_DEBUG)
    // Break on errors
    {
        ID3D11InfoQueue* d3d11_info_queue;
        hr = d3d11_cube_test->device->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3d11_info_queue);
        if (FAILED(hr)) show_error_and_exit("Failed to query info queue!");
        d3d11_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
        d3d11_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        d3d11_info_queue->Release();
    }
#endif
    
    // RasterizerState
    {
        D3D11_RASTERIZER_DESC rasterizer_desc; gj_ZeroStruct(&rasterizer_desc);
        rasterizer_desc.FillMode = D3D11_FILL_SOLID;
        rasterizer_desc.CullMode = D3D11_CULL_BACK;
        d3d11_cube_test->device->CreateRasterizerState(&rasterizer_desc, &d3d11_cube_test->rasterizer_state);
    }

#if defined(CUBE_TEST_DEBUG)
    // Wireframe RasterizerState
    {
        D3D11_RASTERIZER_DESC wireframe_rasterizer_desc; gj_ZeroStruct(&wireframe_rasterizer_desc);
        wireframe_rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
        wireframe_rasterizer_desc.CullMode = D3D11_CULL_NONE;
        d3d11_cube_test->device->CreateRasterizerState(&wireframe_rasterizer_desc, &d3d11_cube_test->wireframe_rasterizer_state);
    }
#endif

    // DepthStencilState
    {
        D3D11_DEPTH_STENCIL_DESC depth_stencil_desc; gj_ZeroStruct(&depth_stencil_desc);
        depth_stencil_desc.DepthEnable    = true;
        depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depth_stencil_desc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
        d3d11_cube_test->device->CreateDepthStencilState(&depth_stencil_desc, &d3d11_cube_test->depth_stencil_state);
    }
    
    // Vertex/Pixel shader for meshes
    {
        hr = d3d11_cube_test->device->CreateVertexShader(VS_BYTES, sizeof(VS_BYTES), NULL, &d3d11_cube_test->vertex_shader);
        if (FAILED(hr)) show_error_and_exit("Failed to create vertex shader!");
        hr = d3d11_cube_test->device->CreatePixelShader(PS_BYTES, sizeof(PS_BYTES), NULL, &d3d11_cube_test->pixel_shader);
        if (FAILED(hr)) show_error_and_exit("Failed to create pixel shader!");

        D3D11_INPUT_ELEMENT_DESC input_elem_descs[] = {
            {"POS",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,               D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(V3f),     D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(V3f) * 2, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        d3d11_cube_test->device->CreateInputLayout(input_elem_descs, gj_ArrayCount(input_elem_descs), VS_BYTES, sizeof(VS_BYTES), &d3d11_cube_test->input_layout);
    }

    // Create constant buffer used globally by all shaders
    {
        D3D11_BUFFER_DESC constant_buffer_desc; gj_ZeroStruct(&constant_buffer_desc);
        constant_buffer_desc.ByteWidth      = sizeof(M4x4) * 4;
        constant_buffer_desc.Usage          = D3D11_USAGE_DYNAMIC;
        constant_buffer_desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        d3d11_cube_test->device->CreateBuffer(&constant_buffer_desc, NULL, &d3d11_cube_test->constant_buffer);
    }
    
    // Sphere
    d3d11_load_mesh(d3d11_cube_test,
                    "obj/sphere.obj", "textures/snow_color.png", "textures/snow_normal.png",
                    &d3d11_cube_test->sphere_mesh.vertex_buffer, &d3d11_cube_test->sphere_mesh.vertex_count,
                    &d3d11_cube_test->sphere_mesh.color_texture, &d3d11_cube_test->sphere_mesh.color_texture_view,
                    &d3d11_cube_test->sphere_mesh.normal_texture, &d3d11_cube_test->sphere_mesh.normal_texture_view);
    // Plane
    d3d11_load_mesh(d3d11_cube_test, "obj/plane.obj", "textures/rock_color.png", "textures/rock_normal.png",
                    &d3d11_cube_test->plane_mesh.vertex_buffer, &d3d11_cube_test->plane_mesh.vertex_count,
                    &d3d11_cube_test->plane_mesh.color_texture, &d3d11_cube_test->plane_mesh.color_texture_view,
                    &d3d11_cube_test->plane_mesh.normal_texture, &d3d11_cube_test->plane_mesh.normal_texture_view);

    // Shadow Map Shader
    {
        d3d11_cube_test->device->CreateVertexShader(VSShadowMap_BYTES, sizeof(VSShadowMap_BYTES), NULL, &d3d11_cube_test->shadow_map_vertex_shader);
        d3d11_cube_test->device->CreatePixelShader(PSShadowMap_BYTES, sizeof(PSShadowMap_BYTES), NULL, &d3d11_cube_test->shadow_map_pixel_shader);
    }
    
    // Skybox
    {
        d3d11_cube_test->device->CreateVertexShader(VSSkybox_BYTES, sizeof(VSSkybox_BYTES), NULL, &d3d11_cube_test->skybox_vertex_shader);
        d3d11_cube_test->device->CreatePixelShader(PSSkybox_BYTES, sizeof(PSSkybox_BYTES), NULL, &d3d11_cube_test->skybox_pixel_shader);

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
        d3d11_cube_test->device->CreateSamplerState(&cube_sampler_desc, &d3d11_cube_test->sampler_state);

        s32 skybox_png_width;
        s32 skybox_png_height;
        s32 skybox_png_channels;
        D3D11_SUBRESOURCE_DATA xneg, xpos, yneg, ypos, zneg, zpos;
        xneg.pSysMem = stbi_load("skybox/xneg.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        xpos.pSysMem = stbi_load("skybox/xpos.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        yneg.pSysMem = stbi_load("skybox/yneg.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        ypos.pSysMem = stbi_load("skybox/ypos.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        zneg.pSysMem = stbi_load("skybox/zneg.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        zpos.pSysMem = stbi_load("skybox/zpos.png", &skybox_png_width, &skybox_png_height, &skybox_png_channels, 4);
        gj_Assert(skybox_png_channels == 4);
        xneg.SysMemPitch = xpos.SysMemPitch = yneg.SysMemPitch = ypos.SysMemPitch = zneg.SysMemPitch = zpos.SysMemPitch = skybox_png_width * sizeof(byte) * skybox_png_channels;
    
        D3D11_TEXTURE2D_DESC skybox_desc;
        skybox_desc.Width              = skybox_png_width;
        skybox_desc.Height             = skybox_png_height;
        skybox_desc.MipLevels          = 1;
        skybox_desc.ArraySize          = 6;
        skybox_desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
        skybox_desc.CPUAccessFlags     = 0;
        skybox_desc.SampleDesc.Count   = 1;
        skybox_desc.SampleDesc.Quality = 0;
        skybox_desc.Usage              = D3D11_USAGE_DEFAULT;
        skybox_desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
        skybox_desc.CPUAccessFlags     = 0;
        skybox_desc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE;
        D3D11_SUBRESOURCE_DATA skybox_texture_data[] = {xpos, xneg, ypos, yneg, zpos, zneg};
        d3d11_cube_test->device->CreateTexture2D(&skybox_desc, skybox_texture_data, &d3d11_cube_test->skybox_texture);
    
        D3D11_SHADER_RESOURCE_VIEW_DESC skybox_texture_view_desc;
        skybox_texture_view_desc.Format                      = skybox_desc.Format;
        skybox_texture_view_desc.ViewDimension               = D3D11_SRV_DIMENSION_TEXTURECUBE;
        skybox_texture_view_desc.TextureCube.MipLevels       = skybox_desc.MipLevels;
        skybox_texture_view_desc.TextureCube.MostDetailedMip = 0;
        d3d11_cube_test->device->CreateShaderResourceView(d3d11_cube_test->skybox_texture, &skybox_texture_view_desc, &d3d11_cube_test->skybox_texture_view);
    
        stbi_image_free((void*)xneg.pSysMem);
        stbi_image_free((void*)xpos.pSysMem);
        stbi_image_free((void*)yneg.pSysMem);
        stbi_image_free((void*)ypos.pSysMem);
        stbi_image_free((void*)zneg.pSysMem);
        stbi_image_free((void*)zpos.pSysMem);

        D3D11_INPUT_ELEMENT_DESC input_elem_descs[] = {
            {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        d3d11_cube_test->device->CreateInputLayout(input_elem_descs, 1, VSSkybox_BYTES, sizeof(VSSkybox_BYTES), &d3d11_cube_test->skybox_input_layout);

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
        d3d11_cube_test->skybox_vertex_buffer = d3d11_create_vertex_buffer(d3d11_cube_test->device, skybox_vertices, sizeof(skybox_vertices), sizeof(V3f));

        s32 skybox_indices[] = {
            // xneg
            0, 2, 6,
            0, 6, 4,
            // xpos
            5, 7, 3,
            5, 3, 1,
            // yneg
            0, 4, 5,
            0, 5, 1,
            // ypos
            6, 2, 3,
            6, 3, 7,
            // zneg
            1, 3, 2,
            1, 2, 0,
            // zpos
            4, 6, 7,
            4, 7, 5
        };
        d3d11_cube_test->skybox_index_count = gj_ArrayCount(skybox_indices);
        d3d11_cube_test->skybox_index_buffer = d3d11_create_index_buffer(d3d11_cube_test->device, skybox_indices, d3d11_cube_test->skybox_index_count);
    }

    // Resize application (including e.g. depth buffer)
    {
        uint32_t width, height;
        get_wnd_dimensions(wnd, &width, &height);
        d3d11_resize(wnd, d3d11_cube_test, width, height);
    }
}

static void
d3d11_render_mesh(D3D11CubeTest* d3d11_cube_test,
                  const M4x4& mesh_transform,
                  ID3D11Buffer* vertex_buffer, size_t vertex_count,
                  ID3D11ShaderResourceView* color_texture_view, ID3D11ShaderResourceView* normal_texture_view, ID3D11ShaderResourceView* shadow_map_texture_view)
{
    D3D11_MAPPED_SUBRESOURCE constant_buffer_data; gj_ZeroStruct(&constant_buffer_data);
    d3d11_cube_test->device_context->Map(d3d11_cube_test->constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constant_buffer_data);
    memcpy(constant_buffer_data.pData,                                             d3d11_cube_test->projection_matrix.a, sizeof(M4x4));
    memcpy((byte*)constant_buffer_data.pData + sizeof(M4x4),                       d3d11_cube_test->model_view_matrix.a, sizeof(M4x4));
    memcpy((byte*)constant_buffer_data.pData + sizeof(M4x4) * 2,                   mesh_transform.a,                     sizeof(M4x4));
    memcpy((byte*)constant_buffer_data.pData + sizeof(M4x4) * 3,                   d3d11_cube_test->light_pos.a,         sizeof(V3f));
    memcpy((byte*)constant_buffer_data.pData + sizeof(M4x4) * 3 + sizeof(V4f),     d3d11_cube_test->camera_pos.a,        sizeof(V3f));
    memcpy((byte*)constant_buffer_data.pData + sizeof(M4x4) * 3 + sizeof(V4f) * 2, &d3d11_cube_test->normal_use_texture, sizeof(bool));
    d3d11_cube_test->device_context->Unmap(d3d11_cube_test->constant_buffer, 0);

    d3d11_cube_test->device_context->VSSetConstantBuffers(0, 1, &d3d11_cube_test->constant_buffer);
    d3d11_cube_test->device_context->PSSetConstantBuffers(0, 1, &d3d11_cube_test->constant_buffer);
    d3d11_cube_test->device_context->PSSetSamplers(0, 1, &d3d11_cube_test->sampler_state);
    ID3D11ShaderResourceView* shader_resources[] = {color_texture_view, normal_texture_view, shadow_map_texture_view};
    d3d11_cube_test->device_context->PSSetShaderResources(0, gj_ArrayCount(shader_resources), shader_resources);

    ID3D11Buffer* vertex_buffers[] = {vertex_buffer};
    UINT strides[] = {sizeof(MeshVertex)};
    UINT offsets[] = {0};
        
    d3d11_cube_test->device_context->IASetVertexBuffers(0, 1, vertex_buffers, strides, offsets);
    d3d11_cube_test->device_context->IASetInputLayout(d3d11_cube_test->input_layout);
    d3d11_cube_test->device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    d3d11_cube_test->device_context->Draw(vertex_count, 0);

    ID3D11ShaderResourceView* null_shader_resources[] = {NULL, NULL, NULL};
    d3d11_cube_test->device_context->PSSetShaderResources(0, gj_ArrayCount(null_shader_resources), null_shader_resources);
}

static void
d3d11_setup_render_pass(HWND wnd, D3D11CubeTest* d3d11_cube_test)
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
}

static void
d3d11_render_scene(D3D11CubeTest* d3d11_cube_test, ID3D11ShaderResourceView* shadow_map_texture_view)
{
    // Spheres
    {
        M4x4 identity_matrix = M4x4_identity();
        d3d11_render_mesh(d3d11_cube_test,
                          identity_matrix,
                          d3d11_cube_test->sphere_mesh.vertex_buffer, d3d11_cube_test->sphere_mesh.vertex_count,
                          d3d11_cube_test->sphere_mesh.color_texture_view, d3d11_cube_test->sphere_mesh.normal_texture_view, shadow_map_texture_view);
        M4x4 light_pos_matrix = M4x4_translation_matrix(d3d11_cube_test->light_pos);
        d3d11_render_mesh(d3d11_cube_test,
                          light_pos_matrix,
                          d3d11_cube_test->sphere_mesh.vertex_buffer, d3d11_cube_test->sphere_mesh.vertex_count,
                          d3d11_cube_test->sphere_mesh.color_texture_view, d3d11_cube_test->sphere_mesh.normal_texture_view, shadow_map_texture_view);
        M4x4 extra_pos_matrix = M4x4_translation_matrix({10.0f, 1.0f, 5.0f});
        d3d11_render_mesh(d3d11_cube_test,
                          extra_pos_matrix,
                          d3d11_cube_test->sphere_mesh.vertex_buffer, d3d11_cube_test->sphere_mesh.vertex_count,
                          d3d11_cube_test->sphere_mesh.color_texture_view, d3d11_cube_test->sphere_mesh.normal_texture_view, shadow_map_texture_view);
    }

    // Plane
    {
        M4x4 identity_matrix = M4x4_identity();
        d3d11_render_mesh(d3d11_cube_test,
                          identity_matrix,
                          d3d11_cube_test->plane_mesh.vertex_buffer, d3d11_cube_test->plane_mesh.vertex_count,
                          d3d11_cube_test->plane_mesh.color_texture_view, d3d11_cube_test->plane_mesh.normal_texture_view, shadow_map_texture_view);
    }
}

static void
d3d11_render(HWND wnd, D3D11CubeTest* d3d11_cube_test)
{
    d3d11_setup_render_pass(wnd, d3d11_cube_test);
    
    float clear_color[] = {0.0f, 0.0f, 0.0f, 1.0f};
    d3d11_cube_test->device_context->ClearRenderTargetView(d3d11_cube_test->render_target_view, clear_color);
    d3d11_cube_test->device_context->ClearDepthStencilView(d3d11_cube_test->depth_buffer_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0.0f);
    d3d11_cube_test->device_context->OMSetRenderTargets(1, &d3d11_cube_test->render_target_view, d3d11_cube_test->depth_buffer_view);
    d3d11_cube_test->device_context->OMSetDepthStencilState(d3d11_cube_test->depth_stencil_state, 0);
    if (d3d11_cube_test->wireframe_mode_on)
    {
        d3d11_cube_test->device_context->RSSetState(d3d11_cube_test->wireframe_rasterizer_state);
    }
    else
    {
        d3d11_cube_test->device_context->RSSetState(d3d11_cube_test->rasterizer_state);
    }

    memcpy(d3d11_cube_test->projection_matrix.a, d3d11_cube_test->world_projection_matrix.a, sizeof(M4x4));
    memcpy(d3d11_cube_test->model_view_matrix.a, d3d11_cube_test->world_model_view_matrix.a, sizeof(M4x4));

    {
        d3d11_cube_test->device_context->VSSetShader(d3d11_cube_test->vertex_shader, NULL, 0);
        d3d11_cube_test->device_context->PSSetShader(d3d11_cube_test->pixel_shader, NULL, 0);
        
        d3d11_render_scene(d3d11_cube_test, NULL);
    }
    
    // Skybox
    {
        D3D11_MAPPED_SUBRESOURCE constant_buffer_data; gj_ZeroStruct(&constant_buffer_data);
        d3d11_cube_test->device_context->Map(d3d11_cube_test->constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constant_buffer_data);
        memcpy(constant_buffer_data.pData,                                         d3d11_cube_test->projection_matrix.a, sizeof(M4x4));
        memcpy((byte*)constant_buffer_data.pData + sizeof(M4x4),                   d3d11_cube_test->model_view_matrix.a, sizeof(M4x4));
        memcpy((byte*)constant_buffer_data.pData + sizeof(M4x4) * 3,               d3d11_cube_test->light_pos.a,         sizeof(V3f));
        memcpy((byte*)constant_buffer_data.pData + sizeof(M4x4) * 3 + sizeof(V4f), d3d11_cube_test->camera_pos.a,        sizeof(V3f));
        d3d11_cube_test->device_context->Unmap(d3d11_cube_test->constant_buffer, 0);

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
        d3d11_cube_test->device_context->PSSetSamplers(0, 1, &d3d11_cube_test->sampler_state);
        d3d11_cube_test->device_context->PSSetShaderResources(0, 1, &d3d11_cube_test->skybox_texture_view);

        d3d11_cube_test->device_context->DrawIndexed(d3d11_cube_test->skybox_index_count, 0, 0);
    }

#if USE_IMGUI
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif
    
    d3d11_cube_test->swap_chain->Present(1, 0);
}

static void
d3d11_render_shadow_map(HWND wnd, D3D11CubeTest* d3d11_cube_test)
{
    d3d11_setup_render_pass(wnd, d3d11_cube_test);
    
    d3d11_cube_test->device_context->ClearDepthStencilView(d3d11_cube_test->shadow_map_render_target_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0.0f);
    d3d11_cube_test->device_context->OMSetRenderTargets(0, NULL, d3d11_cube_test->shadow_map_render_target_view);
    d3d11_cube_test->device_context->OMSetDepthStencilState(d3d11_cube_test->depth_stencil_state, 0);
    d3d11_cube_test->device_context->RSSetState(d3d11_cube_test->rasterizer_state);

    memcpy(d3d11_cube_test->projection_matrix.a, d3d11_cube_test->light_projection_matrix.a, sizeof(M4x4));
    memcpy(d3d11_cube_test->model_view_matrix.a, d3d11_cube_test->light_model_view_matrix.a, sizeof(M4x4));
    
     {
        d3d11_cube_test->device_context->VSSetShader(d3d11_cube_test->shadow_map_vertex_shader, NULL, 0);
        d3d11_cube_test->device_context->PSSetShader(d3d11_cube_test->shadow_map_pixel_shader, NULL, 0);

        d3d11_render_scene(d3d11_cube_test, d3d11_cube_test->shadow_map_texture_view);
    }

    {
        ID3D11RenderTargetView* null_view0[] = {NULL};
        d3d11_cube_test->device_context->OMSetRenderTargets(1, null_view0, NULL);
    }
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

    win32_init_platform_api(0);

    D3D11CubeTest d3d11_cube_test; gj_ZeroStruct(&d3d11_cube_test);
    d3d11_init(wnd, &d3d11_cube_test);
    d3d11_cube_test.pitch = -45.0f;

#if USE_IMGUI
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui_ImplWin32_Init(wnd);
        ImGui_ImplDX11_Init(d3d11_cube_test.device, d3d11_cube_test.device_context);
    }
#endif
    
    uint32_t width, height;
    get_wnd_dimensions(wnd, &width, &height);
    while (true)
    {
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
                        case 'W':        d3d11_cube_test.up_is_down        = true; break;
                        case 'A':        d3d11_cube_test.left_is_down      = true; break;
                        case 'S':        d3d11_cube_test.down_is_down      = true; break;
                        case 'D':        d3d11_cube_test.right_is_down     = true; break;
                        case VK_SPACE:   d3d11_cube_test.space_is_down     = true; break;
                        case VK_CONTROL: d3d11_cube_test.ctrl_is_down      = true; break;
                        case 'C':
                        {
                            d3d11_cube_test.fps_cam_on = !d3d11_cube_test.fps_cam_on;
                            uint32_t _ignored0, _ignored1;
                            center_and_get_mouse_pos(wnd, &_ignored0, &_ignored1);
                        } break;
                        case 'P':        d3d11_cube_test.wireframe_mode_on  = !d3d11_cube_test.wireframe_mode_on; break;
                        case 'O':        d3d11_cube_test.normal_use_texture = !d3d11_cube_test.normal_use_texture; break;
                    }
                } break;

                case WM_KEYUP:
                {
                    switch (msg.wParam)
                    {
                        case 'W':        d3d11_cube_test.up_is_down        = false; break;
                        case 'A':        d3d11_cube_test.left_is_down      = false; break;
                        case 'S':        d3d11_cube_test.down_is_down      = false; break;
                        case 'D':        d3d11_cube_test.right_is_down     = false; break;
                        case VK_SPACE:   d3d11_cube_test.space_is_down     = false; break;
                        case VK_CONTROL: d3d11_cube_test.ctrl_is_down      = false; break;
                    }
                } break;
            }
        }

        // Movement
        {
            float move_delta_x = 0.0f;
            float move_delta_y = 0.0f;
            float move_delta_z = 0.0f;
        
            if (d3d11_cube_test.up_is_down)    move_delta_z += 0.1f;
            if (d3d11_cube_test.down_is_down)  move_delta_z -= 0.1f;
            if (d3d11_cube_test.left_is_down)  move_delta_x -= 0.1f;
            if (d3d11_cube_test.right_is_down) move_delta_x += 0.1f;
            if (d3d11_cube_test.space_is_down) move_delta_y += 0.1f;
            if (d3d11_cube_test.ctrl_is_down)  move_delta_y -= 0.1f;
        
            uint32_t new_mouse_x, new_mouse_y;
            POINT mouse_pos; GetCursorPos(&mouse_pos);
            new_mouse_x = mouse_pos.x;
            new_mouse_y = mouse_pos.y;
            if (d3d11_cube_test.fps_cam_on)
            {
                uint32_t mouse_x, mouse_y;
                center_and_get_mouse_pos(wnd, &mouse_x, &mouse_y);

                d3d11_cube_test.yaw   -= (float)new_mouse_x - (float)mouse_x;
                d3d11_cube_test.pitch -= (float)new_mouse_y - (float)mouse_y;
                d3d11_cube_test.pitch = clamp(-89.0f, d3d11_cube_test.pitch, 89.0f);

                d3d11_cube_test.mouse_pos.x = mouse_x;
                d3d11_cube_test.mouse_pos.y = mouse_y;
            }
            else
            {
                d3d11_cube_test.mouse_pos.x = new_mouse_x;
                d3d11_cube_test.mouse_pos.y = new_mouse_y;
            }

            V3f fps_forward = V3f_fps_forward_vector(d3d11_cube_test.yaw * DEG_TO_RAD, d3d11_cube_test.pitch * DEG_TO_RAD);
        
            V3f forward_movement = fps_forward;
            forward_movement = V3_normalize(forward_movement);
            forward_movement = V3_mul(move_delta_z, forward_movement);
            V3f left_movement = V3_cross(V3_normalize({fps_forward.x, 0.0f, fps_forward.z}), {0.0f, 1.0f, 0.0f});
            left_movement.y = 0.0f;
            left_movement = V3_mul(-move_delta_x, left_movement);
            V3f up_movement = V3_mul(move_delta_y, {0.0f, 1.0f, 0.0f});
            d3d11_cube_test.camera_pos = V3_add(d3d11_cube_test.camera_pos, V3_add(forward_movement, V3_add(left_movement, up_movement)));
        
            d3d11_cube_test.world_model_view_matrix = M4x4_model_view_matrix(
                d3d11_cube_test.camera_pos,
                fps_forward,
                {0.0f, 1.0f, 0.0f});

            d3d11_cube_test.light_model_view_matrix = M4x4_model_view_matrix(
                d3d11_cube_test.light_pos,
                V3_neg(d3d11_cube_test.light_pos),
                {0.0f, 1.0f, 0.0f});
            
            // Mouse raycast
            {
                M4x4 inverse_projection_matrix = M4x4_inverse_projection_matrix(d3d11_cube_test.world_projection_matrix);
                M4x4 inverse_model_view_matrix = M4x4_inverse_model_view_matrix(d3d11_cube_test.world_model_view_matrix, d3d11_cube_test.camera_pos);
                
                V2f mouse_ndc;
                mouse_ndc.x = (2.0f * d3d11_cube_test.mouse_pos.x) / d3d11_cube_test.width - 1.0f;
                mouse_ndc.y = 1.0f - (2.0f * d3d11_cube_test.mouse_pos.y) / d3d11_cube_test.height;
                V4f mouse_eye = M4x4_mul(inverse_projection_matrix, V4f{mouse_ndc.x, mouse_ndc.y, 1.0f, 1.0f});
                mouse_eye.z = 1.0f;
                mouse_eye.w = 0.0f;
                mouse_eye = M4x4_mul(inverse_model_view_matrix, mouse_eye);
                d3d11_cube_test.mouse_world_pos = V3_normalize(mouse_eye.v3);
            }
        }
        
#if USE_IMGUI
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::InputScalar("CameraPos.x", ImGuiDataType_Float, &d3d11_cube_test.camera_pos.x, NULL);
        ImGui::InputScalar("CameraPos.y", ImGuiDataType_Float, &d3d11_cube_test.camera_pos.y, NULL);
        ImGui::InputScalar("CameraPos.z", ImGuiDataType_Float, &d3d11_cube_test.camera_pos.z, NULL);

        ImGui::InputScalar("LightPos.x", ImGuiDataType_Float, &d3d11_cube_test.light_pos.x, NULL);
        ImGui::InputScalar("LightPos.y", ImGuiDataType_Float, &d3d11_cube_test.light_pos.y, NULL);
        ImGui::InputScalar("LightPos.z", ImGuiDataType_Float, &d3d11_cube_test.light_pos.z, NULL);
        
        ImGui::Text("CameraPos (%f,%f,%f)", d3d11_cube_test.camera_pos.x, d3d11_cube_test.camera_pos.y, d3d11_cube_test.camera_pos.z);
        ImGui::Text("Yaw       %f",         d3d11_cube_test.yaw);
        ImGui::Text("Pitch     %f",         d3d11_cube_test.pitch);

        ImGui::Text("MouseWorldPos (%f,%f,%f)", d3d11_cube_test.mouse_world_pos.x, d3d11_cube_test.mouse_world_pos.y, d3d11_cube_test.mouse_world_pos.z);

        ImGui::Render();
#endif

        d3d11_render_shadow_map(wnd, &d3d11_cube_test);
        d3d11_render(wnd, &d3d11_cube_test);
    }
    
    return 0;
}

LRESULT CALLBACK
wndproc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT result = DefWindowProcA(wnd, msg, wp, lp);

    if (ImGui_ImplWin32_WndProcHandler(wnd, msg, wp, lp)) return true;
        
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
            case WM_KEYUP:
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
