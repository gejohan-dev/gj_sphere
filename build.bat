@echo off
setlocal
set vs_bytes=vs_bytes
set ps_bytes=ps_bytes
set shadow_map_vs_bytes=shadow_map_vs_bytes
set shadow_map_ps_bytes=shadow_map_ps_bytes
set skybox_vs_bytes=skybox_vs_bytes
set skybox_ps_bytes=skybox_ps_bytes
set image_vs_bytes=image_vs_bytes
set image_ps_bytes=image_ps_bytes
fxc /nologo /Zi /Od /T vs_5_0 /E VSMain /Fh code/vs.h /Vn %vs_bytes% data/shaders/shader.hlsl
fxc /nologo /Zi /Od /T ps_5_0 /E PSMain /Fh code/ps.h /Vn %ps_bytes% data/shaders/shader.hlsl
fxc /nologo /Zi /Od /T vs_5_0 /E VSShadowMapMain /Fh code/shadow_map_vs.h /Vn %shadow_map_vs_bytes% data/shaders/shader.hlsl
fxc /nologo /Zi /Od /T ps_5_0 /E PSShadowMapMain /Fh code/shadow_map_ps.h /Vn %shadow_map_ps_bytes% data/shaders/shader.hlsl
fxc /nologo /Zi /Od /T vs_5_0 /E VSSkyboxMain /Fh code/skybox_vs.h /Vn %skybox_vs_bytes% data/shaders/shader.hlsl
fxc /nologo /Zi /Od /T ps_5_0 /E PSSkyboxMain /Fh code/skybox_ps.h /Vn %skybox_ps_bytes% data/shaders/shader.hlsl
fxc /nologo /Zi /Od /T vs_5_0 /E VSImageMain /Fh code/image_vs.h /Vn %image_vs_bytes% data/shaders/shader.hlsl
fxc /nologo /Zi /Od /T ps_5_0 /E PSImageMain /Fh code/image_ps.h /Vn %image_ps_bytes% data/shaders/shader.hlsl 
mkdir build
pushd build
cl /DGJ_SPHERE_DEBUG^
   /DGJ_DEBUG^
   /DUSE_IMGUI^
   /DVS_BYTES=%vs_bytes%^
   /DPS_BYTES=%ps_bytes%^
   /DVSShadowMap_BYTES=%shadow_map_vs_bytes%^
   /DPSShadowMap_BYTES=%shadow_map_ps_bytes%^
   /DVSSkybox_BYTES=%skybox_vs_bytes%^
   /DPSSkybox_BYTES=%skybox_ps_bytes%^
   /I..\gj^
   /I..\gj\libs\imgui^
   /Z7 /FC /nologo^
   ..\code\gj_sphere_main.cpp^
   ..\gj\libs\imgui\*.cpp^
   ..\gj\libs\imgui\backends\imgui_impl_dx11.cpp^
   ..\gj\libs\imgui\backends\imgui_impl_win32.cpp^
   user32.lib d3d11.lib
popd
