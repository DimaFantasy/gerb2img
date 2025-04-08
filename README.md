# Gerb2Img

`Gerb2Img` — это библиотека и утилита для преобразования файлов Gerber RS-274X в растровые изображения в форматах TIFF или BMP. Этот проект основан на оригинальном коде `gerb2tiff-1.2`, разработанном Adam Seychell (2001). Оригинальный проект представлял собой исполняемый файл (.exe) и поддерживал только формат TIFF. Проект gerb2tiff-1.2 больше не поддерживается и не развивается.

Проект был переработан в библиотеку DLL и утилиту EXE, что делает его удобным для использования в любых проектах на C++, Delphi, Python и других языках. Также были устранены предупреждения компилятора, исправлены некоторые баги, добавлена поддержка формата BMP (для DLL) и улучшена совместимость.

## Основные возможности

- Конвертация Gerber-файлов в монохромные изображения:
  - DLL поддерживает форматы TIFF и BMP (в зависимости от расширения выходного файла).
  - EXE поддерживает только формат TIFF.
- Поддержка различных параметров: DPI, масштабирование, инверсия полярности, добавление границ.
- Экспорт функций для использования в других приложениях через интерфейс DLL:
  - `processGerber`: Основная функция для обработки Gerber-файлов.
  - `processGerberJSON`: Функция для обработки параметров в формате JSON.

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

# Вызов функции
result = processGerber(
    2400.0, False, False, 0.0, False, 512, 0.0, 1.0, 1.0,
    b"output.bmp", b"example.gbr"
)

if result == 0:
    print("Conversion successful!")
else:
    print("Conversion failed!")
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

var
  Gerb2ImgLib: THandle;
  ProcessGerber: TProcessGerber;

begin
  Gerb2ImgLib := LoadLibrary('gerb2img.dll');
  if Gerb2ImgLib = 0 then
    raise Exception.Create('Не удалось загрузить gerb2img.dll');

  @ProcessGerber := GetProcAddress(Gerb2ImgLib, 'processGerber');
  if not Assigned(ProcessGerber) then
    raise Exception.Create('Не удалось найти функцию processGerber');

  try
    if ProcessGerber(2400.0, False, False, 0.0, False, 512, 0.0, 1.0, 1.0,
      'output.bmp', 'example.gbr') = 0 then
      Writeln('Конвертация успешна!')
    else
      Writeln('Ошибка конвертации!');
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

