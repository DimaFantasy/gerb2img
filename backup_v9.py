import os
import zipfile
import time
import re

current_dir = os.getcwd()

def is_backup(file_name):
    return file_name.startswith("backup_") and file_name.endswith(".zip")

existing_backups = [f for f in os.listdir(current_dir) if is_backup(f)]
base_backup_name = f"backup_{time.strftime('%Y.%m.%d_%H.%M')}"
version_pattern = re.compile(r"_v(\d+)")

existing_versions = []
for backup in existing_backups:
    match = version_pattern.search(backup)
    if match:
        existing_versions.append(int(match.group(1)))

next_version = max(existing_versions, default=0) + 1
backup_name = f"{base_backup_name}_v{next_version}.zip"

files_to_backup = []
# Модифицируем цикл обхода директорий, чтобы не игнорировать скрытые папки
for root, dirs, files in os.walk(current_dir):
    # Убираем проверку на скрытые папки (dirs[:] = ... часто используется для фильтрации)
    for file in files:
        # Проверяем только, не является ли файл бэкапом
        if not is_backup(file):
            files_to_backup.append(os.path.join(root, file))

# Создаем временный файл с информацией о директории
origin_dir_file = "original_dir.txt"
with open(origin_dir_file, "w", encoding="utf-8") as f:
    f.write(f"Резервная копия создана из директории:\n{os.path.abspath(current_dir)}")
files_to_backup.insert(0, origin_dir_file)  # Добавляем файл в начало списка

def create_backup_with_progress():
    full_history = []
    start_time = time.time()

    try:
        total_files = len(files_to_backup)
        if total_files == 0:
            print("Нет файлов для резервного копирования!")
            return

        with zipfile.ZipFile(backup_name, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for i, file in enumerate(files_to_backup):
                rel_path = os.path.relpath(file, current_dir)
                print(f"[{(i + 1) / len(files_to_backup) * 100:.0f}%] {rel_path}")  # Выводим имя файла до добавления в архив
                zipf.write(file, rel_path)
                full_history.append(rel_path)

                # Удаляем временный файл сразу после его добавления в архив
                if file == origin_dir_file and os.path.exists(origin_dir_file):
                    os.remove(origin_dir_file)

        # Сообщение об успешном создании архива выводится только в конце
        print(f"\nАрхив создан: {os.path.abspath(backup_name)}")
        input("Нажмите Enter для выхода.")

    except Exception as e:
        print(f"Ошибка: {str(e)}")
        input("Нажмите Enter для выхода.")

create_backup_with_progress()
