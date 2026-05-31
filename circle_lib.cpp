#pragma once
// это библиотека для рисования крутых кругов (идеальных ровных  и это не многоугольники 
// но если очень сильно увеличить и смотреть на край будут видны пиксели)
// библиотека изначально сделана для рисования множества крутых кругов
#include "simulate_imports.h"
#include <vector>
sf::Shader circle_shader;
sf::RenderStates circle_states;
void circle_lib_init() {
    if (!circle_shader.loadFromFile("circle.frag", sf::Shader::Fragment)) {
        std::cout << ":( file circle.frag not found" << std::endl;
        exit(-1);
    }
    //circle_shader.setUniform("smoothness", 0.05f);

    circle_states.shader = &circle_shader;
    circle_states.blendMode = sf::BlendAlpha;
}
struct CircleData {
    sf::Vector2f position; // позиция центра
    float radius;
    sf::Color color;
    CircleData(float radius): position(sf::Vector2f(0, 0)), color(sf::Color::Black), radius(radius) {}
    CircleData(): position(sf::Vector2f(0, 0)), color(sf::Color::Black), radius(100) {}
    void setPosition(sf::Vector2f pos) {
        position = pos;
    }
    void setFillColor(sf::Color col) {
        color = col;
    }
};
void updateCircleVertices(sf::VertexArray& va, size_t circleIndex, const CircleData& circle) {
    // У каждого круга ровно 6 вершин, вычисляем начальный индекс в массиве
    size_t idx = circleIndex * 6;

    float r = circle.radius;
    float x = circle.position.x;
    float y = circle.position.y;

    // Координаты углов квадрата на экране
    sf::Vector2f topLeft(x - r, y - r);
    sf::Vector2f topRight(x + r, y - r);
    sf::Vector2f bottomLeft(x - r, y + r);
    sf::Vector2f bottomRight(x + r, y + r);

    // Заполняем вершины: Позиция, Цвет, Локальные текстурные координаты [-1, 1]
    // Первый треугольник (TopLeft -> TopRight -> BottomLeft)
    va[idx + 0] = sf::Vertex(topLeft,    circle.color, sf::Vector2f(-1.f, -1.f));
    va[idx + 1] = sf::Vertex(topRight,   circle.color, sf::Vector2f(1.f, -1.f));
    va[idx + 2] = sf::Vertex(bottomLeft,  circle.color, sf::Vector2f(-1.f, 1.f));

    // Второй треугольник (BottomLeft -> TopRight -> BottomRight)
    va[idx + 3] = sf::Vertex(bottomLeft,  circle.color, sf::Vector2f(-1.f, 1.f));
    va[idx + 4] = sf::Vertex(topRight,   circle.color, sf::Vector2f(1.f, -1.f));
    va[idx + 5] = sf::Vertex(bottomRight, circle.color, sf::Vector2f(1.f, 1.f));
}
void draw_circles(sf::RenderWindow& window, std::vector<CircleData> circles, sf::VertexArray& va) {
    va.resize(circles.size()*6);
    for (size_t i = 0; i!=circles.size(); i++) {
        updateCircleVertices(va, i, circles[i]);
    }
    window.draw(va, circle_states);
}
