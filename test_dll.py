import ctypes
from ctypes import *
from pathlib import Path
from typing import Optional
import json


def convert_gerber(
    input_file: str, output_file: str, options: Optional[dict] = None
):
    """
    Конвертирует один Gerber-файл в изображение через DLL (новая версия API)
    """
    if options is None:
        options = {}

    # Выбор DLL в зависимости от архитектуры Python
    arch = ctypes.sizeof(ctypes.c_void_p) * 8
    dll_name = f"gerb2img_x{arch}.dll"
    dll_path = Path(__file__).parent / dll_name
    print(f"Загрузка DLL ({arch}-бит): {dll_path}")

    if not dll_path.exists():
        raise FileNotFoundError(f"Файл DLL не найден: {dll_path}")

    try:
        gerber_dll = cdll.LoadLibrary(str(dll_path))
        print("DLL успешно загружена")
    except OSError as e:
        raise RuntimeError(f"Не удалось загрузить DLL: {e}") from e

    if not hasattr(gerber_dll, "processGerberJSON"):
        raise RuntimeError("Функция 'processGerberJSON' не найдена в DLL")

    # Установка сигнатуры функции
    process_func = gerber_dll.processGerberJSON
    process_func.argtypes = [c_char_p]
    process_func.restype = c_int

    # Формирование JSON
    params = {
        "imageDPI": options.get("imageDPI", 1024),
        "optGrowUnitsMillimeters": options.get("optGrowUnitsMillimeters", False),
        "optBoarderUnitsMillimeters": options.get("optBoarderUnitsMillimeters", False),
        "optBoarder": options.get("optBoarder", 0),
        "optInvertPolarity": options.get("optInvertPolarity", False),
        "rowsPerStrip": options.get("rowsPerStrip", 512),
        "optGrowSize": options.get("optGrowSize", 0),
        "optScaleX": options.get("optScaleX", 1),
        "optScaleY": options.get("optScaleY", 1),
        "outputFilename": str(output_file),
        "inputFilename": str(input_file),
    }

    json_params = json.dumps(params)
    print(f"Передача JSON: {json_params}")

    # Вызов функции
    result = process_func(json_params.encode("utf-8"))
    print(f"Результат processGerberJSON: {result}")

    if result != 0:
        raise RuntimeError(f"Ошибка конвертации (код: {result})")

    return True


if __name__ == "__main__":
    try:
        base_path = Path(__file__).parent
        input_path = base_path / "USB_CAN_MULTI_V.GBR"
        output_path = base_path / "OUTPUT.tif"

        convert_gerber(
            input_file=str(input_path),
            output_file=str(output_path),
            options={
                "imageDPI": 6371,
                "optInvertPolarity": True,
                "optScaleX": 1,
                "optScaleY": 1,
                # другие опции по желанию
            },
        )
        print("Конвертация успешно завершена!")
    except Exception as e:
        print(f"Ошибка: {e}")
        import traceback
        traceback.print_exc()
