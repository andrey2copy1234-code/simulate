#version 430 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color; 

uniform vec2 screenSize;

void main() {
    // Прямой перевод координат без переворотов (как для X, так и для Y)
    float nx = (position.x / screenSize.x) * 2.0 - 1.0;
    float ny = (position.y / screenSize.y) * 2.0 - 1.0; 
    gl_Position = vec4(nx, ny, 0.0, 1.0);
}
