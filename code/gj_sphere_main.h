#if !defined(GJ_SPHERE_MAIN_H)
#define GJ_SPHERE_MAIN_H

struct D3D11Mesh
{
    ID3D11Buffer*             vertex_buffer;
    uint32_t                  vertex_count;
    ID3D11Texture2D*          color_texture;
    ID3D11ShaderResourceView* color_texture_view;
    ID3D11Texture2D*          normal_texture;
    ID3D11ShaderResourceView* normal_texture_view;
};

struct D3D11GJSphere
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
    ID3D11RasterizerState*   wireframe_rasterizer_state;
    ID3D11DepthStencilState* depth_stencil_state;
    
    ID3D11VertexShader*       vertex_shader;
    ID3D11PixelShader*        pixel_shader;
    ID3D11InputLayout*        input_layout;
    ID3D11SamplerState*       sampler_state;
    ID3D11VertexShader*       skybox_vertex_shader;
    ID3D11PixelShader*        skybox_pixel_shader;
    ID3D11Texture2D*          skybox_texture;
    ID3D11ShaderResourceView* skybox_texture_view;
    ID3D11InputLayout*        skybox_input_layout;
    ID3D11Buffer*             skybox_vertex_buffer;
    ID3D11Buffer*             skybox_index_buffer;
    u32                       skybox_index_count;
    ID3D11Buffer*             constant_buffer;
    M4x4                      projection_matrix;
    M4x4                      model_view_matrix;
    M4x4                      world_projection_matrix;
    M4x4                      world_model_view_matrix;
    M4x4                      light_projection_matrix;
    M4x4                      light_model_view_matrix;

    ID3D11Texture2D*          shadow_map_render_target;
    ID3D11DepthStencilView*   shadow_map_render_target_view;
    ID3D11ShaderResourceView* shadow_map_texture_view;
    ID3D11VertexShader*       shadow_map_vertex_shader;
    ID3D11PixelShader*        shadow_map_pixel_shader;

#if defined(GJ_SPHERE_DEBUG)
    ID3D11Texture2D*          debug_image_texture;
    ID3D11ShaderResourceView* debug_image_view;
    ID3D11VertexShader*       debug_image_vertex_shader;
    ID3D11PixelShader*        debug_image_pixel_shader;
#endif
    
    D3D11Mesh sphere_mesh;
    D3D11Mesh plane_mesh;
        
    float yaw;
    float pitch;
    V3f   camera_pos;

    V2f mouse_pos;
    V3f mouse_world_pos;
    
    bool up_is_down;
    bool down_is_down;
    bool left_is_down;
    bool right_is_down;
    bool space_is_down;
    bool ctrl_is_down;

    bool fps_cam_on;
    bool wireframe_mode_on;
    bool normal_use_texture;
    
    V3f   light_pos;
    float light_left;
    float light_right;
    float light_top;
    float light_bottom;
    float light_near;
    float light_far;
};

struct MeshVertex
{
    V3f pos;
    V3f normal;
    V2f uv;
};

#endif
