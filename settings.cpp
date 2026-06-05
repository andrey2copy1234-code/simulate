// settings
#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <type_traits>
#if defined(_WIN32)
    #include <windows.h>
    #include <libloaderapi.h>
    #include <processthreadsapi.h>
#else
    #include <unistd.h>
    #include <limits.h>
#endif

void restart_application() {
    std::string exe_path;

#if defined(_WIN32)
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    exe_path = std::string(buffer);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    
    // Жестко обнуляем структуры памяти, чтобы Windows не наследовала ничего лишнего
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Добавляем флаг DETACHED_PROCESS или CREATE_NEW_CONSOLE, 
    // чтобы новый .exe никак не зависел от ресурсов текущего окна
    if (CreateProcessA(
        exe_path.c_str(), // Путь к .exe
        NULL,             // Командная строка
        NULL,             // Атрибуты процесса
        NULL,             // Атрибуты потока
        FALSE,            // Наследование хэндлов (НЕТ)
        CREATE_NEW_CONSOLE, // Флаг: Запуск в независимой среде
        NULL,             // Переменные окружения
        NULL,             // Текущий каталог
        &si,              // Информация о запуске
        &pi               // Информация о созданном процессе
    )) {
        // Сразу закрываем хэндлы нового процесса, они нам больше не нужны
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        std::cout << "[Система] Новый процесс запущен. Уничтожаю текущий..." << std::endl;

        // ВМЕСТО std::exit(0) — ПРИНУДИТЕЛЬНОЕ УБИЙСТВО ТЕКУЩЕГО ПРОЦЕССА НА УРОВНЕ ЯДРА
        // GetCurrentProcess() возвращает псевдо-хэндл текущей программы
        TerminateProcess(GetCurrentProcess(), 0); 
    } else {
        std::cerr << "[Ошибка] Ошибка CreateProcess. Код: " << GetLastError() << std::endl;
        std::exit(1);
    }
#else
    // Код для Linux остается прежним через execv (он сам заменяет процесс в памяти)
    char buffer[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (count != -1) {
        buffer[count] = '\0';
        exe_path = std::string(buffer);
        char* args[] = { &exe_path, nullptr };
        execv(exe_path.c_str(), args); 
    }
    std::exit(1);
#endif
}

enum class SettingType { Bool, Number, String };
enum class InputType { Checkbox, YesNo, InputField, Slider, Dropdown };

struct Setting {
    std::string name;
    std::string description;
    SettingType type;
    InputType input_type;
    
    // Текущие и дефолтные значения (хранятся как строки для универсальности)
    std::string value;
    std::string default_value;
    
    bool needs_restart = false;
    bool is_dirty = false;       // Изменено ли значение в этой сессии
    int bit_depth = 32;          // Для чисел: разрядность в битах (например, 24)
    
    // Параметры для слайдеров / выпадающих списков
    int min_val = 0;
    int max_val = 100;
    std::vector<std::string> options; 
    template <typename T>
    T get_value() {
        if constexpr (std::is_same_v<T, bool>) {
            return value=="true";
        } else if constexpr (std::is_same_v<T, int>) {
            return std::stoi(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return value;
        } else {
            static_assert(false, "is not bool and is not int and not is std::string");
        }
    }
};
std::string to_hex(int value, int bits) {
    int chars_count = bits / 4;
    std::stringstream ss;
    ss << std::setw(chars_count) << std::setfill('0') << std::hex << value;
    return ss.str();
}

int from_hex(const std::string& hex_str) {
    int value = 0; // Обязательно инициализируем нулем, чтобы не вернуть мусор из памяти
    std::stringstream ss;
    
    ss << hex_str;     // 1. Сначала просто записываем строку "0010" в поток
    ss >> std::hex >> value; // 2. А теперь ЧИТАЕМ её из потока обратно в int, используя флаг std::hex
    
    return value;
}

void setup_write(const std::string& filename, const std::vector<Setting>& settings) {
    // ios::trunc принудительно очищает файл при открытии или создает его, если файла нет
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[Ошибка] Не удалось создать или открыть файл для записи: " << filename << std::endl;
        return;
    }

    for (const auto& s : settings) {
        if (s.type == SettingType::Bool) {
            file << (s.value == "true" ? "1" : "0");
        } 
        else if (s.type == SettingType::Number) {
            file << to_hex(std::stoi(s.value), s.bit_depth);
        } 
        else if (s.type == SettingType::String) {
            // Записываем маркеры [], но внутри только "чистое" значение без старых скобок
            file << "[" << s.value << "]";
        }
    }
    file.close();
}


void setup_read(const std::string& filename, std::vector<Setting>& settings) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        for (auto& seting: settings) {
            seting.value = seting.default_value;
        }
        return;
    };

    std::string content;
    std::getline(file, content);
    file.close();

    size_t cursor = 0;
    for (auto& s : settings) {
        if (cursor >= content.length()) {
            // Файл старый, новые настройки инициализируем дефолтом
            s.value = s.default_value;
            continue;
        }

        if (s.type == SettingType::Bool) {
            s.value = (content[cursor] == '1') ? "true" : "false";
            cursor += 1;
        } else if (s.type == SettingType::Number) {
            int chars_count = s.bit_depth / 4;
            if (cursor + chars_count <= content.length()) {
                std::string hex_part = content.substr(cursor, chars_count);
                s.value = std::to_string(from_hex(hex_part));
                cursor += chars_count;
            }
        } else if (s.type == SettingType::String) {
            if (content[cursor] == '[') {
                size_t close_bracket = content.find(']', cursor);
                if (close_bracket != std::string::npos) {
                    s.value = content.substr(cursor + 1, close_bracket - cursor - 1);
                    cursor = close_bracket + 1;
                }
            }
        }
    }
}
struct UIButton {
    sf::RectangleShape rect;
    sf::Text text;
    
