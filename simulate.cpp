// g++:     cd C:/Users/Azerty/Desktop/Программы/filescpp; g++ simulate.cpp -o simulate.exe -I c:\Users\Azerty\Downloads\SFML-2.6.1/include -L c:\Users\Azerty\Downloads\SFML-2.6.1/lib -lsfml-graphics-d -lsfml-window-d -lsfml-system-d -lopengl32 -lwinmm -lgdi32 -lcomdlg32 -static -O3 -fopenmp
// clang++: cd C:/Users/Azerty/Desktop/Программы/filescpp ; C:\Users\Azerty\AppData\Local\Android\Sdk\ndk\28.2.13676358\toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe --target=x86_64-w64-mingw32 simulate.cpp -o simulate.exe -I c:\Users\Azerty\Downloads\SFML-2.6.1/include -L c:\Users\Azerty\Downloads\SFML-2.6.1/lib -I C:\Users\Azerty\Downloads\mingw64\include -L C:\Users\Azerty\Downloads\mingw64\lib -lsfml-graphics-d -lsfml-window-d -lsfml-system-d -lopengl32 -lwinmm -lgdi32 -lcomdlg32 -static -O3 -march=native -ffast-math -ffp-contract=fast -funroll-loops -fopenmp=libgomp -lgomp -lwinpthread -fopenmp
#include "simulate_imports.h"
#include "circles_lib.cpp"
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cstdio>
#include <omp.h>
#include <cstring>
#include <chrono>
#include <immintrin.h>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/sysinfo.h>
#endif
#include "gpu_loader.cpp" // загружает gpu функции и имеет свои функции
#include "settings_lib.cpp" // для настроек
#include "bloom_test.cpp"
bool gpu_mode = false;
// если что-то добавляется - добавлять в конец
// не меняйте порядок
std::vector<Setting> settings = {
    {
        "gpu_mode",
        "Включает или выключает вычисления через видеокарту (требует перезапуск).",
        SettingType::Bool,
        InputType::Checkbox,
        "false", // Текущее значение
        "false", // Значение по умолчанию
        true,    // Нужен перезапуск
        false,   // Флаг изменения (is_dirty)
        32,      // Разрядность в битах
        0, 0,    // Мин/Макс (не используются для bool)
        {}       // Варианты выбора (нет)
    },
    {
        "Показ атмосферы планет",
        "Включает или выключает визуальное отображение атмосферных эффектов вокруг планет.",
        SettingType::Bool,
        InputType::YesNo,
        "true",
        "true",
        false,   // Применяется мгновенно, перезапуск не нужен
        false,
        32,
        0, 0,
        {}
    },
    {
        "Потоки видеокарты", // Имя в меню
        "Задаёт производительность. рекоминдуется ставить 16-32 с слабой видиокартой и 32+ с мощной (чтоб\n быстрее было)", // Кастомное описание
        SettingType::Number,   // Тип данных: числовой (int)
        InputType::Slider,     // Интерфейс: ползунок (слайдер)
        "16",                 // Текущее выбранное значение
        "16",                 // Заводской дефолт при сбросе настроек
        true,                  // Требует перезапуск: ДА (изменение потоков видеопамяти требует реинициализации контекста)
        false,                 // По умолчанию не изменено
        16,                    // Разрядность: 16 бит. При записи в файл превратится ровно в 4 HEX-символа (256 -> '0100')
        0, 0,                  // Диапазон min/max игнорируется, так как ниже передан жесткий список options
        {"8", "16", "32", "64", "128", "256", "512", "1024"} // Ограничение выбора: ползунок прыгает строго по этим числам
    },
    {
        "Свечение от звёзд в 3d режиме",
        "Включает или выключает эфект свечения звёзд в режиме 3d",
        SettingType::Bool,
        InputType::YesNo,
        "true",
        "true",
        false,   // Применяется мгновенно, перезапуск не нужен
        false,
        32,
        0, 0,
        {}
    },
    {
        "Простое свечение звёзд в 3d режиме", 
        "Включает (нет) или выключает (да) сильное свечение звёзд",
        SettingType::Bool,
        InputType::YesNo,
        "false",
        "false",
        false,   // Применяется мгновенно, перезапуск не нужен
        false,
        32,
        0, 0,
        {}
    },
    {
        "Простые тени планет в 3d режиме", 
        "Включает или выключает тени планет в режиме 3d",
        SettingType::Bool,
        InputType::YesNo,
        "true",
        "true",
        false,   // Применяется мгновенно, перезапуск не нужен
        false,
        32,
        0, 0,
        {}
    }
};
const std::string setup_path = "settings.save";
#define gpu_mode_sett_id 0
#define show_atmosphere_sett_id 1
#define gpu_num_threads_sett_id 2
#define bloom_sett_id 3
#define simple_bloom_stars_sett_id 4
#define simple_shadows_sett_id 5
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
using Vector3d = sf::Vector3<double>;
// 3d штуки
struct CameraDirections {
    Vector3d forward;
    Vector3d right;
};

CameraDirections calculateCameraDirections(const Vector3d& cam_rotate) {
    CameraDirections dirs;
    
    const double DEG_TO_RAD = 3.14159265358979323846 / 180.0;
    double yawRad   = cam_rotate.x * DEG_TO_RAD;
    double pitchRad = cam_rotate.y * DEG_TO_RAD;

    double cosYaw   = std::cos(yawRad);
    double sinYaw   = std::sin(yawRad);
    double cosPitch = std::cos(pitchRad);
    double sinPitch = std::sin(pitchRad);

    // 1. Истинный вектор Forward для правой системы координат (OpenGL/SFML)
    // При нулевых углах камера смотрит строго вдоль оси -Z
    dirs.forward.x = sinYaw * cosPitch;
    dirs.forward.y = sinPitch;
    dirs.forward.z = -cosYaw * cosPitch; 
    
    // 2. Истинный вектор Right (Вправо) через честное произведение Forward x (0, 1, 0)
    dirs.right.x = cosYaw;
    dirs.right.y = 0.0;
    dirs.right.z = sinYaw;
    return dirs;
}
void updateCameraPosition(Vector3d& cam_pos, const CameraDirections& dirs, float speed, float deltaTime) {
    // Вычисляем дистанцию, которую камера должна пройти за этот кадр
    float moveSpeed = speed * deltaTime;

    // Движение Вперед / Назад (W / S)
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
        cam_pos.x += dirs.forward.x * moveSpeed;
        cam_pos.y += dirs.forward.y * moveSpeed; 
        cam_pos.z += dirs.forward.z * moveSpeed;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
        cam_pos.x -= dirs.forward.x * moveSpeed;
        cam_pos.y -= dirs.forward.y * moveSpeed;
        cam_pos.z -= dirs.forward.z * moveSpeed;
    }

    // Движение Влево / Вправо (A / D)
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
        cam_pos.x += dirs.right.x * moveSpeed;
        cam_pos.y += dirs.right.y * moveSpeed;
        cam_pos.z += dirs.right.z * moveSpeed;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
        cam_pos.x -= dirs.right.x * moveSpeed;
        cam_pos.y -= dirs.right.y * moveSpeed;
        cam_pos.z -= dirs.right.z * moveSpeed;
    }
}
void updateCameraRotation(Vector3d& cam_rotate, sf::RenderWindow& window, double sensitivity = 0.1) {
    // 1. Находим центр вашего окна
    sf::Vector2i windowCenter(window.getSize().x / 2, window.getSize().y / 2);

    // 2. Получаем текущую позицию мыши относительно окна
    sf::Vector2i mousePos = sf::Mouse::getPosition(window);

    // 3. Считаем, на сколько пикселей сдвинулась мышь от центра
    double deltaX = -static_cast<double>(mousePos.x - windowCenter.x);
    double deltaY = -static_cast<double>(mousePos.y - windowCenter.y);

    // 4. Изменяем углы поворота (умножаем на чувствительность)
    cam_rotate.x += deltaX * sensitivity; // Yaw (влево / вправо)
    cam_rotate.y -= deltaY * sensitivity; // Pitch (вверх / вниз) — тут минус, чтобы инвертировать ось

    // 5. Ограничиваем вертикальный наклон (Pitch), чтобы нельзя было сделать сальто головой
    // Камера жестко остановится, если посмотреть строго вертикально вверх (90) или вниз (-90)
    if (cam_rotate.y > 89.0)  cam_rotate.y = 89.0;
    if (cam_rotate.y < -89.0) cam_rotate.y = -89.0;

    // 6. ВСЕГДА возвращаем мышь обратно в центр окна, фиксируя её для следующего кадра
    sf::Mouse::setPosition(windowCenter, window);
}
Vector3d rotateVector3D(const Vector3d& vec, const Vector3d& rotate) {
    Vector3d result = vec;

    // Переводим углы из градусов в радианы
    const double DEG_TO_RAD = 3.14159265358979323846 / 180.0;
    double yawRad   = rotate.x * DEG_TO_RAD;
    double pitchRad = rotate.y * DEG_TO_RAD;

    // Считаем тригонометрию
    double cy = std::cos(yawRad);   double sy = std::sin(yawRad);
    double cp = std::cos(pitchRad);  double sp = std::sin(pitchRad);

    // 1. Поворот вокруг оси Y (Yaw / Влево-Вправо)
    double x1 = result.x * cy - result.z * sy;
    double z1 = result.x * sy + result.z * cy;
    double y1 = result.y; // При вращении вокруг Y координата Y не меняется

    // 2. Поворот вокруг оси X (Pitch / Вверх-Вниз)
    // Используем промежуточный результат (x1, y1, z1)
    result.x = x1; // При вращении вокруг X координата X не меняется
    result.y = y1 * cp - z1 * sp;
    result.z = y1 * sp + z1 * cp;

    return result;
}
struct CameraVectors {
    sf::Vector3f forward;
    sf::Vector3f right;
    sf::Vector3f up;
};

