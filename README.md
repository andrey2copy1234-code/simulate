# simulate
Симуляция планет на c++
## Установка
После установки .zip вам понадобится его распоковать. после распоковки не пытайтесь удалять файлы из папки или перемещать их.
## Где информация об управлении в симуляции?
на Control-h справка
## Что симулирует симуляция?
Она симулирует:
* Притяжение
* Приливные силы
## Компиляция
Если вы хотите сами скомпилировать проект то это подробная инструкция по тому как это сделать.
Все файлы изначально написанны под g++. И изначально скомпилированны его версией 15.1.0 msvcrt.
В каждом файле в первой строчке стоит рекоминдуемая команда дял компиляции. Вот все действия для компиляции проекта:
1. для компиляции simulate.cpp надо ввести команду `g++ simulate.cpp -o simulate.exe -I c:\Users\Azerty\Downloads\SFML-2.6.1/include -L c:\Users\Azerty\Downloads\SFML-2.6.1/lib -lsfml-graphics-d -lsfml-window-d -lsfml-system-d -lopengl32 -lwinmm -lgdi32 -lcomdlg32 -static -O3 -fopenmp`. если вы уберёте -static то вам понадобится ещё libgomp.dll
2. для компиляции simulate_imports.h (рекоминдуется ведь тогда компиляция быстрее) нужно ввести команду `g++ simulate_imports.h -c -O3 -I c:\Users\Azerty\Downloads\SFML-2.6.1/include`
И положить в папку с .exe библиотеки libwinpthread-1.dll, sfml-graphics-d-2.dll, sfml-window-d-2.dll, sfml-system-d-2.dll, libstdc++-6.dll, libgcc_s_seh-1.dll
