//This file is distributed under the terms of the GNU General Public License v3.

//#define ENABLE_DEBUG_LOGGING  // Включаем отладочные логи
#define _USE_MATH_DEFINES
#include <vector>
#include <list>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <stdarg.h>
#include <regex>
#include <cctype>
#include <cstring> // Добавлен заголовок для strlen()

using namespace ::std;

#include "polygon.h"
#include "excellon.h"

/**
 * @brief Проверяет, является ли файл форматом Excellon/Drill
 * 
 * @param fp_excellon Указатель на файл для проверки
 * @return true если файл похож на Excellon/Drill, false в противном случае
 */
bool Excellon::isExcellonFile(FILE* fp_excellon) {
    if (!fp_excellon) {
        return false;
    }

    // Запоминаем текущую позицию в файле
    long currentPos = ftell(fp_excellon);
    
    // Проверяемые признаки Excellon/Drill формата
    bool hasM48 = false;            // Начало заголовка
    bool hasUnitMarker = false;     // INCH или METRIC указатель
    bool hasToolDef = false;        // Определения инструментов (TxxCyy.yy)
    bool hasCoordinates = false;    // Координаты (XxxxxYxxxx)
    bool hasM30 = false;            // Завершение программы
    
    // 1. Проверяем начало файла (первые 50 строк)
    rewind(fp_excellon);
    char buffer[1024];
    int lineCount = 0;
    
    while (fgets(buffer, sizeof(buffer), fp_excellon) && lineCount < 50) {
        string line(buffer);
        
        // Переводим в верхний регистр для сравнения без учета регистра
        transform(line.begin(), line.end(), line.begin(), ::toupper);
        
        // Проверяем признаки заголовка Excellon файла
        if (line.find("M48") != string::npos) {
            hasM48 = true;
        }
        
        if (line.find("INCH") != string::npos || line.find("METRIC") != string::npos || 
            line.find("MM") != string::npos) {
            hasUnitMarker = true;
        }
        
        // Проверка на определение инструмента: TxxCyy.yy
        if (regex_search(line, regex("T\\d+C[\\d.]+"))) {
            hasToolDef = true;
        }
        
        lineCount++;
    }
    
    // 2. Если начальные признаки не обнаружены, сразу возвращаемся
    if (!hasM48 || !(hasUnitMarker || hasToolDef)) {
        fseek(fp_excellon, currentPos, SEEK_SET);
        return false;
    }
    
    // 3. Проверяем конец файла (последние примерно 20-30 строк)
    // Находим размер файла
    fseek(fp_excellon, 0, SEEK_END);
    long fileSize = ftell(fp_excellon);
    
    // Определяем позицию для чтения конца файла (около 5-10 КБ от конца файла)
    const long tailSize = 8192; // 8 КБ должно хватить для последних 20-30 строк
    long seekPos = fileSize > tailSize ? fileSize - tailSize : 0;
    fseek(fp_excellon, seekPos, SEEK_SET);
    
    lineCount = 0;
    while (fgets(buffer, sizeof(buffer), fp_excellon)) {
        string line(buffer);
        
        // Переводим в верхний регистр для сравнения без учета регистра
        transform(line.begin(), line.end(), line.begin(), ::toupper);
        
        // Проверяем координаты отверстий
        if (regex_search(line, regex("X[+-]?[\\d.]+Y[+-]?[\\d.]+"))) {
            hasCoordinates = true;
        }
        
        // Проверяем признак завершения файла
        if (line.find("M30") != string::npos || line.find("M00") != string::npos || 
            line.find("M02") != string::npos) {
            hasM30 = true;
        }
        
        lineCount++;
        
        // Если нашли и координаты, и маркер конца, можно не продолжать
        if (hasCoordinates && hasM30) {
            break;
        }
    }
    
    // Возвращаемся к исходной позиции в файле
    fseek(fp_excellon, currentPos, SEEK_SET);
    
    // Основные критерии: заголовок M48, наличие единиц измерения или определения инструмента,
    // а также координаты отверстий в конце файла
    bool isValidHeader = hasM48 && (hasUnitMarker || hasToolDef);
    bool isValidContent = hasCoordinates; // M30 необязателен, некоторые файлы могут его не иметь
    
    // Файл является Excellon, если соответствует критериям заголовка и содержимого
    return (isValidHeader && isValidContent);
}

/**
 * @brief Добавляет предупреждение в список сообщений
 * 
 * @param format Формат сообщения (Printf-style)
 * @param ... Аргументы формата
 */
