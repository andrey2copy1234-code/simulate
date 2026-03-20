// cd C:/Users/Azerty/Desktop/Программы/filescpp; g++ simulate.cpp -o simulate.exe -I c:\Users\Azerty\Downloads\SFML-2.6.1/include -L c:\Users\Azerty\Downloads\SFML-2.6.1/lib -lsfml-graphics-d -lsfml-window-d -lsfml-system-d -lopengl32 -lwinmm -lgdi32 -lcomdlg32
#include "simulate_imports.h"
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cstdio>
// #include "gpu_loader.cpp"
//in_simulate_imports:
// #include <SFML/Graphics.hpp>
// #include <vector>
// #include <cmath>
// #include <random>
// #include <limits>
// #include <iostream>
// #include <mutex>
using Vector2d = sf::Vector2<double>;
double chaos = 410;
std::mutex m;
int width = 800;
int height = 600;
#define random_molec (planets_mode ? []() { \
    int r = random_int(0, 99); \
    if (r < 1) { /* 1% Звезды: честный рандом по всему полю видимости */ \
        return Molecule(random_double(0, width / scale), random_double(0, height / scale), \
                        random_double(1e29, 2e30), random_double(-0.01*chaos, 0.01*chaos), random_double(-0.01*chaos, 0.01*chaos), false); \
    } else if (r < 20) { /* 19% Газовые гиганты: честный рандом по всему полю видимости */ \
        return Molecule(random_double(0, width / scale), random_double(0, height / scale), \
                        random_double(1e26, 1e28), random_double(-0.01*chaos, 0.01*chaos), random_double(-0.01*chaos, 0.01*chaos), false); \
    } else if (r < 80) { /* 60% Планеты */ \
        return Molecule(random_double(0, width / scale), random_double(0, height / scale), \
                        random_double(1e23, 1e25), random_double(-0.1*chaos, 0.1*chaos), random_double(-0.1*chaos, 0.1*chaos), false); \
    } else { /* 20% Астероиды */ \
        return Molecule(random_double(0, width / scale), random_double(0, height / scale), \
                        random_double(1e15, 1e18), random_double(-0.15*chaos, 0.15*chaos), random_double(-0.15*chaos, 0.15*chaos), false); \
    } \
}() : Molecule((double)random_int(0, width / scale), (double)random_int(0, height / scale), \
              (double)random_int(2, 5), (double)random_int(-5, 5), (double)random_int(-5, 5), rand() % 2 == 1))
struct Visuals {
    sf::Color core_color;
    sf::Color glow_color;
    float glow_factor; // Во сколько раз свечение больше радиуса объекта
};

Visuals get_object_params(double m) {
    const double M_EARTH = 5.972e24;
    const double M_JUPITER = 1.898e27;
    const double M_SUN = 1.989e30;

    // 1. АСТЕРОИДЫ И МАЛЫЕ ЛУНЫ (Серые, без свечения)
    if (m < 0.01 * M_EARTH) {
        return { sf::Color(120, 120, 120), sf::Color::Transparent, 0.0f };
    }
    
    // 2. КАМЕНИСТЫЕ ПЛАНЕТЫ (Земного типа - голубоватые/коричневые)
    if (m < 15.0 * M_EARTH) {
        return { sf::Color(100, 150, 255), sf::Color(100, 150, 255, 30), 1.1f };
    }

    // 3. ГАЗОВЫЕ ГИГАНТЫ
    if (m < 0.08 * M_SUN) {
        // тепло-оранжевый
        return { sf::Color(255, 140, 50), sf::Color(255, 100, 0, 60), 1.2f };
    }

    // 4. ЗВЕЗДЫ (Классификация по массе)
    if (m < 0.8 * M_SUN) { // Красные/Оранжевые карлики
        return { sf::Color(255, 60, 0), sf::Color(255, 30, 0, 100), 1.3f };
    }
    if (m < 1.2 * M_SUN) { // Желтые звезды (как Солнце)
        return { sf::Color(255, 255, 200), sf::Color(255, 255, 100, 0), 1.4f };
    }
    
    // Голубые гиганты (Самые яркие)
    return { sf::Color(180, 200, 255), sf::Color(100, 150, 255, 0), 1.5f };
}
inline auto div_vec(const sf::Vector2f& vec, double num) {
    return sf::Vector2f(vec.x/num, vec.y/num);
}
inline double magnitude(double x, double y) {
    return std::sqrt(x*x+y*y);
}
inline double magn(sf::Vector2f vec) {
    return magnitude(vec.x, vec.y);
}
inline double magn(Vector2d vec) {
    return magnitude(vec.x, vec.y);
}
const double strength = 6.67430e-11;
const int distance_find = 80;
double speed = 1;
double dt = 0.016;
double scale = 0.5;
bool planets_mode = false;
int cfps = 60;
bool full_mode = false;
double camx;
double camy;
inline auto to_coords(const Vector2d& vec, double size) {
    if (!full_mode) {
        return sf::Vector2f((vec.x-size)*scale, (vec.y-size)*scale);
    } else {
        return sf::Vector2f(width/2+(vec.x-size-camx)*scale, height/2+(vec.y-size-camy)*scale);
    }
}
inline auto from_coords(const sf::Vector2f& pixel_vec, double size) {
    if (!full_mode) {
        return Vector2d((pixel_vec.x / scale) + size, (pixel_vec.y / scale) + size);
    } else {
        return Vector2d(((pixel_vec.x-width/2) / scale) + size + camx, ((pixel_vec.y-height/2) / scale) + size + camy);
    }
}
class Molecule {
public:
    double x;
    double y;
    double sx;
    double sy;
    double acc_x;
    double acc_y;
    double size;
    bool en;
    Molecule() {
        this->x = 0;
        this->y = 0;
        this->size = 0;
        this->sx = 0;
        this->sy = 0;
    }
    Molecule(double x, double y, double size, double sx, double sy, bool en): acc_x(0), acc_y(0) {
        this->x = x;
        this->y = y;
        this->size = size;
        this->sx = sx;
        this->sy = sy;
        this->en = en;
    }
    // void attract(double other_x, double other_y, double other_mass) {
    //     double dx = other_x - x;
    //     double dy = other_y - y;
    //     double dist_sq = dx * dx + dy * dy;
    //     double dist = std::sqrt(dist_sq);
        
    //     // Закон всемирного тяготения (G = 1 для простоты)
    //     double force = (other_mass*strength/1e9) / dist_sq; 
        
    //     acc_x += force * (dx / dist);
    //     acc_y += force * (dy / dist);
    // }
    bool attract(const Molecule& other) {
        const double dx = other.x-this->x;
        const double dy = other.y-this->y;
        double distanceSq = dx*dx+dy*dy;
        double distance = sqrt(distanceSq);
        if (planets_mode && distance<=get_r()+other.get_r()) {
            return true;
        }
        if (!planets_mode) {
            const double base = (this->size+other.size)*1.5+1;
            distance++;
            if (distance<base) {
                
                distance = -(base-distance+1);
            }
            
            if (other.en) {
                distance = -distance;
            }
        }

        // const double direction = atan2(other.x-this->x, other.y-this->y);
        // double dx = other.x - this->x;
        // double dy = other.y - this->y;
        // // double distSq = dx*dx + dy*dy;
        // double force = (other.size * (strength_all / 1e9)) / distance;

        // this->acc_x += force * dx/distance;
        // this->acc_y += force * dy/distance;
        double inv = 1.0/distance;
        const double directionx = (other.x-this->x)*inv;
        const double directiony = (other.y-this->y)*inv;
        const double ratio_mass = (other.size*strength/1e9)*inv*inv;
        //std::scoped_lock lock(m);
        this->acc_x += directionx*ratio_mass;
        // this->sx += sin(direction)*strength*ratio_mass*dt*speed/distance;
        this->acc_y += directiony*ratio_mass;
        // this->sy += cos(direction)*strength*ratio_mass*dt*speed/distance;
        return false;
    }
    std::pair<double, double> gravity_start() {
        double acc_x_past = this->acc_x;
        double acc_y_past = this->acc_y;
        this->acc_x = 0;
        this->acc_y = 0;
        return {acc_x_past, acc_y_past};
    }
    void gravity_end(std::pair<double, double> past_acc) {
        // это Эйлер (ошибки на маленьком dt огромны)
        // this->sx += this->acc_x*speed*dt;
        // this->sy += this->acc_y*speed*dt;
        // а это Верле (точнее)
        double calc = speed*dt;
        this->sx += 0.5*(this->acc_x+past_acc.first)*calc;
        this->sy += 0.5*(this->acc_y+past_acc.second)*calc;
    }


