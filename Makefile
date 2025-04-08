# 💡 Основные команды:
#   make             # Сборка release DLL и EXE для текущей архитектуры (x32 или x64)
#   make debug       # Сборка debug DLL и EXE
#   make clean       # Удаление всех сгенерированных файлов
#
# 🔧 Отдельные цели:
#   make dll         # Только release DLL
#   make exe         # Только release EXE
#   make dll_debug   # Только debug DLL
#   make exe_debug   # Только debug EXE

# Автоматическое определение окружения
ifeq ($(MSYSTEM), MINGW32)
    BUILD_TARGET := x32
    INCLUDE_PATH := /mingw32/include
    LIB_PATH := /mingw32/lib
else ifeq ($(MSYSTEM), MINGW64)
    BUILD_TARGET := x64
    INCLUDE_PATH := /mingw64/include
    LIB_PATH := /mingw64/lib
else
    $(error Unknown build environment. Please use MSYS2 MINGW32 or MINGW64)
endif

# Указываем текущую директорию как PROJECT_DIR, если она не задана
PROJECT_DIR := $(CURDIR)

# Компилятор
CXX := g++

# Общие флаги
BASE_CXXFLAGS := -Wall -Wextra -static -I$(INCLUDE_PATH) -L$(LIB_PATH)
BASE_CXXFLAGS += -I$(PROJECT_DIR)/include
BASE_CXXFLAGS += -I$(PROJECT_DIR)/EasyBMP

# Режимы компиляции
CXXFLAGS_RELEASE := $(BASE_CXXFLAGS) -O0
CXXFLAGS_DEBUG   := $(BASE_CXXFLAGS) -O0 -g

# Линковка
ifeq ($(BUILD_TARGET), x32)
    LDFLAGS_DLL := -lm -lpthread -lstdc++ -Wl,--output-def,gerb2img_x32.def -Wl,--kill-at
else ifeq ($(BUILD_TARGET), x64)
    LDFLAGS_DLL := -lm -lpthread -lstdc++ -Wl,--output-def,gerb2img_x64.def -Wl,--kill-at
endif
LDFLAGS_EXE := -lm -lpthread -lstdc++

# Библиотеки
LIBS := \
    -ltiff -lzstd -ljpeg -lz -llzma -ljbig -lwebp -ldeflate \
    -llerc -lsharpyuv \
    -lgdi32 -luser32 -lcomdlg32 -lgdiplus -lshlwapi

# Исходники
SRCS_DLL := \
    main_dll.cpp \
    apertures.cpp \
    gerber.cpp \
    polygon.cpp \
    gerber_bison.cc \
    gerber_flex.cc \
    EasyBMP/EasyBMP.cpp

SRCS_EXE := \
    main_exe.cpp \
    apertures.cpp \
    gerber.cpp \
    polygon.cpp \
    gerber_bison.cc \
    gerber_flex.cc \
    EasyBMP/EasyBMP.cpp

# Последний собранный файл
LAST_BUILT :=

# Основные цели
all: dll exe debug
debug: dll_debug exe_debug

# Релиз сборки
ifeq ($(BUILD_TARGET), x32)
dll: x32_dll
exe: x32_exe
dll_debug: x32_dll_debug
exe_debug: x32_exe_debug
else ifeq ($(BUILD_TARGET), x64)
dll: x64_dll
exe: x64_exe
dll_debug: x64_dll_debug
exe_debug: x64_exe_debug
endif

# Release: DLL
x64_dll:
	@echo "Building 64-bit DLL version..."
	@$(CXX) $(CXXFLAGS_RELEASE) -shared -o gerb2img_x64.dll $(SRCS_DLL) $(LIBS) $(LDFLAGS_DLL)
	@$(MAKE) strip_debug LAST_BUILT=gerb2img_x64.dll
	@echo "Done: gerb2img_x64.dll"
#	@echo "Press any key to next step..."
#	@read -n 1 -s -r  # Ожидание нажатия клавиши

x32_dll:
	@echo "Building 32-bit DLL version..."
	@$(CXX) $(CXXFLAGS_RELEASE) -shared -o gerb2img_x32.dll $(SRCS_DLL) $(LIBS) $(LDFLAGS_DLL)
	@$(MAKE) strip_debug LAST_BUILT=gerb2img_x32.dll
	@echo "Done: gerb2img_x32.dll"
