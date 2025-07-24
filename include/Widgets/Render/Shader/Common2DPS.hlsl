Texture2D    g_Texture;
SamplerState g_Texture_sampler;

// 頂点シェーダーからの入力構造体
// TEXCOORD0 は、頂点シェーダーで計算されたUV座標を受け取るためのセマンティクス
struct PSInput
{
    float4 Pos : SV_POSITION; // ピクセルシェーダーでは通常使用しないが、入力として定義されることがある
    float2 UV  : TEXCOORD0;   // UV座標（0.0～1.0）
};

// メイン関数
// SV_TARGET は、この関数がレンダーターゲット（画面のピクセルなど）に出力する色であることを示す
float4 main(PSInput Input) : SV_TARGET
{
    // 入力UV座標を使ってテクスチャから色をサンプリング
    float4 texColor = g_Texture.Sample(g_Texture_sampler, Input.UV);

    // サンプルした色をそのまま出力
    return texColor;
}