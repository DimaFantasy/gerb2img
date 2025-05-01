// Этот файл распространяется на условиях GNU General Public License v3.
// Библиотека для конвертации Gerber файлов в растровые изображения (BMP/TIFF)

// Подключение необходимых библиотек
#include <regex>          // Для работы с регулярными выражениями
#include <time.h>         // Для измерения времени выполнения
#include <iostream>       // Для ввода-вывода
#include <sstream>        // Для работы со строковыми потоками
#include <algorithm>      // Для алгоритмов обработки коллекций
#include <vector>         // Для работы с векторами
#include <list>           // Для работы со списками
#include <map>            // Для работы с ассоциативными массивами
#include <string>         // Для работы со строками
#include <math.h>         // Для математических функций
#include <limits.h>       // Для определения предельных значений типов
#include <ctype.h>        // Для работы с символами
#include "getopt.h"       // Для разбора параметров командной строки
#include <fstream>        // Для работы с файлами
#include <cstdint>        // Для целочисленных типов фиксированной ширины
#include <stdarg.h>       // Для функций с переменным числом аргументов
#include <string.h>       // Для функций работы со строками
#include "config.h"       // Конфигурационные параметры
#include "nlohmann/json.hpp" // Для работы с JSON
using json = nlohmann::json;
#include "polygon.h"      // Для работы с полигонами
#include "apertures.h"    // Для работы с апертурами Gerber
#include "gerber.h"       // Для парсинга Gerber файлов
#include "tiffio.h"       // Для работы с TIFF изображениями
#include "EasyBMP/EasyBMP.h" // Для работы с BMP изображениями
#include "error_codes.h"  // Коды ошибок библиотеки

// Справочное сообщение о программе
const char *help_message =
    "Gerber RS-274X file to raster graphics converter.";

/**
 * Отображает временной интервал между вызовами функции
 * Используется для замера производительности
 * 
 * @param msg Сообщение для вывода вместе с временем
 */
void show_interval(const char *msg = "")
{
    static clock_t start_clock = std::clock();
    double cpu_time_used = ((double)(std::clock() - start_clock)) / CLOCKS_PER_SEC;
    std::printf("time: %.3f s (%s)\n", cpu_time_used, msg);
    start_clock = std::clock();
}

/**
 * Предвычисленная таблица количества установленных битов для всех 
 * возможных значений байта (0-255)
 * Используется для оптимизации подсчета битов в байте
 */
