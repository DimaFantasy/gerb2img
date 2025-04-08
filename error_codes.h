#ifndef ERROR_CODES_H
#define ERROR_CODES_H

// Код без ошибки
#define NO_ERROR 0

// Коды ошибок
#define ERROR_FILE_OPEN_FAILED 2     // Невозможно открыть файл
#define ERROR_GERBER_PROCESSING 3    // Ошибка обработки Gerber
#define ERROR_INVALID_PARAMETERS 4   // Некорректные параметры
#define ERROR_NO_IMAGE 5             // Нет изображения для обработки
#define ERROR_MEMORY_ALLOCATION 6    // Ошибка выделения памяти
#define ERROR_OUTPUT_FILE_CREATION 7 // Ошибка создания выходного файла
#define ERROR_JSON_PROCESSING 8      // Ошибка обработки JSON

#define ERROR_UNKNOWN 9999 // Неизвестная ошибка

#endif // ERROR_CODES_H
