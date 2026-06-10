// bloom test
//#include "simulate_imports.h"
#include <SFML/OpenGL.hpp>
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif

#ifndef GL_FLOAT
#define GL_FLOAT 0x1406
#endif
#include "circles_lib.cpp"
#include <cmath>
sf::Shader shape_bloom_shader;
sf::Shader bloom_shader;
sf::Shader bloom_ends_shader;
sf::RenderTexture tex_bloom;
sf::RenderTexture tex_bloom2;
sf::RenderTexture tex_bloom_final;
sf::RenderTexture tex_glow_map;
sf::Vector2u size_tex;
sf::Vector2u size_tex2;
void bloom_init() {
    if (!bloom_shader.loadFromFile("bloom_end.frag", sf::Shader::Fragment)) {
        std::cout << ":( file bloom_end.frag not found" << std::endl;
        exit(-1);
    }
    if (!bloom_ends_shader.loadFromFile("bloom_ends.frag", sf::Shader::Fragment)) {
        std::cout << ":( file bloom_ends.frag not found" << std::endl;
        exit(-1);
    }
    if (!shape_bloom_shader.loadFromFile("bloom_start.frag", sf::Shader::Fragment)) {
        std::cout << ":( file bloom_start.frag not found" << std::endl;
        exit(-1);
    }
}
void draw_circles_bloom(sf::RenderTexture& tex, std::vector<CircleData> circles, sf::VertexArray& va, float bloom_size) {
    sf::Vector2u size_window = tex.getSize();
    if (size_window!=size_tex) {
        size_tex = size_window;
        tex_bloom.create(size_window.x, size_window.y);
    }
    va.resize(circles.size()*6);
    for (size_t i = 0; i!=circles.size(); i++) {
        updateCircleVertices(va, i, circles[i]);
    }
    tex_bloom.clear(sf::Color::Transparent);
    tex_bloom.draw(va, circle_states);
    tex_bloom.display();
    sf::Sprite screenSprite(tex_bloom.getTexture());
    shape_bloom_shader.setUniform("bloom_size", bloom_size);
    tex.draw(screenSprite, &shape_bloom_shader);
}
void convertToHDRTexture(sf::Texture& texture, unsigned int width, unsigned int height) {
    // 1. Активируем внутренний OpenGL идентификатор текстуры SFML
    sf::Texture::bind(&texture);

    // 2. Перевыделяем память на видеокарте в формате 16-bit Floating Point (HDR)
    // GL_RGBA16F позволяет хранить значения цвета от 0.0 до 65500.0 вместо 0.0 - 1.0
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

    // 3. Отключаем текстуру
    sf::Texture::bind(NULL);
}
void drawBloomCircle(sf::RenderTexture& tex, CircleData& circle, float bloom_size) {
    sf::Vector2u size_window = tex.getSize();
    if (size_window!=size_tex) {
        size_tex = size_window;
        tex_bloom.create(size_window.x, size_window.y);
    }
    sf::VertexArray va(sf::Triangles, 6);
    updateCircleVertices(va, 0, circle);
    tex_bloom.clear(sf::Color::Transparent);
    tex_bloom.draw(va, circle_states);
    tex_bloom.display();
    sf::Sprite screenSprite(tex_bloom.getTexture());
    shape_bloom_shader.setUniform("bloom_size", bloom_size);
    tex.draw(screenSprite, &shape_bloom_shader);
}
void drawBloom(sf::RenderWindow& window, sf::RenderTexture& tex) {
    sf::Vector2u size_window = tex.getSize();
    
    // Контроль размеров для всех вспомогательных буферов
    if (size_window != size_tex2) {
        size_tex2 = size_window;
        
        // Создаем стандартные буферы SFML
        tex_glow_map.create(size_window.x, size_window.y);
        tex_bloom2.create(size_window.x, size_window.y);
        tex_bloom_final.create(size_window.x, size_window.y);

        // ВАЖНО: Принудительно переводим их текстуры в режим HDR (с плавающей запятой)
        // Теперь они НЕ будут обрезать цвет на значении 1.0 (255)
        convertToHDRTexture(const_cast<sf::Texture&>(tex_glow_map.getTexture()), size_window.x, size_window.y);
        convertToHDRTexture(const_cast<sf::Texture&>(tex_bloom2.getTexture()), size_window.x, size_window.y);
        convertToHDRTexture(const_cast<sf::Texture&>(tex_bloom_final.getTexture()), size_window.x, size_window.y);
    }
    
    tex.display();
    
    // === ШАГ 1: ВЫДЕЛЕНИЕ ЯРКОСТИ (ТЕПЕРЬ ТУТ ЧЕСТНЫЙ HDR) ===
    tex_glow_map.clear(sf::Color::Black);

    // Берем спрайт оригинальной сцены, где солнце уже имеет свою яркость
    sf::Sprite baseSprite(tex.getTexture());

    // Копируем сцену «один в один» без шейдеров
    sf::RenderStates extractStates;
    extractStates.blendMode = sf::BlendNone; 

    tex_glow_map.draw(baseSprite, extractStates);
    tex_glow_map.display();
    
    // === ШАГ 2: ГОРИЗОНТАЛЬНОЕ РАЗМЫТИЕ ===
    bloom_shader.setUniform("blurDirection", sf::Glsl::Vec2(1.0f, 0.0f));
    bloom_shader.setUniform("textureSize", sf::Glsl::Vec2(size_window.x, size_window.y));
    bloom_shader.setUniform("blurTexture", tex_glow_map.getTexture()); 

    tex_bloom2.clear(sf::Color::Black);
    sf::Sprite glowSprite(tex_glow_map.getTexture());
    
    sf::RenderStates blurStates(&bloom_shader);
    blurStates.blendMode = sf::BlendNone; // Сохраняем HDR числа при перезаписи
    
    tex_bloom2.draw(glowSprite, blurStates); 
    tex_bloom2.display();
    
    // === ШАГ 3: ВЕРТИКАЛЬНОЕ РАЗМЫТИЕ ===
    bloom_shader.setUniform("blurDirection", sf::Glsl::Vec2(0.0f, 1.0f));
    bloom_shader.setUniform("blurTexture", tex_bloom2.getTexture()); 

    tex_bloom_final.clear(sf::Color::Black);
    sf::Sprite blurStep1Sprite(tex_bloom2.getTexture());
    
    tex_bloom_final.draw(blurStep1Sprite, blurStates); 
    tex_bloom_final.display();
    
    // === ШАГ 4: СМЕШИВАНИЕ (С ОПЕРАТОРОМ TONE MAPPING) ===
    bloom_ends_shader.setUniform("u_base", tex.getTexture());       
    bloom_ends_shader.setUniform("u_glow", tex_bloom_final.getTexture()); 

    sf::Sprite renderQuad(tex_bloom_final.getTexture());
    window.draw(renderQuad, &bloom_ends_shader);
}
// int main() {
//     // 1. Создаем окно: разрешение 800x600, заголовок и стандартный стиль (закрытие, изменение размера)
//     sf::RenderWindow window(sf::VideoMode(800, 600), "SFML Window", sf::Style::Default);