    UIButton(float x, float y, float w, float h, std::string label, const sf::Font& font) {
        rect.setPosition(x, y);
        rect.setSize({w, h});
        rect.setFillColor(sf::Color(70, 70, 70));
        rect.setOutlineThickness(1);
        rect.setOutlineColor(sf::Color::White);
        
        text.setFont(font);
        text.setString(sf::String::fromUtf8(label.begin(), label.end()));
        text.setCharacterSize(16);
        text.setFillColor(sf::Color::White);
        // Центрирование текста
        sf::FloatRect bounds = text.getLocalBounds();
        text.setPosition(x + (w - bounds.width)/2.0f, y + (h - bounds.height)/2.0f - 4);
    }
    
    bool isClicked(sf::Vector2i mousePos) {
        return rect.getGlobalBounds().contains(mousePos.x, mousePos.y);
    }
};

void show_setup_interface(std::vector<Setting>& settings, const std::string& config_path, const sf::Font& font) {
    sf::RenderWindow window(sf::VideoMode(800, 600), "Настройки системы");
    window.setFramerateLimit(60);

    setup_read(config_path, settings);

    bool need_restart_app = false;
    int selected_setting_idx = -1; // Для отображения описания при наведении

    while (window.isOpen()) {
        sf::Event event;
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                //setup_write(config_path, settings);
                window.close();
            }

            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                // Кнопка: Сбросить ВСЁ по умолчанию
                UIButton btnResetAll(20, 530, 240, 40, "Сбросить всё по умолчанию", font);
                if (btnResetAll.isClicked(mousePos)) {
                    for (auto& s : settings) {
                        s.value = s.default_value;
                        if (s.needs_restart) need_restart_app = true;
                    }
                }

                // Кнопка: Перезагрузка (появляется при необходимости)
                UIButton btnRestart(540, 530, 240, 40, "Перезапустить программу", font);
                if (btnRestart.isClicked(mousePos)) {
                    setup_write(config_path, settings);
                    if (need_restart_app) {
                        std::cout << "[Система] Выполнение перезагрузки..." << std::endl;
                        window.close(); // Инициация перезапуска в основном цикле
                        restart_application();
                    }
                    window.close();
                }

                // Логика кликов по элементам настроек
                float current_y = 40;
                for (size_t i = 0; i < settings.size(); ++i) {
                    auto& s = settings[i];
                    
                    // Клики по булевым переключателям (Checkbox или Да/Нет)
                    if (s.type == SettingType::Bool) {
                        sf::FloatRect click_zone(300, current_y, 100, 30);
                        if (click_zone.contains(mousePos.x, mousePos.y)) {
                            s.value = (s.value == "true") ? "false" : "true";
                            if (s.needs_restart) need_restart_app = true;
                        }
                    }
                    
                    // Кнопка одиночного сброса "Сбросить эту настройку"
                    sf::FloatRect reset_zone(680, current_y, 100, 25);
                    if (reset_zone.contains(mousePos.x, mousePos.y)) {
                        s.value = s.default_value;
                        if (s.needs_restart) need_restart_app = true;
                    }

                    // Интерактив для Slider (упрощенный клик по позиции)
                    if (s.input_type == InputType::Slider) {
                        // Зона ползунка (ширина 200 пикселей)
                        sf::FloatRect slider_zone(300, current_y + 10, 200, 16);
                        if (slider_zone.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
                            // Вычисляем процент клика относительно длины слайдера (от 0.0 до 1.0)
                            float pct = (static_cast<float>(mousePos.x) - 300.0f) / 200.0f;
                            if (pct < 0.0f) pct = 0.0f;
                            if (pct > 1.0f) pct = 1.0f;
                            
                            if (!s.options.empty()) {
                                // --- СЛАЙДЕР ПО ЗАДАННЫМ ЗНАЧЕНИЯМ (ДИСКРЕТНЫЙ) ---
                                float total_steps = static_cast<float>(s.options.size());
                                // Находим индекс ближайшего шага с округлением (+0.5f)
                                int idx = static_cast<int>(pct * (total_steps - 1.0f) + 0.5f);
                                
                                // Если выбран новый шаг, обновляем значение
                                if (s.value != s.options[idx]) {
                                    s.value = s.options[idx];
                                    if (s.needs_restart) need_restart_app = true;
                                }
                            } 
                            else {
                                // --- СВОБОДНЫЙ СЛАЙДЕР ---
                                int val = s.min_val + static_cast<int>(pct * static_cast<float>(s.max_val - s.min_val));
                                if (s.value != std::to_string(val)) {
                                    s.value = std::to_string(val);
                                    if (s.needs_restart) need_restart_app = true;
                                }
                            }
                        }
                    }

                    current_y += 60;
                }
            } else if (event.type == sf::Event::MouseMoved && sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                float current_y = 40;
                for (size_t i = 0; i < settings.size(); ++i) {
                    auto& s = settings[i];

                    // Интерактив для Slider (упрощенный клик по позиции)
                    if (s.input_type == InputType::Slider) {
                        // Зона ползунка (ширина 200 пикселей)
                        sf::FloatRect slider_zone(300, current_y + 10, 200, 16);
                        if (slider_zone.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
                            // Вычисляем процент клика относительно длины слайдера (от 0.0 до 1.0)
                            float pct = (static_cast<float>(mousePos.x) - 300.0f) / 200.0f;
                            if (pct < 0.0f) pct = 0.0f;
                            if (pct > 1.0f) pct = 1.0f;
                            
                            if (!s.options.empty()) {
                                // --- СЛАЙДЕР ПО ЗАДАННЫМ ЗНАЧЕНИЯМ (ДИСКРЕТНЫЙ) ---
                                float total_steps = static_cast<float>(s.options.size());
                                // Находим индекс ближайшего шага с округлением (+0.5f)
                                int idx = static_cast<int>(pct * (total_steps - 1.0f) + 0.5f);
                                
                                // Если выбран новый шаг, обновляем значение
                                if (s.value != s.options[idx]) {
                                    s.value = s.options[idx];
                                    if (s.needs_restart) need_restart_app = true;
                                }
                            } 
                            else {
                                // --- СВОБОДНЫЙ СЛАЙДЕР ---
                                int val = s.min_val + static_cast<int>(pct * static_cast<float>(s.max_val - s.min_val));
                                if (s.value != std::to_string(val)) {
                                    s.value = std::to_string(val);
                                    if (s.needs_restart) need_restart_app = true;
                                }
                            }
                        }
                    }

                    current_y += 60;
                }
            }  
        }

        // Обновление индекса для отображения динамических описаний
        selected_setting_idx = -1;
        float test_y = 40;
        for (size_t i = 0; i < settings.size(); ++i) {
            if (sf::FloatRect(20, test_y, 760, 50).contains(mousePos.x, mousePos.y)) {
                selected_setting_idx = i;
            }
            test_y += 60;
        }

        // --- ОТРИСОВКА ---
        window.clear(sf::Color(30, 30, 30));

        // Отрисовка элементов списка
        float current_y = 40;
        for (const auto& s : settings) {
            // Название параметра
            sf::Text txtName(sf::String::fromUtf8(s.name.begin(), s.name.end()), font, 18);
            txtName.setPosition(20, current_y);
            window.draw(txtName);

            // Отрисовка элементов в зависимости от типа ввода
            if (s.input_type == InputType::Checkbox || s.input_type == InputType::YesNo) {
                sf::RectangleShape box({24, 24});
                box.setPosition(300, current_y);
                box.setFillColor(s.value == "true" ? sf::Color::Green : sf::Color::Red);
                window.draw(box);

                std::string status_label = (s.input_type == InputType::YesNo) ? 
                    (s.value == "true" ? "ДА" : "НЕТ") : (s.value == "true" ? "[√]" : "[×]");
                sf::Text txtStatus(sf::String::fromUtf8(status_label.begin(), status_label.end()), font, 16);
                txtStatus.setPosition(340, current_y);
                window.draw(txtStatus);
            } 
            else if (s.input_type == InputType::Slider) {
                // 1. Отрисовка фоновой линии слайдера (длина 200 пикселей)
                sf::RectangleShape line({200.0f, 6.0f});
                line.setPosition(300.0f, current_y + 10.0f);
                line.setFillColor(sf::Color(100, 100, 100));
                window.draw(line);

                // Вычисляем процент заполнения (от 0.0f до 1.0f) для правильного позиционирования кружка
                float pct = 0.0f;

                if (!s.options.empty()) {
                    // --- РАСЧЕТ ДЛЯ ДИСКРЕТНОГО СЛАЙДЕРА ---
                    // Ищем, на каком месте (индексе) стоит текущее значение s.value в массиве вариантов
                    auto it = std::find(s.options.begin(), s.options.end(), s.value);
                    if (it != s.options.end()) {
                        float idx = static_cast<float>(std::distance(s.options.begin(), it));
                        float total_steps = static_cast<float>(s.options.size());
                        pct = idx / (total_steps - 1.0f); // Процент равен: текущий_шаг / максимальные_шаги
                    }
                } 
                else {
                    // --- РАСЧЕТ ДЛЯ СВОБОДНОГО СЛАЙДЕРА ---
                    if (s.max_val != s.min_val) { // Защита от деления на ноль
                        float val_f = std::stof(s.value);
                        pct = (val_f - static_cast<float>(s.min_val)) / static_cast<float>(s.max_val - s.min_val);
                    }
                }

                // Ограничиваем процент в рамках разумного [0.0; 1.0] на всякий случай
                if (pct < 0.0f) pct = 0.0f;
                if (pct > 1.0f) pct = 1.0f;

                // 2. Отрисовка интерактивного кружка (кружок радиусом 8 пикселей)
                sf::CircleShape handle(8.0f);
                // Позиция: X = начало_слайдера + (процент * ширина) - радиус (центрирование)
                handle.setPosition(300.0f + (pct * 200.0f) - 8.0f, current_y + 5.0f);
                handle.setFillColor(sf::Color::Cyan);
                window.draw(handle);

                // 3. Вывод текущего числового значения текстом справа от слайдера
                sf::Text txtVal(s.value, font, 16);
                txtVal.setPosition(515.0f, current_y);
                txtVal.setFillColor(sf::Color::White);
                window.draw(txtVal);
            }
            else {
                // Заглушка для текстовых полей ввода
                sf::Text txtVal(sf::String::fromUtf8(s.value.begin(), s.value.end()), font, 16);
                txtVal.setPosition(300, current_y);
                window.draw(txtVal);
            }

            // Маленькая кнопка одиночного сброса рядом с элементом
            sf::RectangleShape resetBtn({110, 25});
            resetBtn.setPosition(670, current_y);
            resetBtn.setFillColor(sf::Color(50, 50, 50));
            window.draw(resetBtn);
            
            sf::Text resetLabel(sf::String::fromUtf8("Сбросить", "Сбросить"), font, 12);
            resetLabel.setPosition(695, current_y + 4);
            window.draw(resetLabel);

            current_y += 60;
        }

        // Вывод нижней панели описания при наведении
        sf::RectangleShape descBox({760, 60});
        descBox.setPosition(20, 450);
        descBox.setFillColor(sf::Color(20, 20, 20));
        descBox.setOutlineThickness(1);
        descBox.setOutlineColor(sf::Color(60, 60, 60));
        window.draw(descBox);

        if (selected_setting_idx != -1) {
            sf::Text txtDesc(sf::String::fromUtf8(settings[selected_setting_idx].description.begin(), settings[selected_setting_idx].description.end()), font, 14);
            txtDesc.setPosition(30, 460);
            txtDesc.setFillColor(sf::Color(200, 200, 200));
            window.draw(txtDesc);
        }

        // Отрисовка глобальной кнопки сброса
        UIButton btnResetAll(20, 530, 240, 40, "Сбросить все настройки", font);
        window.draw(btnResetAll.rect);
        window.draw(btnResetAll.text);

        // Отрисовка кнопки Перезагрузка при необходимости
        UIButton btnRestart(540, 530, 240, 40, (need_restart_app? "Перезагрузить": "Применить изменения"), font);
        btnRestart.rect.setFillColor(sf::Color(180, 50, 50));
        window.draw(btnRestart.rect);
        window.draw(btnRestart.text);

        window.display();
    }
}