void Excellon::warning(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    
    string warningMsg = string("Warning: ") + buffer + " at line " + to_string(lineNumber);
    messages.push_back(warningMsg);
}

/**
 * @brief Конвертирует значение из единиц измерения в пиксели
 * 
 * @param value Значение в единицах измерения (дюймы или миллиметры)
 * @return Значение в пикселях
 */
double Excellon::getPixelsFromUnits(double value) {
    if (units == INCH) {
        return value * dotsPerInch;
    } else if (units == MILLIMETER) {
        // Корректная конвертация из миллиметров в пиксели
        // 1 дюйм = 25.4 мм, значит для преобразования миллиметров в пиксели
        // нужно умножить на (dotsPerInch / 25.4)
        return value * (dotsPerInch / 25.4);
    } else {
        warning("Неизвестные единицы измерения, используются дюймы");
        return value * dotsPerInch;
    }
}

/**
 * @brief Разбор строки заголовка Excellon файла
 * 
 * @param line Строка для разбора
 */
void Excellon::parseHeader(const string& line) {
    // Удаление пробелов из строки
    string trimmedLine = line;
    trimmedLine.erase(remove_if(trimmedLine.begin(), trimmedLine.end(), ::isspace), trimmedLine.end());
    
    // Проверка команд заголовка
    if (line.find("M48") != string::npos) {
        isHeaderActive = true;
        excellonFormat2 = true;
        return;
    }
    
    if (line.find("M95") != string::npos || line.find("%") != string::npos) {
        isHeaderActive = false;
        return;
    }
    
    // Обработка единиц измерения
    if (line.find("METRIC") != string::npos || line.find("MM") != string::npos) {
        units = MILLIMETER;
        return;
    }
    
    if (line.find("INCH") != string::npos) {
        units = INCH;
        return;
    }
    
    // Обработка формата координат
    if (line.find("LZ") != string::npos) {
        isLeadingZeros = true;
        isTrailingZeros = false;
    } else if (line.find("TZ") != string::npos) {
        isLeadingZeros = false;
        isTrailingZeros = true;
    }
    
    // Поиск информации о формате (например, "FORMAT=2.4" или "FILE_FORMAT=3:3")
    if (line.find("FORMAT") != string::npos || line.find("FMAT") != string::npos) {
        // Сначала проверяем формат вида "FILE_FORMAT=3:3"
        regex formatRegex("FILE_FORMAT=([0-9]):([0-9])");
        smatch formatMatch;
        
        if (regex_search(line, formatMatch, formatRegex)) {
            coordInts = stoi(formatMatch[1]);  // Количество цифр до запятой
            coordDecimals = stoi(formatMatch[2]);  // Количество цифр после запятой
#ifdef ENABLE_DEBUG_LOGGING
            std::ofstream logFile("excellon_debug.log", std::ios::app);
            logFile << "[DEBUG] Обнаружен формат FILE_FORMAT=" << coordInts << ":" << coordDecimals << "\n";
#endif
        } else {
            // Если не нашли такой формат, проверяем традиционный вид с точкой
            size_t pos = line.find('.');
            if (pos != string::npos && pos + 1 < line.length()) {
                coordDecimals = line[pos + 1] - '0';
                // Если есть символ перед точкой, это может быть количество цифр до запятой
                if (pos > 0 && isdigit(line[pos - 1])) {
                    coordInts = line[pos - 1] - '0';
                }
#ifdef ENABLE_DEBUG_LOGGING
                std::ofstream logFile("excellon_debug.log", std::ios::app);
                logFile << "[DEBUG] Обнаружен формат с точкой: " << coordInts << "." << coordDecimals << "\n";
#endif
            }
        }
    }
    
    // Разбор определения инструмента
    if (line.find('T') == 0 || line.find("T") == 0) {
        parseTool(line);
    }
}

/**
 * @brief Разбор определения инструмента
 * 
 * @param line Строка с определением инструмента
 */
void Excellon::parseTool(const string& line) {
    // Регулярное выражение для извлечения номера инструмента
    regex toolNumRegex("T(\\d+)");
    smatch toolNumMatch;
    if (!regex_search(line, toolNumMatch, toolNumRegex)) {
        return;
    }
    
    int toolNum = stoi(toolNumMatch[1]);
    
    // Проверка, является ли это выбором инструмента или его определением
    if (isHeaderActive) {
        // Это определение инструмента в заголовке
        Tool tool;
        tool.number = toolNum;
        
        // Поиск диаметра инструмента
        regex diameterRegex("C([\\d.]+)");
        smatch diamMatch;
        if (regex_search(line, diamMatch, diameterRegex)) {
            tool.diameter = stod(diamMatch[1]);
        }
        
        // Добавление инструмента в карту
        tools[toolNum] = tool;
    } else {
        // Это выбор инструмента вне заголовка
        currentTool = toolNum;
    }
}

