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
    unsigned rowsPerStrip,       // Количество строк в одной полосе TIFF
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
    bool optGrowUnitsMillimeters, // Флаг: единицы измерения роста в миллиметрах
    bool optBoarderUnitsMillimeters, // Флаг: единицы измерения границы в миллиметрах
    double optBoarder,           // Размер границы (в DPI или мм в зависимости от флага)
    bool optInvertPolarity,      // Инвертировать полярность
    unsigned rowsPerStrip,       // Количество строк в одной полосе TIFF
    double optGrowSize,          // Размер роста отверстий (в DPI или мм)
    double optScaleX,            // Масштаб по оси X
    double optScaleY,            // Масштаб по оси Y
    const char *outputFilename,  // Имя выходного файла
    const char *inputFilename    // Имя входного Excellon-файла
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
  "optGrowUnitsMillimeters": false,
  "optBoarderUnitsMillimeters": false,
  "optBoarder": 0.0,
  "optInvertPolarity": false,
  "optGrowSize": 0.0, 
  "optScaleX": 1.0,
  "optScaleY": 1.0,
  "outputFilename": "drill_output.bmp",
  "inputFilename": "input.drl"
}
```
**Примечание:** Параметр `optGrowSize` в Excellon позволяет компенсировать технологические особенности производства: положительные значения увеличивают диаметр отверстий, отрицательные - уменьшают.

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
    ctypes.c_bool, ctypes.c_uint, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.c_char_p, ctypes.c_char_p
]
processGerber.restype = ctypes.c_int

# Вызов функции для Gerber
result = processGerber(
    2400.0, False, False, 0.0, False, 512, 0.0, 1.0, 1.0,
    b"output.bmp", b"example.gbr"
)

if result == 0:
    print("Конвертация Gerber успешна!")
else:
    print("Ошибка конвертации Gerber!")

# Определение функции processExcellon
processExcellon = gerb2img.processExcellon
processExcellon.argtypes = [
    ctypes.c_double, ctypes.c_bool, ctypes.c_bool, ctypes.c_double,
    ctypes.c_bool, ctypes.c_uint, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.c_char_p, ctypes.c_char_p
]
processExcellon.restype = ctypes.c_int

# Вызов функции для Excellon
result = processExcellon(
    2400.0, False, False, 0.0, False, 512, 0.0, 1.0, 1.0,
    b"drill_output.bmp", b"example.drl"
)

if result == 0:
    print("Конвертация Excellon успешна!")
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
    DPI: Double; Invert: Boolean; Mirror: Boolean; Rotation: Double;
    AddBorder: Boolean; BorderSize: Cardinal; ScaleX, ScaleY, OffsetX: Double;
    OutputFile, InputFile: PAnsiChar
  ): Integer; stdcall;

  TProcessExcellon = function(
    DPI: Double; Invert: Boolean; Mirror: Boolean; Rotation: Double;
    AddBorder: Boolean; BorderSize: Cardinal; ScaleX, ScaleY, OffsetX: Double;
    OutputFile, InputFile: PAnsiChar
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
    if ProcessGerber(2400.0, False, False, 0.0, False, 512, 0.0, 1.0, 1.0,
      'output.bmp', 'example.gbr') = 0 then
      Writeln('Конвертация Gerber успешна!')
    else
      Writeln('Ошибка конвертации Gerber!');

    if ProcessExcellon(2400.0, False, False, 0.0, False, 512, 0.0, 1.0, 1.0,
      'drill_output.bmp', 'example.drl') = 0 then
      Writeln('Конвертация Excellon успешна!')
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

