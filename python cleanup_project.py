import os
from pathlib import Path

# Список расширений и файлов для удаления
UNNECESSARY_EXTENSIONS = [".o", ".dll", ".exe", ".a", ".tiff"]
UNNECESSARY_FILES = ["Makefile.am", "Makefile.in"]

def delete_unnecessary_files(directory: Path):
    """
    Удаляет временные и ненужные файлы из указанной директории.
    """
    for root, dirs, files in os.walk(directory):
        for file in files:
            file_path = Path(root) / file
            # Удаляем файлы с ненужными расширениями
            if file_path.suffix in UNNECESSARY_EXTENSIONS or file_path.name in UNNECESSARY_FILES:
                try:
                    print(f"Удаление файла: {file_path}")
                    file_path.unlink()
                except Exception as e:
                    print(f"Ошибка при удалении {file_path}: {e}")

        # Исключаем папку настроек .vscode из обработки
        if ".vscode" in dirs:
            dirs.remove(".vscode")

if __name__ == "__main__":
    project_dir = Path(__file__).parent
    print(f"Очистка проекта в директории: {project_dir}")
    delete_unnecessary_files(project_dir)
    print("Очистка завершена.")