#version 130

void main() {    
	gl_TexCoord[0] = gl_MultiTexCoord0 *  gl_TextureMatrix[0];
	gl_Position = gl_ProjectionMatrix * gl_Vertex;
}