const unsigned char nbitsTable[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

/**
 * Нормализует путь к файлу, заменяя все слеши на двойные обратные слеши
 * 
 * @param path Исходный путь
 * @return Нормализованный путь с двойными обратными слешами
 */
std::string normalizePathToDoubleBackslashes(const std::string &path)
{
    return std::regex_replace(path, std::regex(R"([\\/]+)"), R"(\\)");
}

/**
 * Оптимизированная функция для рисования горизонтальной линии в монохромном растре
 * 
 * @param x1 Начальная координата X
 * @param x2 Конечная координата X
 * @param buffer Указатель на буфер, содержащий строку изображения
 * @param polarity Полярность рисования:
 *                 DARK - установка битов (черная линия)
 *                 CLEAR - сброс битов (белая линия)
 *                 XOR - инверсия битов
 */
void horizontalLine(int x1, int x2, unsigned char *buffer, Polarity_t polarity)
{
    // Гарантируем, что x1 <= x2
    if (x1 > x2)
        std::swap(x1, x2);

    // Вычисляем смещение битов для начальной и конечной точек
    const unsigned char b1 = static_cast<unsigned char>(x1 & 7);  // x1 % 8
    const unsigned char b2 = static_cast<unsigned char>(x2 & 7);  // x2 % 8
    
    // Получаем указатели на байты, содержащие начальную и конечную точки
    unsigned char *px1 = buffer + (x1 >> 3);  // buffer + x1/8
    unsigned char *px2 = buffer + (x2 >> 3);  // buffer + x2/8

    switch (polarity)
    {
    case DARK:  // Установка битов (рисование черным)
        if (px1 == px2)  // Если линия находится в одном байте
        {
            // Создаем маску для битов между x1 и x2 и устанавливаем их
            *px1 |= (0xFF >> b1) & ~(0xFF >> (b2 + 1));
        }
        else  // Если линия охватывает несколько байтов
        {
            // Устанавливаем биты в первом байте
            *px1 |= (0xFF >> b1);
            // Устанавливаем биты в последнем байте
            *px2 |= ~(0xFF >> (b2 + 1));
            
            // Если есть байты между первым и последним, заполняем их единицами
            if (px2 - px1 > 1)
                memset(px1 + 1, 0xFF, px2 - px1 - 1);
        }
        break;
        
    case CLEAR:  // Сброс битов (рисование белым)
        if (px1 == px2)  // Если линия находится в одном байте
        {
            // Создаем маску для битов между x1 и x2 и сбрасываем их
            *px1 &= ~((0xFF >> b1) & ~(0xFF >> (b2 + 1)));
        }
        else  // Если линия охватывает несколько байтов
        {
            // Сбрасываем биты в первом байте
            *px1 &= ~(0xFF >> b1);
            // Сбрасываем биты в последнем байте
            *px2 &= (0xFF >> (b2 + 1));
            
            // Если есть байты между первым и последним, заполняем их нулями
            if (px2 - px1 > 1)
                memset(px1 + 1, 0x00, px2 - px1 - 1);
        }
        break;
        
    case XOR:  // Инвертирование битов
        if (px1 == px2)  // Если линия находится в одном байте
        {
            // Создаем маску для битов между x1 и x2 и инвертируем их
            *px1 ^= ((0xFF >> b1) & ~(0xFF >> (b2 + 1)));
        }
        else  // Если линия охватывает несколько байтов
        {
            // Инвертируем биты в первом байте
            *px1 ^= (0xFF >> b1);
            // Инвертируем биты в последнем байте
            *px2 ^= ~(0xFF >> (b2 + 1));
            
            // Если есть байты между первым и последним, инвертируем их
            if (px2 - px1 > 1)
                for (auto p = px1 + 1; p < px2; ++p)
                    *p ^= 0xFF;
        }
        break;
    }
}

/**
 * Основная функция для обработки Gerber файла и преобразования его в растровое изображение
 * 
 * @param imageDPI_ Разрешение выходного изображения в DPI
 * @param optGrowUnitsMillimeters_ Флаг: единицы измерения для optGrowSize в миллиметрах
 * @param optBoarderUnitsMillimeters_ Флаг: единицы измерения для optBoarder в миллиметрах
 * @param optBoarder_ Размер границы вокруг изображения
 * @param optInvertPolarity_ Флаг инвертирования полярности изображения
 * @param rowsPerStrip_ Количество строк на полосу для TIFF файла
 * @param optGrowSize_ Значение для увеличения размера фигур
 * @param optScaleX_ Масштабный коэффициент по оси X
 * @param optScaleY_ Масштабный коэффициент по оси Y
 * @param outputFilename_ Имя выходного файла
 * @param inputFilename_ Имя входного Gerber файла
 * @return Код ошибки (0 в случае успеха)
 */
extern "C" __declspec(dllexport) int __stdcall processGerber(
    double imageDPI_,
    bool optGrowUnitsMillimeters_,
    bool optBoarderUnitsMillimeters_,
    double optBoarder_,
    bool optInvertPolarity_,
    unsigned rowsPerStrip_,
    double optGrowSize_,
    double optScaleX_,
    double optScaleY_,
    const char *outputFilename_,
    const char *inputFilename_)
{
    try {
        // Проверка на NULL параметры
        if (!outputFilename_ || !inputFilename_) {
            return ERROR_INVALID_PARAMETERS;
        }

        // Открытие входного файла
        FILE *file = fopen(inputFilename_, "rb");
        if (!file) return ERROR_FILE_OPEN_FAILED;

        // Нормализация путей к файлам
        std::string normalizedOutputFilename = normalizePathToDoubleBackslashes(outputFilename_);
        std::string normalizedInputFilename = normalizePathToDoubleBackslashes(inputFilename_);

        // Парсинг Gerber файла и создание объекта Gerber
        std::list<Gerber*> gerbers;
        try {
            gerbers.push_back(new Gerber(file, imageDPI_, optGrowSize_, optScaleX_, optScaleY_));
        } catch (...) {
            fclose(file);
            return ERROR_GERBER_PROCESSING;
        }
        fclose(file);

        // Вывод сообщений об ошибках, найденных при парсинге Gerber файла
        for (auto msg : gerbers.back()->messages) {
            std::cout << "(" << normalizedInputFilename << ") " << msg << std::endl;
        }

        // Проверка на наличие ошибок при парсинге
        if (gerbers.back()->isError) return ERROR_GERBER_PROCESSING;

        // Проверка входных параметров
        if (imageDPI_ < 1 || optBoarder_ < 0) {
            return ERROR_INVALID_PARAMETERS;
        }

        // Преобразование размеров из миллиметров в пиксели, если необходимо
        if (optGrowUnitsMillimeters_) optGrowSize_ *= imageDPI_ / 25.4;
        if (optBoarderUnitsMillimeters_) optBoarder_ *= imageDPI_ / 25.4;

        // Определение границ изображения
        int miny = INT_MAX, minx = INT_MAX, maxy = INT_MIN, maxx = INT_MIN;
        std::list<Polygon> globalPolygons;

        // Объединение полигонов из всех Gerber объектов
        for (auto g : gerbers) {
            globalPolygons.merge(g->polygons);
        }

        // Проверка на наличие полигонов
        if (globalPolygons.empty()) return ERROR_NO_IMAGE;

        // Вычисление границ изображения на основе всех полигонов
        for (auto& poly : globalPolygons) {
            minx = std::min(minx, poly.pixelMinX);
            maxx = std::max(maxx, poly.pixelMaxX);
            miny = std::min(miny, poly.pixelMinY);
            maxy = std::max(maxy, poly.pixelMaxY);
        }

        // Расчет размеров выходного изображения с учетом границы
        unsigned imageWidth = unsigned(std::ceil((maxx - minx) + 2 * optBoarder_ + 1));
        unsigned imageHeight = unsigned(std::ceil((maxy - miny) + 2 * optBoarder_ + 1));
        int xOffset = int(std::floor(optBoarder_));
        int yOffset = xOffset;

        // Определение полярности изображения
        bool isPolarityDark = optInvertPolarity_ ^ gerbers.front()->imagePolarityDark;

        // Корректировка rowsPerStrip для TIFF
        if (rowsPerStrip_ == 0 || rowsPerStrip_ > imageHeight) {
            rowsPerStrip_ = imageHeight;
        }

        // Определение формата выходного файла по расширению
        std::string outputLower = normalizedOutputFilename;
        std::transform(outputLower.begin(), outputLower.end(), outputLower.begin(), ::tolower);

        bool isBMP = outputLower.find(".bmp") != std::string::npos;

        // Обработка для формата BMP
        if (isBMP)
        {
            // Создание объекта BMP изображения
            BMP output;
            output.SetSize(imageWidth, imageHeight);
            output.SetBitDepth(1); // Монохромное изображение
            output.SetDPI(int(imageDPI_), int(imageDPI_));

            // Создание буфера для монохромного изображения
            size_t bytesPerRow = (imageWidth + 7) / 8;
            std::vector<uint8_t> buffer(bytesPerRow * imageHeight, 0x00);

            // Заполнение буфера начальным значением в зависимости от полярности
            if (!isPolarityDark)
                std::fill(buffer.begin(), buffer.end(), 0xFF);

            // Корректировка смещения по X с учетом минимальной координаты
            xOffset -= minx;

            // Отрисовка всех полигонов в буфер изображения
            for (auto& poly : globalPolygons)
            {
                // Определяем полярность для текущего полигона 
                // (учитываем глобальную полярность изображения)
                Polarity_t pol = poly.polarity;
                if ((pol == DARK) != isPolarityDark) pol = CLEAR;

                int slcCount;
                int *slcTable;

                // Проходим по всем строкам полигона
                for (int y = poly.pixelMinY; y <= poly.pixelMaxY; ++y)
                {
                    // Получаем пары координат X (начало-конец) для текущей строки Y
                    poly.getNextLineX1X2Pairs(slcTable, slcCount);
                    
                    // Отрисовываем все горизонтальные линии в текущей строке
                    for (int i = 0; i < slcCount; i += 2)
                    {
                        horizontalLine(
                            xOffset + poly.pixelOffsetX + slcTable[i],     // Начальная X координата
                            xOffset + poly.pixelOffsetX + slcTable[i + 1], // Конечная X координата
                            buffer.data() + ((y - miny + yOffset) * bytesPerRow), // Указатель на строку в буфере
                            pol // Полярность (DARK, CLEAR или XOR)
                        );
                    }
                }
            }

            // Преобразование битового буфера в BMP формат
            // (каждый бит в буфере соответствует одному пикселю в BMP)
            for (unsigned y = 0; y < imageHeight; y++) {
                for (unsigned x = 0; x < imageWidth; x++) {
                    // Вычисляем байт и бит внутри байта
                    size_t bytePos = (y * bytesPerRow) + (x / 8);
                    int bitPos = 7 - (x % 8); // В битмапе биты идут справа налево внутри байта
                    
                    // Проверяем, установлен ли бит
                    bool isPixelSet = (buffer[bytePos] & (1 << bitPos)) != 0;
                    
                    // Устанавливаем соответствующий пиксель в BMP
                    RGBApixel pixel;
                    if (isPixelSet) {
                        pixel.Red = 0; pixel.Green = 0; pixel.Blue = 0; // Черный
                    } else {
                        pixel.Red = 255; pixel.Green = 255; pixel.Blue = 255; // Белый
                    }
                    output.SetPixel(x, y, pixel);
                }
            }

            // Запись BMP файла
            if (!output.WriteToFile(normalizedOutputFilename.c_str()))
                return ERROR_OUTPUT_FILE_CREATION;
        }
        else // Обработка для формата TIFF
        {
            // Создание TIFF файла
            TIFF *tif = TIFFOpen(normalizedOutputFilename.c_str(), "w");
            if (!tif) return ERROR_OUTPUT_FILE_CREATION;

            // Установка параметров TIFF файла
            TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);  // Планарность - смежная
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE); // Полярность - минимум=белый
            TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_CCITTRLE);   // Сжатие - RLE
            TIFFSetField(tif, TIFFTAG_IMAGELENGTH, imageHeight);            // Высота изображения
            TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, imageWidth);              // Ширина изображения
            TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, 2);                   // Единица измерения DPI - дюймы
            TIFFSetField(tif, TIFFTAG_YRESOLUTION, imageDPI_);              // Разрешение по Y
            TIFFSetField(tif, TIFFTAG_XRESOLUTION, imageDPI_);              // Разрешение по X
            TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rowsPerStrip_);         // Строк на полосу

            // Расчет размера буфера для полосы TIFF
            size_t bytesPerScanline = ((imageWidth + 7) >> 3);  // Количество байтов на строку
            size_t bitmapBytes = bytesPerScanline * rowsPerStrip_;  // Общий размер буфера полосы
            std::vector<unsigned char> bitmap(bitmapBytes);  // Создание буфера

            // Корректировка смещения
            xOffset -= minx;

            // Итераторы для эффективной обработки полигонов
            auto polyIter = globalPolygons.begin();
            std::list<PolygonReference> activePolys;  // Список активных полигонов для текущей строки

            // Обработка изображения полосами для TIFF
            for (int ystart = miny - yOffset, stripCounter = 0;
                 ystart < (int(imageHeight) + miny - yOffset);
                 ystart += rowsPerStrip_, ++stripCounter)
            {
                // Инициализация буфера в зависимости от полярности изображения
                if (isPolarityDark)
                    memset(bitmap.data(), 0x00, bitmapBytes);  // Черный фон
                else
                    memset(bitmap.data(), 0xFF, bitmapBytes);  // Белый фон

                unsigned char *bufferLine = bitmap.data();  // Указатель на текущую строку в буфере

                // Обработка строк в текущей полосе
                for (int y = ystart; (y - ystart) < (int)rowsPerStrip_ && y <= maxy;
                     ++y, bufferLine += bytesPerScanline)
                {
                    // Добавление полигонов, которые начинаются на текущей строке
                    while (polyIter != globalPolygons.end() && y == polyIter->pixelMinY)
                    {
                        activePolys.push_back(PolygonReference());
                        activePolys.back().polygon = &(*polyIter);
                        activePolys.sort();  // Сортировка для эффективной обработки
                        ++polyIter;
                    }

                    // Обработка всех активных полигонов на текущей строке
                    for (auto it = activePolys.begin(); it != activePolys.end();)
                    {
                        // Удаление полигонов, которые больше не активны на текущей строке
                        if (y > it->polygon->pixelMaxY)
                        {
                            it = activePolys.erase(it);
                            continue;
                        }

                        // Получение пар координат для текущей строки полигона
                        int slcCount;
                        int *slcTable;
                        it->polygon->getNextLineX1X2Pairs(slcTable, slcCount);

                        // Определение полярности для текущего полигона
                        Polarity_t pol = it->polygon->polarity;
                        if ((pol == DARK) != isPolarityDark) pol = CLEAR;

                        // Отрисовка всех линий полигона в текущей строке
                        for (int i = 0; i < slcCount; i += 2)
                        {
                            horizontalLine(
                                xOffset + it->polygon->pixelOffsetX + slcTable[i],      // Начало X
                                xOffset + it->polygon->pixelOffsetX + slcTable[i + 1],  // Конец X
                                bufferLine,                                             // Буфер строки
                                pol                                                     // Полярность
                            );
                        }
                        ++it;
                    }
                }

                // Запись полосы в TIFF файл
                TIFFWriteEncodedStrip(tif, stripCounter, bitmap.data(),
                                      bytesPerScanline * std::min(rowsPerStrip_, imageHeight - rowsPerStrip_ * stripCounter));
            }

            // Закрытие TIFF файла
            TIFFClose(tif);
        }
        return NO_ERROR;  // Успешное завершение
    }
    catch (...) {
        return ERROR_UNKNOWN;  // Обработка непредвиденных исключений
    }
}

