#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inLocalCoord;
layout(location = 2) in vec4 inDrawRect;
layout(location = 3) in vec4 inAtlasRect;
layout(location = 4) in vec4 inColor;

layout(location = 0) out vec2 fragLocalCoord;
layout(location = 1) out vec4 fragDrawRect;
layout(location = 2) out vec4 fragAtlasRect;
layout(location = 3) out vec4 fragColor;

void main()
{
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragLocalCoord = inLocalCoord;
    fragDrawRect = inDrawRect;
    fragAtlasRect = inAtlasRect;
    fragColor = inColor;
}
