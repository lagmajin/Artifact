// SDFRaymarch.hlsl
// GPU SDF Raymarching Compute Shader
// Camera matrices passed via constant buffer, objects via StructuredBuffer.
// Renders the SDF scene into a UAV RGBA16F texture.
//
// Dispatch: (width/8, height/8, 1) — each thread handles one pixel.

// ---------------------------------------------------------------------------
// Constant buffer: per-frame camera + scene params
// ---------------------------------------------------------------------------
cbuffer SDFRaymarchCB : register(b0)
{
    float4x4 g_InvViewProj;      // Inverse ViewProj for ray reconstruction
    float3   g_CameraPos;
    float    g_SmoothK;          // Smooth union blending factor
    float3   g_LightDir;         // Normalised directional key light
    int      g_ObjectCount;
    uint     g_Width;
    uint     g_Height;
    int      g_CombineOp;        // 0 = Union, 1 = SmoothUnion
    float    g_HitEpsilon;       // Surface hit threshold (default 0.5)
};

// ---------------------------------------------------------------------------
// SDF Object (mirrors Artifact::SDFObject on the CPU side)
// ---------------------------------------------------------------------------
struct SDFObjectGPU
{
    float3  position;
    float   _pad0;
    float3  rotation;   // Euler degrees XYZ
    float   _pad1;
    float3  scale;
    float   param0;     // Torus minor radius ratio
    float4  color;      // RGBA linear
    int     shapeType;  // 0=Sphere, 1=Box, 2=Torus
    float3  _pad2;
};

StructuredBuffer<SDFObjectGPU> g_Objects : register(t0);
RWTexture2D<float4>            g_Output  : register(u0);

// ---------------------------------------------------------------------------
// Math helpers
// ---------------------------------------------------------------------------
float3x3 eulerZYX(float3 deg)
{
    float3 r = deg * (3.14159265f / 180.0f);
    float cx = cos(r.x), sx = sin(r.x);
    float cy = cos(r.y), sy = sin(r.y);
    float cz = cos(r.z), sz = sin(r.z);
    return float3x3(
         cy*cz,          cy*sz,         -sy,
        -cx*sz+sx*sy*cz,  cx*cz+sx*sy*sz, sx*cy,
         sx*sz+cx*sy*cz, -sx*cz+cx*sy*sz, cx*cy
    );
}

// Smooth minimum (polynomial, Inigo Quilez)
float smin(float a, float b, float k)
{
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return lerp(b, a, h) - k*h*(1.0-h);
}

// ---------------------------------------------------------------------------
// SDF primitives
// ---------------------------------------------------------------------------
float sdSphere(float3 p, float r) { return length(p) - r; }