/**
 * @brief Разбор координатной строки
 * 
 * @param line Строка с координатами
 */
void Excellon::parseCoordinate(const string& line) {
    double x = currentX;
    double y = currentY;
    bool hasNewX = false;
    bool hasNewY = false;
    
    // Регулярные выражения для извлечения координат
    regex xRegex("X([+-]?[\\d.]+)");
    regex yRegex("Y([+-]?[\\d.]+)");
    
    smatch xMatch, yMatch;
    
    // Извлечение X координаты
    if (regex_search(line, xMatch, xRegex)) {
        x = parseCoord(xMatch[1], true);
        hasNewX = true;
    }
    
    // Извлечение Y координаты
    if (regex_search(line, yMatch, yRegex)) {
        y = parseCoord(yMatch[1], false);
        hasNewY = true;
    }
    
    // Если координаты относительные, добавляем к текущим
    if (!isAbsoluteCoords) {
        if (hasNewX) x += currentX;
        if (hasNewY) y += currentY;
    }
    
    // Обновляем текущие координаты
    if (hasNewX) currentX = x;
    if (hasNewY) currentY = y;
    
    // Проверяем, есть ли активный инструмент
    if (currentTool == 0) {
        warning("Не выбран инструмент для сверления на X%f Y%f", x, y);
        return;
    }
    
    // Проверяем, определен ли текущий инструмент
    if (tools.find(currentTool) == tools.end()) {
        warning("Инструмент T%d не определен, но используется на X%f Y%f", currentTool, x, y);
        return;
    }
    
    // Проверяем команду действия (сверления)
    if (line.find('M') == string::npos) { // Если нет M команд, это команда сверления
        // Увеличиваем счетчик использования инструмента
        tools[currentTool].count++;
        
        // Добавляем позицию сверления
        DrillPosition pos;
        pos.x = x;
        pos.y = y;
        pos.toolNumber = currentTool;
        drillPositions.push_back(pos);
        
        // Выбираем диаметр отверстия - либо унифицированный, либо из инструмента
        double diameter = (uniformDrillDiameter > 0) ? uniformDrillDiameter : tools[currentTool].diameter;
        
        // Создаем полигон для отверстия с выбранным диаметром
        createDrillPolygon(x, y, diameter);
    }
}

/**
 * @brief Разбор значения координаты
 * 
 * @param coord Строка с координатой
 * @param isX Флаг, указывающий, является ли это X координатой
 * @return Значение координаты в единицах измерения
 */
double Excellon::parseCoord(const string& coord, 
	#ifdef ENABLE_DEBUG_LOGGING
		bool isX
	#else
		bool /* isX */
	#endif
	) {
    // Логируем входные данные
#ifdef ENABLE_DEBUG_LOGGING
    std::ofstream logFile("excellon_debug.log", std::ios::app);
    logFile << "[DEBUG] Разбор координаты " << (isX ? "X" : "Y") << ": " << coord;
#endif

    // Проверка на наличие десятичной точки
    if (coord.find('.') != string::npos) {
        double result = stod(coord);
#ifdef ENABLE_DEBUG_LOGGING
        logFile << " (с десятичной точкой) -> " << result << "\n";
#endif
        return result;
    }
    
    // Получаем строку без знака
    string coordStr = coord;
    bool isNegative = false;
    
    if (!coordStr.empty() && (coordStr[0] == '+' || coordStr[0] == '-')) {
        isNegative = (coordStr[0] == '-');
        coordStr = coordStr.substr(1); // Убираем знак
    }
    
    // Если задан формат FILE_FORMAT=X:Y, используем его для разделения
    if (coordInts > 0) {
        // Строковый метод разделения на целую и дробную части
        string integerPart, fractionalPart;
        
        // Если длина строки <= количеству целых цифр, то дробной части нет
        if (static_cast<int>(coordStr.length()) <= coordInts) {
            integerPart = coordStr;
            fractionalPart = "0";
        } else {
            // Разделяем строку на целую и дробную части согласно формату
            integerPart = coordStr.substr(0, coordInts);
            fractionalPart = coordStr.substr(coordInts);
        }
        
        // Собираем итоговое число в виде строки с десятичной точкой
        string resultStr = integerPart + "." + fractionalPart;
        double result = stod(resultStr);
        
        // Применяем знак, если был отрицательный
        if (isNegative) {
            result = -result;
        }
        
#ifdef ENABLE_DEBUG_LOGGING
        logFile << " (формат " << coordInts << ":" << coordDecimals 
                << ", целая часть: " << integerPart 
                << ", дробная часть: " << fractionalPart 
                << ") -> " << result << "\n";
#endif
        return result;
    } else {
        // Если формат не задан явно, используем стандартный подход с coordDecimals
        double value = stod(coord);
        double divisor = pow(10, coordDecimals);
        double result = value / divisor;
        
#ifdef ENABLE_DEBUG_LOGGING
        logFile << " (стандартный формат, " << coordDecimals 
                << " цифр после запятой) -> " << result << "\n";
#endif
        return result;
    }
}

