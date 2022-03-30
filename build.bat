@echo off
setlocal
set vs_bytes=vs_bytes
set ps_bytes=ps_bytes
set skybox_vs_bytes=skybox_vs_bytes
set skybox_ps_bytes=skybox_ps_bytes
fxc /nologo /Zi /Od /T vs_5_0 /E VSMain /Fh code/vs.h /Vn %vs_bytes% data/shaders/shader.hlsl
fxc /nologo /Zi /Od /T ps_5_0 /E PSMain /Fh code/ps.h /Vn %ps_bytes% data/shaders/shader.hlsl
fxc /nologo /Zi /Od /T vs_5_0 /E VSSkyboxMain /Fh code/skybox_vs.h /Vn %skybox_vs_bytes% data/shaders/shader.hlsl
fxc /nologo /Zi /Od /T ps_5_0 /E PSSkyboxMain /Fh code/skybox_ps.h /Vn %skybox_ps_bytes% data/shaders/shader.hlsl 
mkdir build
pushd build
cl /DCUBE_TEST_DEBUG^
   /DGJ_DEBUG^
   /DVS_BYTES=%vs_bytes%^
   /DPS_BYTES=%ps_bytes%^
   /DVSSkybox_BYTES=%skybox_vs_bytes%^
   /DPSSkybox_BYTES=%skybox_ps_bytes%^
   /I..\..\gj^
   /Z7 /FC /nologo^
   ..\code\cube_test_main.cpp^
   user32.lib d3d11.lib
popd