/**
 * Обертка для processGerber, получающая параметры из JSON-строки
 * 
 * @param jsonParams Строка с JSON объектом, содержащим параметры
 * @return Код ошибки (0 в случае успеха)
 */
extern "C" __declspec(dllexport) int __stdcall processGerberJSON(const char *jsonParams)
{
    try
    {
        // Парсинг JSON строки
        json j = json::parse(jsonParams);
        
        // Вызов основной функции с параметрами из JSON
        return processGerber(
            j.value("imageDPI", 2400.0),                     // Разрешение (DPI)
            j.value("optGrowUnitsMillimeters", false),       // Единицы роста в мм
            j.value("optBoarderUnitsMillimeters", false),    // Единицы границы в мм
            j.value("optBoarder", 0.0),                      // Размер границы
            j.value("optInvertPolarity", false),             // Инвертировать полярность
            j.value("rowsPerStrip", 512u),                   // Строк на полосу для TIFF
            j.value("optGrowSize", 0.0),                     // Размер для увеличения фигур
            j.value("optScaleX", 1.0),                       // Масштаб по X
            j.value("optScaleY", 1.0),                       // Масштаб по Y
            j.value("outputFilename", "").c_str(),           // Выходной файл
            j.value("inputFilename", "").c_str()             // Входной файл
        );
    }
    catch (...) {
        return ERROR_JSON_PROCESSING;  // Ошибка при обработке JSON
    }
}