#version 460 core

uniform sampler2D Tex0;

out vec4 FragColor;

in vec3 FragIn_VertexColor;
in vec2 FragIn_UV;

uniform int UseTexture;

void main(){
	if (UseTexture == 0)
		FragColor = vec4(FragIn_VertexColor, 1.0f);
	else
		FragColor = texture(Tex0, FragIn_UV);
}
