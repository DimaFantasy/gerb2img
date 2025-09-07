#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <algorithm>  // для std::min, std::max
#include <cctype>
#include <cfloat>  // для DBL_MAX
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include "scanline_filler.h"
#include "polygon.h"

using namespace std;

#define ENABLE_DEBUG_LOGGING_POLIGON

/*
 *  Polygon initialisation.
 *   - Sets min and max variables from vertex data.
 *   - Creates scan line intercept X data used for filling the polygon by scan line method.
 *
 *   This function shall be called after polygon vertex data has been created.
 */
void Polygon::initialise() {
    // Вычисляем минимальную X-координату полигона в пикселях на экране/битмапе.
    // Это левая граница области, где будет отрисован полигон.
    // minx — минимальная X-координата в логических единицах, offset.x — смещение позиции.
    // Результат округляется до ближайшего целого, так как пиксели — целочисленные.
    pixelMinX = std::round(vdata->minx + offset.x);

    // Максимальная X-координата: левая граница + ширина полигона в пикселях.
    // Определяет правую границу области отрисовки.
    pixelMaxX = pixelMinX + vdata->pixelWidth;

    // Вычисляем минимальную Y-координату полигона в пикселях.
    // Это верхняя граница (в системах координат с Y, растущим вниз).
    // Учитывается miny (логическая координата) и смещение по Y.
    pixelMinY = std::round(vdata->miny + offset.y);

    // Максимальная Y-координата: верхняя граница + высота полигона в пикселях.
    // Определяет нижнюю границу области отрисовки.
    pixelMaxY = pixelMinY + vdata->pixelHeight;

    // Сохраняем округлённое смещение по оси X как отдельное значение.
    // Может использоваться в дальнейшем для быстрого доступа при расчётах позиций.
    pixelOffsetX = std::round(offset.x);

	// сброс текущей строки
    currentLine = 0;  

}

void VertexData::initialise() {
    if (vertices.empty()) return;

    // Инициализация
    minx = 0;
    miny = 0;
    maxx = 0;
    maxy = 0;
    pixelWidth = 0;
    pixelHeight = 0;		
	gxIntersects.clear();
    linesInCounts.clear();

    // --- 1. Формируем контур ---
    scanline::Contour contour;
    contour.reserve(vertices.size());
    for (const auto& v : vertices) {
        contour.push_back(scanline::Point{v.x, v.y});
    }

    // --- 2. Выполняем растеризацию и получаем ВСЁ ---
    scanline::ScanlineFiller filler;
    auto result = filler.fillWithResult({contour});  // передаём вектор контуров

    // --- 3. Если не было заливки — выходим ---
    if (!result.success) {
#ifdef ENABLE_DEBUG_LOGGING_POLIGON
        std::ofstream logFile("gerber_debug_poligon.log", std::ios::app);
        if (logFile.is_open()) {
            logFile << "  [SKIP] No fill (empty or 1x1)\n";
            logFile.flush();
        }
#endif
        return;
    }

    // --- 4. Копируем результат ---
    gxIntersects = std::move(result.xSpans);
    linesInCounts = std::move(result.spanCounts);
    // Сохраняем bounding box и размеры
    minx = result.minx;
    miny = result.miny;
    maxx = result.maxx;
    maxy = result.maxy;
    pixelWidth = result.pixelWidth;
    pixelHeight = result.pixelHeight;

    // --- 5. Логирование (опционально) ---
#ifdef ENABLE_DEBUG_LOGGING_POLIGON
    std::ofstream logFile("gerber_debug_poligon.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << "---- VertexData::initialise dump ----\n";
        logFile << "  BBox: [" << minx << ", " << miny << "] -> [" << maxx << ", " << maxy << "]\n";
        logFile << "  Pixel size: " << pixelWidth << " x " << pixelHeight << "\n";
        logFile << "  gxIntersects: ";
        for (size_t i = 0; i < gxIntersects.size(); ++i) {
            if (i > 0) logFile << ", ";
            logFile << gxIntersects[i];
        }
        logFile << "\n";
        logFile << "  linesInCounts: ";
        for (int c : linesInCounts) logFile << c << " ";
        logFile << "\n";
        logFile << "------------------------------------\n";
        logFile.flush();
    }
#endif
}


/*
 * Добавляет новую вершину в список вершин полигона, но только если она
 * значительно отличается от последней добавленной точки.
 *
 * Цель: избежать добавления почти одинаковых точек (например, из-за ошибок округления
 * или избыточной генерации), которые могут нарушить растеризацию или увеличить нагрузку.
 *
 * @param P  Точка (x, y), которую нужно добавить
 *
 * Условие добавления:
 *   - Если список вершин пуст — точка добавляется (это первая вершина).
 *   - Иначе: добавляется только если квадрат расстояния между P и lastVertex > 0.25
 *     (то есть расстояние > 0.5 единицы).
 *
 * Примечание: используется квадрат расстояния (abs_sq), чтобы избежать cost-вычисления sqrt.
 */
