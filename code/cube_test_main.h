#if !defined(CUBE_TEST_MAIN_H)
#define CUBE_TEST_MAIN_H

struct D3D11CubeTest
{
    uint32_t width;
    uint32_t height;
    
    IDXGISwapChain*          swap_chain;
    ID3D11Device*            device;
    ID3D11DeviceContext*     device_context;
    ID3D11RenderTargetView*  render_target_view;
    ID3D11Texture2D*         depth_buffer;
    ID3D11DepthStencilView*  depth_buffer_view;
    ID3D11RasterizerState*   rasterizer_state;
    ID3D11DepthStencilState* depth_stencil_state;
    
    ID3D11VertexShader*       vertex_shader;
    ID3D11PixelShader*        pixel_shader;
    ID3D11InputLayout*        input_layout;
    ID3D11VertexShader*       skybox_vertex_shader;
    ID3D11PixelShader*        skybox_pixel_shader;
    ID3D11SamplerState*       skybox_sampler_state;
    ID3D11Texture2D*          skybox_texture;
    ID3D11ShaderResourceView* skybox_texture_view;
    ID3D11InputLayout*        skybox_input_layout;
    ID3D11Buffer*             skybox_vertex_buffer;
    ID3D11Buffer*             skybox_index_buffer;
    u32                       skybox_index_count;
    ID3D11Buffer*             constant_buffer;
    M4x4                      projection_matrix;
    M4x4                      model_view_matrix;
    
    ID3D11Buffer* sphere_vertex_buffer;
    uint32_t      sphere_vertex_count;

    float yaw;
    float pitch;
    V3f   camera_pos;
};

struct SphereVertex
{
    V3f pos;
    V3f normal;
    V2f uv;
};

#endif
