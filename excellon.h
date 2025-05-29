// This file is distributed under the terms of the GNU General Public License v3.

#ifndef EXCELLON_H_
#define EXCELLON_H_

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <cmath>
#include <cstdio>

#include "polygon.h"

using namespace::std;

/**
 * @brief Класс для обработки формата Excellon (файлов сверловки)
 * 
 * Формат Excellon используется для задания координат отверстий на печатных платах.
 * Этот класс обрабатывает файлы формата Excellon и создает соответствующие
 * полигоны, которые представляют отверстия для сверления.
 */
class Excellon {
private:
    // Константы класса
    const double dotsPerInch;    // Разрешение в точках на дюйм
    const double growSize;       // Размер для увеличения полигонов
    const double optScaleX;      // Масштабирование по оси X
    const double optScaleY;      // Масштабирование по оси Y
    const double uniformDrillDiameter; // Значение диаметра для всех отверстий (если > 0)

    // Перечисление для единиц измерения
    typedef enum {MILLIMETER, INCH, UNDEFINED} Units_t;

    // Структура для инструмента
    struct Tool {
        int number;              // Номер инструмента
        double diameter;         // Диаметр сверла в единицах измерения
        int count;               // Счетчик использования
        string description;      // Описание инструмента (опционально)

        Tool() : number(0), diameter(0), count(0) {}
    };

    // Структура для представления позиции сверления
    struct DrillPosition {
        double x;               // Координата X
        double y;               // Координата Y
        int toolNumber;         // Номер используемого инструмента
    };

    // Внутренние переменные состояния
    map<int, Tool> tools;       // Карта инструментов (ключ - номер)
    int currentTool;            // Текущий выбранный инструмент
    double currentX;            // Текущая X координата
    double currentY;            // Текущая Y координата
    Units_t units;              // Единицы измерения
    bool isHeaderActive;        // Находимся ли мы в заголовке файла
    bool isAbsoluteCoords;      // Абсолютные или относительные координаты
    bool isLeadingZeros;        // Подавление ведущих нулей
    bool isTrailingZeros;       // Подавление конечных нулей
    int coordDecimals;          // Количество цифр после запятой
    int coordInts;              // Количество цифр до запятой
    int lineNumber;             // Номер текущей строки
    bool excellonFormat2;       // Формат Excellon 2
    double scaleFactor;         // Коэффициент масштабирования для конвертации в пиксели

    // Список позиций сверления
    vector<DrillPosition> drillPositions;

    // Внутренние функции
    void parseHeader(const string& line);
    void parseTool(const string& line);
    void parseCoordinate(const string& line);
    void processExcellonLine(const string& line);
    double parseCoord(const string& coord, bool isX);
    double getPixelsFromUnits(double value);
    void createDrillPolygon(double x, double y, double diameter);

public:
    /**
     * @brief Проверяет, является ли файл форматом Excellon/Drill
     * 
     * @param fp_excellon Указатель на файл для проверки
     * @return true если файл похож на Excellon/Drill, false в противном случае
     */
    static bool isExcellonFile(FILE* fp_excellon);

    /**
     * @brief Конструктор класса Excellon
     * 
     * @param fp_excellon Указатель на файл для чтения
     * @param dotsPerInch Разрешение в точках на дюйм
     * @param growSize Размер для увеличения полигонов
     * @param optScaleX Масштабирование по оси X
     * @param optScaleY Масштабирование по оси Y
     * @param uniformDrillDiameter Если > 0, использовать этот диаметр для всех отверстий
     * @param uniformDrillInMillimeters Указывает, задан ли uniformDrillDiameter в миллиметрах (true) или дюймах (false)
     */
    Excellon(FILE* fp_excellon, const double dotsPerInch, const double growSize, 
            double optScaleX, double optScaleY, double uniformDrillDiameter = 0, 
            bool uniformDrillInMillimeters = false);
    
    /**
     * @brief Деструктор класса
     */
    ~Excellon();

    // Публичные данные
    vector<string> messages;          // Сообщения о предупреждениях
    ostringstream errorMessage;       // Сообщения об ошибках
    bool isError;                     // Флаг ошибки
    list<Polygon> polygons;           // Список полигонов (отверстий)
    bool imagePolarityDark;           // Полярность изображения (темная/светлая)
    string layerName;                 // Имя слоя

    /**
     * @brief Добавляет предупреждение
     * 
     * @param format Формат строки (в стиле printf)
     * @param ... Аргументы формата
     */
    void warning(const char* format, ...);
};

#endif // EXCELLON_H_