/**
 * @brief Обработка строки файла Excellon
 * 
 * @param line Строка для обработки
 */
void Excellon::processExcellonLine(const string& line) {
    // Проверяем комментарии, содержащие информацию о формате
    if (line[0] == ';') {
        // Проверяем, содержит ли комментарий информацию о формате
        if (line.find("FILE_FORMAT=") != string::npos) {
            regex formatRegex("FILE_FORMAT=([0-9]):([0-9])");
            smatch formatMatch;
            
            if (regex_search(line, formatMatch, formatRegex)) {
                coordInts = stoi(formatMatch[1]);  // Количество цифр до запятой
                coordDecimals = stoi(formatMatch[2]);  // Количество цифр после запятой
                
#ifdef ENABLE_DEBUG_LOGGING
                std::ofstream logFile("excellon_debug.log", std::ios::app);
                logFile << "[DEBUG] Из комментария обнаружен формат FILE_FORMAT=" << coordInts << ":" << coordDecimals << "\n";
#endif
            }
        }
        return; // Для остальных комментариев просто возвращаемся
    }
    
    // Игнорируем пустые строки
    if (line.empty()) {
        return;
    }
    
    // Обработка заголовка
    if (isHeaderActive || line.find("M48") != string::npos || 
        line.find("M95") != string::npos || line.find("%") != string::npos) {
        parseHeader(line);
        return;
    }
    
    // Обработка выбора инструмента (начинается с T)
    if (line[0] == 'T' || line[0] == 't') {
        parseTool(line);
        return;
    }
    
    // Обработка команд завершения (M30, M00, M02)
    if (line.find("M30") != string::npos || 
        line.find("M00") != string::npos || 
        line.find("M02") != string::npos) {
        // Завершение обработки файла
        return;
    }
    
    // Обработка координат
    if (line.find('X') != string::npos || line.find('Y') != string::npos) {
        parseCoordinate(line);
        return;
    }
    
    // Другие команды - игнорируем или выводим предупреждение
    if (line.find('G') != string::npos || line.find('M') != string::npos) {
        // Известные G-коды и M-коды
        if (line.find("G90") != string::npos) {
            isAbsoluteCoords = true;
        } else if (line.find("G91") != string::npos) {
            isAbsoluteCoords = false;
        } else if (line.find("G05") != string::npos) {
            // Режим сверления - обычно игнорируем
        } else {
            warning("Необработанный G/M код: %s", line.c_str());
        }
    }
}

/**
 * @brief Создает полигон для представления отверстия
 * 
 * @param x Координата X центра отверстия
 * @param y Координата Y центра отверстия
 * @param diameter Диаметр отверстия
 */
void Excellon::createDrillPolygon(double x, double y, double diameter) {
    // Преобразуем координаты и диаметр в пиксели
    double px = getPixelsFromUnits(x);
    double py = getPixelsFromUnits(y);
    double pDiameter = getPixelsFromUnits(diameter);
    
    // Добавляем запас к диаметру, если задан growSize
    pDiameter += growSize;
    
    // Применяем масштабирование
    px *= optScaleX;
	py *= -optScaleY;  // ИЗМЕНЕНИЕ: Добавляем отрицательный масштаб по Y, как в Gerber
    
    // Создаем новый полигон
    polygons.push_back(Polygon());
    Polygon& poly = polygons.back();
    
    // Устанавливаем полярность полигона (отверстия обычно темные)
    poly.polarity = DARK;
    
    // Количество сегментов для аппроксимации окружности
    const int segments = 36;
    
    // Создаем круг из сегментов
    double radius = pDiameter / 2.0;
    for (int i = 0; i < segments; ++i) {
        double angle = 2.0 * M_PI * i / segments;
        double ptx = px + radius * cos(angle);
        double pty = py + radius * sin(angle);
        poly.vdata->add(ptx, pty);
    }
    
    // Инициализируем полигон
    poly.vdata->initialise();
}

