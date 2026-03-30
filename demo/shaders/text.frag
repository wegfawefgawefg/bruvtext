#version 450

layout(set = 0, binding = 0) uniform sampler2D atlasTexture;

layout(location = 0) in vec2 fragLocalCoord;
layout(location = 1) in vec4 fragDrawRect;
layout(location = 2) in vec4 fragAtlasRect;
layout(location = 3) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main()
{
    ivec2 atlasSize = textureSize(atlasTexture, 0);
    bool nativeScale =
        abs(fragDrawRect.z - fragAtlasRect.z) < 0.01 &&
        abs(fragDrawRect.w - fragAtlasRect.w) < 0.01;

    ivec2 texel = ivec2(0);
    if (nativeScale) {
        vec2 localPixel = floor(gl_FragCoord.xy - fragDrawRect.xy);
        localPixel = clamp(localPixel, vec2(0.0), fragAtlasRect.zw - vec2(1.0));
        texel = ivec2(
            int(fragAtlasRect.x + localPixel.x),
            int(fragAtlasRect.y + localPixel.y)
        );
    } else {
        vec2 clampedLocal = clamp(fragLocalCoord, vec2(0.0), vec2(0.999999));
        texel = ivec2(
            int(floor(fragAtlasRect.x + clampedLocal.x * fragAtlasRect.z)),
            int(floor(fragAtlasRect.y + clampedLocal.y * fragAtlasRect.w))
        );
    }
    texel = clamp(texel, ivec2(0), atlasSize - ivec2(1));
    float alpha = texelFetch(atlasTexture, texel, 0).a;
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}
