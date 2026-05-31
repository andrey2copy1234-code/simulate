#version 120

void main() {
    // Получаем локальные координаты от -1.0 до 1.0
    vec2 coord = gl_TexCoord[0].xy;
    
    // Считаем расстояние от центра
    float d = length(coord);
    
    // fwidth вычисляет размер ОДНОГО пикселя на экране
    float delta = fwidth(d); 
    
    // Жесткий переход со сглаживанием строго в 1 пиксель
    float alpha = smoothstep(1.0, 1.0 - delta, d);
    
    // Если пиксель полностью прозрачен — отбрасываем
    if (alpha <= 0.0) {
        discard;
    }
    
    // Выводим итоговый цвет (gl_Color — встроенный цвет вершины из SFML)
    gl_FragColor = gl_Color * vec4(1.0, 1.0, 1.0, alpha);
}
