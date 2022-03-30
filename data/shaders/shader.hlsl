
cbuffer cb
{
    float4x4 Proj;
    float4x4 MV;
};

SamplerState Sampler;

////////////////////////////////////////
// Main Shader
////////////////////////////////////////
struct VS_INPUT
{
    float3 Pos    : POS;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD;
};

struct PS_Input
{
    float4 Pos : SV_POSITION;
};

PS_Input VSMain(VS_INPUT input)
{
    PS_Input Out;
    const float4x4 ToHLSLCoords =
    {
        { 1.0f, 0.0f,  0.0f, 0.0f },
        { 0.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { 0.0f, 0.0f,  0.0f, 1.0f }
    };
    float4x4 MVP = mul(MV, mul(ToHLSLCoords, Proj));
    float4 _Pos = float4(input.Pos, 1.0f);
    Out.Pos = mul(_Pos, MVP);
    return Out;
}

float4 PSMain(PS_Input input) : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}

////////////////////////////////////////
// Skybox Shader
////////////////////////////////////////
TextureCube SkyboxTexture;

struct VSSkybox_Input
{
    float3 Pos : POS;
};

struct VSSkybox_Output
{
    float4 Pos : SV_POSITION;
    float3 UV  : TEXCOORD;
};


VSSkybox_Output VSSkyboxMain(VSSkybox_Input input)
{
    VSSkybox_Output Out;
    Out.Pos = mul(float4(input.Pos, 1.0f), Proj).xyww;
    Out.UV = input.Pos;
    return Out;
}

float4 PSSkyboxMain(VSSkybox_Output input) : SV_Target
{
    return SkyboxTexture.Sample(Sampler, input.UV);
}