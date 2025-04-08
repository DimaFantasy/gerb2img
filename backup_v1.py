import os
import zipfile
import time
from tqdm import tqdm
import re

# Получаем текущую директорию
current_dir = os.getcwd()

# Функция для проверки, является ли файл бекапом
def is_backup(file_name):
    return file_name.startswith("backup_") and file_name.endswith(".zip")

# Получаем все существующие бекапы в текущей директории
existing_backups = [f for f in os.listdir(current_dir) if is_backup(f)]

# Определяем базовое имя для нового бекапа
base_backup_name = f"backup_{time.strftime('%Y.%m.%d_%H.%M')}"

# Регулярное выражение для поиска версии в имени файла
# Ищем _v, за которым идет цифра (версия), и после которой могут быть любые символы до .zip
version_pattern = re.compile(r"_v(\d+)")

# Находим наибольшую версию существующих бекапов
existing_versions = []
for backup in existing_backups:
    match = version_pattern.search(backup)
    if match:
        version = int(match.group(1))  # Извлекаем номер версии
        existing_versions.append(version)

# Определяем следующую версию для нового бекапа
next_version = max(existing_versions, default=0) + 1

# Формируем окончательное имя для нового бекапа с маленькой буквой v перед версией
backup_name = f"{base_backup_name}_v{next_version}.zip"

# Собираем все файлы и каталоги в текущем каталоге и его подкаталогах
files_to_backup = []
for root, dirs, files in os.walk(current_dir):
    for file in files:
        # Если файл является бекапом, пропускаем его
        if not is_backup(file):
            files_to_backup.append(os.path.join(root, file))

# Создаем ZIP-архив
try:
    with zipfile.ZipFile(backup_name, 'w', zipfile.ZIP_DEFLATED) as zipf:
        # Инициализируем прогресс-бар
        with tqdm(files_to_backup, desc="Создание архива", unit="файл") as progress:
            for file in progress:
                # Добавляем файл в архив
                zipf.write(file, os.path.relpath(file, current_dir))
                # Выводим полный путь файла, который архивируется
                file_path = os.path.abspath(file)
                tqdm.write(f"Архивируется: {file_path}")
    print(f"Бекап успешно создан: {backup_name}")
except Exception as e:
    print(f"Ошибка при создании бекапа: {e}")

# Ожидание нажатия клавиши для завершения
input("Нажмите любую клавишу для завершения...")