#	@echo "Press any key to next step..."
#	@read -n 1 -s -r  # Ожидание нажатия клавиши

# Release: EXE
x64_exe:
	@echo "Building 64-bit EXE version..."
	@$(CXX) $(CXXFLAGS_RELEASE) -o gerb2img_x64.exe $(SRCS_EXE) $(LIBS) $(LDFLAGS_EXE)
	@$(MAKE) strip_debug LAST_BUILT=gerb2img_x64.exe
	@echo "Done: gerb2img_x64.exe"
#	@echo "Press any key to next step..."
#	@read -n 1 -s -r  # Ожидание нажатия клавиши

x32_exe:
	@echo "Building 32-bit EXE version..."
	@$(CXX) $(CXXFLAGS_RELEASE) -o gerb2img_x32.exe $(SRCS_EXE) $(LIBS) $(LDFLAGS_EXE)
	@$(MAKE) strip_debug LAST_BUILT=gerb2img_x32.exe
	@echo "Done: gerb2img_x32.exe"
#	@echo "Press any key to next step..."
#	@read -n 1 -s -r  # Ожидание нажатия клавиши

# Debug: DLL
x64_dll_debug:
	@echo "Building 64-bit DEBUG DLL version..."
	@$(CXX) $(CXXFLAGS_DEBUG) -shared -o gerb2img_x64_debug.dll $(SRCS_DLL) $(LIBS) $(LDFLAGS_DLL)
	@echo "Done: gerb2img_x64_debug.dll"
#	@echo "Press any key to next step..."
#	@read -n 1 -s -r  # Ожидание нажатия клавиши

x32_dll_debug:
	@echo "Building 32-bit DEBUG DLL version..."
	@$(CXX) $(CXXFLAGS_DEBUG) -shared -o gerb2img_x32_debug.dll $(SRCS_DLL) $(LIBS) $(LDFLAGS_DLL)
	@echo "Done: gerb2img_x32_debug.dll"
#	@echo "Press any key to next step..."
#	@read -n 1 -s -r  # Ожидание нажатия клавиши

# Debug: EXE
x64_exe_debug:
	@echo "Building 64-bit DEBUG EXE version..."
	@$(CXX) $(CXXFLAGS_DEBUG) -o gerb2img_x64_debug.exe $(SRCS_EXE) $(LIBS) $(LDFLAGS_EXE)
	@echo "Done: gerb2img_x64_debug.exe"
#	@echo "Press any key to next step..."
#	@read -n 1 -s -r  # Ожидание нажатия клавиши

x32_exe_debug:
	@echo "Building 32-bit DEBUG EXE version..."
	@$(CXX) $(CXXFLAGS_DEBUG) -o gerb2img_x32_debug.exe $(SRCS_EXE) $(LIBS) $(LDFLAGS_EXE)
	@echo "Done: gerb2img_x32_debug.exe"
#	@echo "Press any key to next step..."
#	@read -n 1 -s -r  # Ожидание нажатия клавиши

# Очистка
clean:
	@rm -f \
		gerb2img_x32.dll gerb2img_x64.dll \
		gerb2img_x32.exe gerb2img_x64.exe \
		gerb2img_x32_debug.dll gerb2img_x64_debug.dll \
		gerb2img_x32_debug.exe gerb2img_x64_debug.exe
	@echo "Cleaned build artifacts."
#	@echo "Press any key to next step..."
#	@read -n 1 -s -r  # Ожидание нажатия клавиши

# Удаление отладочной информации
strip_debug:
	@echo "Stripping debug information..."
	@if [ -n "$(LAST_BUILT)" ] && [ -f "$(LAST_BUILT)" ]; then \
		strip --strip-unneeded $(LAST_BUILT); \
		echo "Stripped: $(LAST_BUILT)"; \
	else \
		echo "No file to strip."; \
	fi
#	@echo "Press any key to next step..."
#	@read -n 1 -s -r  # Ожидание нажатия клавиши

.PHONY: all dll exe debug dll_debug exe_debug \
        x64_dll x32_dll x64_exe x32_exe \
        x64_dll_debug x32_dll_debug x64_exe_debug x32_exe_debug \
        clean strip_debug

