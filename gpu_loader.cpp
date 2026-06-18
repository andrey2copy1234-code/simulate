#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h> // Если этого файла нет, скачай его (1 файл) с сайта Khronos

// Объявляем указатели на функции
PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLBINDBUFFERBASEPROC glBindBufferBase;
PFNGLDISPATCHCOMPUTEPROC glDispatchCompute;
PFNGLMEMORYBARRIERPROC glMemoryBarrier;
PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORM1DPROC glUniform1d;
PFNGLUNIFORM1UIPROC glUniform1ui;
PFNGLBUFFERSUBDATAPROC glBufferSubData;
PFNGLGETSHADERIVPROC glGetShaderiv;           // Узнать, скомпилировался ли
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog; // Получить текст ошибки
PFNGLGETPROGRAMIVPROC glGetProgramiv;         // Узнать статус линковки
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog; // Ошибка линковки
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;
PFNGLDELETEBUFFERSPROC glDeleteBuffers;
// надо эту функцию в main в начале вызвать
void loadGPUFunctions() {
    glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
    glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)wglGetProcAddress("glBindBufferBase");
    glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)wglGetProcAddress("glDispatchCompute");
    glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)wglGetProcAddress("glMemoryBarrier");
    glGetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC)wglGetProcAddress("glGetBufferSubData");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    glUniform1d = (PFNGLUNIFORM1DPROC)wglGetProcAddress("glUniform1d");
    glUniform1ui = (PFNGLUNIFORM1UIPROC)wglGetProcAddress("glUniform1ui");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)wglGetProcAddress("glBufferSubData");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    glUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
    glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC)wglGetProcAddress("glGetBufferParameteriv");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffers");
}
#include <iostream>

// 1. Функция для проверки ошибок OpenGL после команд
void checkGL(const char* label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cout << "ОШИБКА OpenGL [" << label << "]: 0x" << std::hex << err << std::dec << std::endl;
        exit(1);
    }
}

GLuint compileShader(const char* shaderSource) {
    // 1. Создаем шейдер
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    
    // ДОБАВЬ ЭТОТ ВЫВОД:
    std::cout << "DEBUG: Попытка создания Compute Shader. ID: " << shader << std::endl;
    
    if (shader == 0) {
        std::cout << "ОШИБКА: Видеокарта вернула 0. Compute Shaders не поддерживаются!" << std::endl;
        return 0;
    }
    GLint length = (GLint)strlen(shaderSource);
    glShaderSource(shader, 1, &shaderSource, &length);
    glCompileShader(shader);

    // Проверяем, скомпилировался ли шейдер
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "ОШИБКА КОМПИЛЯЦИИ ШЕЙДЕРА:\n" << infoLog << std::endl;
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    // Проверяем линковку программы
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << "ОШИБКА ЛИНКОВКИ ПРОГРАММЫ:\n" << infoLog << std::endl;
        return 0;
    }
    
    std::cout << "Шейдер успешно собран! ID: " << program << std::endl;
    return program;
}

GLuint createEmptySSBO(size_t sizeInBytes) {
    if (sizeInBytes == 0) return 0;

    // 1. ПОЛНАЯ РАЗБЛОКИРОВКА перед созданием
    glUseProgram(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0); // GL_SHADER_STORAGE_BUFFER, слот 0, NULL
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); 
    glFinish();
    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo); 
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeInBytes, NULL, 0x88E0); 
    checkGL("createEmptySSBO (BufferData)");
    glBindBuffer(0x90D2, 0); 
    
    return ssbo;
}
void deleteSSBO(GLuint ssbo) {
    checkGL("no deleteSSBO");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glFinish(); 
    glDeleteBuffers(1, &ssbo);
    checkGL("deleteSSBO");
}
#include <vector>
template<typename T>
void updateSSBOData(GLuint ssbo, const std::vector<T>& data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, data.size() * sizeof(T), data.data());
    checkGL("updateSSBOData");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
template<typename T>
void updateSSBODataIndex(GLuint ssbo, size_t index, const std::vector<T>& data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(T)*index, sizeof(T), &data[index]);
    checkGL("updateSSBOData");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}


template<typename T>
void downloadSSBOData(GLuint ssbo, std::vector<T>& data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, data.size() * sizeof(T), data.data());
    checkGL("downloadSSBOData");
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
void _set_unif(unsigned int prog) {} 

