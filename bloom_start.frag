#version 120

uniform sampler2D u_startTex; 
uniform float bloom_size;

void main() {
    // ИСПРАВЛЕНО: Явное обращение к нулевому текстурному слою
    vec2 uv = gl_TexCoord[0].st;
    vec4 pixelColor = texture2D(u_startTex, uv);
    
    // Получаем финальный HDR цвет объекта
    vec4 hdrColor = pixelColor * gl_Color * bloom_size;
    
    // Считаем относительную яркость пикселя
    float brightness = dot(hdrColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    
    // Выводим финальный цвет
    gl_FragColor = hdrColor;
}