    inline void forward() {
        // это Эйлер
        // this->x += this->sx*dt*speed;
        // this->y += this->sy*dt*speed;
        // а это Верле
        double s_all = dt*speed;
        this->x += this->sx*s_all+0.5*this->acc_x*s_all*s_all;
        this->y += this->sy*s_all+0.5*this->acc_y*s_all*s_all;
    }
    void draw(sf::RenderTexture& r) {
        double visualRadius = get_r();
        sf::CircleShape circle(std::max(visualRadius * scale, 2.0));

        circle.setPosition(to_coords(Vector2d(this->x, this->y), visualRadius));
        Visuals visuals;
        if (planets_mode) {
            visuals = get_object_params(this->size);
        }
        sf::Color color;
        if (planets_mode) {
            color = visuals.core_color;
        } else {
            color = this->en ? sf::Color::Red : sf::Color::Green;
        }
        
        circle.setFillColor(color);
        if (planets_mode && visuals.glow_factor!=0 && visualRadius*scale>=2) {
            sf::Vector2f pos = to_coords(Vector2d(this->x, this->y), 0);
            sf::VertexArray glow(sf::TriangleFan, 32);
            sf::Color coreColor = visuals.core_color;
            sf::Color edgeColor = visuals.glow_color;

            glow[0].position = pos;
            glow[0].color = coreColor;

            for (int i = 1; i < 32; ++i) {
                float angle = i * 2 * 3.14159f / 30;
                glow[i].position = pos + sf::Vector2f(cos(angle), sin(angle)) * (float)(visualRadius*visuals.glow_factor*scale);
                glow[i].color = edgeColor;
            }
            r.draw(glow);
        }
        r.draw(circle);
    }
    // inline bool is_in_cache_r() const {
    //     if (molecs.empty()) return false;
        
    //     // Приводим всё к единому типу const Molecule*
    //     const Molecule* start = (const Molecule*)molecs.data(); 
    //     const Molecule* end   = start + molecs.size(); // Указывает на "за край" вектора
    //     const Molecule* curr  = this;

    //     return (curr >= start && curr < end);
    // }
    inline bool is_in_cache_r() const;

    // inline double get_r() const {
    //     if (is_in_cache_r()) {
    //         return r_cache[this - (const Molecule*)molecs.data()];
    //     } else {
    //         return get_r();
    //     }
    // }
    inline double get_r() const;
    inline double get_r_real() const {
        if (!planets_mode) {
            return this->size;
        }
        if (!full_mode) {
            return (log10(this->size)-12);
        }
        // Константы масс (кг)
        const double M_EARTH = 5.972e24;
        const double M_SUN   = 1.989e30;

        double rho; // Плотность (кг/м3)

        // Классификация
        if (this->size < 0.01 * M_EARTH) {
            rho = 3000.0;   // Астероиды / Луны
        } else if (this->size < 15.0 * M_EARTH) {
            rho = 5514.0;   // Каменистые (Земного типа)
        } else if (this->size < 0.08 * M_SUN) {
            rho = 1327.0;   // Газовые гиганты
        } else {
            rho = 1408.0;   // Звезды (типа Солнца)
        }

        // Формула: r = куб_корень(3m / 4πρ)
        double volume = this->size / rho;
        double radius = std::cbrt((3.0 * volume) / (4.0 * M_PI));

        return radius/1000.0;
    }
    double getMaterialCohesionForce() const {
        double radius_m = get_r();
        double area = 3.14159265 * radius_m * radius_m;
        
        double sigma = 10.0;

        if (this->size < 1e15) { 
            sigma = 1e7;
        }
        else if (this->size < 1e21) {
            sigma = 1e3; // Очень слабая связность
        }
        else if (this->size < 1e25) {
            sigma = 1e5;
        }

        return sigma * area;
    }
    double get_prilive_strengh(const Molecule& other) const {
        double radius = get_r();
        double dx = this->x-other.x;
        double dy = this->y-other.y;
        double distSq = dx*dx+dy*dy;
        double distCb = distSq*sqrt(distSq);
        double f_prilive = 2*strength/1e6*other.size*this->size*radius/distCb;
        double f_grav = strength/1e6*this->size*this->size/radius/radius;
        return f_prilive-f_grav;
    }
};
std::vector<Molecule> molecs;
std::vector<double> r_cache;
inline bool Molecule::is_in_cache_r() const {
    if (molecs.empty()) return false;
    
    // Приводим всё к единому типу const Molecule*
    const Molecule* start = (const Molecule*)molecs.data(); 
    const Molecule* end   = start + molecs.size(); // Указывает на "за край" вектора
    const Molecule* curr  = this;

    return (curr >= start && curr < end);
}
inline double Molecule::get_r() const {
    if (is_in_cache_r()) {
        return r_cache[this - (const Molecule*)molecs.data()];
    } else {
        return get_r_real();
    }
}
int random_int(int min_val, int max_val) {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(min_val, max_val);
    return dist(rd); 
}
double random_double(double min_val, double max_val) {
    // Статический генератор инициализируется один раз при первом вызове
    static std::mt19937_64 gen(std::random_device{}());
    
    // Распределение для double (работает с любыми порядками: от 1e-20 до 1e30)
    std::uniform_real_distribution<double> dis(min_val, max_val);
    
    return dis(gen);
}
std::string* keyToString(sf::Keyboard::Key keyCode) {
    if (keyCode >= sf::Keyboard::Num0 && keyCode <= sf::Keyboard::Num9) {
        return new std::string(1, static_cast<char>('0' + (keyCode - sf::Keyboard::Num0)));
    }
    return nullptr;
}
sf::Font defaultFont;
int getNumber(const std::string& title, const std::string& content, sf::RenderWindow& mainWindow) {
    sf::VideoMode vidioMode(300, 200);
    sf::RenderWindow input_window(vidioMode, title);
    input_window.setFramerateLimit(30);

    sf::Text contentText;
    contentText.setString(content);
    contentText.setCharacterSize(std::min(20, (int)(600/content.length())));
    contentText.setFont(defaultFont);
    contentText.setPosition(sf::Vector2f(10.f, 10.f)); 
    contentText.setFillColor(sf::Color::Black);
    sf::Text inputText;
    inputText.setString("Input: ");
    inputText.setCharacterSize(20);
    inputText.setPosition(sf::Vector2f(5, 40));
    inputText.setFont(defaultFont);
    inputText.setFillColor(sf::Color::Black);
    std::string input_string = "";
    sf::Event event;
    while (input_window.isOpen() && mainWindow.isOpen()) {
        while (input_window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                input_string = "";
                input_window.close();
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Enter) {
                    input_window.close();
                } else if (event.key.code == sf::Keyboard::BackSpace) {
                    if (input_string != "") {
                        input_string.pop_back();
                        inputText.setString("Input: "+input_string);
                    }
                } else if (event.key.code == sf::Keyboard::Subtract) {
                    input_string += "-";
                    inputText.setString("Input: "+input_string);
                }
                std::string* char_string = keyToString(event.key.code);
                if (char_string != nullptr) {
                    std::string string = *char_string;
                    input_string.append(string);
                    delete char_string;
                    inputText.setString("Input: "+input_string);
                }
            }

        }
        while (mainWindow.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                input_window.close();
                mainWindow.close();
            }
        }
        input_window.clear(sf::Color::White);
        input_window.draw(contentText);
        input_window.draw(inputText);
        input_window.display();
        //mainWindow.display();
    }
    if (input_string == "") {
        return -1;
    }
    try {
        return std::stoi(input_string);
    } catch (std::exception& _) {
        return -1;
    }
}
double getDouble(const std::string& title, const std::string& content, sf::RenderWindow& mainWindow) {
    sf::VideoMode vidioMode(300, 200);
    sf::RenderWindow input_window(vidioMode, title);
    input_window.setFramerateLimit(30);

    sf::Text contentText;
    contentText.setString(content);
    contentText.setCharacterSize(std::min(20, (int)(600/content.length())));
    contentText.setFont(defaultFont);
    contentText.setPosition(sf::Vector2f(10.f, 10.f)); 
    contentText.setFillColor(sf::Color::Black);
    sf::Text inputText;
    inputText.setString("Input: ");
    inputText.setCharacterSize(20);
    inputText.setPosition(sf::Vector2f(5, 40));
    inputText.setFont(defaultFont);
    inputText.setFillColor(sf::Color::Black);
    std::string input_string = "";
    sf::Event event;
    while (input_window.isOpen() && mainWindow.isOpen()) {
        while (input_window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                input_string = "";
                input_window.close();
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Enter) {
                    input_window.close();
                } else if (event.key.code == sf::Keyboard::BackSpace) {
                    if (input_string != "") {
                        input_string.pop_back();
                        inputText.setString("Input: "+input_string);
                    }
                } else if (event.key.code == sf::Keyboard::Subtract) {
                    if (input_string.empty() || input_string[input_string.size()-1]=='e') {
                        input_string += "-";
                        inputText.setString("Input: "+input_string);
                    }
                } else if (event.key.code == sf::Keyboard::E) {
                    if (input_string.find("e")==-1 && !input_string.empty() && (input_string.length()!=1 || input_string[0]!='-')) {
                        input_string += "e";
                        inputText.setString("Input: "+input_string);
                    }
                }
                std::string* char_string = keyToString(event.key.code);
                if (char_string != nullptr) {
                    std::string string = *char_string;
                    input_string.append(string);
                    delete char_string;
                    inputText.setString("Input: "+input_string);
                }
            }

        }
        while (mainWindow.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                input_window.close();
                mainWindow.close();
            }
        }
        input_window.clear(sf::Color::White);
        input_window.draw(contentText);
        input_window.draw(inputText);
        input_window.display();
        //mainWindow.display();
    }
    if (input_string == "") {
        return -1;
    }
    try {
        return std::stod(input_string);
    } catch (std::exception& _) {
        return -1;
    }
}
void generateCosmos(std::vector<Molecule>& molecs, double width, double height, sf::RenderWindow& window) {
    int density = getNumber("Density", "1: Sparse, 2: Normal, 3: Chaos", window);
    if (density <= 0) density = 1;
    srand(getNumber("Seed", "Enter seed:", window));

    molecs.clear();
    const double G = 6.6743e-20; 

    // Разбрасываем системы по огромному полю (теперь это реально космос)
    int numSystems = (int)((width * height) / 5e6) * density;
    if (numSystems < 1) numSystems = 1;

    for (int s = 0; s < numSystems; ++s) {
        double marginX = width * 0.3;
        double marginY = height * 0.3;

        // Диапазон для рандома: от margin до (width - margin)
        double centerX = marginX + (rand() % (int)(width - 2 * marginX));
        double centerY = marginY + (rand() % (int)(height - 2 * marginY));
        
        // ЗВЕЗДА (Умеренная, чтобы не ломать dt)
        double starMass = 1e25+(rand()%20)*1e25;

        molecs.push_back({centerX, centerY, starMass, 0, 0, false});

        // ПЛАНЕТЫ на ДИСТАНЦИИ (в километрах)
        // int planetsCount = 2 + (rand() % 3);
        int planetsCount = 1;
        for (int p = 0; p < planetsCount; ++p) {
            // Расстояние от 2000 до 8000 км (теперь они не "трутся" боками)
            double r = 200.0 + (p * 200) + (rand() % 20); 
            double angle = (rand() % 360) * 3.1415 / 180.0;
            
            double m = 1e22+(rand()%50)*1e22;
            // v = sqrt(G * M / r) — теперь r адекватный
            double vOrb = sqrt((G/1000000000) * (starMass + m)  / r);

            double px = centerX + r * cos(angle);
            double py = centerY + r * sin(angle);
            double pvx = -sin(angle) * vOrb;
            double pvy =  cos(angle) * vOrb;
            // double pvy = 0 - sin(angle) * vOrb;
            // double pvx = 0 + cos(angle) * vOrb;
            std::cout << vOrb << std::endl;

            molecs.push_back({px, py, m, pvx, pvy, false}); // Планета 10^12 кг
        }
    }

    // // АСТЕРОИДЫ (Рассыпаны далеко)
    // for (int a = 0; a < 20 * density; ++a) {
    //     double x = (rand() % (int)width);
    //     double y = (rand() % (int)height);
    //     molecs.push_back({x, y, 1e12+(rand()%1000)*1e12, (rand()%200-100)/500.0, (rand()%200-100)/500.0, false});
    // }
}
std::optional<std::string> SaveFile(const sf::RenderWindow& window) {
    // Используем char (узкие символы)
    char filePath[MAX_PATH] = {0}; 
    HWND id_win = window.getSystemHandle();
    
    // Используем OPENFILENAMEA (A = ANSI/узкие символы)
    OPENFILENAMEA ofn = {0}; 
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = id_win;
    ofn.lpstrFile = filePath; // ОК: LPSTR = char*
    ofn.nMaxFile = MAX_PATH;
    
    // Фильтр — обычные строки (без префикса L)
    ofn.lpstrFilter = "Binary Data Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    // Вызываем GetSaveFileNameA
    if (GetSaveFileNameA(&ofn) == TRUE) { 
        return std::string(filePath); // ОК: возвращаем std::string
    } else {
        return std::nullopt;
    }
}