/**
 * @brief Конструктор класса Excellon
 * 
 * @param fp_excellon Указатель на файл Excellon
 * @param dotsPerInch Разрешение в точках на дюйм
 * @param growSize Размер для увеличения полигонов
 * @param optScaleX Масштабирование по оси X
 * @param optScaleY Масштабирование по оси Y
 * @param uniformDrillDiameter Если > 0, использовать этот диаметр для всех отверстий
 * @param uniformDrillInMillimeters Указывает, задан ли uniformDrillDiameter в миллиметрах (true) или дюймах (false)
 */
Excellon::Excellon(FILE* fp_excellon, const double dotsPerInch, const double growSize, 
                   double optScaleX, double optScaleY, double uniformDrillDiameter,
                   bool uniformDrillInMillimeters)
    : dotsPerInch(dotsPerInch), growSize(growSize), optScaleX(optScaleX), optScaleY(optScaleY),
      uniformDrillDiameter(uniformDrillInMillimeters && uniformDrillDiameter > 0 ? 
                          uniformDrillDiameter / 25.4 : uniformDrillDiameter), // Конвертация из мм в дюймы если нужно
      currentTool(0), currentX(0), currentY(0), 
      units(INCH), isHeaderActive(false), isAbsoluteCoords(true), isLeadingZeros(true), 
      isTrailingZeros(false), coordDecimals(4), coordInts(0), lineNumber(1), excellonFormat2(false), 
      scaleFactor(1.0), isError(false), imagePolarityDark(true)
{
#ifdef ENABLE_DEBUG_LOGGING
    std::ofstream logFile("excellon_debug.log", std::ios::app);
    logFile << "[DEBUG] Входим в конструктор Excellon\n";
#endif

    if (!fp_excellon) {
        isError = true;
        errorMessage << "Ошибка: Невалидный указатель на файл";
#ifdef ENABLE_DEBUG_LOGGING
        logFile << "[ОШИБКА] Указатель на файл нулевой\n";
#endif
        return;
    }
    
    try {
        // Читаем файл строка за строкой
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), fp_excellon)) {
            // Удаляем символы новой строки и возврата каретки
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';
            if (len > 1 && buffer[len-2] == '\r') buffer[len-2] = '\0';
            
            // Обрабатываем строку
            string line(buffer);
#ifdef ENABLE_DEBUG_LOGGING
            logFile << "[DEBUG] Обрабатываем строку " << lineNumber << ": " << line << "\n";
#endif
            processExcellonLine(line);
            lineNumber++;
        }
        
#ifdef ENABLE_DEBUG_LOGGING
        logFile << "[DEBUG] Завершен разбор файла Excellon\n";
        logFile << "[DEBUG] Найдено " << tools.size() << " инструментов\n";
        logFile << "[DEBUG] Создано " << polygons.size() << " полигонов для сверления\n";
#endif

        // Вывод статистики инструментов
        for (const auto& tool : tools) {
            string msg = "Инструмент T" + to_string(tool.first) + 
                         " (диаметр: " + to_string(tool.second.diameter) + 
                         (units == INCH ? " дюймов" : " мм") + 
                         ") использован " + to_string(tool.second.count) + " раз";
            messages.push_back(msg);
        }
        
    } catch (const std::exception& e) {
        isError = true;
        errorMessage << "Ошибка: " << e.what() << " в строке " << lineNumber;
#ifdef ENABLE_DEBUG_LOGGING
        logFile << "[ОШИБКА] Исключение при разборе: " << e.what() << "\n";
#endif
    } catch (...) {
        isError = true;
        errorMessage << "Ошибка: Неизвестное исключение в строке " << lineNumber;
#ifdef ENABLE_DEBUG_LOGGING
        logFile << "[ОШИБКА] Неизвестное исключение при разборе\n";
#endif
    }
    
#ifdef ENABLE_DEBUG_LOGGING
    logFile.close();
#endif
}

/**
 * @brief Деструктор класса Excellon
 */
Excellon::~Excellon() {
    // Освобождение ресурсов, если необходимо
}
