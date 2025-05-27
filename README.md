# Gerb2Img

`Gerb2Img` is a library and utility for converting Gerber RS-274X and Excellon (drill) files into raster images in TIFF or BMP formats. This project is based on the original `gerb2tiff-1.2` code developed by Adam Seychell (2001). The original project was an executable file (.exe) and only supported the TIFF format. The gerb2tiff-1.2 project is no longer maintained or developed.

The project has been reworked into a DLL library and EXE utility, making it convenient for use in any C++, Delphi, Python, and other language projects. Compiler warnings have also been eliminated, some bugs fixed, BMP format support added (for DLL), and compatibility improved.

## Main Features

- Converting Gerber files to monochrome images:
  - DLL supports TIFF and BMP formats (depending on the output file extension).
  - EXE only supports TIFF format.
- Support for Excellon files (drill format) in the DLL implementation.
- Support for various parameters: DPI, scaling, polarity inversion, adding borders.
- Export of functions for use in other applications through the DLL interface:
  - `processGerber`: The main function for processing Gerber files.
  - `processExcellon`: Function for processing Excellon files (drilling).
  - `processGerberJSON`: Function for processing Gerber files with parameters in JSON format.
  - `processExcellonJSON`: Function for processing Excellon files with parameters in JSON format.

## Exported DLL Functions

### processGerber
```c
int __stdcall processGerber(
    double imageDPI,              // Image resolution in DPI
    bool optGrowUnitsMillimeters, // Flag: growth units in millimeters
    bool optBoarderUnitsMillimeters, // Flag: border units in millimeters
    double optBoarder,           // Border size (in DPI or mm depending on flag)
    bool optInvertPolarity,      // Invert polarity
    double optGrowSize,          // Growth size (in DPI or mm depending on flag)
    double optScaleX,            // Scale factor on X axis
    double optScaleY,            // Scale factor on Y axis
    const char *outputFilename,  // Output file name
    const char *inputFilename    // Input Gerber file name
);
```

### processGerberJSON
```c
int __stdcall processGerberJSON(const char *jsonParams);
```

Accepts a JSON string with parameters:
```json
{
  "imageDPI": 2400.0,
  "optGrowUnitsMillimeters": false,
  "optBoarderUnitsMillimeters": false,
  "optBoarder": 0.0,
  "optInvertPolarity": false,
  "optGrowSize": 0.0,
  "optScaleX": 1.0,
  "optScaleY": 1.0,
  "outputFilename": "output.bmp",
  "inputFilename": "input.gbr"
}
```

### processExcellon
```c
int __stdcall processExcellon(
    double imageDPI,              // Image resolution in DPI
    bool unitsMillimeters,        // Units: true - millimeters, false - pixels
    double optBoarder,            // Border size (in mm or pixels)
    bool optInvertPolarity,       // Invert polarity
    double optGrowSize,           // Hole growth size (in mm or pixels)
    double optScaleX,             // Scale factor on X axis
    double optScaleY,             // Scale factor on Y axis
    bool uniformDrills,           // Use uniform diameter for all holes
    bool uniformDrillsMillimeters,// For uniformDrillDiameter: true - millimeters, false - inches
    double uniformDrillDiameter,  // Diameter value for all holes (if uniformDrills=true) (mm/in)
    const char *outputFilename,   // Output file name
    const char *inputFilename,    // Input Excellon file name
    int *drillCount               // Pointer to variable for returning the number of drilled holes
);
```

### processExcellonJSON
```c
int __stdcall processExcellonJSON(const char *jsonParams);
```

Accepts a JSON string with parameters:
```json
{
  "imageDPI": 2400.0,
  "unitsMillimeters": false,
  "optBoarder": 0.0,
  "optInvertPolarity": false,
  "optGrowSize": 0.0,
  "optScaleX": 1.0,
  "optScaleY": 1.0,
  "uniformDrills": false,
  "uniformDrillsMillimeters": false,
  "uniformDrillDiameter": 0.0,
  "outputFilename": "drill_output.bmp",
  "inputFilename": "input.drl"
}
```
**Note:** The `optGrowSize` parameter in Excellon allows compensation for technological peculiarities in production: positive values increase hole diameter, negative values decrease it. The function returns the number of processed drill holes through the special `drillCount` parameter.