void VertexData::add(const Point &P) {
    // Условие добавления:
    // 1. Список пуст — добавляем первую точку.
    // 2. Расстояние от последней точки больше 0.5 (проверяется через квадрат: > 0.25)
    if (vertices.size() == 0 || abs_sq(lastVertex - P) > 0.25) {
        vertices.push_back(P);  // Добавляем точку в вектор
        lastVertex = P;         // Обновляем кэш последней добавленной точки
    }
    // Если точка слишком близка — игнорируем (фильтрация дубликатов)
}

/*
 * Удобная перегрузка метода add: принимает координаты x и y напрямую.
 * Создаёт временную точку Point(x, y) и вызывает основной метод add(const Point&).
 *
 * @param x  X-координата точки
 * @param y  Y-координата точки
 *
 * Эта функция позволяет добавлять точки без явного создания объекта Point:
 *   vertexData.add(10.0, 20.0);  // вместо vertexData.add(Point(10.0, 20.0));
 */
void VertexData::add(double x, double y) {
    add(Point(x, y));  // Передаём созданную точку в основной метод
}

/*
 * Добавляет дугу окружности в список вершин полигона.
 *
 * @param start_angle  Начальный угол дуги в радианах (относительно центра x0, y0)
 * @param end_angle    Конечный угол дуги в радианах
 * @param radius       Радиус дуги
 * @param x0, y0       Центр окружности
 * @param clockwise    Направление дуги: true — по часовой стрелке, false — против
 */
void VertexData::addArc(double start_angle, double end_angle, double radius, double x0, double y0,
                        bool clockwise) {
    // Устанавливаем максимально допустимое отклонение (в мм или условных единицах)
    // Это — насколько сильно хорда (отрезок) может отклоняться от истинной дуги.
    double deviation = 0.01;

    // Минимальный радиус — 0.5 единицы (защита от слишком маленьких дуг)
    if (radius < 0.5) radius = 0.5;

    // Адаптируем допустимое отклонение в зависимости от радиуса:
    // Чем меньше радиус, тем меньше deviation, чтобы сохранить плавность.
    // При радиусе 150 и больше — deviation = 0.01, при меньшем радиусе — пропорционально уменьшается.
    if (radius < 150) deviation *= (radius / 150.0);

    // Гарантируем минимальное значение отклонения, чтобы не делать слишком много точек
    if (deviation < 0.01) deviation = 0.01;

    // Вычисляем максимальный угловой шаг (в радианах), при котором хорда отклоняется не более чем на
    // `deviation` Формула: step = 2 * acos(1 - deviation / radius) Основана на геометрии сегмента окружности:
    // максимальное отклонение между дугой и хордой.
    double step = 2 * acos(1 - deviation / radius);

    // Нормализуем углы в диапазон [0, 2π)
    if (start_angle < 0) start_angle += 2 * M_PI;
    if (end_angle < 0) end_angle += 2 * M_PI;

    // Текущий угол для итераций
    double theta = start_angle;

    // Вычисляем угловую длину дуги (в радианах) от start_angle до end_angle (против часовой стрелки)
    double arc = end_angle - start_angle;
    if (arc < 0) arc += 2 * M_PI;  // Если end_angle < start_angle, "переходим" через 0

    // Если дуга по часовой стрелке, то реальный угол — остаток от полного круга
    if (clockwise) arc = 2 * M_PI - arc;

    // Количество сегментов (хорд), необходимых для аппроксимации дуги
    // ceil(arc / step) — округляем вверх, чтобы гарантировать, что отклонение ≤ deviation
    int N = static_cast<int>(ceil(arc / step));

    // Обработка случаев, когда дуга слишком мала (менее одного шага)
    if (N < 2) {
        if (N == 1) {
            // Добавляем только одну точку — начальную (или конечную, но они близки)
            double const x = radius * cos(start_angle) + x0;
            double const y = radius * sin(start_angle) + y0;
            add(x, y);  // Добавляем точку в список вершин
        }
        return;  // Нечего аппроксимировать — выходим
    }

    // Пересчитываем угловой шаг, чтобы равномерно распределить N-1 интервалов между N точками
    step = arc / (N - 1);

    // Если направление — по часовой стрелке, шаг становится отрицательным
    if (clockwise) step *= -1;

    // Генерируем N точек по дуге
    for (int i = 0; i < N; i++) {
        // Вычисляем координаты точки на окружности
        double const x = radius * cos(theta) + x0;
        double const y = radius * sin(theta) + y0;

        // Добавляем точку в список вершин
        add(x, y);

        // Увеличиваем угол для следующей точки
        theta += step;
    }
}

/*
 * Добавляет вершины правильного многоугольника (вписанного в окружность).
 *
 * @param vertex_radius  Радиус от центра до вершины (расстояние от центра до углов)
 * @param start_angle    Начальный угол первой вершины (в радианах)
 * @param num_sides      Количество сторон (и вершин). Минимум 3.
 * @param x0, y0         Координаты центра многоугольника
 *
 * Пример: num_sides=4 → квадрат, num_sides=6 → шестиугольник.
 */
