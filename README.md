# Gerb2Img

`Gerb2Img` — это библиотека для преобразования файлов Gerber RS-274X в растровые изображения в форматах TIFF или BMP. Этот проект основан на оригинальном коде `gerb2tiff-1.2`, разработанном Adam Seychell (2001). Оригинальный проект представлял собой исполняемый файл (.exe) и поддерживал только формат TIFF. Проект gerb2tiff-1.2 больше не поддерживается и не развивается.

Проект был переработан в библиотеку DLL, что делает его удобным для использования в любых проектах на C++, Delphi, Python и других языках. Также были устранены ошибки и предупреждения при компиляции, добавлена поддержка формата BMP и улучшена совместимость.

## Основные возможности

- Конвертация Gerber-файлов в монохромные изображения форматов TIFF или BMP (в зависимости от расширения выходного файла).
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

## Лицензия

Этот проект распространяется под лицензией [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.txt).

## Благодарности

- Adam Seychell за оригинальный проект `gerb2tiff-1.2`.
- Сообщество Open Source за предоставленные библиотеки и инструменты.

