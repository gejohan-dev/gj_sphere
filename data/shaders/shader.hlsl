
cbuffer cb : register(b0)
{
    float4x4 Proj;
    float4x4 MV;
    float4x4 MeshTransform;
    float3   LightPos;
    float3   CameraPos;
//    bool4    Flag;
};

SamplerState Sampler : register(s0);

static const float4x4 ToHLSLCoords =
{
    { 1.0f, 0.0f,  0.0f, 0.0f },
    { 0.0f, 1.0f,  0.0f, 0.0f },
    { 0.0f, 0.0f, -1.0f, 0.0f },
    { 0.0f, 0.0f,  0.0f, 1.0f }
};
    
////////////////////////////////////////
// Sphere Shader
////////////////////////////////////////
struct VS_INPUT
{
    float3 Pos    : POS;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD;
};

struct PS_Input
{
    float4 Pos       : SV_POSITION;
    float3 VertexPos : POS;
    float3 Normal    : NORMAL;
    float2 UV        : TEXCOORD;
};

PS_Input VSMain(VS_INPUT input)
{
    PS_Input Out;
    float4x4 MVP = mul(MeshTransform, mul(MV, mul(ToHLSLCoords, Proj)));
    float4 _Pos = float4(input.Pos, 1.0f);
    Out.Pos       = mul(_Pos, MVP);
    Out.VertexPos = mul(float4(input.Pos, 1.0f), MeshTransform).xyz;
    Out.Normal    = input.Normal;
    Out.UV        = input.UV;
    return Out;
}

Texture2D SphereColorTexture  : register(t0);
Texture2D SphereNormalTexture : register(t1);
Texture2D ShadowMapTexture    : register(t2);

float4 PSMain(PS_Input input) : SV_TARGET
{
//    float3 Normal;
//    if (Flag.x) Normal = normalize(SphereNormalTexture.Sample(Sampler, input.UV).xyz);
//    else        Normal = normalize(input.Normal);
    float3 Normal = normalize(input.Normal);
    
    // Light
    float3 LightColor = float3(1.0f, 1.0f, 1.0f);
    
    float3 LightDirection = normalize(LightPos - input.VertexPos);
    float3 ViewDirection  = normalize(CameraPos - input.VertexPos);

    float  Ambient      = 0.5f;
    float3 AmbientLight = LightColor;

    float  Diffuse      = max(dot(Normal, LightDirection), 0.0f);
    float3 DiffuseLight = LightColor;

    float3 ReflectionDirection = reflect(-LightDirection, Normal);
    float  Specular      = 0.5f * pow(max(dot(ViewDirection, ReflectionDirection), 0.0f), 32);
    float3 SpecularLight = LightColor;
    
    float3 Light = Ambient * AmbientLight +
                   Diffuse * DiffuseLight +
                   Specular * SpecularLight;
                   
    // Sample
    return SphereColorTexture.Sample(Sampler, input.UV) * float4(Light, 1.0f);
}

////////////////////////////////////////
// Shadow Map Shader
////////////////////////////////////////
float4 VSShadowMapMain(VS_INPUT input) : SV_POSITION
{
    PS_Input Out;
    float4x4 MVP = mul(MeshTransform, mul(MV, mul(ToHLSLCoords, Proj)));
    float4 _Pos = float4(input.Pos, 1.0f);
    return mul(_Pos, MVP);
}

float4 PSShadowMapMain(float4 Pos : SV_POSITION) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}

////////////////////////////////////////
// Skybox Shader
////////////////////////////////////////
TextureCube SkyboxTexture : register(t0);

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
    float4x4 MVWithoutTranslation = MV;
    MVWithoutTranslation[3][0] = 0.0f;
    MVWithoutTranslation[3][1] = 0.0f;
    MVWithoutTranslation[3][2] = 0.0f;
    float4x4 MVP = mul(MVWithoutTranslation, mul(ToHLSLCoords, Proj));
    Out.Pos = mul(float4(input.Pos, 1.0f), MVP).xyww;
    Out.UV = input.Pos;
    return Out;
}

float4 PSSkyboxMain(VSSkybox_Output input) : SV_Target
{
    return SkyboxTexture.Sample(Sampler, input.UV);
}