void VertexData::addRegularPolygon(double vertex_radius, double start_angle, int num_sides, double x0,
                                   double y0) {
    // Правильный многоугольник должен иметь минимум 3 стороны
    if (num_sides < 3) return;

    // Угол между соседними вершинами: 360° / num_sides = 2π / num_sides
    double step = 2 * M_PI / static_cast<double>(num_sides);

    // Начальный угол для первой вершины
    double theta = start_angle;

    // Радиус — расстояние от центра до вершины (не до середины стороны)
    // Это стандартное определение для вписанного многоугольника
    double radius = vertex_radius;

    // Генерируем num_sides вершин, равномерно распределённых по окружности
    for (int i = 0; i < num_sides; i++) {
        // Вычисляем координаты вершины на окружности
        double x = radius * cos(theta) + x0;
        double y = radius * sin(theta) + y0;

        // Добавляем точку в список вершин
        add(x, y);

        // Переходим к следующей вершине
        theta += step;
    }
}

/*
 * Добавляет четыре вершины, образующие прямоугольник.
 * Вершины добавляются по порядку: нижний левый → нижний правый → верхний правый → верхний левый.
 * Полигон остаётся **незамкнутым** — замыкание (если нужно) делается отдельно.
 *
 * @param x_size  Ширина прямоугольника
 * @param y_size  Высота прямоугольника
 * @param x0, y0  Центр прямоугольника
 */
void VertexData::addRectangle(double x_size, double y_size, double x0, double y0) {
    // 🔧 Привязываем центр к сетке 0.5 пикселя
    x0 = round(x0 * 2.0) / 2.0;
    y0 = round(y0 * 2.0) / 2.0;

    // Вычисляем координаты углов:
    const double x1 = x0 - x_size / 2;  // Левый X
    const double y1 = y0 - y_size / 2;  // Нижний Y (предполагаем стандартную систему координат)
    const double x2 = x1 + x_size;      // Правый X
    const double y2 = y1 + y_size;      // Верхний Y

    // Добавляем вершины против часовой стрелки (CCW) — стандарт для правильной ориентации
    add(x1, y1);  // Нижний левый
    add(x2, y1);  // Нижний правый
    add(x2, y2);  // Верхний правый
    add(x1, y2);  // Верхний левый
    // (не замыкаем явно — последняя точка соединится с первой при рендеринге, если нужно)
}

/*
 * Поворачивает все вершины полигона вокруг начала координат (0,0)
 * в противоположном направлении часовой стрелки (против часовой стрелки).
 *
 * @param theta  Угол поворота в радианах
 *
 * Использует матрицу поворота:
 *   x' = x * cos(θ) - y * sin(θ)
 *   y' = x * sin(θ) + y * cos(θ)
 */
void VertexData::rotate(double theta) {
    // Применяем поворот к каждой вершине
    for (int i = 0; i < static_cast<int>(vertices.size()); i++) {
        vertices[i].rotate(theta);  // Вызывает метод Point::rotate
    }
}

/*
 * Масштабирует все вершины полигона относительно начала координат (0,0).
 * Умножает X-координаты на scaleX, Y-координаты на scaleY.
 *
 * @param scaleX  Коэффициент масштабирования по оси X
 * @param scaleY  Коэффициент масштабирования по оси Y
 *
 * Пример: scaleX=2 → растяжение по X в 2 раза.
 *         scaleY=0.5 → сжатие по Y вдвое.
 */
void VertexData::scale(double scaleX, double scaleY) {
    int N = vertices.size();
    if (N == 0) return;  // Нечего масштабировать

    // Умножаем каждую координату на соответствующий коэффициент
    for (int i = 0; i < N; i++) {
        vertices[i].x *= scaleX;
        vertices[i].y *= scaleY;
    }
}

/*
 * Сдвигает (транслирует) все вершины на заданный вектор (x_shift, y_shift).
 * Это перемещение всего полигона без поворота или масштабирования.
 *
 * @param x_shift  Смещение по оси X
 * @param y_shift  Смещение по оси Y
 */
void VertexData::shift(double x_shift, double y_shift) {
    // Используем итератор по вектору вершин
    for (vector<Point>::iterator it = vertices.begin(); it != vertices.end(); it++) {
        it->x += x_shift;  // Сдвиг X
        it->y += y_shift;  // Сдвиг Y
    }
}

/*
 * Поворачивает точку вокруг начала координат (0,0) на заданный угол
 * в направлении против часовой стрелки.
 *
 * @param radian  Угол поворота в радианах
 *
 * Использует стандартные формулы поворота в 2D:
 *   x' = x·cos(θ) - y·sin(θ)
 *   y' = x·sin(θ) + y·cos(θ)
 *
 * Изменяет координаты точки на месте.
 */
void Point::rotate(const double &radian) {
    // Сохраняем старые координаты, так как они используются в обоих вычислениях
    double _x = x * cos(radian) - y * sin(radian);
    double _y = y * cos(radian) + x * sin(radian);

    // Обновляем координаты точки
    x = _x;
    y = _y;
}