## Usage Examples

### Python (via ctypes)
```python
import ctypes

# Loading the library
gerb2img = ctypes.WinDLL("gerb2img.dll")

# Defining the processGerber function
processGerber = gerb2img.processGerber
processGerber.argtypes = [
    ctypes.c_double, ctypes.c_bool, ctypes.c_bool, ctypes.c_double,
    ctypes.c_bool, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.c_char_p, ctypes.c_char_p
]
processGerber.restype = ctypes.c_int

# Calling the function for Gerber
result = processGerber(
    2400.0, False, False, 0.0, False, 0.0, 1.0, 1.0,
    b"output.bmp", b"example.gbr"
)

if result == 0:
    print("Gerber conversion successful!")
else:
    print("Gerber conversion error!")

# Defining the processExcellon function
processExcellon = gerb2img.processExcellon
processExcellon.argtypes = [
    ctypes.c_double, ctypes.c_bool, ctypes.c_double,
    ctypes.c_bool, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.c_bool, ctypes.c_bool,
    ctypes.c_double, ctypes.c_char_p, ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_int)
]
processExcellon.restype = ctypes.c_int

# Calling the function for Excellon
drill_count = ctypes.c_int(0)
result = processExcellon(
    2400.0, False, 0.0, False, 0.0, 1.0, 1.0,
    False, False, 0.0,
    b"drill_output.bmp", b"example.drl",
    ctypes.byref(drill_count)
)

if result == 0:
    print(f"Excellon conversion successful! Number of drills: {drill_count.value}")
else:
    print("Excellon conversion error!")

# Using JSON API
processGerberJSON = gerb2img.processGerberJSON
processGerberJSON.argtypes = [ctypes.c_char_p]
processGerberJSON.restype = ctypes.c_int

processExcellonJSON = gerb2img.processExcellonJSON
processExcellonJSON.argtypes = [ctypes.c_char_p]
processExcellonJSON.restype = ctypes.c_int

# JSON example for Gerber
gerber_json = b'''{"imageDPI": 2400.0, "optGrowUnitsMillimeters": false, 
"optBoarderUnitsMillimeters": false, "optBoarder": 0.0, "optInvertPolarity": false, 
"optGrowSize": 0.0, "optScaleX": 1.0, "optScaleY": 1.0, 
"outputFilename": "output_json.bmp", "inputFilename": "example.gbr"}'''

# JSON example for Excellon
excellon_json = b'''{"imageDPI": 2400.0, "optGrowUnitsMillimeters": false, 
"optBoarderUnitsMillimeters": false, "optBoarder": 0.0, "optInvertPolarity": false, 
"optGrowSize": 0.02, "optScaleX": 1.0, "optScaleY": 1.0, 
"outputFilename": "drill_output_json.bmp", "inputFilename": "example.drl"}'''

# Calling JSON functions
result_gerber_json = processGerberJSON(gerber_json)
result_excellon_json = processExcellonJSON(excellon_json)
```

