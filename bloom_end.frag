#version 120

uniform sampler2D blurTexture; // ИСПРАВЛЕНО: Переименовано зарезервированное слово texture
uniform vec2 blurDirection;    // Передаем из C++: (1, 0) или (0, 1)
uniform vec2 textureSize;      // Размер вашей текстуры (например, 800, 600)

void main() {
    vec2 uv = gl_TexCoord[0].xy; // ИСПРАВЛЕНО: Для версии 120 правильно использовать gl_TexCoord[0]
    //uv.y = 1.0 - uv.y;
    float blurRadius = 1.0; 
    vec2 step = (blurDirection / textureSize) * blurRadius; 

    // Берем текущий пиксель и соседние с весами (размытие Гаусса)
    vec4 sum = texture2D(blurTexture, uv) * 0.2270270270;
    
    sum += texture2D(blurTexture, uv + step * 1.38461538) * 0.231268;
    sum += texture2D(blurTexture, uv - step * 1.38461538) * 0.231268;
    
    // Средние шаги
    sum += texture2D(blurTexture, uv + step * 3.23076923) * 0.1216216;
    sum += texture2D(blurTexture, uv - step * 3.23076923) * 0.1216216;
    
    // Дальние шаги (сглаживают переходы на расстоянии)
    sum += texture2D(blurTexture, uv + step * 5.1557223) * 0.054054;
    sum += texture2D(blurTexture, uv - step * 5.1557223) * 0.054054;
    
    // Экстремальные шаги (мягко уводят сияние в ноль, убирая резкие границы)
    sum += texture2D(blurTexture, uv + step * 7.0270270) * 0.016216;
    sum += texture2D(blurTexture, uv - step * 7.0270270) * 0.016216;

    gl_FragColor = sum;
}