CameraVectors calculateCameraVectors(Vector3d cam_rotate) {
    const float PI = 3.1415926535f;

    // Переводим углы в радианы
    float yaw   = cam_rotate.x * (PI / 180.0f);
    float pitch = cam_rotate.y * (PI / 180.0f);
    float roll  = cam_rotate.z * (PI / 180.0f);

    float cosPitch = std::cos(pitch);
    float sinPitch = std::sin(pitch);
    float cosYaw   = std::cos(yaw);
    float sinYaw   = std::sin(yaw);

    // 1. Истинный вектор Forward (Правая система координат, взгляд на -Z при нуле)
    sf::Vector3f forward;
    forward.x = sinYaw * cosPitch;
    forward.y = sinPitch;
    forward.z = -cosYaw * cosPitch;

    // Вспомогательный вектор "Верх мира"
    sf::Vector3f worldUp(0.0f, 1.0f, 0.0f);

    // Лямбда для нормализации
    auto normalize = [](sf::Vector3f v) {
        float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return (length > 0.0f) ? sf::Vector3f(v.x / length, v.y / length, v.z / length) : v;
    };

    forward = normalize(forward);

    // 2. Строим вектор Right через Cross(Forward, WorldUp)
    // Чтобы при нулевых углах Right смотрел строго в +X
    sf::Vector3f right;
    right.x = forward.z * worldUp.z - forward.z * worldUp.y; // Упрощается
    // Расписываем честное векторное произведение:
    right.x = forward.z * worldUp.y - forward.y * worldUp.z;
    right.y = forward.x * worldUp.z - forward.z * worldUp.x;
    right.z = forward.y * worldUp.x - forward.x * worldUp.y;
    right = normalize(right);

    // Корректируем, если смотрим строго вертикально вверх/вниз (deadlock)
    if (std::abs(cosPitch) < 0.0001f) {
        right = sf::Vector3f(cosYaw, 0.0f, sinYaw);
    }

    // 3. Строим истинный вектор Up через Cross(Right, Forward)
    // Он всегда будет строго перпендикулярен направлению взгляда
    sf::Vector3f up;
    up.x = right.y * forward.z - right.z * forward.y;
    up.y = right.z * forward.x - right.x * forward.z;
    up.z = right.x * forward.y - right.y * forward.x;
    up = normalize(up);

    // 4. Если есть вращение по Z (Roll) — аккуратно поворачиваем оси Right и Up
    if (std::abs(roll) > 0.0001f) {
        float cosRoll = std::cos(roll);
        float sinRoll = std::sin(roll);
        
        sf::Vector3f oldRight = right;
        sf::Vector3f oldUp = up;

        right = oldRight * cosRoll + oldUp * sinRoll;
        up = -oldRight * sinRoll + oldUp * cosRoll;
    }

    return { forward, right, up };
}

struct ProjectedCircle {
    // 1. Блок double (8 байт каждый) — ставим в самый верх для идеального выравнивания
    double depth;        // Расстояние до камеры (8 байт) — нужно для сортировки

    // 2. Блок sf::Vector2f (8 байт каждый, состоит из двух float)
    sf::Vector2f pos;       // Позиция в мире XZ (8 байт)
    sf::Vector2i screenPos; // Позиция на экране XY (8 байт)

    // 3. Блок одиночных float (4 байта каждый)
    float radius;       // Радиус в мировых километрах (4 байт)
    float screenRadius;  // Радиус на экране в пикселях (4 байта)
    float bloom_size;    // Светимость для Блума (4 байта)
    float bloom_light;   // Вычисленный поток света (4 байта)
    float alberto;