//     // Ограничиваем кадры до 60 FPS, чтобы видеокарта не работала на 100% вхолостую
//     window.setFramerateLimit(60);
//     circle_lib_init();
//     bloom_init();

//     double time = 0.0;
//     sf::RenderTexture tex_display;
//     tex_display.create(800, 600);
//     convertToHDRTexture(const_cast<sf::Texture&>(tex_display.getTexture()), 800, 600);
//     sf::Clock clock;
//     // 2. Главный игровой цикл (работает, пока окно открыто)
//     while (window.isOpen()) {
        
//         sf::Event event;
//         while (window.pollEvent(event)) {
//             if (event.type == sf::Event::Closed) {
//                 window.close();
//             }
//         }

//         // 4. Очистка экрана (закрашиваем прошлый кадр цветом, например, темно-серым)
//         window.clear(sf::Color(30, 30, 30));
//         tex_display.clear(sf::Color(30, 30, 30));

//         float moon_pos = fmod(time, 20)/20*1200-100;
//         CircleData moon(100);
//         moon.setPosition(sf::Vector2f(moon_pos, 300));
//         moon.setFillColor(sf::Color(70, 70, 70));
//         CircleData solar(103);
//         solar.setPosition(sf::Vector2f(400, 300));
//         solar.setFillColor(sf::Color(200, 200, 50));
//         drawBloomCircle(tex_display, solar, 2.5);
//         drawBloomCircle(tex_display, moon, 1.0);

//         drawBloom(window, tex_display);
//         // tex_display.display();
//         // sf::Sprite testSprite(tex_display.getTexture());
//         // window.draw(testSprite); 
//         window.display();
//         time += clock.restart().asSeconds();
//     }

//     return 0;
// }