### Delphi
```delphi
library Gerb2ImgDemo;

uses
  Windows, SysUtils;

type
  TProcessGerber = function(
    imageDPI: Double; 
    optGrowUnitsMillimeters: Boolean;
    optBoarderUnitsMillimeters: Boolean;
    optBoarder: Double;
    optInvertPolarity: Boolean;
    optGrowSize: Double;
    optScaleX: Double;
    optScaleY: Double;
    outputFilename: PAnsiChar;
    inputFilename: PAnsiChar
  ): Integer; stdcall;

  TProcessExcellon = function(
    imageDPI: Double;
    optGrowUnitsMillimeters: Boolean;
    optBoarderUnitsMillimeters: Boolean;
    optBoarder: Double;
    optInvertPolarity: Boolean;
    optGrowSize: Double;
    optScaleX: Double;
    optScaleY: Double;
    outputFilename: PAnsiChar;
    inputFilename: PAnsiChar;
    drillCount: PInteger
  ): Integer; stdcall;

var
  Gerb2ImgLib: THandle;
  ProcessGerber: TProcessGerber;
  ProcessExcellon: TProcessExcellon;

begin
  Gerb2ImgLib := LoadLibrary('gerb2img.dll');
  if Gerb2ImgLib = 0 then
    raise Exception.Create('Failed to load gerb2img.dll');

  @ProcessGerber := GetProcAddress(Gerb2ImgLib, 'processGerber');
  if not Assigned(ProcessGerber) then
    raise Exception.Create('Failed to find processGerber function');

  @ProcessExcellon := GetProcAddress(Gerb2ImgLib, 'processExcellon');
  if not Assigned(ProcessExcellon) then
    raise Exception.Create('Failed to find processExcellon function');

  try
    if ProcessGerber(2400.0, False, False, 0.0, False, 0.0, 1.0, 1.0,
      'output.bmp', 'example.gbr') = 0 then
      Writeln('Gerber conversion successful!')
    else
      Writeln('Gerber conversion error!');

    var drillCount: Integer;
    if ProcessExcellon(2400.0, False, False, 0.0, False, 0.0, 1.0, 1.0,
      'drill_output.bmp', 'example.drl', @drillCount) = 0 then
      Writeln('Excellon conversion successful! Drill count: ', drillCount)
    else
      Writeln('Excellon conversion error!');
  finally
    FreeLibrary(Gerb2ImgLib);
  end;
end.
```

## Demo

The project includes demonstration examples for using the library:
- **Delphi**: Example of using DLL for processing Gerber files.
- **Python**: Example of usage via `ctypes`.

## Building