// Функция возвращает std::string
std::optional<std::string> OpenFile(const sf::RenderWindow& window) {
    // Используем char (узкие символы)
    char filePath[MAX_PATH] = {0};
    HWND id_win = window.getSystemHandle();
    
    // Используем OPENFILENAMEA (A = ANSI/узкие символы)
    OPENFILENAMEA ofn = {0}; 
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = id_win;
    ofn.lpstrFile = filePath; // ОК: LPSTR = char*
    ofn.nMaxFile = MAX_PATH;
    
    // Фильтр — обычные строки (без префикса L)
    ofn.lpstrFilter = "Binary Data Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    // Вызываем GetOpenFileNameA
    if (GetOpenFileNameA(&ofn) == TRUE) { 
        return std::string(filePath); // ОК: возвращаем std::string
    } else {
        return std::nullopt;
    }
}
std::string format_dist(double val) {
    std::stringstream ss;
    ss << std::setprecision(15) << (std::round(val * 10.0) / 10.0); 
    std::string s = ss.str();
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    int posf = s.find(".");
    if (posf==-1) {
        posf = s.size();
    }
    for (int i = posf-3; i>=0; i-=3) {
        s.insert(i, " ");
    }
    return s;
}
std::string format_sspeed(double s) {
    if (s == 0) return "0 sec/s (Pause)";
    
    double abs_s = std::abs(s);
    std::string unit = " sec/s";
    double display_val = abs_s;

    // --- ОПРЕДЕЛЕНИЕ МАСШТАБА ---
    if (abs_s >= 31536000000.0) { // Больше 1000 лет
        display_val /= 31536000.0;
        unit = " years/s"; 
    }
    else if (abs_s >= 31536000.0) { // Больше года
        display_val /= 31536000.0;
        unit = " year/s";
    }
    else if (abs_s >= 86400.0) { // Больше дня
        display_val /= 86400.0;
        unit = " days/s";
    }
    else if (abs_s >= 3600.0) { // Больше часа
        display_val /= 3600.0;
        unit = " hours/s";
    }
    else if (abs_s >= 60.0) { // Больше минуты
        display_val /= 60.0;
        unit = " min/s";
    }
    else if (abs_s < 1e-6) { // Наносекунды
        display_val *= 1e9;
        unit = " ns/s";
    }
    else if (abs_s < 1e-3) { // Микросекунды
        display_val *= 1e6;
        unit = " us/s";
    }
    else if (abs_s < 1.0) { // Миллисекунды
        display_val *= 1e3;
        unit = " ms/s";
    }

    // --- ФОРМАТИРОВАНИЕ ЧИСЛА ---
    std::stringstream ss;
    // Используем 3 значащих цифры, чтобы видеть разницу (например, 1.25 или 125.5)
    ss << std::setprecision(4) << display_val;
    std::string s_res = ss.str();

    // Чистим хвосты от точек, если число целое
    if (s_res.find('.') != std::string::npos) {
        while (s_res.back() == '0') s_res.pop_back();
        if (s_res.back() == '.') s_res.pop_back();
    }

    return s_res + unit;
}
std::string format_mass(double m) {
    if (m == 0) return "0 kg";

    std::stringstream ss;
    
    // Если масса огромная (больше миллиона кг), уходим в научный формат
    if (m >= 1e6 || m <= 1e-3) {
        // Выведет тип 1.99e+30
        ss << std::scientific << std::setprecision(2) << m;
    } else {
        // Для обычных "бытовых" масс выводим просто число
        ss << std::fixed << std::setprecision(1) << m;
        std::string s = ss.str();
        if (s.find('.') != std::string::npos) {
            while (s.back() == '0') s.pop_back();
            if (s.back() == '.') s.pop_back();
        }
        return s + " kg";
    }

    return ss.str() + " kg";
}
std::string format_speed(double v) {
    if (v == 0) return "0 m/s";

    double abs_v = std::abs(v);
    std::string unit = " m/s";
    double display_v = abs_v;

    // Если скорость больше 1000 м/с, переходим на км/с
    if (abs_v >= 1000.0) {
        display_v /= 1000.0;
        unit = " km/s";
    }

    std::stringstream ss;
    // Максимум 1 знак после запятой
    ss << std::fixed << std::setprecision(1) << display_v;
    std::string s = ss.str();

    // Убираем ".0", если число целое (например, 7.0 -> 7)
    if (s.size() > 2 && s.substr(s.size() - 2) == ".0") {
        s.erase(s.size() - 2);
    }

    return s + unit;
}
void drawArrow(sf::RenderTarget& target, sf::Vector2f start, sf::Vector2f end, sf::Color color) {
    sf::Vertex line[] = {
        sf::Vertex(start, color),
        sf::Vertex(end, color)
    };
    target.draw(line, 2, sf::Lines);

    // Вектор от начала к концу
    sf::Vector2f dir = end - start;
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);

    if (len < 1.0f) return; // Слишком короткая стрелка

    // Параметры наконечника
    float arrowSize = std::min(len * 0.25f, 15.0f); // наконечник не больше 25% длины или 15 пикс
    float angle = 0.4f; // Угол раскрытия (~23 градуса)
    
    // Нормализованный обратный вектор (от острия к началу)
    sf::Vector2f unitDir = -dir / len;

    // Поворот вектора для создания "усиков"
    auto rotate = [](sf::Vector2f v, float a) {
        return sf::Vector2f(v.x * cos(a) - v.y * sin(a), v.x * sin(a) + v.y * cos(a));
    };

    sf::Vertex head[] = {
        sf::Vertex(end, color),
        sf::Vertex(end + rotate(unitDir, angle) * arrowSize, color),
        sf::Vertex(end, color),
        sf::Vertex(end + rotate(unitDir, -angle) * arrowSize, color)
    };
    target.draw(head, 4, sf::Lines);
}
std::wstring helpData = 
L"# Режимы\n"
"* Планетный - симулируются планеты\n"
"- с full - без границ\n"
"- без full - с границами\n"
"* Молекульный - симулируются молекулы\n"
"# Горячии клавиши\n"
"* p - Переключение режима планет на молекулы и обратно\n"
"* d - Показ плотности (для молекул)\n"
"* t - Показ температуры (для молекул)\n"
"* e - Отдалить\n"
"* q - Приблизить\n"
"* Control-s - Сохранить в файл позицию симуляции\n"
"* Control-l - Загрузить из файла позицию симуляции (перед эти переключите на нужный режим)\n"
"* i - Измерить растояние\n"
"* Shift-i - Показать информацию о планете/молекуле\n"
"* c - Создать молекулу/планету\n"
"* m - Выбрать массу создаваймой планеты\n"
"* Left - Замедлить время\n"
"* Right - Ускорить время\n"
"* l - показать информацию о симуляции в консоль (кол-во об., ср. x, ср. y, энерия)\n"
"* r - сгенерировать n рандомных объектов\n"
"* g - сгенерировать систему\n"
"* Control-h - вызвать это\n"
"* Control-h - вызвать это\n"
"* h - скрыть/показать объекты\n"
"* a - Увеличить chaos (насколько быстро при r летают объекты)\n"
"* Shift-a - Уменьшить chaos (насколько быстро при r летают объекты)\n"
"* f - Настроить новый FPS\n"
"* Esc - Остановить/возобновить симуляцию\n"
"* Alt-f - Переключится между full режимом и не full (рекоминдуется не нажимать на режиме молекул)\n"
"* s - показ статики (сколько объектов какого класса)\n"
"* Shift-r - режим удаления\n"
"* + - увеличить кисть удаления\n"
"* - - увеличить кисть удаления\n"
"# Управление (с full):\n"
"* колёсико мыши - приближение/отдаление из точки куда указывает мышка\n"
;
void showHelp() {
    sf::RenderWindow helpWindow(sf::VideoMode(640, 480), L"Справка", sf::Style::Titlebar | sf::Style::Close);
    helpWindow.setFramerateLimit(60);

    std::vector<sf::Text> textLines;
    float currentY = 20.f;
    float margin = 20.f;
    float maxWidth = 600.f;

    // Парсим текст
    std::wstringstream ss(helpData);
    std::wstring line;
    while (std::getline(ss, line)) {
        sf::Text text;
        text.setFont(defaultFont);
        text.setFillColor(sf::Color::White);
        
        unsigned int fontSize = 18;
        sf::Text::Style style = sf::Text::Regular;

        if (line.find(L"# ") == 0) {
            fontSize = 32; style = sf::Text::Bold; line = line.substr(2);
        } else if (line.find(L"## ") == 0) {
            fontSize = 24; style = sf::Text::Bold; line = line.substr(3);
        }

        text.setCharacterSize(fontSize);
        text.setStyle(style);
        
        // Логика переноса слов
        std::wstring wrappedLine;
        std::wstring word;
        std::wstringstream lineStream(line);
        
        while (lineStream >> word) {
            sf::Text temp = text;
            temp.setString(wrappedLine + word + L" ");
            if (temp.getGlobalBounds().width > maxWidth) {
                wrappedLine += L"\n";
            }
            wrappedLine += word + L" ";
        }

        text.setString(wrappedLine);
        text.setPosition(margin, currentY);
        textLines.push_back(text);
        
        currentY += text.getGlobalBounds().height + 15.f;
    }

    // Настройка прокрутки (View)
    sf::View view = helpWindow.getDefaultView();
    float scrollSpeed = 20.f;
    float totalHeight = currentY; 

    while (helpWindow.isOpen()) {
        sf::Event event;
        while (helpWindow.pollEvent(event)) {
            if (event.type == sf::Event::Closed) helpWindow.close();
            
            // Прокрутка колесиком мыши
            if (event.type == sf::Event::MouseWheelScrolled) {
                if (event.mouseWheelScroll.delta > 0 && view.getCenter().y > view.getSize().y / 2)
                    view.move(0, -scrollSpeed*event.mouseWheelScroll.delta);
                else if (event.mouseWheelScroll.delta < 0 && view.getCenter().y < totalHeight - view.getSize().y / 2)
                    view.move(0, -scrollSpeed*event.mouseWheelScroll.delta);
            }
        }

        helpWindow.clear(sf::Color(45, 45, 45));
        helpWindow.setView(view);
        for (const auto& t : textLines) helpWindow.draw(t);
        helpWindow.display();
    }
}
void printStats(const std::vector<Molecule>& molecs) {
    if (molecs.empty()) return;

    size_t total = molecs.size();
    int stars = 0, giants = 0, planets = 0, dust = 0;

    for (const auto& m : molecs) {
        if (m.size >= 1e29) stars++;
        else if (m.size >= 1e26) giants++;
        else if (m.size >= 1e22) planets++;
        else dust++;
    }

    auto pct = [&](int count) { return (count * 100.0) / total; };

    printf("\n--- System Status ---\n");
    printf("Total objects: %zu\n", total);
    printf("Stars:   %d (%.2f%%)\n", stars,   pct(stars));
    printf("Giants:  %d (%.2f%%)\n", giants,  pct(giants));
    printf("Planets: %d (%.2f%%)\n", planets, pct(planets));
    printf("Debris:  %d (%.2f%%)\n", dust,    pct(dust));
    printf("---------------------\n");
}
sf::ConvexShape createRoundedRect(float width, float height, float radius) {
    sf::ConvexShape shape;
    unsigned int quality = 10; // Сколько точек на угол
    shape.setPointCount(quality * 4);

    for (int i = 0; i < 4; i++) {
        float centerX = (i == 0 || i == 3) ? radius : width - radius;
        float centerY = (i == 0 || i == 1) ? radius : height - radius;
        for (int j = 0; j < quality; j++) {
            float angle = (i + (float)j / (quality - 1)) * 3.14159f / 2.0f + 3.14159f;
            shape.setPoint(i * quality + j, sf::Vector2f(centerX + radius * cos(angle), centerY + radius * sin(angle)));
        }
    }
    return shape;
}
sf::Text createTextForButton(std::string str, sf::Vector2f pos, float width, float height, float radius) {
    width -= radius;
    width -= radius;
    height -= radius;
    height -= radius;
    int size_char = width/str.length()*1.5;
    if (size_char>height) {
        size_char = height;
    }
    sf::Text text;
    text.setFont(defaultFont);
    text.setCharacterSize(size_char);
    text.setString(str);
    text.setPosition(pos.x+radius+(width-size_char/1.5*str.length())*0.5, pos.y+radius+(height-size_char)*0.5);
    return text;
}
bool is_click_button(sf::Vector2f pos, sf::Vector2f size, sf::Vector2f click) {
    return pos.x<=click.x && click.x<=pos.x+size.x && pos.y<=click.y && click.y<=pos.y+size.y;
}
const float padd_buttons_info = 5.0f;
const float round_buttons = 5.0f;
// buttons
const float button_view_width = 80;
const float button_view_height = 30;
int main() {
    defaultFont.loadFromFile("C:/Windows/Fonts/arial.ttf");
    molecs.resize(100);
    for (int i = 0; i!=100; i++) {
        molecs[i] = random_molec;
    }
    Vector2d pos_obj;
    bool hide_molecs = false;
    bool density_snow = false;
    bool temperature_snow = false;
    bool stop_simulate = false;

    bool create_mode = false;
    double obj_mass = 5;

    bool metr_mode = false;
    Vector2d start_pos_metr;
    Vector2d end_pos_metr;
    double distance_metr = 0;

    bool info_mode = false;
    bool view_mode = false;
    int info_planet_index = 0;

    bool remove_mode = false;
    double remove_size = 100000;

    int chunk = 5;
    sf::VideoMode vidioMode(800, 600);
    sf::View view(sf::FloatRect(0.f, 0.f, 800.f, 600.f));
    sf::RenderWindow window(vidioMode, "Simulate (SFML)");
    window.setFramerateLimit(30);
    sf::Clock clock;
    sf::RenderTexture renderTexture;
    if (!renderTexture.create(800, 600)) {
        std::cout << ":( что-то пошло не так с GPU" << std::endl;
        return -1;
    }
    sf::Vector2i lastMousePos;
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0.f, 0.f, event.size.width, event.size.height);
                width = event.size.width;
                height = event.size.height;
                window.setView(sf::View(visibleArea));
                renderTexture.create(width, height);
                clock.restart();
            } else if (event.type == sf::Event::MouseButtonPressed && !remove_mode) {
                const sf::Vector2f mouse_posp = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                const Molecule& molec = molecs[info_planet_index];

                float screen_radius = std::max(molec.get_r() * scale, 2.0);
                // screen_radius += 2;

                sf::Vector2f screen_pos = to_coords(Vector2d(molec.x, molec.y), 0);

                sf::Vector2f pos_button_view(screen_pos.x + screen_radius + 5.0f, screen_pos.y - 15.0f + 16.0f*2 + padd_buttons_info);
                if (info_mode && is_click_button(pos_button_view, sf::Vector2f(button_view_width, button_view_height), mouse_posp)) {
                    view_mode = !view_mode;
                } else {
                    info_mode = true;
                    Vector2d mouse_pos = from_coords(window.mapPixelToCoords(sf::Mouse::getPosition(window)), 0);
                    bool find = false;
                    double min_dist = HUGE_VAL;
                    for (int i = 0; i!=molecs.size(); i++) {
                        double dist = magnitude(mouse_pos.x-molecs[i].x, mouse_pos.y-molecs[i].y);
                        if (dist<=1.5*std::max(molecs[i].get_r()*scale, 2.0)/scale && min_dist>dist) {
                            info_planet_index = i;
                            min_dist = dist;
                            find = true;
                        }
                    }
                    if (!find) {
                        info_mode = false;
                        view_mode = false;
                    }
                }
            } else if (event.type == sf::Event::MouseMoved && !remove_mode) {
                if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                    Vector2d past_pos = from_coords(sf::Vector2f(lastMousePos), 0);
                    Vector2d new_pos = from_coords(sf::Vector2f(event.mouseMove.x, event.mouseMove.y), 0);
                    Vector2d diff = new_pos-past_pos;
                    camx -= diff.x;
                    camy -= diff.y;
                }
                lastMousePos = sf::Vector2i(event.mouseMove.x, event.mouseMove.y);
            } else if (event.type == sf::Event::MouseWheelScrolled) {
                bool isControl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
                const float sensor = 20;
                sf::Vector2f moved(0, 0);
                if ((isControl || abs(event.mouseWheelScroll.delta)==abs(round(event.mouseWheelScroll.delta))) && event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel && planets_mode) {
                    sf::Vector2f beforeCoord = window.mapPixelToCoords(sf::Mouse::getPosition(window));

                    Vector2d start_pos = from_coords(beforeCoord, 0);
                    if (event.mouseWheelScroll.delta>0) {
                        scale *= pow(1.2, event.mouseWheelScroll.delta);
                    } else {
                        scale /= pow(0.8, event.mouseWheelScroll.delta);
                    }
                    Vector2d end_pos = from_coords(beforeCoord, 0);
                    Vector2d move = start_pos-end_pos;
                    camx += move.x;
                    camy += move.y;
                } else if (!isControl && event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                    moved.y += event.mouseWheelScroll.delta*sensor;
                }  else if (!isControl && event.mouseWheelScroll.wheel == sf::Mouse::HorizontalWheel) {
                    moved.x += event.mouseWheelScroll.delta*sensor;
                }
                if (!(moved.x==0 && moved.y==0)) {
                    Vector2d past_pos = from_coords(sf::Vector2f(0, 0), 0);
                    Vector2d new_pos = from_coords(moved, 0);
                    Vector2d diff = new_pos-past_pos;
                    camx -= diff.x;
                    camy -= diff.y;
                }
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::E) {
                    scale /= 1.2;
                } else if (event.key.code == sf::Keyboard::Q) {
                    scale *= 1.2;
                } else if (event.key.code == sf::Keyboard::Left) {
                    speed /= 1.2;
                } else if (event.key.code == sf::Keyboard::Right) {
                    speed *= 1.2;
                } else if (event.key.code == sf::Keyboard::Subtract || event.key.code == sf::Keyboard::Hyphen) {
                    remove_size /= 1.2;
                } else if (event.key.code == sf::Keyboard::Add || (event.key.code == sf::Keyboard::Equal && event.key.shift)) {
                    remove_size *= 1.2;
                } else if (event.key.code == sf::Keyboard::R) {
                    if (!event.key.shift) {
                        int len = getNumber("Input", "Input lenght objects", window);
                        if (len != -1) {
                            info_mode = false;
                            if (len != -2) {
                                molecs.resize(len);
                            } else {
                                len = molecs.size();
                            }
                            for (int i = 0; i<len; i++) {
                                molecs[i] = random_molec;
                            }
                            camx = width/scale/2;
                            camy = width/scale/2;
                            double x_avg = 0;
                            double y_avg = 0;
                            for (const auto& molec: molecs) {
                                x_avg += molec.x;
                                y_avg += molec.y;
                            }
                            if (!molecs.empty()) {
                                std::cout << x_avg/molecs.size() << std::endl;
                                std::cout << y_avg/molecs.size() << std::endl;
                            }
                            double totalEk = 0;
                            double totalEp = 0;
                            const double G = 6.6743e-20; // Твоя константа для км

                            for (size_t i = 0; i < molecs.size(); ++i) {
                                // 1. Кинетическая энергия каждого тела: (m * v^2) / 2
                                double vSq = molecs[i].sx * molecs[i].sx + molecs[i].sy * molecs[i].sy;
                                totalEk += 0.5 * molecs[i].size * vSq;

                                // 2. Потенциальная энергия взаимодействия со ВСЕМИ остальными
                                // Считаем пары i и j только один раз (j = i + 1)
                                for (size_t j = i + 1; j < molecs.size(); ++j) {
                                    double dx = molecs[j].x - molecs[i].x;
                                    double dy = molecs[j].y - molecs[i].y;
                                    double dist = sqrt(dx*dx + dy*dy + 100.0); // Тот же softening, что в симуляции

                                    // Формула: Ep = -G * (m1 * m2) / r
                                    totalEp -= G * (molecs[i].size * molecs[j].size) / dist;
                                }
                            }
                            std::cout << "Energy:" << totalEk+totalEp << std::endl;
                        }
                    } else  {
                        remove_mode = !remove_mode;
                    }
                    clock.restart();
                } else if (event.key.code == sf::Keyboard::H) {
                    if (event.key.control) {
                        showHelp();
                    } else {
                        hide_molecs = !hide_molecs;
                    }
                } else if (event.key.code == sf::Keyboard::D) {
                    density_snow = !density_snow;
                } else if (event.key.code == sf::Keyboard::P) {
                    planets_mode = !planets_mode;
                    if (!planets_mode) {
                        full_mode = false;
                    }
                } else if (event.key.code == sf::Keyboard::C) {
                    create_mode = !create_mode;
                    if (create_mode) {
                        pos_obj = from_coords(window.mapPixelToCoords(sf::Mouse::getPosition(window)), 0);
                    } else {
                        auto pos_end = from_coords(window.mapPixelToCoords(sf::Mouse::getPosition(window)), 0);
                        auto speed = pos_end-pos_obj;
                        molecs.emplace_back(pos_obj.x, pos_obj.y, obj_mass, speed.x/(full_mode? 10000: 1), speed.y/(full_mode? 10000: 1), false);
                        // std::cout << "speed2:" << magnitude(molecs[molecs.size()-1].sx, molecs[molecs.size()-1].sy) << "km/c\n";
                    }
                } else if (event.key.code == sf::Keyboard::G) {
                    info_mode = false;
                    generateCosmos(molecs, 800/scale, 600/scale, window);
                } else if (event.key.code == sf::Keyboard::Space) {
                    create_mode = false;
                    info_mode = false;
                    metr_mode = false;
                    distance_metr = 0;
                    remove_mode = false;
                    view_mode = false;
                } else if (event.key.code == sf::Keyboard::Escape) {
                    stop_simulate = !stop_simulate;
                } else if (event.key.code == sf::Keyboard::M) {
                    double mass = getDouble("Input", "Input Mass", window);
                    if (mass>0) {
                        obj_mass = mass;
                    }
                    clock.restart();
                } else if (event.key.code == sf::Keyboard::T) {
                    temperature_snow = !temperature_snow;
                } else if (event.key.code == sf::Keyboard::F) {
                    if (event.key.alt) {
                        if (planets_mode) {
                            full_mode = !full_mode;
                            if (full_mode) {
                                camx = width/2;
                                camy = width/2;
                            }
                        }
                    } else {
                        int fps = getNumber("Input", "Input Fps", window);
                        if (fps>0) {
                            window.setFramerateLimit(fps);
                            cfps = fps;
                        }
                        clock.restart();
                    }
                } else if (event.key.control) {
                    if (event.key.code == sf::Keyboard::S) {
                        std::optional<std::string> filename = SaveFile(window);
                        if (filename) {
                            std::ofstream file(filename.value(), std::ios::binary);
                            const auto& vec = molecs;
                            size_t size = vec.size();
                            file.write(reinterpret_cast<const char*>(&size), sizeof(size));

                            if (!vec.empty()) {
                                file.write(reinterpret_cast<const char*>(vec.data()), sizeof(Molecule) * vec.size());
                            }
                            file.close();
                        }
                        clock.restart();
                    } else if (event.key.code == sf::Keyboard::L) {
                        std::optional<std::string> filename = OpenFile(window);
                        if (filename) {
                            // 1. Конвертируем string (ANSI) в wstring (UTF-16)
                            int size_needed = MultiByteToWideChar(CP_ACP, 0, filename->c_str(), -1, NULL, 0);
                            std::wstring wstr(size_needed, 0);
                            MultiByteToWideChar(CP_ACP, 0, filename->c_str(), -1, &wstr[0], size_needed);

                            // 2. Открываем файл через "широкий" указатель (Windows-way)
                            FILE* f = _wfopen(wstr.c_str(), L"rb");
                            if (f) {
                                size_t size = 0;
                                fread(&size, sizeof(size), 1, f);
                                
                                molecs.clear();
                                molecs.resize(size);
                                
                                if (size > 0) {
                                    fread(molecs.data(), sizeof(Molecule), size, f);
                                }
                                fclose(f);
                                std::cout << "load: " << size << " objects" << std::endl;

                            } else {
                                std::cerr << "I doт`t open file!" << std::endl;
                            }
                        }
                    }
                } else if (event.key.code == sf::Keyboard::L) {
                    std::cout << molecs.size() << std::endl;
                    double x_avg = 0;
                    double y_avg = 0;
                    for (const auto& molec: molecs) {
                        x_avg += molec.x;
                        y_avg += molec.y;
                    }
                    if (!molecs.empty()) {
                        std::cout << x_avg/molecs.size() << std::endl;
                        std::cout << y_avg/molecs.size() << std::endl;
                    }
                    double totalEk = 0;
                    double totalEp = 0;
                    const double G = 6.6743e-20; // Твоя константа для км

                    for (size_t i = 0; i < molecs.size(); ++i) {
                        // 1. Кинетическая энергия каждого тела: (m * v^2) / 2
                        double vSq = molecs[i].sx * molecs[i].sx + molecs[i].sy * molecs[i].sy;
                        totalEk += 0.5 * molecs[i].size * vSq;

                        // 2. Потенциальная энергия взаимодействия со ВСЕМИ остальными
                        // Считаем пары i и j только один раз (j = i + 1)
                        for (size_t j = i + 1; j < molecs.size(); ++j) {
                            double dx = molecs[j].x - molecs[i].x;
                            double dy = molecs[j].y - molecs[i].y;
                            double dist = sqrt(dx*dx + dy*dy);

                            // Формула: Ep = -G * (m1 * m2) / r
                            totalEp -= G * (molecs[i].size * molecs[j].size) / dist;
                        }
                    }
                    std::cout << "Energy:" << totalEk+totalEp << std::endl;
                } else if (event.key.code == sf::Keyboard::I) {
                    if (!event.key.shift) {
                        metr_mode = !metr_mode;
                        if (metr_mode) {
                            start_pos_metr = from_coords(window.mapPixelToCoords(sf::Mouse::getPosition(window)), 0);
                        } else {
                            end_pos_metr = from_coords(window.mapPixelToCoords(sf::Mouse::getPosition(window)), 0);
                            Vector2d dist_metr = end_pos_metr-start_pos_metr;
                            distance_metr = magnitude(dist_metr.x, dist_metr.y);
                        }
                    } else {
                        info_mode = !info_mode;
                        if (info_mode) {
                            Vector2d mouse_pos = from_coords(window.mapPixelToCoords(sf::Mouse::getPosition(window)), 0);
                            bool find = false;
                            double min_dist = HUGE_VAL;
                            for (int i = 0; i!=molecs.size(); i++) {
                                double dist = magnitude(mouse_pos.x-molecs[i].x, mouse_pos.y-molecs[i].y);
                                if (dist<=4*std::max(molecs[i].get_r()*scale, 2.0)/scale && min_dist>dist) {
                                    info_planet_index = i;
                                    min_dist = dist;
                                    find = true;
                                }
                            }
                            if (!find) {
                                info_mode = false;
                                view_mode = false;
                            }
                        }
                    }
                } else if (event.key.code == sf::Keyboard::A) {
                    if (event.key.shift) {
                        chaos /= 1.2;
                    } else {
                        chaos *= 1.2;
                    }
                } else if (event.key.code==sf::Keyboard::S) {
                    printStats(molecs);
                }
            }
        }
        sf::Time delta = clock.restart();
        dt = delta.asSeconds();
        // dt = 1.0/cfps;
        window.setTitle("Simulate (SFML) (FPS="+std::to_string(static_cast<int>(1/dt))+", Speed="+ format_sspeed(speed)+", chaos="+ format_dist(chaos)+", width="+ format_dist(width/scale) +" km)");
        if (!stop_simulate) {
            r_cache.resize(molecs.size());
            #pragma omp parallel for num_threads(4) schedule(static)
            for (int i = 0; i!=molecs.size(); i++) {
                r_cache[i] = molecs[i].get_r_real();
            }
            #pragma omp parallel for num_threads(4) schedule(static)
            for (auto& molec: molecs) {
                Vector2d pos_start(molec.x, molec.y);
                molec.forward();
                Vector2d pos_end(molec.x, molec.y);
                if (view_mode && &molecs[info_planet_index]==&molec) [[unlikely]] {
                    Vector2d diff = pos_end-pos_start;
                    camx += diff.x;
                    camy += diff.y;
                }
                if (!full_mode) {
                    if (molec.x<0 || molec.x>width/scale) {
                        molec.sx = -molec.sx;
                        if (molec.x<0) {
                            molec.x = 0;
                        } else {
                            molec.x = width/scale;
                        }
                    }
                    if (molec.y<0 || molec.y>height/scale) {
                        molec.sy = -molec.sy;
                        if (molec.y<0) {
                            molec.y = 0;
                        } else {
                            molec.y = height/scale;
                        }
                    }
                }
                // molec.forward();
            }
            if (full_mode) {
                std::vector<Molecule> need_create;
                #pragma omp parallel for num_threads(4) schedule(static)
                for (int i = 0; i < molecs.size(); ++i) { // Работаем через индекс для OpenMP
                    auto& molec = molecs[i];
                    if (molec.size<1e20) {
                        continue;
                    }
                    for (const auto& molec2 : molecs) {
                        if (&molec == &molec2) continue; // Не рвем сами себя

                        double prilive_strenght = molec.get_prilive_strengh(molec2);
                        // std::cout << prilive_strenght<< std::endl;
                        double max_f = molec.getMaterialCohesionForce();

                        if (prilive_strenght > max_f) {
                            double r_equil = sqrt((strength / 1e6 * molec.size) / (prilive_strenght / molec.get_r())); 
                            double delta_R = std::max(0.0, molec.get_r() - r_equil);

                            // 2. Считаем массу этого слоя (упрощенно как объем сферического слоя)
                            // dm = 4 * PI * R^2 * delta_R * плотность
                            double density = molec.size / (1.333 * 3.1415 * pow(molec.get_r(), 3));
                            double get_m = 4.0 * 3.1415 * pow(molec.get_r(), 2) * delta_R * density * dt * speed;

                            // 3. Ограничиваем, чтобы за один шаг не оторвать больше половины (защита интегратора)
                            get_m = std::min(get_m, molec.size * 0.5);
                            if (get_m>molec.size/100) {
                                // Расчет направления от звезды к планете
                                double dx = molec.x - molec2.x;
                                double dy = molec.y - molec2.y;
                                double dist = sqrt(dx*dx + dy*dy);
                                double ux = dx / dist; // Вектор направления
                                double uy = dy / dist;

                                // Разделяем общую массу отрыва на несколько мелких кусков
                                int num = 5; 
                                double fragment_m = get_m / num;

                                m.lock();
                                for (int f = 0; f < num; ++f) {
                                    Molecule new_molec = molec;
                                    new_molec.size = fragment_m;
                                    
                                    // 1. Позиция: выносим цепочкой вдоль приливной оси
                                    double side = (rand() % 2 == 0) ? 1.0 : -1.0; 
                                    // Разносим их так, чтобы они не касались друг друга и мамы
                                    double spawn_dist = molec.get_r() + new_molec.get_r() * (2.0 + f * 1.2); 
                                    new_molec.x = molec.x + ux * spawn_dist * side;
                                    new_molec.y = molec.y + uy * spawn_dist * side;

                                    molec.size -= fragment_m;
                                    need_create.push_back(new_molec);
                                }
                                // Пересчитываем радиус планеты ОДИН РАЗ после удаления всей массы
                                m.unlock();
                            }
                        }
                    }
                }
                for (auto& molec: need_create) {
                    molecs.push_back(molec);
                }
                int past_size = r_cache.size();
                r_cache.resize(molecs.size());
                for (int i = past_size; i!=r_cache.size(); i++) {
                    r_cache[i] = molecs[i].get_r_real();
                }
            }
            std::vector<std::pair<Molecule*, Molecule*>> is_collizes;
            #pragma omp parallel for num_threads(4) schedule(static)
            for (auto& molec: molecs) {
                auto info = molec.gravity_start();
                for (auto& molec2: molecs) {
                    if (&molec != &molec2) {
                        bool is_collize = molec.attract(molec2);
                        if (is_collize) {
                            m.lock();
                            is_collizes.emplace_back(&molec, &molec2);
                            m.unlock();
                        }
                    }
                }
                molec.gravity_end(info);
            }
            std::vector<bool> destroyed(molecs.size(), false);

            for (auto& [colz1, colz2] : is_collizes) {
                int idx1 = colz1 - &molecs[0];
                int idx2 = colz2 - &molecs[0];
                if (destroyed[idx1] || destroyed[idx2]) continue;

                if (colz1->size < colz2->size) {
                    double totalM = colz1->size + colz2->size;
                    colz2->sx = (colz2->sx * colz2->size + colz1->sx * colz1->size) / totalM;
                    colz2->sy = (colz2->sy * colz2->size + colz1->sy * colz1->size) / totalM;
                    
                    colz2->size = totalM;
                    destroyed[idx1] = true;
                } 
                else {
                    double totalM = colz1->size + colz2->size;
                    colz1->sx = (colz1->sx * colz1->size + colz2->sx * colz2->size) / totalM;
                    colz1->sy = (colz1->sy * colz1->size + colz2->sy * colz2->size) / totalM;
                    
                    colz1->size = totalM;
                    destroyed[idx2] = true;
                }
            }

            for (int i = molecs.size() - 1; i >= 0; i--) {
                if (destroyed[i]) {
                    if (info_planet_index>i) {
                        info_planet_index--;
                    } else if (info_planet_index==i) {
                        info_mode = false;
                        view_mode = false;
                    }
                    molecs.erase(molecs.begin() + i);
                    r_cache.erase(r_cache.begin() + i);
                }
            }
        } else if (r_cache.size()!=molecs.size()) {
            if (r_cache.size()<molecs.size()) {
                std::cerr << "error recreate r_cache\n";
            }
            int past_size = r_cache.size();
            r_cache.resize(molecs.size());
            for (int i = past_size; i!=r_cache.size(); i++) {
                r_cache[i] = molecs[i].get_r_real();
            }
        }
        window.clear(sf::Color::White);
        renderTexture.clear(sf::Color::White);
        if (!hide_molecs) {
            for (auto molec: molecs) {
                molec.draw(renderTexture);
            }
        }
        if (create_mode) {
            double size = Molecule(0,0, obj_mass, 0,0, false).get_r();
            sf::CircleShape shape(size*scale<2 ? 2: size*scale);
            shape.setPosition(to_coords(pos_obj, size));
            if (planets_mode) {
                sf::Color color = get_object_params(obj_mass).core_color;
               shape.setFillColor(color);
            } else {
                shape.setFillColor(sf::Color::Blue);
            }
            renderTexture.draw(shape);
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            sf::Vector2f startPos = to_coords(pos_obj, 0);
            const Vector2d simVelS = ((Vector2d)mousePos - (Vector2d)startPos)/scale/(full_mode? 10000.0: 1.0);
            Vector2d simVel = simVelS;
            // std::cout << "speed1:" << magn(simVel) << "km/c\n";
            Vector2d simPos = pos_obj; 
            const int steps = 1000;
            float prediction_dt = (full_mode? 500.0f: 0.01f);
            
            sf::VertexArray trajectory(sf::PrimitiveType::LineStrip, steps);
            
            auto backup_molecs = molecs; 

            for (int i = 0; i < steps; ++i) {
                for (size_t a = 0; a < molecs.size(); ++a) {
                    for (size_t b = a + 1; b < molecs.size(); ++b) {
                        double dx = molecs[b].x - molecs[a].x;
                        double dy = molecs[b].y - molecs[a].y;
                        double distSq = dx*dx + dy*dy;
                        
                        double common_f = (strength / 1000000000.0) / distSq;
                        double dist = sqrt(distSq);

                        double force_to_A = common_f * molecs[b].size;
                        double force_to_B = common_f * molecs[a].size;

                        molecs[a].sx += (force_to_A * dx / dist) * prediction_dt;
                        molecs[a].sy += (force_to_A * dy / dist) * prediction_dt;

                        molecs[b].sx -= (force_to_B * dx / dist) * prediction_dt;
                        molecs[b].sy -= (force_to_B * dy / dist) * prediction_dt;
                    }
                }
                #pragma paralel for num_threads(4)
                for (auto& m : molecs) {
                    double dx = m.x - simPos.x;
                    double dy = m.y - simPos.y;
                    double distSq = dx*dx + dy*dy;
                    
                    double common_f = (strength / 1000000000.0) / distSq;
                    double dist = sqrt(distSq);

                    // Молекула тянет simPos
                    double force_on_sim = common_f * m.size;
                    simVel.x += (force_on_sim * dx / dist) * prediction_dt;
                    simVel.y += (force_on_sim * dy / dist) * prediction_dt;

                    // simPos тянет молекулу (используем obj_mass)
                    double force_on_m = common_f * obj_mass;
                    m.sx -= (force_on_m * dx / dist) * prediction_dt;
                    m.sy -= (force_on_m * dy / dist) * prediction_dt;
                }

                simPos.x += simVel.x * prediction_dt;
                simPos.y += simVel.y * prediction_dt;

                for (auto& m : molecs) {
                    m.x += m.sx * prediction_dt;
                    m.y += m.sy * prediction_dt;
                }

                trajectory[i].position = to_coords(simPos, 0);
                
                sf::Color tColor = sf::Color::Black;
                tColor.a = static_cast<sf::Uint8>(255 * (1.0f - (float)i / steps));
                trajectory[i].color = tColor;
            }

            molecs = backup_molecs;
            renderTexture.draw(trajectory);

            sf::VertexArray line(sf::Lines, 2);
            line[0].position = to_coords(pos_obj, 0);
            line[0].color = sf::Color::Green;
            line[1].position = mousePos;
            line[1].color = sf::Color::Green;
            renderTexture.draw(line);

            sf::Text text;
            text.setFont(defaultFont);
            text.setFillColor(sf::Color::Black);
            text.setCharacterSize(20);
            text.setPosition(to_coords(pos_obj, 0)+sf::Vector2f(40,40));
            text.setString(format_speed(magn(simVelS)*1000));
            renderTexture.draw(text);
        }
        if (metr_mode) {
            const int size_point = 5;
            sf::VertexArray line(sf::Lines, 2);
            line[0].position = to_coords(start_pos_metr, 0);
            line[0].color = sf::Color(0, 0, 0, 150);
            line[1].position = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            line[1].color = sf::Color(0, 0, 0, 150);
            renderTexture.draw(line);

            sf::CircleShape point1(size_point);
            point1.setFillColor(sf::Color::Green);
            point1.setPosition(to_coords(start_pos_metr, size_point/scale));
            renderTexture.draw(point1);

            sf::CircleShape point2(size_point);
            point2.setFillColor(sf::Color::Green);
            point2.setPosition(window.mapPixelToCoords(sf::Mouse::getPosition(window))-sf::Vector2f(size_point, size_point));
            renderTexture.draw(point2);
        } else if (distance_metr!=0) {
            const int size_point = 5;
            sf::VertexArray line(sf::Lines, 2);
            line[0].position = to_coords(start_pos_metr, 0);
            line[0].color = sf::Color(0, 0, 0, 150);
            line[1].position = to_coords(end_pos_metr, 0);
            line[1].color = sf::Color(0, 0, 0, 150);
            renderTexture.draw(line);

            sf::CircleShape point1(size_point);
            point1.setFillColor(sf::Color::Green);
            point1.setPosition(to_coords(start_pos_metr, size_point/scale));
            renderTexture.draw(point1);

            sf::CircleShape point2(size_point);
            point2.setFillColor(sf::Color::Green);
            point2.setPosition(to_coords(end_pos_metr, size_point/scale));
            renderTexture.draw(point2);

            sf::Text result;
            result.setFont(defaultFont);
            result.setFillColor(sf::Color::Black);
            result.setCharacterSize(20);
            result.setString(format_dist(distance_metr) + " km");
            sf::FloatRect textRect = result.getLocalBounds();
            result.setOrigin(textRect.left + textRect.width / 2.0f, 
                            textRect.top  + textRect.height / 2.0f);
            result.setPosition(((to_coords(start_pos_metr, 0)+to_coords(end_pos_metr, 0))/(float)2)+sf::Vector2f(0, -25));
            renderTexture.draw(result);
        }
        if (info_mode) {
            const Molecule& molec = molecs[info_planet_index];

            float screen_radius = std::max(molec.get_r() * scale, 2.0);
            // screen_radius += 2;

            sf::Vector2f screen_pos = to_coords(Vector2d(molec.x, molec.y), 0);
            sf::Vector2f screen_pos_circle = to_coords(Vector2d(molec.x, molec.y), screen_radius/scale);

            // 4. Считаем скорость через твою функцию magnitude
            double v_mod = magnitude(molec.sx, molec.sy);

            if (v_mod>10 || full_mode) {
                drawArrow(renderTexture, to_coords(Vector2d(molec.x, molec.y), 0), to_coords(Vector2d(molec.x+molec.sx*(full_mode? 1000: 1), molec.y+molec.sy*(full_mode? 1000: 1)), 0), sf::Color::Cyan);
            }

            // 5. Подготовка текста
            sf::Text info;
            info.setFont(defaultFont);
            info.setCharacterSize(16);
            info.setFillColor(sf::Color::Black); // Черный на белом фоне
            
            // Используем format_speed и format_dist, чтобы не было "забора" из цифр
            std::string content = "V: " + format_speed(v_mod*1000) + "\n" +
                                "M: " + format_mass(molec.size);
            info.setString(content);

            info.setPosition(screen_pos.x + screen_radius + 5.0f, screen_pos.y - 15.0f);

            renderTexture.draw(info);

            // buttons
            const sf::Vector2f mouse_pos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            auto button_view = createRoundedRect(button_view_width, button_view_height, round_buttons);
            sf::Vector2f pos_button_view(screen_pos.x + screen_radius + 5.0f, screen_pos.y - 15.0f + 16.0f*2 + padd_buttons_info);
            button_view.setPosition(pos_button_view);
            if (is_click_button(pos_button_view, sf::Vector2f(button_view_width, button_view_height), mouse_pos)) {
                button_view.setFillColor(sf::Color(200, 200, 200));
            } else {
                button_view.setFillColor(sf::Color(210, 210, 210));
            }
            button_view.setOutlineThickness(1.5f);
            button_view.setOutlineColor(sf::Color(180, 180, 180));
            renderTexture.draw(button_view);
            auto text_view = createTextForButton((view_mode? "stop view": "view"), pos_button_view, button_view_width, button_view_height, round_buttons);
            text_view.setFillColor(sf::Color(50, 50, 50));
            renderTexture.draw(text_view);

            sf::CircleShape selector(screen_radius);
            selector.setPosition(screen_pos_circle);
            selector.setFillColor(sf::Color::Transparent);
            selector.setOutlineThickness(2.0f);
            selector.setOutlineColor(sf::Color::Red);
            renderTexture.draw(selector);
        }
        if (remove_mode) {
            sf::CircleShape remove_zone(remove_size*scale);
            remove_zone.setPosition(window.mapPixelToCoords(sf::Mouse::getPosition(window))-sf::Vector2f(remove_size*scale, remove_size*scale));
            remove_zone.setFillColor(sf::Color(255, 0, 0, 150));
            renderTexture.draw(remove_zone);
            if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                Vector2d pos = from_coords(window.mapPixelToCoords(sf::Mouse::getPosition(window)), 0);
                bool destroys[molecs.size()];
                #pragma omp paralel for num_threads(4) schedule(static)
                for (int i = 0; i!=molecs.size(); i++) {
                    const auto& molec = molecs[i];
                    if (magn(Vector2d(molec.x, molec.y)-pos)<=remove_size) {
                        destroys[i] = true;
                    } else {
                        destroys[i] = false;
                    }
                }
                int bias_i = 0;
                for (int i = 0; i!=molecs.size(); i++) {
                    if (destroys[i]) {
                        molecs.erase(molecs.begin()+i-bias_i);
                        r_cache.erase(r_cache.begin()+i-bias_i);
                        bias_i += 1;
                    }
                }
            }
        }

        if (density_snow || temperature_snow) {
            double density = 0;
            double temperature = 0;
            sf::RectangleShape rect(sf::Vector2f(chunk, chunk));
            //#pragma omp parallel for num_threads(4) schedule(static)
            for (int x = 0; x<width; x+=chunk) {
                for (int y = 0; y<height; y+=chunk) {
                    Vector2d vec = from_coords(sf::Vector2f(x+chunk/2, y+chunk/2), 0);
                    const double centryx = vec.x;
                    const double centryy = vec.y;
                    // double centryy = (y+chunk/2)/scale;
                    std::vector<Molecule> finds;
                    if (temperature_snow) {
                        // double avg_sx = 0;
                        // double avg_sy = 0;
                        double ek = 0;
                        // #pragma omp parallel for num_threads(4) schedule(static)
                        for (const auto& molec: molecs) {
                            if (molec.x-centryx>distance_find/scale || molec.y-centryy>distance_find/scale) {
                                continue;
                            }
                            double distance = magnitude(molec.x-centryx, molec.y-centryy);
                            if (distance<distance_find/scale) {
                                // m.lock();
                                ek += (molec.sx*molec.sx+molec.sy*molec.sy)*molec.size;
                                finds.push_back(molec);
                                // m.unlock();
                            }
                        }
                        if (!finds.empty()) {
                            double max_change = ek/distance_find/scale;
                            // for (const auto& molec: molecs) {
                            //     double change = (abs(molec.sx-avg_sx)+abs(molec.sy-avg_sy))/(magnitude(molec.x-centryx, molec.y-centryy)+2);
                            //     if (change>max_change) {
                            //         max_change = change;
                            //     }
                            // }

                            temperature = max_change/1e32;
                            sf::Color col(0,0,0, 128);
                            if (temperature>40) {
                                if (temperature<255) {
                                    col.r = temperature;
                                } else {
                                    col.r = 255;
                                    if (temperature<255*2) {
                                        col.g = temperature-255;
                                        col.b = temperature-255;
                                    } else {
                                        col.g = 255;
                                        col.b = 255;
                                    }
                                }
                            } else {
                                col.b = (40-temperature)*256/40;
                            }
                            //m.lock();
                            rect.setPosition(sf::Vector2f(x, y));
                            rect.setFillColor(col);
                            renderTexture.draw(rect);
                            //m.unlock();
                        }
                    }
                    if (density_snow) {
                        density = 0;
                        for (const auto& molec: molecs) {
                            if (molec.x-centryx>distance_find/scale || molec.y-centryy>distance_find/scale) {
                                continue;
                            }
                            double distance = magnitude(molec.x-centryx, molec.y-centryy);
                            if (distance<distance_find/scale) {
                                density += 1/distance;
                            }
                        }
                        density *= 1000;
                        if (density>20) {
                            sf::Color col(0,0,0, 128);
                            if (density<255) {
                                col.b = density;
                            } else {
                                col.b = 255;
                            }
                            //m.lock();
                            rect.setPosition(sf::Vector2f(x, y));
                            rect.setFillColor(col);
                            renderTexture.draw(rect);
                            //m.unlock();
                        }
                    }
                }
            }
        }
        renderTexture.display();
        sf::Sprite resultSprite(renderTexture.getTexture());
        resultSprite.setPosition(sf::Vector2f(0,0));
        window.draw(resultSprite);
        window.display();
    }   
}