float sdBox(float3 p, float3 b)
{
    float3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdTorus(float3 p, float R, float r)
{
    float2 q = float2(length(p.xz) - R, p.y);
    return length(q) - r;
}

// ---------------------------------------------------------------------------
// Per-object evaluation with transform
// ---------------------------------------------------------------------------
float evalObject(float3 p, SDFObjectGPU obj)
{
    float3 lp = p - obj.position;
    lp = mul(eulerZYX(-obj.rotation), lp);
    float3 s = max(obj.scale, 0.001);
    float3 sp = lp / s;
    float minScale = min(s.x, min(s.y, s.z));

    float d;
    if (obj.shapeType == 0) {
        d = sdSphere(sp, 1.0) * minScale;
    } else if (obj.shapeType == 1) {
        d = sdBox(sp, float3(1,1,1)) * minScale;
    } else {
        float mr = clamp(obj.param0, 0.05, 0.49);
        d = sdTorus(sp, 1.0 - mr, mr) * minScale;
    }
    return d;
}

// ---------------------------------------------------------------------------
// Scene evaluation
// ---------------------------------------------------------------------------
struct SceneHit { float dist; float4 color; };

SceneHit evalScene(float3 p)
{
    SceneHit res;
    res.dist = 1e30;
    res.color = float4(0,0,0,0);

    if (g_ObjectCount == 0) return res;

    res.dist  = evalObject(p, g_Objects[0]);
    res.color = g_Objects[0].color;

    for (int i = 1; i < g_ObjectCount; ++i)
    {
        float d2 = evalObject(p, g_Objects[i]);
        float4 c2 = g_Objects[i].color;

        if (g_CombineOp == 1 && g_SmoothK > 1e-6) {
            float h = clamp(0.5 + 0.5*(d2-res.dist)/g_SmoothK, 0.0, 1.0);
            res.color = lerp(c2, res.color, h);
            res.dist  = smin(res.dist, d2, g_SmoothK);
        } else {
            if (d2 < res.dist) {
                res.dist  = d2;
                res.color = c2;
            }
        }
    }
    return res;
}

// Normal via central differences
float3 calcNormal(float3 p)
{
    float e = 0.5;
    return normalize(float3(
        evalScene(float3(p.x+e, p.y,   p.z  )).dist - evalScene(float3(p.x-e, p.y,   p.z  )).dist,
        evalScene(float3(p.x,   p.y+e, p.z  )).dist - evalScene(float3(p.x,   p.y-e, p.z  )).dist,
        evalScene(float3(p.x,   p.y,   p.z+e)).dist - evalScene(float3(p.x,   p.y,   p.z-e)).dist
    ));
}

// ---------------------------------------------------------------------------
// Blinn-Phong shading
// ---------------------------------------------------------------------------
float3 shade(float3 pos, float3 n, float3 baseColor, float3 camDir)
{
    float3 ld  = normalize(g_LightDir);
    float  diff = clamp(-dot(n, ld), 0.0, 1.0);
    float  rim  = clamp(1.0 - abs(dot(n, camDir)), 0.0, 1.0) * 0.25;
    float  amb  = 0.15;
    float3 vd   = normalize(-camDir);
    float3 hv   = normalize(-ld + vd);
    float  spec = pow(clamp(dot(n, hv), 0.0, 1.0), 32.0) * 0.4;
    float  lit  = clamp(amb + diff*0.75 + rim, 0.0, 1.0);
    return clamp(baseColor * lit + spec, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Main compute entry
// ---------------------------------------------------------------------------
[numthreads(8, 8, 1)]
void SDFRaymarchCS(uint3 dispatchID : SV_DispatchThreadID)
{
    uint px = dispatchID.x;
    uint py = dispatchID.y;
    if (px >= g_Width || py >= g_Height) return;

    // Reconstruct world-space ray from NDC
    float u = (float(px) + 0.5) / float(g_Width)  * 2.0 - 1.0;
    float v = 1.0 - (float(py) + 0.5) / float(g_Height) * 2.0;

    float4 nearH = mul(g_InvViewProj, float4(u, v, -1.0, 1.0));
    float4 farH  = mul(g_InvViewProj, float4(u, v,  1.0, 1.0));
    float3 near3 = nearH.xyz / nearH.w;
    float3 far3  = farH.xyz  / farH.w;

    float3 ro = g_CameraPos;
    float3 rd = normalize(far3 - near3);

    // Ray march
    const int   MAX_STEPS = 64;
    const float MAX_DIST  = 3000.0;
    float t = 0.0;
    float4 result = float4(0, 0, 0, 0);

    for (int s = 0; s < MAX_STEPS && t < MAX_DIST; ++s)
    {
        float3 p = ro + rd * t;
        SceneHit hit = evalScene(p);
        if (hit.dist < g_HitEpsilon)
        {
            float3 n       = calcNormal(p);
            float3 camDir  = rd;
            float3 shaded  = shade(p, n, hit.color.rgb, camDir);
            float  alpha   = hit.color.a;
            result = float4(shaded * alpha, alpha);
            break;
        }
        t += hit.dist * 0.9;
    }

    g_Output[uint2(px, py)] = result;
}