    // 4. Мелкие типы (4 байта) — ставим в самый конец
    sf::Color color;     // Цвет RGBA (4 байта: по 1 байту на канал)
};
float calcScale(float windowHeight, float fovDeg = 90.0f) {
    const float PI = 3.1415926535f;
    float fovRad = fovDeg * (PI / 180.0f);
    return (windowHeight * 0.5f) / std::tan(fovRad * 0.5f);
}
double precomputeStarPower(double mass_kg) {
    if (mass_kg <= 0.0) return 0.0;

    const double SUN_MASS_KG = 1.989e30;
    double mass_relative = mass_kg / SUN_MASS_KG;
    return std::pow(mass_relative, 3.5);
}
// считает по физике
float getStarBloomSize(float massInKg) {
    // 1. Константы
    float SUN_MASS = 1.989e30;       // Масса Солнца в кг
    float SUN_TEMP = 5778.0;         // Температура Солнца в Кельвинах
    float SIGMA = 5.67e-8;           // Постоянная Стефана-Больцмана
    float EARTH_LUX = 75000.0;       // Базовая земная освещенность (наш 1.0)
    float m = massInKg / SUN_MASS;
    float starTemp = SUN_TEMP * pow(m, 0.505); 
    starTemp = std::max(starTemp, 2000.0f);
    float t2 = starTemp * starTemp;
    float energyWatts = SIGMA * (t2 * t2);
    float tNormalized = starTemp / 6000.0;
    float lumensPerWatt = 95.0 * (tNormalized * tNormalized * (3.0 - 2.0 * tNormalized));
    lumensPerWatt = std::max(lumensPerWatt, 5.0f);
    float starLux = energyWatts * lumensPerWatt;
    float starHdrBrightness = starLux / EARTH_LUX;
    float bloomSource = std::max(starHdrBrightness - 1.0, 0.0);
    float exposure = 0.005; 
    float bloom_size = (1.0 - exp(-bloomSource * exposure)) * 2.0;
    return bloom_size;
}
// считает не по физике
float getSimpleStarBloom(float massInKg, float userSensitivity) {
    const float SUN_MASS = 1.989e30f;
    float relativeMass = massInKg / SUN_MASS;

    // Ограничиваем влияние массы, чтобы тяжелые звезды не ломали экран
    float baseGlow = relativeMass * 0.5f;
    if (baseGlow < 0.2f) baseGlow = 0.2f;
    if (baseGlow > 1.5f) baseGlow = 1.5f;

    // Умножаем на 2.0, чтобы вписаться в рамки вашей шкалы [0.0 - 2.0]
    float bloom_size = baseGlow * userSensitivity * 2.0f;

    // Жестко гарантируем, что значение не превысит 2.0
    return std::min(bloom_size, 2.0f);
}
// molec path
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
        return { sf::Color(255, 140, 50), sf::Color(255, 100, 10, 60), 1.2f };
    }

    // 4. ЗВЕЗДЫ (Классификация по массе)
    if (m < 0.8 * M_SUN) { // Красные/Оранжевые карлики
        return { sf::Color(255, 60, 0), sf::Color(255, 30, 5, 100), 1.3f };
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
const double G_km = 6.67430e-20;
const double sb_temperature = 5.67e-8;
const int distance_find = 80;
double speed = 42*60;
double dt = 0.016;
double scale = 0.0000333;
bool planets_mode = true;
int cfps = 60;
bool full_mode = true;
double camx = 0;
double camy = 0;
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
const double SUN_MASS = 1.989e30;
const double EARTH_MASS = 5.972e24;
const double SUN_LUMINOSITY = 3.828e26;
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
        const double directionx = dx*inv;
        const double directiony = dy*inv;
        const double ratio_mass = (other.size*G_km)*inv*inv;
        //std::scoped_lock lock(m);
        this->acc_x += directionx*ratio_mass;
        // this->sx += sin(direction)*strength*ratio_mass*dt*speed/distance;
        this->acc_y += directiony*ratio_mass;
        // this->sy += cos(direction)*strength*ratio_mass*dt*speed/distance;
        return false;
    }
    bool attract_opt(const Molecule& other) {
        const double dx = other.x-this->x;
        const double dy = other.y-this->y;
        double distanceSq = dx*dx+dy*dy;
        double distanceSq_inv = 1.0/distanceSq;
        double distance_inv = sqrt(distanceSq_inv);

        const double directionx = dx*distance_inv;
        const double directiony = dy*distance_inv;
        const double force = (other.size*G_km)*distanceSq_inv;
        this->acc_x += directionx*force;
        this->acc_y += directiony*force;
        return false;
    }
    inline std::pair<double, double> gravity_start() {
        double acc_x_past = this->acc_x;
        double acc_y_past = this->acc_y;
        this->acc_x = 0;
        this->acc_y = 0;
        return {acc_x_past, acc_y_past};
    }
    inline void gravity_end(std::pair<double, double> past_acc) {
        // это Эйлер (ошибки на маленьком dt огромны)
        // this->sx += this->acc_x*speed*dt;
        // this->sy += this->acc_y*speed*dt;
        // а это Верле (точнее)
        double calc = speed*dt;
        this->sx += 0.5*(this->acc_x+past_acc.first)*calc;
        this->sy += 0.5*(this->acc_y+past_acc.second)*calc;
    }
    inline void gravity_end(std::pair<double, double> past_acc, double calc) {
        // это Эйлер (ошибки на маленьком dt огромны)
        // this->sx += this->acc_x*speed*dt;
        // this->sy += this->acc_y*speed*dt;
        // а это Верле (точнее)
        this->sx += 0.5*(this->acc_x+past_acc.first)*calc;
        this->sy += 0.5*(this->acc_y+past_acc.second)*calc;
    }


    inline void forward() {
        // это Эйлер
        // this->x += this->sx*dt*speed;
        // this->y += this->sy*dt*speed;
        // а это Верле
        double s_all = dt*speed;
        double s_allSq = s_all*s_all;
        this->x += this->sx*s_all+0.5*this->acc_x*s_allSq;
        this->y += this->sy*s_all+0.5*this->acc_y*s_allSq;
    }
    inline void forward(double s_all, double s_allSq) {
        // это Эйлер
        // this->x += this->sx*s_all;
        // this->y += this->sy*s_all;
        // а это Верле
        this->x += this->sx*s_all+0.5*this->acc_x*s_allSq;
        this->y += this->sy*s_all+0.5*this->acc_y*s_allSq;
    }
    bool draw(std::vector<CircleData>& circles, sf::VertexArray& glows, size_t count_glows, size_t count_circle) const {
        double visualRadius = get_r();
        CircleData circle(std::max(visualRadius * scale, 2.0));

        //circle.setPosition(to_coords(Vector2d(this->x, this->y), visualRadius));
        circle.setPosition(to_coords(Vector2d(this->x, this->y), 0));
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
        bool is_use_glow = false;
        if (planets_mode && settings[show_atmosphere_sett_id].get_value<bool>() && visuals.glow_factor!=0 && visualRadius*scale>=2) {
            sf::Vector2f pos = to_coords(Vector2d(this->x, this->y), 0);
            // sf::VertexArray glow(sf::TriangleFan, 32);
            sf::Color coreColor = visuals.core_color;
            sf::Color edgeColor = visuals.glow_color;
            size_t glows_id = count_glows*90;
            // glows[glows_id].position = pos;
            // glows[glows_id].color = coreColor;
            float angle = -1 * 2 * 3.14159f / 30;
            sf::Vector2f last_pos = pos + sf::Vector2f(cos(angle), sin(angle)) * (float)(visualRadius*visuals.glow_factor*scale);
            for (int i = 0; i < 30; ++i) {
                angle = i * 2 * 3.14159f / 30;
                sf::Vector2f new_pos = pos + sf::Vector2f(cos(angle), sin(angle)) * (float)(visualRadius*visuals.glow_factor*scale);
                glows[glows_id+i*3].position = new_pos;
                glows[glows_id+i*3].color = edgeColor;
                glows[glows_id+i*3+1].position = last_pos;
                glows[glows_id+i*3+1].color = edgeColor;
                glows[glows_id+i*3+2].position = pos;
                glows[glows_id+i*3+2].color = coreColor;
                last_pos = new_pos;
            }
            // r.draw(glow);
            is_use_glow = true;
        }
        //r.draw(circle);
        circles[count_circle] = circle;
        return is_use_glow;
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
        double radius_inv = 1.0/radius;
        double ss = strength/1e6*this->size;
        double f_prilive = 2*ss*other.size*radius/distCb;
        double f_grav = ss*this->size*radius_inv*radius_inv;
        return f_prilive-f_grav;
    }
    double get_two_space(const Molecule& other) {
        return sqrt(2*strength/1e9*this->size/(other.get_r()+get_r()));
    }
    double getInternalPower() const {
        double m_earth = this->size / 5.972e24;
        double coreHeat = 4.7e13 * m_earth;
        if (this->size > 1.5e29) {
            double m_sun = this->size / 1.989e30;
            return coreHeat + (3.828e26 * std::pow(m_sun, 3.5));
        }
        return coreHeat;
    }

    double getAverageAlbedo() const {
        double m_earth = this->size / EARTH_MASS;
        
        if (m_earth < 0.1) return 0.1;       // Типа Луны (темный реголит)
        if (m_earth < 2.0) return 0.3;       // Типа Земли (камни + немного облаков)
        if (m_earth < 10.0) return 0.35;     // Суперземли
        return 0.45;                         // Газовые гиганты (высокая отражаемость облаков)
    }
    double getSpecificHeat() const {
        double m_earth = this->size / EARTH_MASS;

        if (m_earth < 0.1) return 800.0;    // Мелкие астероиды, Луна (сухой камень)
        if (m_earth < 5.0) return 1000.0;   // Планеты земного типа
        if (m_earth < 15.0) return 2500.0;  // Нептуны / Ледяные гиганты
        return 13000.0;                     // Газовые гиганты (водород/гелий)
    }

    double getTotalHeatCapacity() const {
        // Считаем массу слоя глубиной h (например, 50 метров)
        // double layerDepth = 50.0; 
        // double density = (this->size / 5.972e24 < 10.0) ? 3000.0 : 1.3; // Камень vs Газ
        
        // double surfaceArea = 4.0 * M_PI * get_r() * get_r();
        // double layerMass = surfaceArea * layerDepth * density;

        // // Масса слоя не может быть больше массы всей планеты (для мелких астероидов)
        // double effectiveMass = std::min(this->size, layerMass);

        return this->size * getSpecificHeat();
    }
    double calc_delta_t_this(double currT) {
        double rSq = get_r()*get_r();
        double rSq_pi = rSq*3*M_PI*1e6;
        return (getInternalPower()/rSq_pi-sb_temperature*currT*currT*currT*currT)*rSq_pi*speed*dt/getTotalHeatCapacity();
    }
    double calc_delta_t(const Molecule& star) {
        double dx = this->x-star.x;
        double dy = this->y-star.y;
        double distSq = (dx*dx+dy*dy)*1e6;
        double rSq = get_r()*get_r();
        double rSq_pi = rSq*3*M_PI*1e6;
        return (star.getInternalPower()*(1-getAverageAlbedo())/16/distSq)*rSq_pi*speed*dt/getTotalHeatCapacity();
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
    if (is_in_cache_r()) [[likely]] {
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
    mainWindow.setActive(true);
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
    mainWindow.setActive(true);
    if (input_string == "") {
        return -1;
    }
    try {
        return std::stod(input_string);
    } catch (std::exception& _) {
        return -1;
    }
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
bool destroy_mode = false;
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
"* Shift-F - режим разрушения\n"
"* o - открыть настройки\n"
"* v - посмотреть в 3d\n"
"# Управление (с full):\n"
"* колёсико мыши - приближение/отдаление из точки куда указывает мышка\n"
"* нажатие - выделить планету (тогда ещё можно будет проследить, изменить её массу)/сбросить выделение\n"
"* перемещение мыши с зажатой левой кнопкой мыши  - переместиться\n"
"* на тачпаде 2 пальца в какую-то сторону переместить - переместиться\n"
"* на тачпаде 2 пальца в стороны/друг к другу переместить - приблизить/отдалить\n"
"# Оптимизация:\n"
"на среднем ноутбуке:\n"
"* С cpu будет 36-37 fps на 2000 объектов\n"
"* С gpu на встроеной intel видиокарте будет 17-19 fps на 2000 объектов (в 16 потоков на ядро как по умолчанию)\n"
"* С gpu на встроеной intel видиокарте будет 11 fps на 2000 объектов (в 64 потоков на ядро)\n"
"* С gpu от amd лучше ставить 32 потока а не 16 потоков на ядро (меняется в настройках)\n"
"* Отрисовка не сильно снижает fps\n"
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

long long get_last_boot_time() {
    auto now = std::chrono::system_clock::now();
    auto now_sec = std::chrono::system_clock::to_time_t(now);

#ifdef _WIN32
    unsigned long long uptime_ms = GetTickCount64();
    return now_sec - (uptime_ms / 1000);
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return now_sec - info.uptime;
    }
    return 0;
#endif
}
bool was_rebooted_since(long long x_time_t) {
    long long last_boot = get_last_boot_time();
    return last_boot > x_time_t;
}
bool was_rebooted_n_seconds_ago(long long seconds_passed_since_x) {
    long long now = std::time(nullptr); 
    long long time_x = now - seconds_passed_since_x;
    long long last_boot = get_last_boot_time(); 
    return last_boot > time_x;
}
const float padd_buttons_info = 5.0f;
const float round_buttons = 5.0f;
// buttons
const float button_view_width = 80;
const float button_view_height = 30;
#if defined(__GNUC__) || defined(__clang__)
    // Отключение для GCC и Clang
    #pragma GCC diagnostic ignored "-Wwrite-strings"
#elif defined(_MSC_VER)
    // Отключение для Visual Studio (MSVC)
    #pragma warning(disable : 4267) // Отключает связанные предупреждения приведения типов
#endif
char* gravSourse = R"(#version 430 core
// Включаем поддержку double для встроек
#extension GL_ARB_compute_shader : enable
#extension GL_ARB_shader_storage_buffer_object : enable
#extension GL_ARB_gpu_shader_fp64 : enable
//#extension GL_EXT_control_flow_attributes : enable

// 16 - лучше всего
layout(local_size_x = GPU_THREADS) in;

struct Molecule {
    double x, y;
    double vx, vy;
    double acc_x;
    double acc_y;
    double mass;
    double padd;
};
struct Collize {
    uint idx1;
    uint idx2;
};

layout(std430, binding = 0) buffer MolecBuffer {
    Molecule molecs[];
};
layout(std430, binding = 1) buffer RadiusBuffer {
    double r_cache[];
};
layout(std430, binding = 2) buffer CollizeBuffer {
    Collize collizes[];
};
layout(std430, binding = 3) buffer CollizeCBuffer {
    uint count_coliz[];
};

uniform double dt;
uniform uint total;
//const double G = 6.67430e-20; // для старого кода с double
const vec2 G = vec2(6.6743003e-20, -3.111812e-27);
// double - медлено. но есть float но он не точный - тогда 2 float чтоб было точнее (в итоге в 4-8 раз быстрее double)
// Перевод обратно из эмулированного vec2 (High/Low) в честный double
double vec2ToDouble(vec2 val) {
    return double(val.x) + double(val.y); // Просто складываем компоненты в double
}
vec2 ds_add(vec2 a, vec2 b) {
    float t1 = a.x + b.x;
    float e = t1 - a.x;
    float t2 = ((b.x - e) + (a.x - (t1 - e))) + a.y + b.y;
    
    // Возвращаем новую пару high и low
    float high = t1 + t2;
    float low = t2 - (high - t1);
    return vec2(high, low);
}
vec2 ds_sub(vec2 a, vec2 b) {
    // Вычитаем старшие части
    float s1 = a.x - b.x;
    float e = s1 - a.x;
    // Вычисляем точную погрешность вычитания старших частей и добавляем младшие
    float s2 = ((-b.x - e) + (a.x - (s1 - e))) + a.y - b.y;
    
    float high = s1 + s2;
    float low = s2 - (high - s1);
    return vec2(high, low);
}
vec2 twoproduct(float a, float b) {
    float p = a * b;
    // Разделяем числа на старшие и младшие биты, чтобы найти точную ошибку умножения
    float c = 4097.0 * a;
    float a_h = c - (c - a);
    float a_l = a - a_h;
    
    c = 4097.0 * b;
    float b_h = c - (c - b);
    float b_l = b - b_h;
    
    float err = ((a_h * b_h - p) + a_h * b_l + a_l * b_h) + a_l * b_l;
    return vec2(p, err);
}

// --- УМНОЖЕНИЕ двух эмулированных double (A * B) ---
vec2 ds_mul(vec2 a, vec2 b) {
    vec2 p = twoproduct(a.x, b.x);
    p.y += a.x * b.y;
    p.y += a.y * b.x;
    
    float high = p.x + p.y;
    float low = p.y - (high - p.x);
    return vec2(high, low);
}

// --- ДЕЛЕНИЕ двух эмулированных double (A / B) ---
vec2 ds_div(vec2 a, vec2 b) {
    float q1 = a.x / b.x;
    vec2 p = twoproduct(q1, b.x);
    
    float r = a.x - p.x;
    r -= p.y;
    r += a.y;
    r -= q1 * b.y;
    
    float q2 = r / b.x;
    float high = q1 + q2;
    float low = q2 - (high - q1);
    return vec2(high, low);
}
vec2 ds_inversesqrt(vec2 a) {
    // Шаг 1: Получаем грубое начальное приближение (точность float)
    float x0 = inversesqrt(a.x);
    
    // Шаг 2: Итерация Ньютона-Рафсона для Double-Single точности.
    // Формула: x1 = x0 * (1.5 - 0.5 * a * x0 * x0)
    
    // Считаем: 0.5 * a
    vec2 half_a = vec2(0.5 * a.x, 0.5 * a.y);
    
    // Считаем: x0 * x0
    vec2 x0Sq = vec2(x0 * x0, 0.0);
    
    // Считаем: 0.5 * a * x0 * x0
    vec2 term = ds_mul(half_a, x0Sq);
    
    // Считаем: 1.5 - term
    vec2 one_point_five = vec2(1.5, 0.0);
    vec2 factor = ds_sub(one_point_five, term);
    
    // Итоговое значение x1 = x0 * factor
    vec2 x1 = ds_mul(vec2(x0, 0.0), factor);
    
    return x1; // Возвращает точный 1.0 / sqrt(A) в формате vec2
}
vec2 doubleToVec2(double val) {
    // 1. Распаковываем double на два 32-битных uint (младшие и старшие 32 бита)
    uvec2 bits = unpackDouble2x32(val);
    
    // 2. Сбрасываем (зануляем) нижние 12 бит мантиссы у младшего слова.
    // Это мгновенно и чисто «отрезает» хвост точности на уровне железа
    uint low_bits_cleared = bits.x & 0xFFFFF000u;
    
    // 3. Собираем старшую часть (High) обратно в double и приводим к float
    float high = float(packDouble2x32(uvec2(low_bits_cleared, bits.y)));
    
    // 4. Младшая часть — это просто остаток. 
    // Для видеокарты такое вычитание СЕЙЧАС будет быстрым, 
    // так как у high занулен хвост, и операция оптимизируется на конвейере.
    float low = float(val - double(high)); 
    
    return vec2(high, low);
}
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= total) return; // Защита от выхода за пределы массива

    vec2 dt_v2 = doubleToVec2(dt);

    vec2 p_ix = doubleToVec2(molecs[i].x);
    vec2 p_iy = doubleToVec2(molecs[i].y);
    //vec2 m_i = doubleToVec2(molecs[i].mass);

    vec2 accX = vec2(0.0, 0.0);
    vec2 accY = vec2(0.0, 0.0);

    for (uint j = 0; j < total; j++) {
        if (i == j) continue;

        vec2 dx = ds_sub(doubleToVec2(molecs[j].x),  p_ix);
        vec2 dy = ds_sub(doubleToVec2(molecs[j].y), p_iy);

        vec2 distSq = ds_add(ds_mul(dx, dx), ds_mul(dy, dy));
        
        // скорость за точность (но теперь даже быстрее без сильной потери точности)
        vec2 dist_inv = ds_inversesqrt(distSq);
        vec2 dist = ds_mul(dist_inv, distSq); 
        //double dist = sqrt(distSq);
        //double dist_inv = 1.0/dist;

        // vec2 force = (G * molecs[j].mass) * dist_inv * dist_inv;
        vec2 force = ds_mul(ds_mul(G, doubleToVec2(molecs[j].mass)), ds_mul(dist_inv, dist_inv));

        // Проекция ускорения: a = F * (d / r) -> force * d * dist_inv
        //accX += force * dx * dist_inv;
        //accY += force * dy * dist_inv;
        accX = ds_add(accX, ds_mul(force, ds_mul(dx, dist_inv)));
        accY = ds_add(accY, ds_mul(force, ds_mul(dy, dist_inv)));

        if (dist.x < float(r_cache[i]) + float(r_cache[j])) {
            uint index = atomicAdd(count_coliz[0], 1);
            if (index < total * 6) {
                collizes[index].idx1 = i;
                collizes[index].idx2 = j;
            }
        }
    }
    vec2 p_accx = doubleToVec2(molecs[i].acc_x);
    vec2 p_accy = doubleToVec2(molecs[i].acc_y);
    //double new_x = p_ix + molecs[i].vx * dt + 0.5 * molecs[i].acc_x * dt * dt;
    //double new_y = p_iy + molecs[i].vy * dt + 0.5 * molecs[i].acc_y * dt * dt;
    double new_x = vec2ToDouble(ds_add(ds_add(p_ix, ds_mul(doubleToVec2(molecs[i].vx), dt_v2)), ds_mul(ds_mul(vec2(0.5, 0.0), ds_mul(p_accx, dt_v2)), dt_v2)));
    double new_y = vec2ToDouble(ds_add(ds_add(p_iy, ds_mul(doubleToVec2(molecs[i].vy), dt_v2)), ds_mul(ds_mul(vec2(0.5, 0.0), ds_mul(p_accy, dt_v2)), dt_v2)));

    //molecs[i].vx += 0.5 * (accX + molecs[i].acc_x) * dt;
    //molecs[i].vy += 0.5 * (accY + molecs[i].acc_y) * dt;
    molecs[i].vx += vec2ToDouble(ds_mul(vec2(0.5, 0), ds_mul(ds_add(accX, p_accx), dt_v2)));
    molecs[i].vy += vec2ToDouble(ds_mul(vec2(0.5, 0), ds_mul(ds_add(accY, p_accy), dt_v2)));

    molecs[i].x = new_x;
    molecs[i].y = new_y;
    molecs[i].acc_x = vec2ToDouble(accX);
    molecs[i].acc_y = vec2ToDouble(accY);
}
)";
// вызывайте когда хоть что-нибудь изменилось в planets
// синхронизирует данные с видиокартой
// но r_cache обновляйте сами и до вызова этой функции
// но если вы изменили только массу одного объекта или вы изменили позицию только одного объекта делайте сами
void changePlanets(size_t& ssbo_planets_size, GLuint& ssbo_count_colz, GLuint& ssbo_planets, GLuint& ssbo_r_cache, GLuint& ssbo_collizes, GLuint& ssbo_project_circles, bool change_mass, bool no_change_molecs=false) {
    if (ssbo_planets_size!=molecs.size()) {
        if (ssbo_planets!=0) {
            if (gpu_mode) {
                deleteSSBO(ssbo_planets);
                deleteSSBO(ssbo_r_cache);
                deleteSSBO(ssbo_collizes);
            }
            if (ssbo_project_circles!=0) {
                deleteSSBO(ssbo_project_circles);
            }
        }
        if (gpu_mode) {
            ssbo_planets = createEmptySSBO(sizeof(Molecule)*molecs.size());
            ssbo_r_cache = createEmptySSBO(sizeof(double)*molecs.size());
            ssbo_collizes = createEmptySSBO(sizeof(std::pair<uint32_t, uint32_t>)*molecs.size()*6);
        }
        if (ssbo_project_circles!=0) {
            ssbo_project_circles = createEmptySSBO(sizeof(ProjectedCircle)*molecs.size());
        }
        if (ssbo_planets!=0 && gpu_mode) {
            updateSSBOData<Molecule>(ssbo_planets, molecs);
            updateSSBOData<double>(ssbo_r_cache, r_cache);
        }
        ssbo_planets_size = molecs.size();
    } else if (!no_change_molecs && gpu_mode && ssbo_planets!=0) {
        updateSSBOData<Molecule>(ssbo_planets, molecs);
        if (change_mass) {
            updateSSBOData<double>(ssbo_r_cache, r_cache);
        }
    }

}
// вызывайте в main чтобы не делать длинных строк
#define changePlanets_m(change_mass, change_molecs) changePlanets(ssbo_planets_size, ssbo_count_colz, ssbo_planets, ssbo_r_cache, ssbo_collizes, ssbo_project_circles, change_mass, !change_molecs)
char* inject_gpu_threads(const char* shader_source_code, const char* threads_count) {
    if (!shader_source_code || !threads_count) return nullptr;

    // Переводим в std::string исключительно внутри функции для безопасного поиска позиций
    std::string source(shader_source_code);
    std::string final_source = "";
    
    std::string version_token = "#version";
    size_t version_pos = source.find(version_token);
    
    if (version_pos != std::string::npos) {
        size_t end_of_line = source.find("\n", version_pos);
        if (end_of_line != std::string::npos) {
            final_source += source.substr(0, end_of_line + 1);
            final_source += "#define GPU_THREADS ";
            final_source += threads_count;
            final_source += "\n";
            final_source += source.substr(end_of_line + 1);
        }
    } else {
        final_source = std::string("#define GPU_THREADS ") + threads_count + "\n" + source;
    }

    // Выделяем новую динамическую память под размер получившейся строки + 1 байт для нуль-терминатора '\0'
    char* result_glsl = new char[final_source.length() + 1];
    
    // Копируем данные в выделенный буфер
#if defined(_WIN32) && defined(_MSC_VER)
    strcpy_s(result_glsl, final_source.length() + 1, final_source.c_str());
#else
    std::strcpy(result_glsl, final_source.c_str());
#endif

    return result_glsl; // Возвращаем сырой указатель char*
}
int main() {
    defaultFont.loadFromFile("C:/Windows/Fonts/arial.ttf");
    setup_read(setup_path, settings);
    gpu_mode = settings[gpu_mode_sett_id].get_value<bool>();
    gravSourse = inject_gpu_threads(gravSourse, settings[gpu_num_threads_sett_id].value.c_str());

    std::vector<double> temperatures;
    std::vector<CircleData> circles(100);
    molecs.resize(100);
    for (int i = 0; i!=100; i++) {
        molecs[i] = random_molec;
    }
    #ifdef _OPENMP
    int num_threads = omp_get_max_threads();
    #else
    int num_threads = 1;
    #endif
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
    bool change_mass_mode = false;
    int info_planet_index = 0;

    bool remove_mode = false;
    double remove_size = 100000;

    bool mode_3d = false;
    std::vector<ProjectedCircle> project_circles(molecs.size());

    Vector3d cam_pos;
    Vector3d cam_rotate;
    const float start_speed = 2e6;
    float speed_cam = start_speed; // 12 млн. км/с
    float speed_pow_sec_cam = 1.2;

    int chunk = 5;
    sf::VideoMode vidioMode(800, 600);
    sf::View view(sf::FloatRect(0.f, 0.f, 800.f, 600.f));
    sf::RenderWindow window(vidioMode, "Simulate (SFML)");
    window.setFramerateLimit(30);
    sf::Clock clock;
    circle_lib_init();
    bloom_init();
    //if (gpu_mode) {
    loadGPUFunctions();
    //}
    sf::VertexArray va_glows(sf::Triangles);
    sf::VertexArray va_circles(sf::Triangles);
    std::vector<bool> vis_table;
    std::vector<bool> destroyed(molecs.size());
    sf::Vector2i lastMousePos;
    r_cache.resize(molecs.size());
    #pragma omp parallel for schedule(static)
    for (int i = 0; i!=molecs.size(); i++) {
        r_cache[i] = molecs[i].get_r_real();
    }
    // gpu load
    // для вычислений на видиокарте
    GLuint ssbo_planets;
    size_t ssbo_planets_size;
    GLuint ssbo_r_cache;
    GLuint ssbo_collizes;
    GLuint ssbo_count_colz;
    GLuint gravShader;
    if (gpu_mode) {
        ssbo_planets = 0;
        ssbo_planets_size = 0;
        ssbo_r_cache = 0;
        ssbo_collizes = 0;
        ssbo_count_colz = createEmptySSBO(4);
        {
            std::vector<uint32_t> colz(1);
            colz[0] = 0;
            updateSSBOData(ssbo_count_colz, colz);
        }
        gravShader = compileShader(gravSourse);
    }
    // для 3d
    sf::Shader display3d_shader; // считает в 3d тени, куда попал луч (матиматически), заготавливает сверх-яркие пиксели (яркость больше 1.0 тоесть 256+)
    if (!display3d_shader.loadFromFile("3d_planets.frag", sf::Shader::Fragment)) {
        std::cout << "error in shader or don't found file 3d_planets.frag" << std::endl;
        exit(-1);
    }
    GLuint ssbo_project_circles = 0;
    sf::RenderTexture tex3d_display;
    while (window.isOpen()) {
        if (gpu_mode) {
            // проверка (но лучше вызывайте changePlanets сами)
            changePlanets_m(false, false);
        }
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0.f, 0.f, event.size.width, event.size.height);
                width = event.size.width;
                height = event.size.height;
                if (mode_3d) {
                    tex3d_display.create(width, height);
                    if (settings[bloom_sett_id].get_value<bool>()) {
                        convertToHDRTexture(const_cast<sf::Texture&>(tex3d_display.getTexture()), width, height);
                    }
                }
                window.setView(sf::View(visibleArea));
                clock.restart();
            } else if (event.type == sf::Event::MouseButtonPressed && !remove_mode) {
                const sf::Vector2f mouse_posp = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                const Molecule& molec = molecs[info_planet_index];

                float screen_radius = std::max(molec.get_r() * scale, 2.0);
                // screen_radius += 2;

                sf::Vector2f screen_pos = to_coords(Vector2d(molec.x, molec.y), 0);

                sf::Vector2f pos_button_view(screen_pos.x + screen_radius + 5.0f, screen_pos.y - 15.0f + 16.0f*2 + padd_buttons_info);
                sf::Vector2f pos_button_change_mass = pos_button_view+sf::Vector2f(0, padd_buttons_info+button_view_height);
                if (info_mode && is_click_button(pos_button_view, sf::Vector2f(button_view_width, button_view_height), mouse_posp)) {
                    view_mode = !view_mode;
                } else if (info_mode && is_click_button(pos_button_change_mass, sf::Vector2f(button_view_width, button_view_height), mouse_posp)) {
                    change_mass_mode = true;
                    obj_mass = molec.size;
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
                        change_mass_mode = false;
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
                            r_cache.resize(molecs.size());
                            #pragma omp parallel for schedule(static)
                            for (int i = 0; i!=molecs.size(); i++) {
                                r_cache[i] = molecs[i].get_r_real();
                            }
                            changePlanets_m(true, true);
                            if (mode_3d) {
                                sf::Mouse::setPosition(sf::Vector2i(window.getSize().x / 2, window.getSize().y / 2), window);
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
                                    double dist = sqrt(dx*dx + dy*dy);

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
                        clock.restart();
                        if (mode_3d) {
                            sf::Mouse::setPosition(sf::Vector2i(window.getSize().x / 2, window.getSize().y / 2), window);
                        }
                    } else {
                        hide_molecs = !hide_molecs;
                    }
                } else if (event.key.code == sf::Keyboard::D) {
                    if (!mode_3d) {
                        density_snow = !density_snow;
                    }
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
                        auto speed = (pos_end-pos_obj)/(full_mode? 10000.0: 1.0);
                        if (view_mode) {
                            speed += Vector2d(molecs[info_planet_index].sx, molecs[info_planet_index].sy);
                        }
                        molecs.emplace_back(pos_obj.x, pos_obj.y, obj_mass, speed.x, speed.y, false);
                        r_cache.push_back(molecs[molecs.size()-1].get_r_real());
                        changePlanets_m(true, true);
                        // std::cout << "speed2:" << magnitude(molecs[molecs.size()-1].sx, molecs[molecs.size()-1].sy) << "km/c\n";
                    }
                } else if (event.key.code == sf::Keyboard::Space) {
                    create_mode = false;
                    info_mode = false;
                    metr_mode = false;
                    distance_metr = 0;
                    remove_mode = false;
                    view_mode = false;
                    change_mass_mode = false;
                } else if (event.key.code == sf::Keyboard::Escape) {
                    stop_simulate = !stop_simulate;
                } else if (event.key.code == sf::Keyboard::M) {
                    double mass = getDouble("Input", "Input Mass", window);
                    if (mass>0) {
                        obj_mass = mass;
                    }
                    clock.restart();
                    if (mode_3d) {
                        sf::Mouse::setPosition(sf::Vector2i(window.getSize().x / 2, window.getSize().y / 2), window);
                    }
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
                    } else if (event.key.shift) {
                        destroy_mode = !destroy_mode;
                    } else {
                        int fps = getNumber("Input", "Input Fps", window);
                        if (fps>0) {
                            window.setFramerateLimit(fps);
                            cfps = fps;
                        }
                        clock.restart();
                        if (mode_3d) {
                            sf::Mouse::setPosition(sf::Vector2i(window.getSize().x / 2, window.getSize().y / 2), window);
                        }
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
                                r_cache.resize(molecs.size());
                                #pragma omp parallel for schedule(static)
                                for (int i = 0; i!=molecs.size(); i++) {
                                    r_cache[i] = molecs[i].get_r_real();
                                }
                                changePlanets_m(true, true);
                                clock.restart();
                            } else {
                                std::cerr << "I don`t open file!" << std::endl;
                            }
                        }
                    }
                } else if (event.key.code == sf::Keyboard::L) {
                    std::cout << molecs.size() << std::endl;
                    double x_avg = 0;
                    double y_avg = 0;
                    double m_avg = 0;
                    for (const auto& molec: molecs) {
                        x_avg += molec.x;
                        y_avg += molec.y;
                        m_avg += molec.size;
                    }
                    if (!molecs.empty()) {
                        std::cout << "x:" << x_avg/molecs.size() << std::endl;
                        std::cout << "y:" << y_avg/molecs.size() << std::endl;
                        std::cout << "m:" << m_avg/molecs.size() << std::endl;
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
                    double tempEp = 0;
                    if (temperature_snow) {
                        for (int i = 0; i!=molecs.size(); i++) {
                            tempEp += molecs[i].getTotalHeatCapacity()*temperatures[i]/1e6;
                        }
                    }
                    std::cout << "Energy:" << totalEk+totalEp+tempEp << std::endl;
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
                                change_mass_mode = false;
                            }
                        }
                    }
                } else if (event.key.code == sf::Keyboard::A) {
                    if (!mode_3d) {
                        if (event.key.shift) {
                            chaos /= 1.2;
                        } else {
                            chaos *= 1.2;
                        }
                    }
                } else if (event.key.code==sf::Keyboard::S) {
                    if (!mode_3d) {
                        printStats(molecs);
                    }
                } else if (event.key.code==sf::Keyboard::O) {
                    show_setup_interface(settings, setup_path, defaultFont);
                    window.setActive(true);
                    if (mode_3d) {
                        sf::Mouse::setPosition(sf::Vector2i(window.getSize().x / 2, window.getSize().y / 2), window);
                    }
                    clock.restart();
                } else if (event.key.code==sf::Keyboard::V) {
                    mode_3d = !mode_3d;
                    window.setMouseCursorVisible(!mode_3d);
                    if (mode_3d) {
                        sf::Mouse::setPosition(sf::Vector2i(window.getSize().x / 2, window.getSize().y / 2), window);
                        ssbo_project_circles = createEmptySSBO(sizeof(ProjectedCircle)*molecs.size());
                        tex3d_display.create(width, height);
                        if (settings[bloom_sett_id].get_value<bool>()) {
                            convertToHDRTexture(const_cast<sf::Texture&>(tex3d_display.getTexture()), width, height);
                        }
                        cam_pos = Vector3d(camx, 0, camy);
                    } else if (ssbo_project_circles!=0) {
                        deleteSSBO(ssbo_project_circles);
                        ssbo_project_circles = 0;
                        tex3d_display.create(1, 1);
                    }
                }
            }
        }
        sf::Time delta = clock.restart();
        dt = delta.asSeconds();
        if (was_rebooted_n_seconds_ago(dt+1)) [[unlikely]] {
            continue;
        }
        if (mode_3d) {
            CameraDirections dirs = calculateCameraDirections(cam_rotate);
            Vector3d last_cam_pos = cam_pos;
            updateCameraPosition(cam_pos, dirs, speed_cam, dt);
            if (last_cam_pos==cam_pos) {
                speed_cam = start_speed;
            }
            speed_cam *= std::pow(speed_pow_sec_cam, dt);
            updateCameraRotation(cam_rotate, window);
            // std::cout << "cam_pos:" << cam_pos.x << " " << cam_pos.y << " " << cam_pos.z << std::endl;
            // std::cout << "cam_rotate:" << cam_rotate.x << " " << cam_rotate.y << " " << cam_rotate.z << std::endl;
            //sf::Mouse::setPosition(sf::Vector2i(window.getSize().x / 2, window.getSize().y / 2), window);
        }
        // dt = 1.0/cfps;
        window.setTitle("Simulate (SFML) (FPS="+std::to_string(static_cast<int>(1/dt))+", Speed="+ format_sspeed(speed)+", chaos="+ format_dist(chaos)+", width="+ format_dist(width/scale) +" km)");
        if (!stop_simulate) {
            r_cache.resize(molecs.size());
            if (temperature_snow) {
                temperatures.resize(molecs.size(), 0);
            }
            if (!gpu_mode) {
                #pragma omp parallel for schedule(static)
                for (auto& molec: molecs) {
                    Vector2d pos_start(molec.x, molec.y);
                    molec.forward();
                    Vector2d pos_end(molec.x, molec.y);
                    if (view_mode && &molecs[info_planet_index]==&molec) [[unlikely]] {
                        Vector2d diff = pos_end-pos_start;
                        camx += diff.x;
                        camy += diff.y;
                        pos_obj += diff;
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
            }
            if (full_mode) {
                std::vector<std::vector<Molecule>> need_create;
                std::vector<std::vector<double>> temperatures_need;
                need_create.resize(num_threads);
                temperatures_need.resize(num_threads);
                #pragma omp parallel for schedule(static)
                for (int i = 0; i < molecs.size(); ++i) { // Работаем через индекс для OpenMP
                    auto& molec = molecs[i];
                    if (molec.size<1e20) {
                        continue;
                    }
                    int j = 0;
                    for (const auto& molec2 : molecs) {
                        j += 1;
                        if (&molec == &molec2) continue; // Не рвем сами себя

                        double prilive_strenght = molec.get_prilive_strengh(molec2);
                        // std::cout << prilive_strenght<< std::endl;
                        double max_f = molec.getMaterialCohesionForce();

                        if (prilive_strenght > max_f) {
                            double radius = molec.get_r();
                            double r_equil = sqrt((strength / 1e6 * molec.size) / (prilive_strenght / radius)); 
                            double delta_R = std::max(0.0, radius - r_equil);

                            // 2. Считаем массу этого слоя (упрощенно как объем сферического слоя)
                            // dm = 4 * PI * R^2 * delta_R * плотность
                            double density = molec.size / (1.333 * 3.1415 * radius*radius*radius);
                            double get_m = 4.0 * 3.1415 * radius*radius * delta_R * density * dt * speed;

                            // 3. Ограничиваем, чтобы за один шаг не оторвать больше половины (защита интегратора)
                            get_m = std::min(get_m, molec.size * 0.5);
                            if (get_m>molec.size/100) {
                                // Расчет направления от звезды к планете
                                double dx = molec.x - molec2.x;
                                double dy = molec.y - molec2.y;
                                double dist = sqrt(dx*dx + dy*dy);
                                double dist_inv = 1/dist;
                                double ux = dx * dist_inv; // Вектор направления
                                double uy = dy * dist_inv;

                                // Разделяем общую массу отрыва на несколько мелких кусков
                                int num = 5;
                                double fragment_m = get_m / num;
                                #ifdef _OPENMP
                                int thread_id = omp_get_thread_num();
                                #else
                                int thread_id = 0;
                                #endif
                                Molecule new_molec = molec;
                                new_molec.size = fragment_m;
                                for (int f = 0; f < num; ++f) {
                                    // 1. Позиция: выносим цепочкой вдоль приливной оси
                                    double side = ((rand() & 1) == 0) ? 1.0 : -1.0; 
                                    // Разносим их так, чтобы они не касались друг друга и мамы
                                    double spawn_dist = molec.get_r() + new_molec.get_r() * (2.0 + f * 1.2); 
                                    new_molec.x = molec.x + ux * spawn_dist * side;
                                    new_molec.y = molec.y + uy * spawn_dist * side;

                                    molec.size -= fragment_m;
                                    r_cache[i] = molec.get_r_real();
                                    need_create[thread_id].push_back(new_molec);
                                    if (temperature_snow) {
                                        temperatures_need[thread_id].push_back(temperatures[j-1]);
                                    }
                                }
                            }
                        }
                    }
                }
                size_t added = 0;
                for (size_t i = 0; i!=num_threads; i++) {
                    added += need_create[i].size();
                }
                molecs.reserve(molecs.size()+added);
                if (temperature_snow) {
                    temperatures.reserve(temperatures.size()+added);
                }
                for (auto& vec: need_create) {
                    for (auto& molec: vec) {
                        molecs.push_back(molec);
                    }
                }
                for (auto& vec: temperatures_need) {
                    for (auto t: vec) {
                        temperatures.push_back(t);
                    }
                }
                int past_size = r_cache.size();
                r_cache.resize(molecs.size());
                for (int i = past_size; i!=r_cache.size(); i++) {
                    r_cache[i] = molecs[i].get_r_real();
                }
            }
            changePlanets_m(false, false);
            std::vector<std::pair<uint32_t, uint32_t>> is_collizes;
            if (!gpu_mode) {
                std::vector<std::vector<std::pair<Molecule*, Molecule*>>> is_collizes_paral;
                is_collizes_paral.resize(num_threads);
                #pragma omp parallel for schedule(static)
                for (auto& molec: molecs) {
                    auto info = molec.gravity_start();
                    for (auto& molec2: molecs) {
                        if (&molec != &molec2) {
                            bool is_collize = molec.attract(molec2);
                            if (is_collize) {
                                // m.lock();
                                #ifdef _OPENMP
                                int thread_id = omp_get_thread_num();
                                #else
                                int thread_id = 0;
                                #endif
                                is_collizes_paral[thread_id].emplace_back(&molec, &molec2);
                                // m.unlock();
                            }
                        }
                    }
                    molec.gravity_end(info);
                }
                for (auto& is_collizes2: is_collizes_paral) {
                    for (size_t i = 0; i!=is_collizes2.size(); i++) {
                        auto& [colz1, colz2] = is_collizes2[i];
                        is_collizes.emplace_back(colz1-&molecs[0], colz2-&molecs[0]);
                    }
                }
            }
            if (!molecs.empty() && gpu_mode) {
                bindSSBO(ssbo_planets, 0);
                bindSSBO(ssbo_r_cache, 1);
                bindSSBO(ssbo_collizes, 2);
                bindSSBO(ssbo_count_colz, 3);
                gpuRunnb(gravShader, molecs.size(), "total", (uint32_t)molecs.size(), "dt", dt*speed);
                clearBind(0);
                clearBind(1);
                clearBind(2);
                clearBind(3);
                downloadSSBOData(ssbo_planets, molecs);
                std::vector<uint32_t> colz(1);
                downloadSSBOData(ssbo_count_colz, colz);
                if (colz[0]>molecs.size()*6) {
                    // std::cout << "The application has overwritten something's memory by " << (colz[0]-molecs.size()*2)*4*2 << " byte" << std":endl;
                    // std::cout << ""
                    std::cout << "error: too many collisions. buffer at " << molecs.size()*2 << " collisions but at the moment at " << colz[0] << " collisions" << std::endl;
                    std::cout << "if you want to continue, press n and press Enter and hold it for " << colz[0]/(molecs.size()*2)/30 << " seconds at 30 fps, otherwise press y and then Enter" << std::endl;
                    std::string str;
                    std::cin >> str;
                    if (str=="n") {
                        exit(1);
                    }
                }
                std::vector<uint32_t> last_colz = colz;
                is_collizes.resize(colz[0]);
                downloadSSBOData(ssbo_collizes, is_collizes);
                colz[0] = 0;
                updateSSBOData(ssbo_count_colz, colz);
            }
            destroyed.resize(molecs.size());
            for (size_t i = 0; i!=molecs.size(); i++) {
                destroyed[i] = false;
            }
            std::vector<Molecule> need_molecs;
            //std::cout << "start " << last_colz[0] << " " << molecs.size() << std::endl;
            // for (auto& is_collizes2 : is_collizes) {
                //for (auto& [colz1, colz2] : is_collizes2) {
            for (auto [idx1, idx2] : is_collizes) {
                auto colz1 = &molecs[idx1];
                auto colz2 = &molecs[idx2];
                if (idx2>=destroyed.size() || idx1>=destroyed.size() || destroyed[idx1] || destroyed[idx2]) continue;
                double dx = colz1->sx-colz2->sx;
                double dy = colz1->sy-colz2->sy;
                double uSq = dx*dx+dy*dy;

                if (colz1->size < colz2->size) {
                    double totalM = colz1->size + colz2->size;
                    colz2->sx = (colz2->sx * colz2->size + colz1->sx * colz1->size) / totalM;
                    colz2->sy = (colz2->sy * colz2->size + colz1->sy * colz1->size) / totalM;
                    
                    double mass2 = colz2->size;
                    colz2->size = totalM;
                    double two_space = colz2->get_two_space(*colz1);
                    double u = sqrt(uSq);
                    double k = (destroy_mode && two_space<u)? (0.05+0.65/(1+std::pow(M_E, -2*(u/two_space-1)))): 1;
                    colz2->size = mass2;
                    if (temperature_snow) {
                        temperatures[idx2] = (temperatures[idx2]*colz2->getTotalHeatCapacity()+temperatures[idx1]*colz1->getTotalHeatCapacity())*k/(colz1->getTotalHeatCapacity()+colz2->getTotalHeatCapacity());
                        temperatures[idx2] += 500000*uSq*colz1->size*colz2->size/colz2->getSpecificHeat()/totalM/totalM;
                    }
                    colz2->size = totalM;
                    r_cache[idx2] = colz2->get_r_real();

                    destroyed[idx1] = true;
                    if (destroy_mode && two_space<u) {
                        double k_minus_mass = 0.5*(1-std::pow(M_E, -(u/two_space-1)));
                        double minus_mass2 = mass2*(1-k_minus_mass);
                        double minus_mass1 = colz1->size*(1-k_minus_mass);
                        if (minus_mass2+minus_mass1<colz2->size) {
                            double minus = 0;
                            double dx = colz1->x-colz2->x;
                            double dy = colz1->y-colz2->y;
                            double dist = sqrt(dx*dx+dy*dy);
                            double tx = dx/dist;
                            double ty = dy/dist;
                            double u_exit = u*sqrt(1-k);
                            if (minus_mass1>1e20) {
                                double u_x1 = colz2->sx-tx*u_exit*mass2/colz2->size;
                                double u_y1 = colz2->sy-ty*u_exit*mass2/colz2->size;
                                Molecule part1(0, 0, minus_mass1, u_x1, u_y1, false);
                                double x1 = colz2->x-tx*(colz2->get_r()*1.01+part1.get_r());
                                double y1 = colz2->y-ty*(colz2->get_r()*1.01+part1.get_r());
                                part1.x = x1;
                                part1.y = y1;
                                need_molecs.push_back(part1);
                                if (temperature_snow) {
                                    temperatures.push_back(temperatures[idx2]);
                                }
                                minus += minus_mass1;
                            }
                            if (minus_mass2>1e20) {
                                double u_x2 = colz2->sx+tx*u_exit*colz1->size/colz2->size;
                                double u_y2 = colz2->sy+ty*u_exit*colz1->size/colz2->size;
                                Molecule part2(0, 0, minus_mass2, u_x2, u_y2, false);
                                double x2 = colz2->x+tx*(colz2->get_r()*1.01+part2.get_r());
                                double y2 = colz2->y+ty*(colz2->get_r()*1.01+part2.get_r());
                                part2.x = x2;
                                part2.y = y2;
                                need_molecs.push_back(part2);
                                if (temperature_snow) {
                                    temperatures.push_back(temperatures[idx2]);
                                }
                                minus += minus_mass2;
                            }

                            colz2->size -= minus;
                        }
                    }
                } 
                else {
                    double totalM = colz1->size + colz2->size;
                    colz1->sx = (colz1->sx * colz1->size + colz2->sx * colz2->size) / totalM;
                    colz1->sy = (colz1->sy * colz1->size + colz2->sy * colz2->size) / totalM;
                    double mass1 = colz1->size;
                    colz1->size = totalM;
                    double two_space = colz1->get_two_space(*colz2);
                    double u = sqrt(uSq);
                    double k = (destroy_mode && two_space<u)? (0.05+0.65/(1+std::pow(M_E, -2*(u/two_space-1)))): 1;
                    colz1->size = mass1;
                    if (temperature_snow) {
                        temperatures[idx1] = (temperatures[idx2]*colz2->getTotalHeatCapacity()+temperatures[idx1]*colz1->getTotalHeatCapacity())*k/(colz1->getTotalHeatCapacity()+colz2->getTotalHeatCapacity());
                        temperatures[idx1] += 500000*uSq*colz1->size*colz2->size/colz1->getSpecificHeat()/totalM/totalM;
                    }
                    colz1->size = totalM;
                    r_cache[idx1] = colz1->get_r_real();
                    destroyed[idx2] = true;
                    if (destroy_mode && two_space<u) {
                        double k_minus_mass = 0.5*(1-std::pow(M_E, -(u/two_space-1)));
                        double minus_mass1 = mass1*(1-k_minus_mass);
                        double minus_mass2 = colz2->size*(1-k_minus_mass);
                        if (minus_mass1+minus_mass2<colz1->size) {
                            double minus = 0;
                            double dx = colz1->x-colz2->x;
                            double dy = colz1->y-colz2->y;
                            double dist = sqrt(dx*dx+dy*dy);
                            double tx = dx/dist;
                            double ty = dy/dist;
                            double u_exit = u*sqrt(1-k);
                            if (minus_mass1>1e20) {
                                double u_x1 = colz1->sx-tx*u_exit*colz2->size/colz1->size;
                                double u_y1 = colz1->sy-ty*u_exit*colz2->size/colz1->size;
                                Molecule part1(0, 0, minus_mass1, u_x1, u_y1, false);
                                double x1 = colz1->x-tx*(colz1->get_r()*1.01+part1.get_r());
                                double y1 = colz1->y-ty*(colz1->get_r()*1.01+part1.get_r());
                                part1.x = x1;
                                part1.y = y1;
                                need_molecs.push_back(part1);
                                if (temperature_snow) {
                                    temperatures.push_back(temperatures[idx1]);
                                }
                                minus += minus_mass1;
                            }
                            if (minus_mass2>1e20) {
                                double u_x2 = colz1->sx+tx*u_exit*mass1/colz1->size;
                                double u_y2 = colz1->sy+ty*u_exit*mass1/colz1->size;
                                Molecule part2(0, 0, minus_mass2, u_x2, u_y2, false);
                                double x2 = colz1->x+tx*(colz1->get_r()*1.01+part2.get_r());
                                double y2 = colz1->y+ty*(colz1->get_r()*1.01+part2.get_r());
                                part2.x = x2;
                                part2.y = y2;
                                need_molecs.push_back(part2);
                                if (temperature_snow) {
                                    temperatures.push_back(temperatures[idx1]);
                                }
                                minus += minus_mass2;
                            }

                            colz1->size -= minus;
                        }
                    }
                }
            }
            //}

            for (int i = molecs.size() - 1; i >= 0; i--) {
                if (destroyed[i]) {
                    if (info_planet_index>i) {
                        info_planet_index--;
                    } else if (info_planet_index==i) {
                        info_mode = false;
                        view_mode = false;
                        change_mass_mode = false;
                    }
                    molecs.erase(molecs.begin() + i);
                    r_cache.erase(r_cache.begin() + i);
                    if (temperature_snow) {
                        temperatures.erase(temperatures.begin() + i);
                    }
                }
            }
            for (const auto& molec: need_molecs) {
                molecs.push_back(molec);
            }
            changePlanets_m(true, true);
            if (temperature_snow) {
                #pragma omp parallel for schedule(static)
                for (int i = 0; i!=temperatures.size(); i++) {
                    Molecule& planet = molecs[i];
                    double delta = 0;
                    for (int j = 0; j!=molecs.size(); j++) {
                        if (i!=j) {
                            const Molecule& star = molecs[j];
                            delta += planet.calc_delta_t(star);
                        }
                    }
                    delta += planet.calc_delta_t_this(temperatures[i]);
                    temperatures[i] += delta;
                }
            }
        } else if (r_cache.size()!=molecs.size()) {
            if (r_cache.size()>molecs.size()) {
                std::cerr << "error recreate r_cache\n";
                exit(1);
            }
            if (temperature_snow) {
                temperatures.resize(molecs.size(), 0);
            }
            int past_size = r_cache.size();
            r_cache.resize(molecs.size());
            for (int i = past_size; i!=r_cache.size(); i++) {
                r_cache[i] = molecs[i].get_r_real();
            }
        }
        if (r_cache.size()!=temperatures.size() && temperature_snow) {
            temperatures.resize(molecs.size(), 0);
            // std::cerr << "err recreate temperatures ("+format_dist(r_cache.size())+","+format_dist(temperatures.size())+")" << std::endl;
            // exit(1);
        }
        if (temperature_snow) {
            for (int i = 0; i!=temperatures.size(); i++) {
                temperatures[i] = std::min(std::max(0.0, temperatures[i]), 1000000.0);
            }
        }
        window.clear(!mode_3d?sf::Color::White:sf::Color::Black);
        if (!hide_molecs) {
            if (!mode_3d) {
                size_t count_circle = 0;
                size_t count_glows = 0;
                size_t atmosfere_count = 0;
                vis_table.resize(molecs.size());
                if (planets_mode) {
                    // for (const auto& molec: molecs) {
                    for (size_t i = 0; i!=molecs.size(); i++) {
                        const auto& molec = molecs[i];
                        double visualRadius = molec.get_r();
                        CircleData circle(std::max(visualRadius * scale, 2.0)*1.5);
                        circle.setPosition(to_coords(Vector2d(molec.x, molec.y), 0));
                        bool vis = circle.in_cam(width, height);
                        vis_table[i] = vis;
                        if (vis) {
                            count_circle++;
                            if (settings[show_atmosphere_sett_id].get_value<bool>() && get_object_params(molec.size).glow_factor!=0.0 && visualRadius*scale>=2) {
                                atmosfere_count++;
                            }
                        }
                    }
                }
                circles.resize(count_circle);
                va_glows.resize(atmosfere_count*90);
                count_circle = 0;
                for (size_t i = 0; i!=molecs.size(); i++) {
                    const auto& molec = molecs[i];
                    if (vis_table[i]) {
                        if (molec.draw(circles, va_glows, count_glows, count_circle)) {
                            count_glows++;
                        }
                        count_circle++;
                    }

                }
                //std::cout << atmosfere_count << " " << count_glows << std::endl;
                window.draw(va_glows);
                draw_circles(window, circles, va_circles);
            } else {
                float scale = calcScale(height);
                project_circles.resize(molecs.size());
                for (size_t i = 0; i!=molecs.size(); i++) {
                    auto& molec = molecs[i];
                    Vector3d planet_pos = Vector3d(molec.x, 0, molec.y);
                    Vector3d different = cam_pos-planet_pos;
                    double distSq = different.x*different.x+different.y*different.y+different.z*different.z;
                    double dist = sqrt(distSq);
                    double dist_inv = 1.0/dist;
                    double r = molec.get_r();
                    double visualRadius = r*dist_inv;
                    Vector3d planet_pos_rotated = rotateVector3D(different, -cam_rotate);
                    double inv_z = 1.0/planet_pos_rotated.z;
                    sf::Vector2f screen_pos_planet;
                    if (planet_pos_rotated.z>0) {
                        screen_pos_planet = sf::Vector2f((float)(planet_pos_rotated.x*inv_z), (float)(planet_pos_rotated.y*inv_z));
                    } else {
                        screen_pos_planet = sf::Vector2f(-visualRadius, -visualRadius);
                    }
                    if (planet_pos_rotated.z>0) {
                        project_circles[i].screenPos = sf::Vector2i(screen_pos_planet*scale+sf::Vector2f(width/2, height/2));
                    } else {
                        project_circles[i].screenPos = sf::Vector2i(screen_pos_planet*scale)-sf::Vector2i(1, 1);
                    }
                    project_circles[i].screenRadius = visualRadius*scale;
                    project_circles[i].depth = dist;
                    project_circles[i].radius = r;
                    project_circles[i].pos = sf::Vector2f(molec.x, molec.y);
                    
                    // проверяем не звезда ли планета
                    if (1.5e29<=molec.size) {
                        double p = precomputeStarPower(molec.size);
                        if (settings[simple_bloom_stars_sett_id].get_value<bool>()) {
                            project_circles[i].bloom_size = getSimpleStarBloom(molec.size, 0.3);
                        } else {
                            project_circles[i].bloom_size = getStarBloomSize(molec.size);
                        }
                        project_circles[i].bloom_light = p*3.827e26;
                    } else {
                        // проверяем включины ли тени
                        if (settings[simple_shadows_sett_id].get_value<bool>()) {
                            project_circles[i].bloom_size = 0; // шейдер увидив такое подумает что это планета и нужны тени
                        } else {
                            // шейдер увидив такое подумает что это звезда (так как есть bloom изначально хоть какой-то) и не надо ему делать тени
                            project_circles[i].bloom_size = 1;
                        }
                        project_circles[i].bloom_light = 0;
                    }
                    Visuals v = get_object_params(molec.size);
                    project_circles[i].color = v.core_color;
                    project_circles[i].alberto = molec.getAverageAlbedo();
                    //project_circles[i].alberto = 0.2;
                }
                updateSSBOData<ProjectedCircle>(ssbo_project_circles, project_circles);
                tex3d_display.clear(sf::Color::Black);
                sf::VertexArray va_3d_display(sf::Triangles, 6);
                va_3d_display[0].position = sf::Vector2f(0, 0);
                va_3d_display[1].position = sf::Vector2f(width, 0);
                va_3d_display[2].position = sf::Vector2f(width, height);
                va_3d_display[3].position = sf::Vector2f(width, height);
                va_3d_display[4].position = sf::Vector2f(0, height);
                va_3d_display[5].position = sf::Vector2f(0, 0);
                bindSSBO(ssbo_project_circles, 0);
                glUseProgram(display3d_shader.getNativeHandle());
                GLint totalLocation = glGetUniformLocation(display3d_shader.getNativeHandle(), "total");
                if (totalLocation != -1) {
                    glUniform1ui(totalLocation, static_cast<GLuint>(project_circles.size()));
                }
                display3d_shader.setUniform("scale", scale);
                display3d_shader.setUniform("tanFOV", float(height/scale*0.5));
                display3d_shader.setUniform("cam_pos", sf::Vector3f(cam_pos));
                display3d_shader.setUniform("screenSize", sf::Vector2f(width, height));
                auto vecs = calculateCameraVectors(cam_rotate);
                display3d_shader.setUniform("camForward", vecs.forward);
                display3d_shader.setUniform("camRight", vecs.right);
                display3d_shader.setUniform("camUp", vecs.up);

                tex3d_display.draw(va_3d_display, &display3d_shader);
                clearBind(0);
                if (settings[bloom_sett_id].get_value<bool>()) {
                    drawBloom(window, tex3d_display);
                } else {
                    tex3d_display.display();
                    sf::Sprite testSprite(tex3d_display.getTexture());
                    window.draw(testSprite); 
                }
            }
        }
        if (create_mode) {
            double size = Molecule(0,0, obj_mass, 0,0, false).get_r();
            double size_screen = std::max(size*scale, 2.0);
            sf::CircleShape shape(size_screen);
            shape.setPosition(to_coords(pos_obj, size_screen/scale));
            //shape.setPosition(to_coords(pos_obj, 0));
            if (planets_mode) {
                sf::Color color = get_object_params(obj_mass).core_color;
               shape.setFillColor(color);
            } else {
                shape.setFillColor(sf::Color::Blue);
            }
            window.draw(shape);
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            sf::Vector2f startPos = to_coords(pos_obj, 0);
            const Vector2d simVelS = ((Vector2d)mousePos - (Vector2d)startPos)/scale/(full_mode? 10000.0: 1.0);
            Vector2d simVel = simVelS;
            if (view_mode) {
                simVel += Vector2d(molecs[info_planet_index].sx, molecs[info_planet_index].sy);
            }
            // std::cout << "speed1:" << magn(simVel) << "km/c\n";
            Vector2d simPos = pos_obj; 
            const int steps = 1000;
            const double prediction_dt = (full_mode? 500.0f: 0.01f);
            const double prediction_dtSq = prediction_dt*prediction_dt;
            
            sf::VertexArray trajectory(sf::PrimitiveType::LineStrip, steps);
            
            auto simulate_molecs = molecs; 
            size_t sim_molec_idx = simulate_molecs.size();
            simulate_molecs.emplace_back(simPos.x, simPos.y, obj_mass, simVel.x, simVel.y, false);
            for (int i = 0; i < steps; ++i) {
                #pragma omp parallel for
                for (size_t a = 0; a < simulate_molecs.size(); ++a) {
                    auto d = simulate_molecs[a].gravity_start();
                    for (size_t b = 0; b < simulate_molecs.size(); ++b) {
                        if (a!=b) {
                            simulate_molecs[a].attract_opt(simulate_molecs[b]);
                        }
                    }
                    simulate_molecs[a].gravity_end(d, prediction_dt);
                }
                #pragma omp parallel for
                for (auto& m : simulate_molecs) {
                    m.forward(prediction_dt, prediction_dtSq);
                }
                if (view_mode) {
                    Vector2d old_pos(molecs[info_planet_index].x, molecs[info_planet_index].y);
                    Vector2d new_pos(simulate_molecs[info_planet_index].x, simulate_molecs[info_planet_index].y);
                    trajectory[i].position = to_coords(Vector2d(simulate_molecs[sim_molec_idx].x, simulate_molecs[sim_molec_idx].y)+old_pos-new_pos, 0);
                } else {
                    trajectory[i].position = to_coords(Vector2d(simulate_molecs[sim_molec_idx].x, simulate_molecs[sim_molec_idx].y), 0);
                }
                
                sf::Color tColor = sf::Color::Black;
                tColor.a = static_cast<sf::Uint8>(255 * (1.0f - (float)i / steps));
                trajectory[i].color = tColor;
            }
            window.draw(trajectory);

            sf::VertexArray line(sf::Lines, 2);
            line[0].position = to_coords(pos_obj, 0);
            line[0].color = sf::Color::Green;
            line[1].position = mousePos;
            line[1].color = sf::Color::Green;
            window.draw(line);

            sf::Text text;
            text.setFont(defaultFont);
            text.setFillColor(sf::Color::Black);
            text.setCharacterSize(20);
            text.setPosition(to_coords(pos_obj, 0)+sf::Vector2f(40,40));
            text.setString(format_speed(magn(simVelS)*1000));
            window.draw(text);
        }
        if (metr_mode) {
            const int size_point = 5;
            sf::VertexArray line(sf::Lines, 2);
            line[0].position = to_coords(start_pos_metr, 0);
            line[0].color = sf::Color(0, 0, 0, 150);
            line[1].position = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            line[1].color = sf::Color(0, 0, 0, 150);
            window.draw(line);

            sf::CircleShape point1(size_point);
            point1.setFillColor(sf::Color::Green);
            point1.setPosition(to_coords(start_pos_metr, size_point/scale));
            window.draw(point1);

            sf::CircleShape point2(size_point);
            point2.setFillColor(sf::Color::Green);
            point2.setPosition(window.mapPixelToCoords(sf::Mouse::getPosition(window))-sf::Vector2f(size_point, size_point));
            window.draw(point2);
        } else if (distance_metr!=0) {
            const int size_point = 5;
            sf::VertexArray line(sf::Lines, 2);
            line[0].position = to_coords(start_pos_metr, 0);
            line[0].color = sf::Color(0, 0, 0, 150);
            line[1].position = to_coords(end_pos_metr, 0);
            line[1].color = sf::Color(0, 0, 0, 150);
            window.draw(line);

            sf::CircleShape point1(size_point);
            point1.setFillColor(sf::Color::Green);
            point1.setPosition(to_coords(start_pos_metr, size_point/scale));
            window.draw(point1);

            sf::CircleShape point2(size_point);
            point2.setFillColor(sf::Color::Green);
            point2.setPosition(to_coords(end_pos_metr, size_point/scale));
            window.draw(point2);

            sf::Text result;
            result.setFont(defaultFont);
            result.setFillColor(sf::Color::Black);
            result.setCharacterSize(20);
            result.setString(format_dist(distance_metr) + " km");
            sf::FloatRect textRect = result.getLocalBounds();
            result.setOrigin(textRect.left + textRect.width / 2.0f, 
                            textRect.top  + textRect.height / 2.0f);
            result.setPosition(((to_coords(start_pos_metr, 0)+to_coords(end_pos_metr, 0))/(float)2)+sf::Vector2f(0, -25));
            window.draw(result);
        }
        if (info_mode) {
            Molecule& molec = molecs[info_planet_index];
            if (change_mass_mode) {
                molec.size = obj_mass;
            }

            float screen_radius = std::max(molec.get_r() * scale, 2.0);
            // screen_radius += 2;

            sf::Vector2f screen_pos = to_coords(Vector2d(molec.x, molec.y), 0);
            sf::Vector2f screen_pos_circle = to_coords(Vector2d(molec.x, molec.y), screen_radius/scale);

            // 4. Считаем скорость через твою функцию magnitude
            double v_mod = magnitude(molec.sx, molec.sy);

            if (v_mod>10 || full_mode) {
                drawArrow(window, to_coords(Vector2d(molec.x, molec.y), 0), to_coords(Vector2d(molec.x+molec.sx*(full_mode? 1000: 1), molec.y+molec.sy*(full_mode? 1000: 1)), 0), sf::Color::Cyan);
            }

            // 5. Подготовка текста
            sf::Text info;
            info.setFont(defaultFont);
            info.setCharacterSize(16);
            info.setFillColor(sf::Color::Black); // Черный на белом фоне
            
            // Используем format_speed и format_dist, чтобы не было "забора" из цифр
            std::string content = "V: " + format_speed(v_mod*1000) + "\n" +
                                "M: " + format_mass(molec.size)+(temperature_snow? ("\nT="+format_dist(temperatures[info_planet_index])): "");
            info.setString(content);

            info.setPosition(screen_pos.x + screen_radius + 5.0f, screen_pos.y - 15.0f-(temperature_snow? 16.0f: 0.0f));

            window.draw(info);

            // buttons
            const sf::Vector2f mouse_pos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            auto button_view = createRoundedRect(button_view_width, button_view_height, round_buttons);
            const sf::Vector2f pos_button_view(screen_pos.x + screen_radius + 5.0f, screen_pos.y - 15.0f + 16.0f*2 + padd_buttons_info);
            button_view.setPosition(pos_button_view);
            if (is_click_button(pos_button_view, sf::Vector2f(button_view_width, button_view_height), mouse_pos)) {
                button_view.setFillColor(sf::Color(200, 200, 200));
            } else {
                button_view.setFillColor(sf::Color(210, 210, 210));
            }
            button_view.setOutlineThickness(1.5f);
            button_view.setOutlineColor(sf::Color(180, 180, 180));
            window.draw(button_view);
            auto text_view = createTextForButton((view_mode? "stop view": "view"), pos_button_view, button_view_width, button_view_height, round_buttons);
            text_view.setFillColor(sf::Color(50, 50, 50));
            window.draw(text_view);

            auto button_change_mass = createRoundedRect(button_view_width, button_view_height, round_buttons);
            const sf::Vector2f pos_button_change_mass = pos_button_view+sf::Vector2f(0, padd_buttons_info+button_view_height);
            button_change_mass.setPosition(pos_button_change_mass);
            if (is_click_button(pos_button_change_mass, sf::Vector2f(button_view_width, button_view_height), mouse_pos)) {
                button_change_mass.setFillColor(sf::Color(200, 200, 200));
            } else {
                button_change_mass.setFillColor(sf::Color(210, 210, 210));
            }
            button_change_mass.setOutlineThickness(1.5f);
            button_change_mass.setOutlineColor(sf::Color(180, 180, 180));
            window.draw(button_change_mass);
            auto text_change_mass = createTextForButton((change_mass_mode? "stop change mass" : "change mass"), pos_button_change_mass, button_view_width, button_view_height, round_buttons);
            text_change_mass.setFillColor(sf::Color(50, 50, 50));
            window.draw(text_change_mass);

            sf::CircleShape selector(screen_radius);
            selector.setPosition(screen_pos_circle);
            selector.setFillColor(sf::Color::Transparent);
            selector.setOutlineThickness(2.0f);
            selector.setOutlineColor(sf::Color::Red);
            window.draw(selector);
        }
        if (remove_mode) {
            sf::CircleShape remove_zone(remove_size*scale);
            remove_zone.setPosition(window.mapPixelToCoords(sf::Mouse::getPosition(window))-sf::Vector2f(remove_size*scale, remove_size*scale));
            remove_zone.setFillColor(sf::Color(255, 0, 0, 150));
            window.draw(remove_zone);
            if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                Vector2d pos = from_coords(window.mapPixelToCoords(sf::Mouse::getPosition(window)), 0);
                bool destroys[molecs.size()];
                #pragma omp parallel for schedule(static)
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
                        if (temperature_snow) {
                            temperatures.erase(temperatures.begin()+i-bias_i);
                        }
                        bias_i += 1;
                    }
                }
            }
        }

        if (!full_mode && (density_snow || temperature_snow)) {
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
                            window.draw(rect);
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
                            window.draw(rect);
                            //m.unlock();
                        }
                    }
                }
            }
        }
        // renderTexture.display();
        // sf::Sprite resultSprite(renderTexture.getTexture());
        // resultSprite.setPosition(sf::Vector2f(0,0));
        window.display();
    }   
}