### Requirements
- Compiler with C++11 or higher support.
- Libraries:
  - [libtiff](http://www.libtiff.org/)
  - [EasyBMP](http://easybmp.sourceforge.net/) - Already included
  - [nlohmann/json](https://github.com/nlohmann/json) - Already included

## 🔧 Building Instructions for Windows (MinGW)

### 1. Install the `libtiff` dependency (other dependencies are already included in the project):

- For **64-bit MinGW environment**:
  ```bash
  pacman -S mingw-w64-x86_64-libtiff
  ```

- For **32-bit MinGW environment**:
  ```bash
  pacman -S mingw-w64-i686-libtiff
  ```

### 2. Compile the project:

- Run `mingw64.exe` (for 64-bit) or `mingw32.exe` (for 32-bit).
- Navigate to the project folder:
  ```bash
  cd gerb2img
  ```
- Execute the build:
  ```bash
  make
  ```

### 💡 Main commands:
- `make`             — Build release DLL and EXE for the current architecture (x32 or x64).
- `make debug`       — Build debug DLL and EXE.
- `make clean`       — Remove all generated files.

### 🔧 Individual targets:
- `make dll`         — Only release DLL.
- `make exe`         — Only release EXE.
- `make dll_debug`   — Only debug DLL.
- `make exe_debug`   — Only debug EXE.

## License

This project is distributed under the [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.txt).

## Acknowledgements

- Adam Seychell for the original `gerb2tiff-1.2` project.
- The Open Source community for provided libraries and tools.

## Project Future

The project will be actively developed. Plans include:
- Adding BMP format support for EXE.
- Improving performance and functionality.
- Expanding documentation and usage examples.

# Gerb2Img

`Gerb2Img` — это библиотека и утилита для преобразования файлов Gerber RS-274X и Excellon (сверловка) в растровые изображения в форматах TIFF или BMP. Этот проект основан на оригинальном коде `gerb2tiff-1.2`, разработанном Adam Seychell (2001). Оригинальный проект представлял собой исполняемый файл (.exe) и поддерживал только формат TIFF. Проект gerb2tiff-1.2 больше не поддерживается и не развивается.

Проект был переработан в библиотеку DLL и утилиту EXE, что делает его удобным для использования в любых проектах на C++, Delphi, Python и других языках. Также были устранены предупреждения компилятора, исправлены некоторые баги, добавлена поддержка формата BMP (для DLL) и улучшена совместимость.

## Основные возможности

- Конвертация Gerber-файлов в монохромные изображения:
  - DLL поддерживает форматы TIFF и BMP (в зависимости от расширения выходного файла).
  - EXE поддерживает только формат TIFF.
- Поддержка Excellon файлов (формат сверловки) в DLL реализации.
- Поддержка различных параметров: DPI, масштабирование, инверсия полярности, добавление границ.
- Экспорт функций для использования в других приложениях через интерфейс DLL:
  - `processGerber`: Основная функция для обработки Gerber-файлов.
  - `processExcellon`: Функция для обработки Excellon файлов (сверловка).
  - `processGerberJSON`: Функция для обработки Gerber-файлов с параметрами в формате JSON.
  - `processExcellonJSON`: Функция для обработки Excellon файлов с параметрами в формате JSON.

## Экспортируемые функции DLL

### processGerber
```c
int __stdcall processGerber(
    double imageDPI,              // Разрешение изображения в DPI
    bool optGrowUnitsMillimeters, // Флаг: единицы измерения роста в миллиметрах
    bool optBoarderUnitsMillimeters, // Флаг: единицы измерения границы в миллиметрах
    double optBoarder,           // Размер границы (в DPI или мм в зависимости от флага)
    bool optInvertPolarity,      // Инвертировать полярность
    double optGrowSize,          // Размер роста (в DPI или мм в зависимости от флага)
    double optScaleX,            // Масштаб по оси X
    double optScaleY,            // Масштаб по оси Y
    const char *outputFilename,  // Имя выходного файла
    const char *inputFilename    // Имя входного Gerber-файла
);
```

### processGerberJSON
```c
int __stdcall processGerberJSON(const char *jsonParams);
```

Принимает JSON-строку с параметрами:
```json
{
  "imageDPI": 2400.0,
  "optGrowUnitsMillimeters": false,
  "optBoarderUnitsMillimeters": false,
  "optBoarder": 0.0,
  "optInvertPolarity": false,
  "optGrowSize": 0.0,
  "optScaleX": 1.0,
  "optScaleY": 1.0,
  "outputFilename": "output.bmp",
  "inputFilename": "input.gbr"
}
```

### processExcellon
```c
int __stdcall processExcellon(
    double imageDPI,              // Разрешение изображения в DPI
    bool unitsMillimeters,        // Единицы измерения: true - миллиметры, false - пиксели
    double optBoarder,            // Размер отступа от края изображения (в мм или пикселях)
    bool optInvertPolarity,       // Инвертировать полярность
    double optGrowSize,           // Размер роста отверстий (в мм или пикселях)
    double optScaleX,             // Масштаб по оси X
    double optScaleY,             // Масштаб по оси Y
    bool uniformDrills,           // Использовать одинаковый диаметр для всех отверстий
    bool uniformDrillsMillimeters,// Для uniformDrillDiameter: true - миллиметры, false - дюймы
    double uniformDrillDiameter,  // Значение диаметра для всех отверстий (если uniformDrills=true) (мм/дюймы)
    const char *outputFilename,   // Имя выходного файла
    const char *inputFilename,    // Имя входного Excellon-файла
    int *drillCount               // Указатель на переменную для возврата количества сверловок
);
```

### processExcellonJSON
```c
int __stdcall processExcellonJSON(const char *jsonParams);
```

Принимает JSON-строку с параметрами:
```json
{
  "imageDPI": 2400.0,
  "unitsMillimeters": false,
  "optBoarder": 0.0,
  "optInvertPolarity": false,
  "optGrowSize": 0.0,
  "optScaleX": 1.0,
  "optScaleY": 1.0,
  "uniformDrills": false,
  "uniformDrillsMillimeters": false,
  "uniformDrillDiameter": 0.0,
  "outputFilename": "drill_output.bmp",
  "inputFilename": "input.drl"
}
```
**Примечание:** Параметр `optGrowSize` в Excellon позволяет компенсировать технологические особенности производства: положительные значения увеличивают диаметр отверстий, отрицательные - уменьшают. Функция возвращает количество обработанных сверловок через специальный параметр `drillCount`.

## Пример использования

### Python (через ctypes)
```python
import ctypes

# Загрузка библиотеки
gerb2img = ctypes.WinDLL("gerb2img.dll")

# Определение функции processGerber
processGerber = gerb2img.processGerber
processGerber.argtypes = [
    ctypes.c_double, ctypes.c_bool, ctypes.c_bool, ctypes.c_double,
    ctypes.c_bool, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.c_char_p, ctypes.c_char_p
]
processGerber.restype = ctypes.c_int

# Вызов функции для Gerber
result = processGerber(
    2400.0, False, False, 0.0, False, 0.0, 1.0, 1.0,
    b"output.bmp", b"example.gbr"
)

if result == 0:
    print("Конвертация Gerber успешна!")
else:
    print("Ошибка конвертации Gerber!")

# Определение функции processExcellon
processExcellon = gerb2img.processExcellon
processExcellon.argtypes = [
    ctypes.c_double, ctypes.c_bool, ctypes.c_double,
    ctypes.c_bool, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.c_bool, ctypes.c_bool,
    ctypes.c_double, ctypes.c_char_p, ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_int)
]
processExcellon.restype = ctypes.c_int

# Вызов функции для Excellon
drill_count = ctypes.c_int(0)
result = processExcellon(
    2400.0, False, 0.0, False, 0.0, 1.0, 1.0,
    False, False, 0.0,
    b"drill_output.bmp", b"example.drl",
    ctypes.byref(drill_count)
)

if result == 0:
    print(f"Конвертация Excellon успешна! Количество сверловок: {drill_count.value}")
else:
    print("Ошибка конвертации Excellon!")

# Использование JSON API
processGerberJSON = gerb2img.processGerberJSON
processGerberJSON.argtypes = [ctypes.c_char_p]
processGerberJSON.restype = ctypes.c_int

processExcellonJSON = gerb2img.processExcellonJSON
processExcellonJSON.argtypes = [ctypes.c_char_p]
processExcellonJSON.restype = ctypes.c_int

# JSON пример для Gerber
gerber_json = b'''{"imageDPI": 2400.0, "optGrowUnitsMillimeters": false, 
"optBoarderUnitsMillimeters": false, "optBoarder": 0.0, "optInvertPolarity": false, 
"optGrowSize": 0.0, "optScaleX": 1.0, "optScaleY": 1.0, 
"outputFilename": "output_json.bmp", "inputFilename": "example.gbr"}'''

# JSON пример для Excellon
excellon_json = b'''{"imageDPI": 2400.0, "optGrowUnitsMillimeters": false, 
"optBoarderUnitsMillimeters": false, "optBoarder": 0.0, "optInvertPolarity": false, 
"optGrowSize": 0.02, "optScaleX": 1.0, "optScaleY": 1.0, 
"outputFilename": "drill_output_json.bmp", "inputFilename": "example.drl"}'''

# Вызов JSON функций
result_gerber_json = processGerberJSON(gerber_json)
result_excellon_json = processExcellonJSON(excellon_json)
```

### Delphi
```delphi
library Gerb2ImgDemo;

uses
  Windows, SysUtils;

type
  TProcessGerber = function(
    imageDPI: Double; 
    optGrowUnitsMillimeters: Boolean;
    optBoarderUnitsMillimeters: Boolean;
    optBoarder: Double;
    optInvertPolarity: Boolean;
    optGrowSize: Double;
    optScaleX: Double;
    optScaleY: Double;
    outputFilename: PAnsiChar;
    inputFilename: PAnsiChar
  ): Integer; stdcall;

  TProcessExcellon = function(
    imageDPI: Double;
    optGrowUnitsMillimeters: Boolean;
    optBoarderUnitsMillimeters: Boolean;
    optBoarder: Double;
    optInvertPolarity: Boolean;
    optGrowSize: Double;
    optScaleX: Double;
    optScaleY: Double;
    outputFilename: PAnsiChar;
    inputFilename: PAnsiChar;
    drillCount: PInteger
  ): Integer; stdcall;

var
  Gerb2ImgLib: THandle;
  ProcessGerber: TProcessGerber;
  ProcessExcellon: TProcessExcellon;

begin
  Gerb2ImgLib := LoadLibrary('gerb2img.dll');
  if Gerb2ImgLib = 0 then
    raise Exception.Create('Не удалось загрузить gerb2img.dll');

  @ProcessGerber := GetProcAddress(Gerb2ImgLib, 'processGerber');
  if not Assigned(ProcessGerber) then
    raise Exception.Create('Не удалось найти функцию processGerber');

  @ProcessExcellon := GetProcAddress(Gerb2ImgLib, 'processExcellon');
  if not Assigned(ProcessExcellon) then
    raise Exception.Create('Не удалось найти функцию processExcellon');

  try
    if ProcessGerber(2400.0, False, False, 0.0, False, 0.0, 1.0, 1.0,
      'output.bmp', 'example.gbr') = 0 then
      Writeln('Конвертация Gerber успешна!')
    else
      Writeln('Ошибка конвертации Gerber!');

    var drillCount: Integer;
    if ProcessExcellon(2400.0, False, False, 0.0, False, 0.0, 1.0, 1.0,
      'drill_output.bmp', 'example.drl', @drillCount) = 0 then
      Writeln('Конвертация Excellon успешна! Количество сверловок: ', drillCount)
    else
      Writeln('Ошибка конвертации Excellon!');
  finally
    FreeLibrary(Gerb2ImgLib);
  end;
end.
```

## Демо

В проект включены демонстрационные примеры для использования библиотеки:
- **Delphi**: Пример использования DLL для обработки Gerber-файлов.
- **Python**: Пример использования через `ctypes`.

## Сборка

### Требования
- Компилятор с поддержкой C++11 или выше.
- Библиотеки:
  - [libtiff](http://www.libtiff.org/)
  - [EasyBMP](http://easybmp.sourceforge.net/) - Уже есть
  - [nlohmann/json](https://github.com/nlohmann/json) - Уже есть

## 🔧 Инструкции по сборке под Windows (MinGW)

### 1. Установите зависимость `libtiff` (остальные зависимости уже включены в проект):

- Для **64-битной среды MinGW**:
  ```bash
  pacman -S mingw-w64-x86_64-libtiff
  ```

- Для **32-битной среды MinGW**:
  ```bash
  pacman -S mingw-w64-i686-libtiff
  ```

### 2. Скомпилируйте проект:

- Запустите `mingw64.exe` (для 64-bit) или `mingw32.exe` (для 32-bit).
- Перейдите в папку проекта:
  ```bash
  cd gerb2img
  ```
- Выполните сборку:
  ```bash
  make
  ```

### 💡 Основные команды:
- `make`             — Сборка release DLL и EXE для текущей архитектуры (x32 или x64).
- `make debug`       — Сборка debug DLL и EXE.
- `make clean`       — Удаление всех сгенерированных файлов.

### 🔧 Отдельные цели:
- `make dll`         — Только release DLL.
- `make exe`         — Только release EXE.
- `make dll_debug`   — Только debug DLL.
- `make exe_debug`   — Только debug EXE.

## Лицензия

Этот проект распространяется под лицензией [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.txt).

## Благодарности

- Adam Seychell за оригинальный проект `gerb2tiff-1.2`.
- Сообщество Open Source за предоставленные библиотеки и инструменты.

## Будущее проекта

Проект будет активно развиваться. В планах:
- Добавление поддержки формата BMP для EXE.
- Улучшение производительности и функциональности.
- Расширение документации и примеров использования.