// 2. Основной шаблон, который "разбирает" пары: имя юниформа + значение
template<typename T, typename... Args>
void _set_unif(unsigned int prog, const char* name, T val, Args... args) {
    int loc = glGetUniformLocation(prog, name);
    if (loc != -1) {
        // Проверяем тип данных и вызываем нужную OpenGL функцию
        if constexpr (std::is_same_v<T, double>)        glUniform1d(loc, val);
        else if constexpr (std::is_same_v<T, float>)    glUniform1f(loc, (float)val);
        else if constexpr (std::is_same_v<T, uint32_t>) glUniform1ui(loc, (uint32_t)val);
        else if constexpr (std::is_same_v<T, int32_t>)  glUniform1i(loc, (int)val);
    }
    // Рекурсивный вызов для следующей пары аргументов
    _set_unif(prog, args...); 
}
// Функции для юниформ без изменений, просто добавим одну проверку в gpuRun
template<typename... Args>
void gpuRun(unsigned int prog, unsigned int count, GLuint ssbo, Args... uniforms) {
    if (prog == 0) return; // Не запускаем, если шейдер битый

    glUseProgram(prog);
    glBindBufferBase(0x90D2, 0, ssbo);

    _set_unif(prog, uniforms...);

    glDispatchCompute((count + 63) / 64, 1, 1);
    
    // Барьер памяти и проверка, не "упала" ли видеокарта после вычислений
    glMemoryBarrier(0x00008000); 
    checkGL("gpuRun (Dispatch)");

    glUseProgram(0);
    glBindBufferBase(0x90D2, 0, 0);
    glBindBuffer(0x90D2, 0);
}
template<typename... Args>
void gpuRun2(unsigned int prog, unsigned int count, GLuint ssboIn, GLuint ssboOut, Args... uniforms) {
    if (prog == 0) return;

    glUseProgram(prog);

    // Привязываем ВХОДНОЙ буфер к binding = 0
    glBindBufferBase(0x90D2, 0, ssboIn);
    // Привязываем ВЫХОДНОЙ буфер к binding = 1
    glBindBufferBase(0x90D2, 1, ssboOut);

    _set_unif(prog, uniforms...);

    // Запуск. count — общее число клеток
    glDispatchCompute((count + 63) / 64, 1, 1);
    
    glMemoryBarrier(0x00008000); // GL_SHADER_STORAGE_BARRIER_BIT
    checkGL("gpuRun (Double SSBO Dispatch)");

    // Очистка состояний
    glUseProgram(0);
    glBindBufferBase(0x90D2, 0, 0);
    glBindBufferBase(0x90D2, 1, 0);
}
void bindSSBO(GLuint ssbo, int binding) {
    glBindBufferBase(0x90D2, binding, ssbo);
}
void clearBind(int binding) {
    glBindBufferBase(0x90D2, binding, 0);
}
template<typename... Args>
void gpuRunnb(unsigned int prog, unsigned int count, Args... uniforms) {
    if (prog == 0) return;

    glUseProgram(prog);

    _set_unif(prog, uniforms...);

    // Запуск. count — общее число клеток
    glDispatchCompute((count + 63) / 64, 1, 1);
    
    glMemoryBarrier(0x00008000); // GL_SHADER_STORAGE_BARRIER_BIT
    checkGL("gpuRun (Double SSBO Dispatch)");

    // Очистка состояний
    glUseProgram(0);
}
template<typename... Args>
void gpuRun22d(unsigned int prog, unsigned int countx, unsigned int county, GLuint ssboIn, GLuint ssboOut, Args... uniforms) {
    if (prog == 0) return;

    glUseProgram(prog);

    // Привязываем ВХОДНОЙ буфер к binding = 0
    glBindBufferBase(0x90D2, 0, ssboIn);
    // Привязываем ВЫХОДНОЙ буфер к binding = 1
    glBindBufferBase(0x90D2, 1, ssboOut);

    _set_unif(prog, uniforms...);

    glDispatchCompute((countx + 15) / 16, (county + 15) / 16, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // GL_SHADER_STORAGE_BARRIER_BIT
    checkGL("gpuRun (Double SSBO Dispatch)");

    // Очистка состояний
    glUseProgram(0);
    glBindBufferBase(0x90D2, 0, 0);
    glBindBufferBase(0x90D2, 1, 0);
}
template <typename T>
class SSBO {
private:
    GLuint ssbo = 0;
    uint32_t _size = 0;
public:
    SSBO() = default;
    ~SSBO() {
        clear();
    }
    void clear() {
        if (ssbo!=0) {
            deleteSSBO(ssbo);
            ssbo = 0;
            _size = 0;
        }
    }
    void resize(size_t size) {
        if (_size!=size) {
            if (ssbo!=0) {
                this->clear();
            }
            if (size!=0) {
                ssbo = createEmptySSBO(sizeof(T)*size);
                _size = size;
            }
        }
    }
    void update(const std::vector<T>& data) {
        if (_size!=data.size()) [[unlikely]]{
            std::cerr << "no need size" << std::endl;
            exit(1);
        }
        if (ssbo!=0) {
            updateSSBOData(ssbo, data);
        }
    }
    void bind(int num) {
        bindSSBO(ssbo, num);
    }
    void unbind(int num) {
        clearBind(num);
    }
};
