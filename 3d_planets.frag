#version 430
#extension GL_ARB_shader_storage_buffer_object : enable
#extension GL_ARB_gpu_shader_fp64 : enable
#define INFINITY 1e300
#define toVec3(VEC2) vec3(VEC2.x, 0.0, VEC2.y)
struct ProjectCircle {
    // 1. Блок double (8 байт каждый) — ставим в самый верх для идеального выравнивания
    double depth;        // Расстояние до камеры (8 байт) — нужно для сортировки

    // 2. Блок sf::Vector2f (8 байт каждый, состоит из двух float)
    vec2 pos;       // Позиция в мире XZ (8 байт)
    ivec2 screenPos; // Позиция на экране XY (8 байт)

    // 3. Блок одиночных float (4 байта каждый)
    float radius;       // Радиус в мировых километрах (4 байт)
    float screenRadius;  // Радиус на экране в пикселях (4 байта)
    float bloom_size;    // Светимость для Блума (4 байта)
    float bloom_light;   // Вычисленный поток света (4 байта)
    float albedo;

    // 4. Мелкие типы (4 байта) — ставим в самый конец
    uint color;
};
layout(std430, binding = 0) buffer MolecBuffer {
    ProjectCircle circles[];
};

uniform uint total;
uniform vec3 cam_pos;
uniform float scale;
uniform vec3 camForward;
uniform vec3 camRight;
uniform vec3 camUp;
uniform vec2 screenSize;
uniform float tanFOV;
vec3 getPlanetNormal(vec3 camPos, vec3 camForward, vec3 camRight, vec3 camUp, 
                     float fovScale, vec2 screenSize, vec3 planetCenter, float planetRadius) {
    
    float aspect = screenSize.x / screenSize.y;
    vec2 ndc = (gl_FragCoord.xy / screenSize) * 2.0 - vec2(1.0);
    //ndc.y = -ndc.y;
    ndc.x *= aspect; // Теперь aspect учтен один раз
    vec3 viewVector = normalize(camForward + (camRight * ndc.x * tanFOV) + (camUp * ndc.y * tanFOV));
    vec3 camToCenter = planetCenter - camPos;
    float projection = dot(camToCenter, viewVector);
    float hitTest = fma(projection, projection, planetRadius * planetRadius) - dot(camToCenter, camToCenter);
    if (hitTest < 0.0) {
        return vec3(0.0); 
    }
    float sqrtHit = sqrt(hitTest);
    float distToSurface = projection - sqrtHit;
    if (distToSurface < 0.0) {
        distToSurface = projection + sqrtHit;
    }
    if (distToSurface < 0.0) {
        return vec3(0.0);
    }
    vec3 hitCoords = camPos + (viewVector * distToSurface);
    return normalize(hitCoords - planetCenter);
}
float calculateLightAtPlanet(vec3 starWorldPos, vec3 planetWorldPos, float starBloomLight) {
    vec3 vectorToStar = starWorldPos - planetWorldPos;
    float distanceSq = dot(vectorToStar, vectorToStar);
    // 3. Закон обратных квадратов в масштабе километров:
    // Интенсивность = Мощность_Из_SSBO * / Текущее_Расстояние^2
    float lightIntensity = starBloomLight / distanceSq;
    return lightIntensity;
}
float convertLightToBloom(float reflectedLight) {
    float bloomSize = (reflectedLight / 1.7e12);
    return bloomSize;
}
void main() {
    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    uint best_circle = -1;
    double best_dist = INFINITY;
    for (uint i = 0; i!=total; i++) {
        if (circles[i].depth<best_dist) {
            ivec2 diff = circles[i].screenPos-pixelCoord;
            vec2 diff_f = vec2(float(diff.x), float(diff.y));
            float sdist = diff_f.x*diff_f.x+diff_f.y*diff_f.y;
            if (sdist<=circles[i].screenRadius*circles[i].screenRadius) {
                best_circle = i;
                best_dist = circles[i].depth;
            }
        }
    }
    if (best_circle==-1) {
        discard;
    }
    float bloom_size = circles[best_circle].bloom_size;
    if (bloom_size!=0.0) {
        gl_FragColor = unpackUnorm4x8(circles[best_circle].color)*vec4(bloom_size, bloom_size, bloom_size, 1.0);
        return;
    }
    vec3 surfaceNormal = getPlanetNormal(cam_pos, camForward, camRight, camUp, scale, screenSize, vec3(circles[best_circle].pos.x, 0, circles[best_circle].pos.y), circles[best_circle].radius);
    for (uint i = 0; i!=total; i++) {
        if (circles[i].bloom_light!=0.0) {
            vec2 starToPlanetVector2 = circles[i].pos-circles[best_circle].pos;
            vec3 starToPlanetVector = vec3(starToPlanetVector2.x, 0.0, starToPlanetVector2.y);
            vec3 starToPlanetNormal = normalize(starToPlanetVector);

            float light_k = max(dot(surfaceNormal, starToPlanetNormal), 0.0);
            //light_k = 1.0;
            // if (light_k != 0.0) {
            float light_at_planet = calculateLightAtPlanet(toVec3(circles[i].pos), toVec3(circles[best_circle].pos), float(circles[i].bloom_light))*circles[best_circle].albedo*light_k;
            bloom_size += convertLightToBloom(light_at_planet);
            // } else {
            //     bloom_size += 1.0;
            // }
        }
    }
    gl_FragColor = unpackUnorm4x8(circles[best_circle].color)*vec4(bloom_size, bloom_size, bloom_size, 1.0);
}
