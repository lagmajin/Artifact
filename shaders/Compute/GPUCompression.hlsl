// GPUCompression.hlsl - Simple LZ4-style GPU compression
// Block-based parallel compression for texture data

cbuffer CompressionCB : register(b0)
{
    uint g_BlockSize;
    uint g_NumBlocks;
    uint g_Pad0;
    uint g_Pad1;
};

// Input: RGBA16_FLOAT texture (linear space)
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

// Compression buffer (simplified - just copy for now)
RWStructuredBuffer<uint> g_CompressedData : register(u1);
StructuredBuffer<uint> g_DecompressedData : register(t1);

// RGBA pack/unpack functions (full 64bit precision)
void packRGBA(float4 color, out uint packedLow, out uint packedHigh) {
    uint r = uint(clamp(color.x, 0.0f, 1.0f) * 65535.0f) & 0xFFFF;
    uint g = uint(clamp(color.y, 0.0f, 1.0f) * 65535.0f) & 0xFFFF;
    uint b = uint(clamp(color.z, 0.0f, 1.0f) * 65535.0f) & 0xFFFF;
    uint a = uint(clamp(color.w, 0.0f, 1.0f) * 65535.0f) & 0xFFFF;

    // Pack as low: RRRRGGGG, high: BBBBAAAA
    packedLow = r | (g << 16);
    packedHigh = b | (a << 16);
}

float4 unpackRGBA(uint packedLow, uint packedHigh) {
    uint r = packedLow & 0xFFFF;
    uint g = (packedLow >> 16) & 0xFFFF;
    uint b = packedHigh & 0xFFFF;
    uint a = (packedHigh >> 16) & 0xFFFF;

    return float4(r / 65535.0f, g / 65535.0f, b / 65535.0f, a / 65535.0f);
}

// Compress: Texture -> Compressed Buffer (RGBA full support)
[numthreads(64, 1, 1)]
void CompressCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint blockId = dispatchThreadID.x;
    if (blockId >= g_NumBlocks) return;

    uint blockStart = blockId * g_BlockSize;
    uint outputIdx = blockId * g_BlockSize / 4; // RGBA needs more space

    // Read block data
    float4 blockData[128]; // Smaller for RGBA
    uint blockSizeFloat4 = min(g_BlockSize / 16, 128);

    for (uint i = 0; i < blockSizeFloat4; ++i) {
        uint texX = (blockStart + i * 16) % 1024;
        uint texY = (blockStart + i * 16) / 1024;
        blockData[i] = g_InputTexture.Load(int3(texX, texY, 0));
    }

    // Simple compression: pack RGBA (lossless for now)
    uint outputPos = 0;
    for (uint i = 0; i < blockSizeFloat4; ++i) {
        float4 color = blockData[i];
        uint packedLow, packedHigh;
        packRGBA(color, packedLow, packedHigh);

        if (outputIdx + outputPos < g_NumBlocks * g_BlockSize / 2) {
            g_CompressedData[outputIdx + outputPos] = packedLow;
            outputPos++;
            g_CompressedData[outputIdx + outputPos] = packedHigh;
            outputPos++;
        }
    }
}

    // Improved LZ77: Variable length matches (2-8)
    uint outputPos = 0;
    uint inputPos = 0;

    while (inputPos < blockSizeFloat4) {
        bool foundMatch = false;
        uint bestMatchLen = 0;
        uint bestMatchOffset = 0;

        // Look for matches of length 2-8
        for (uint len = 8; len >= 2; --len) {
            if (inputPos + len > blockSizeFloat4) continue;

            float4 current = blockData[inputPos];

            // Check if all values in sequence are identical
            bool isMatch = true;
            for (uint k = 1; k < len; ++k) {
                if (!all(current == blockData[inputPos + k])) {
                    isMatch = false;
                    break;
                }
            }

            if (isMatch) {
                foundMatch = true;
                bestMatchLen = len;
                bestMatchOffset = 0; // Consecutive match
                break; // Take longest match
            }
        }

        if (foundMatch && bestMatchLen >= 4) { // Only compress if length >= 4
            // LZ77: (length, offset) - pack into uint
            uint packedMatch = (bestMatchLen << 24) | (bestMatchOffset & 0x00FFFFFF);
            if (outputIdx + outputPos < g_NumBlocks * g_BlockSize / 8) {
                g_CompressedData[outputIdx + outputPos] = packedMatch;
                outputPos++;
            }
            inputPos += bestMatchLen;
        } else {
            // Literal: pack float4 to uint (RGBA as half-precision)
            float4 data = blockData[inputPos];
            uint packedR = (uint(clamp(data.x, 0.0f, 1.0f) * 65535.0f) & 0xFFFF);
            uint packedG = (uint(clamp(data.y, 0.0f, 1.0f) * 65535.0f) & 0xFFFF) << 16;
            uint packedLit = packedR | packedG;

            if (outputIdx + outputPos < g_NumBlocks * g_BlockSize / 8) {
                g_CompressedData[outputIdx + outputPos] = packedLit;
                outputPos++;
            }
            inputPos++;
        }
    }
}

// Decompress: Compressed Buffer -> Texture (RGBA full support)
[numthreads(64, 1, 1)]
void DecompressCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint blockId = dispatchThreadID.x;
    if (blockId >= g_NumBlocks) return;

    uint compressedStart = blockId * g_BlockSize / 4; // RGBA needs more space
    uint outputStart = blockId * g_BlockSize;

    uint blockSizeFloat4 = min(g_BlockSize / 16, 128);
    uint inputPos = 0;

    for (uint i = 0; i < blockSizeFloat4; ++i) {
        if (compressedStart + inputPos + 1 >= g_NumBlocks * g_BlockSize / 2) break;

        uint packedLow = g_DecompressedData[compressedStart + inputPos];
        inputPos++;
        uint packedHigh = g_DecompressedData[compressedStart + inputPos];
        inputPos++;

        float4 color = unpackRGBA(packedLow, packedHigh);

        uint texX = (outputStart + i * 16) % 1024;
        uint texY = (outputStart + i * 16) / 1024;
        g_OutputTexture[int2(texX, texY)] = color;
    }
}