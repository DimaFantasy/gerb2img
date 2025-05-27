// This file is distributed under the terms of the GNU General Public License v3.
#include <regex>
#include <time.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <math.h>
#include <limits.h>
#include <ctype.h>
#include "getopt.h"
#include <fstream>
#include <cstdint>

#include <stdarg.h>
#include <string.h>
#include "config.h"
#include "nlohmann/json.hpp" // Локальный путь к json.hpp
using json = nlohmann::json;

#include "polygon.h"
#include "apertures.h"
#include "gerber.h"
#include "excellon.h" // Добавлен include для Excellon
#include "tiffio.h"
#include "EasyBMP/EasyBMP.h"
#include "error_codes.h"

unsigned char *DEGUB_bitmap_ptr_end;

unsigned char nbitsTable[256];

const char *help_message =
	"Gerber RS-274X file to raster graphics converter.";

void show_interval(const char *msg = "")
{
	static clock_t start_clock = std::clock();
	double cpu_time_used = ((double)(std::clock() - start_clock)) / CLOCKS_PER_SEC;
	std::printf("time: %.3f s (%s)\n", cpu_time_used, msg);
	start_clock = std::clock();
}

//***************************************************
// Global variables of plotting parameters
//**************************************************
double imageDPI = 2400;
bool optGrowUnitsMillimeters = false;
bool optBoarderUnitsMillimeters = false;
double optBoarder = 0;
bool optInvertPolarity = false;
unsigned rowsPerStrip = 512;
double total_area_cmsq = 0;
double optGrowSize = 0;
double optScaleX = 1;
double optScaleY = 1;
unsigned int bytesPerScanline;
unsigned int bitmapBytes;
unsigned char *bitmap;

//**********************************************************
// Optimised horizontal line drawing from x1,y to x2,y in the monochrome bitmap
// polarity specifies how pixels are changed.
// DRAW_ON = line is drawn bits set
// DRAW_OFF = line is drawn bits cleared
// DRAW_REVERSE  = line is drawn bits inverted
//
// global dependencies:	bytesPerScanline, bitmap
//**********************************************************
void horizontalLine(int x1, int x2, unsigned char *buffer, Polarity_t polarity)
{
	if (x1 > x2)
		std::swap(x1, x2);

	static unsigned char fillSingle[64] = {
		0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xC0, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xE0, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xF0, 0x70, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00,
		0xF8, 0x78, 0x38, 0x18, 0x08, 0x00, 0x00, 0x00,
		0xFC, 0x7C, 0x3C, 0x1C, 0x0C, 0x04, 0x00, 0x00,
		0xFE, 0x7E, 0x3E, 0x1E, 0x0E, 0x06, 0x02, 0x00,
		0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01};

	static unsigned char fillLast[8] = {0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};
	static unsigned char fillFirst[8] = {0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01};

	const unsigned char b1 = static_cast<unsigned char>(x1 & 7);
	const unsigned char b2 = static_cast<unsigned char>(x2 & 7);

	unsigned char *px1 = buffer + (x1 >> 3);
	unsigned char *px2 = buffer + (x2 >> 3);

	// left pixel = MSB
	// right pixel = LSB
	switch (polarity)
	{
	case DARK: // plot line with set bits
		// fill in the pixels at the byte x1, and x2 occupy.
		if (px1 == px2)
		{ // x1 and x2 occupy the same  byte
			*px1 |= fillSingle[b1 + (b2 << 3)];
		}
		else
		{ // x1 and x2 occupy different bytes
			*px1 |= fillFirst[b1];
			*px2 |= fillLast[b2];
			// fill only the whole bytes in buffer between x1 and x2
			px1++;
			memset(px1, 0xFF, (px2 - px1));
		}
		break;

	case CLEAR: // plot line with cleared bits

		if (px1 == px2) // fill in the pixels at the byte x1, and x2 occupy.
		{				// x1 and x2 occupy the same  byte
			*px1 &= ~fillSingle[b1 + (b2 << 3)];
		}
		else
		{ // x1 and x2 occupy different bytes
			*px1 &= ~fillFirst[b1];
			*px2 &= ~fillLast[b2];
			// fill only the whole bytes in buffer between x1 and x2
			px1++;
			memset(px1, 0x0, (px2 - px1));
		}
		break;

	case XOR: // invert the pixels
		// fill in the pixels at the byte x1, and x2 occupy.
		if (px1 == px2)
		{ // x1 and x2 occupy the same  byte
			*px1 ^= fillSingle[b1 + (b2 << 3)];
		}
		else
		{ // x1 and x2 occupy different bytes
			*px1 ^= fillFirst[b1];
			*px2 ^= fillLast[b2];
			// XOR only the whole bytes in buffer between x1 and x2 (exclusive)
			px1++;
			while (px1 < px2)
			{
				*px1 ^= 0xFF;
				px1++;
			}
		}
		break;
	}

} // end HorizontalLine()

std::string normalizePathToDoubleBackslashes(const std::string &path)
{
	// Заменяет все вхождения /, //, \, \\ (один или несколько подряд) на двойной обратный слэш
	std::regex allSlashes(R"([\\/]+)");
	return std::regex_replace(path, allSlashes, R"(\\)");
}

//**********************************************************
extern "C" __declspec(dllexport) int __stdcall processGerber(
	double imageDPI,					// Разрешение изображения в DPI (точках на дюйм)
	bool optGrowUnitsMillimeters,		// Единицы измерения для optGrowSize: true - миллиметры, false - пиксели
	bool optBoarderUnitsMillimeters,	// Единицы измерения для optBoarder: true - миллиметры, false - пиксели
	double optBoarder,					// Размер отступа от края изображения (mm/pix)
	bool optInvertPolarity,				// Инвертировать полярность изображения
	double optGrowSize,					// Значение увеличения размера объектов (mm/pix) 
	double optScaleX,					// Масштаб по оси X
	double optScaleY,					// Масштаб по оси Y
	const char *outputFilename,			// Имя выходного файла
	const char *inputFilename			// Имя входного файла Gerber
	)
{
	try
	{
		clock_t start_time = std::clock(); // Начало измерения времени

		if (!outputFilename || !inputFilename)
		{
			return ERROR_INVALID_PARAMETERS; // код ошибки: некорректные параметры
		}

		// Нормализация путей
		std::string normalizedOutputFilename = normalizePathToDoubleBackslashes(outputFilename);
		std::string normalizedInputFilename = normalizePathToDoubleBackslashes(inputFilename);

		if (normalizedOutputFilename.empty() || normalizedInputFilename.empty())
		{
			return ERROR_INVALID_PARAMETERS; // код ошибки: некорректные параметры
		}

		std::ostringstream normalizedOutputBytes, normalizedInputBytes;
		for (size_t i = 0; i < normalizedOutputFilename.size(); i++)
			normalizedOutputBytes << std::hex << static_cast<int>(static_cast<unsigned char>(normalizedOutputFilename[i])) << " ";
		for (size_t i = 0; i < normalizedInputFilename.size(); i++)
			normalizedInputBytes << std::hex << static_cast<int>(static_cast<unsigned char>(normalizedInputFilename[i])) << " ";

		// Проверка входного файла
		FILE *file = fopen(normalizedInputFilename.c_str(), "rb");
		if (file == NULL)
		{
			return ERROR_FILE_OPEN_FAILED;
		}

		if (normalizedOutputFilename.empty())
			normalizedOutputFilename = normalizedInputFilename + ".tiff";

		std::ostringstream gerberParamsLog;
		gerberParamsLog << "file: " << normalizedInputFilename << "\n"
						<< "imageDPI: " << imageDPI << "\n"
						<< "optGrowSize: " << optGrowSize << "\n"
						<< "optScaleX: " << optScaleX << "\n"
						<< "optScaleY: " << optScaleY;

		std::list<Gerber *> gerbers;
		try
		{

			gerbers.push_back(new Gerber(file, imageDPI, optGrowSize, optScaleX, optScaleY));
		}
		catch (const std::exception &e)
		{

			fclose(file);
			return ERROR_GERBER_PROCESSING; // код ошибки: ошибка обработки Gerber
		}
		catch (...)
		{

			fclose(file);
			return ERROR_GERBER_PROCESSING; // код ошибки: ошибка обработки Gerber
		}
		fclose(file);

		// Вывод предупреждений
		for (std::size_t i = 0; i < gerbers.back()->messages.size(); i++)
		{
			if (i == 0)
				std::cout << "\n";
			std::cout << "(" << normalizedInputFilename << ") " << gerbers.back()->messages[i] << std::endl;
		}

		// Вывод ошибок
		if (gerbers.back()->isError)
		{

			return ERROR_GERBER_PROCESSING; // код ошибки: ошибка обработки Gerber
		}

		// Создание таблицы для подсчета битов
		for (int i = 0; i < 256; i++)
		{
			nbitsTable[i] = 0;
			if ((i & 0x01))
				nbitsTable[i]++;
			if ((i & 0x02))
				nbitsTable[i]++;
			if ((i & 0x04))
				nbitsTable[i]++;
			if ((i & 0x08))
				nbitsTable[i]++;
			if ((i & 0x10))
			nbitsTable[i]++;
			if ((i & 0x20))
			nbitsTable[i]++;
			if ((i & 0x40))
				nbitsTable[i]++;
			if ((i & 0x80))
				nbitsTable[i]++;
		}

		if (imageDPI < 1 || optBoarder < 0)
		{
			std::cerr << "Error: invalid DPI or border parameters." << std::endl;
			return ERROR_INVALID_PARAMETERS; // код ошибки: некорректные параметры
		}

		// Корректировка единиц измерения
		if (optGrowUnitsMillimeters)
			optGrowSize *= imageDPI / 25.4;
		if (optBoarderUnitsMillimeters)
			optBoarder *= imageDPI / 25.4;

		int miny = INT_MAX; // holds min and max dimentions of the occupied gerber images (superimposed)
		int minx = INT_MAX;
		int maxy = INT_MIN;
		int maxx = INT_MIN;
		std::list<Polygon> globalPolygons; // Contains polygons created by the all gerbers.

		for (std::list<Gerber *>::iterator it = gerbers.begin(); it != gerbers.end(); it++)
		{
			globalPolygons.merge((*it)->polygons);
		}

		if (globalPolygons.size() == 0)
		{ // Если нечего рисовать, завершить с ошибкой

			return ERROR_NO_IMAGE; // код ошибки: нет изображения
		}

		// find extreme (x,y) coordinates for all polygons
		for (std::list<Polygon>::iterator it = globalPolygons.begin(); it != globalPolygons.end(); it++)
		{
			if (minx > it->pixelMinX)
				minx = it->pixelMinX;
			if (maxx < it->pixelMaxX)
				maxx = it->pixelMaxX;
			if (miny > it->pixelMinY)
				miny = it->pixelMinY;
			if (maxy < it->pixelMaxY)
				maxy = it->pixelMaxY;
		}

		// use the world coordinate limits <maxx, minx, maxx, minx> to determine the
		// sized  of the bitmap buffer to allocate for drawing the image
		// always make image imageWidth multiple of 8
		unsigned imageWidth = unsigned(std::ceil((maxx - minx) + 2 * optBoarder + 1));
		unsigned imageHeight = unsigned(std::ceil((maxy - miny) + 2 * optBoarder + 1));
		int xOffset = int(std::floor(optBoarder));
		int yOffset = xOffset;

		// ВАЖНО: Не инвертируем полярность из файла Gerber, вместо этого используем напрямую
		bool isPolarityDark = gerbers.front()->imagePolarityDark; 

		// Применяем invert только если это явно запрошено пользователем
		if (optInvertPolarity) {
			isPolarityDark = !isPolarityDark;
		}
		
		if (rowsPerStrip > static_cast<unsigned>(imageHeight) || rowsPerStrip == 0)
		{
			rowsPerStrip = imageHeight;
		}

		// Convert output filename to lowercase for extension check
		std::string outputLower = normalizedOutputFilename;
		std::transform(outputLower.begin(), outputLower.end(), outputLower.begin(), ::tolower);

		// Check if output should be BMP
		bool isBMP = (outputLower.find(".bmp") != std::string::npos);

		if (isBMP)
		{

			// Create BMP using EasyBMP
			BMP output;
			output.SetSize(imageWidth, imageHeight);
			output.SetBitDepth(1); // Monochrome BMP

			// Set DPI information
			output.SetDPI(int(imageDPI), int(imageDPI));

			// Цвета для пикселей
			RGBApixel white;
			white.Red = white.Green = white.Blue = white.Alpha = 255;
			RGBApixel black;
			black.Red = black.Green = black.Blue = black.Alpha = 0;

			// В GERBER файлах фон всегда противоположен полярности полигона:
			// - Для DARK полигонов (основной слой платы) фон всегда БЕЛЫЙ
			// - Для CLEAR полигонов (прорези и отверстия) фон всегда ЧЕРНЫЙ
			// НО в BMP форматe белый = 255, черный = 0, что инвертировано по отношению к TIFF
			// Очищаем холст с учетом запрошенной полярности
			RGBApixel bgColor = isPolarityDark ? white : black;
			for (unsigned y = 0; y < imageHeight; y++)
			{
				for (unsigned x = 0; x < imageWidth; x++)
				{
					output.SetPixel(x, y, bgColor);
				}
			}

			// Draw polygons
			xOffset -= minx;
			// Рисуем полигоны строго последовательно, в том порядке, как они были определены в Gerber
			// Это обеспечит правильную очередность наложения и соблюдение полярности
			for (std::list<Polygon>::iterator it = globalPolygons.begin(); it != globalPolygons.end(); it++)
			{
				Polarity_t pol = it->polarity;
				
				// Преобразуем полярность с учетом инверсии только если это включено пользователем
				if (optInvertPolarity) {
					if (pol == DARK) pol = CLEAR;
					else if (pol == CLEAR) pol = DARK;
				}

				int sliCount;
				int *sliTable;
				for (int y = it->pixelMinY; y <= it->pixelMaxY; y++)
				{
					it->getNextLineX1X2Pairs(sliTable, sliCount);
					for (int i = 0; i < sliCount; i += 2)
					{
						for (int x = xOffset + it->pixelOffsetX + sliTable[i];
							 x <= xOffset + it->pixelOffsetX + sliTable[i + 1];
							 x++)
						{
							if (x >= 0 && x < (int)imageWidth &&
								(y - miny + yOffset) >= 0 &&
								(y - miny + yOffset) < (int)imageHeight)
							{
								output.SetPixel(x, y - miny + yOffset,
												(pol == DARK) ? black : white);
							}
						}
					}
				}
			}

			// Write BMP file
			if (!output.WriteToFile(normalizedOutputFilename.c_str()))
			{

				return ERROR_OUTPUT_FILE_CREATION; // код ошибки: ошибка создания выходного файла
			}
		}
		else
		{

			// Default to TIFF if not BMP
			// Initialise TIFF with the libtiff library
			TIFF *tif = TIFFOpen(normalizedOutputFilename.c_str(), "w");
			if (tif == NULL)
			{

				return ERROR_OUTPUT_FILE_CREATION; // код ошибки: ошибка создания выходного файла
			}

			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);	// avoid errors, dispite TIFF spec saying this tag not needed in monochrome images.
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE); // white pixels are zero
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_CCITTRLE);	// use CCITT Group 3 1-Dimensional Modified Huffman run length encoding
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, imageHeight);
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, imageWidth);
			TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, 2); // Resulution unit in inches
			TIFFSetField(tif, TIFFTAG_YRESOLUTION, imageDPI);
			TIFFSetField(tif, TIFFTAG_XRESOLUTION, imageDPI);
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rowsPerStrip);

			//
			// Calculate size and allocate buffer for drawing. The image will be rendered sequential blocks of
			// imageWidth wide by rowsPerStrip high.
			//
			bytesPerScanline = ((imageWidth + 7) >> 3);
			bitmapBytes = bytesPerScanline * rowsPerStrip;
			bitmap = (unsigned char *)std::malloc(bitmapBytes);
			if (bitmap == 0)
			{
				std::cerr << "Error: memory allocation failed." << std::endl;
				return ERROR_MEMORY_ALLOCATION; // код ошибки: ошибка выделения памяти
			}

			//-----------------------------------------------------------------------
			// Draw polygons
			//-----------------------------------------------------------------------
			xOffset -= minx;

			int stripCounter = 0;
			std::list<Polygon>::iterator polyIterator = globalPolygons.begin();
			std::list<PolygonReference> activePolys;

			// The bitmap will be divided into strips, of height rowsPerStrip.
			// Polygons are plotted for each strip consecutively in a loop, where the strip y coordinate equals ystart
			for (int ystart = miny - yOffset; ystart < (int(imageHeight) + miny - yOffset); ystart += rowsPerStrip)
			{
				// blank entire strip buffer, set pixels on/off depending on polarity of the 1st Gerber.
				if (isPolarityDark)
					memset(bitmap, 0x00, bitmapBytes);
				else
					memset(bitmap, 0xff, bitmapBytes);

				unsigned char *bufferLine = bitmap;

				// Loop over each row of the strip and fill with horizontal lines from the polygon raster data.
				// All polygon are sorted in the list globalPolygons. Iterating each polygon for raster data will guarantee no missing lines.
				for (int y = ystart; (y - ystart) < static_cast<int>(rowsPerStrip) && (y <= maxy); y++, bufferLine += bytesPerScanline)
				{
					while (polyIterator != globalPolygons.end() && y == (polyIterator->pixelMinY))
					{
						activePolys.push_back(PolygonReference());
						activePolys.back().polygon = &(*polyIterator);
						activePolys.sort();
						polyIterator++;
					}

					for (std::list<PolygonReference>::iterator it = activePolys.begin(); it != activePolys.end();)
					{
						if (y > it->polygon->pixelMaxY)
						{
							it = activePolys.erase(it);
							continue;
						}
						int sliCount;
						int *sliTable;
						it->polygon->getNextLineX1X2Pairs(sliTable, sliCount);

						Polarity_t pol = it->polygon->polarity;
						if ((pol == DARK) && !isPolarityDark)
							pol = CLEAR;
						if ((pol == CLEAR) && isPolarityDark)
							pol = DARK;

						for (int i = 0; i < sliCount; i += 2)
						{
							horizontalLine(xOffset + it->polygon->pixelOffsetX + sliTable[i],
										   xOffset + it->polygon->pixelOffsetX + sliTable[i + 1],
										   bufferLine, pol);
						}
						it++;
					}
				}

				//
				// Write strip buffer to TIFF
				//
				unsigned lines = std::min(rowsPerStrip, imageHeight - rowsPerStrip * stripCounter);
				TIFFWriteEncodedStrip(tif, stripCounter++, bitmap, bytesPerScanline * lines);
			}
			TIFFClose(tif);
			std::free(bitmap);
		}

		double elapsed_time = static_cast<double>(std::clock() - start_time) / CLOCKS_PER_SEC;
		std::cout << "Processing time: " << elapsed_time << " seconds." << std::endl;

		return NO_ERROR;
	}
	catch (const std::exception &e)
	{
		return ERROR_UNKNOWN; // код ошибки: неизвестная ошибка
	}
	catch (...)
	{
		return ERROR_UNKNOWN; // код ошибки: неизвестная ошибка
	}
}

extern "C" __declspec(dllexport) int __stdcall processGerberJSON(const char *jsonParams)
{
	try
	{
		// Десериализация JSON в параметры
		json j = json::parse(jsonParams);

		double imageDPI = j.value("imageDPI", 2400.0);
		bool optGrowUnitsMillimeters = j.value("optGrowUnitsMillimeters", false);
		bool optBoarderUnitsMillimeters = j.value("optBoarderUnitsMillimeters", false);
		double optBoarder = j.value("optBoarder", 0.0);
		bool optInvertPolarity = j.value("optInvertPolarity", false);		
		double optGrowSize = j.value("optGrowSize", 0.0);
		double optScaleX = j.value("optScaleX", 1.0);
		double optScaleY = j.value("optScaleY", 1.0);
		std::string outputFilename = j.value("outputFilename", "");
		std::string inputFilename = j.value("inputFilename", "");

		// Вызов основного процесса
		return processGerber(
			imageDPI,
			optGrowUnitsMillimeters,
			optBoarderUnitsMillimeters,
			optBoarder,
			optInvertPolarity,
			optGrowSize,
			optScaleX,
			optScaleY,
			outputFilename.c_str(),
			inputFilename.c_str());
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error processing JSON: " << e.what() << std::endl;
		return ERROR_JSON_PROCESSING; // код ошибки: ошибка обработки JSON
	}
}

//**********************************************************
// Функция для обработки Excellon файлов (формат сверловки плат)
//**********************************************************
extern "C" __declspec(dllexport) int __stdcall processExcellon(
    double imageDPI,            // Разрешение изображения в DPI (точках на дюйм)
    bool unitsMillimeters,      // Единицы измерения: true - миллиметры, false - пиксели
    double optBoarder,          // Размер отступа от края изображения
    bool optInvertPolarity,     // Инвертировать полярность изображения
    double optGrowSize,         // Значение увеличения размера объектов
    double optScaleX,           // Масштаб по оси X
    double optScaleY,           // Масштаб по оси Y
    bool uniformDrills,         // Использовать одинаковый диаметр для всех отверстий	
    bool uniformDrillsMillimeters, // Для uniformDrillDiameter: true - миллиметры, false - дюймы
    double uniformDrillDiameter,// Значение диаметра для всех отверстий (если uniformDrills=true)
    const char *outputFilename, // Имя выходного файла
    const char *inputFilename   // Имя входного файла Excellon
)
{
	try
	{
		clock_t start_time = std::clock(); // Начало измерения времени

		if (!outputFilename || !inputFilename)
		{
			return ERROR_INVALID_PARAMETERS; // код ошибки: некорректные параметры
		}

		// Нормализация путей
		std::string normalizedOutputFilename = normalizePathToDoubleBackslashes(outputFilename);
		std::string normalizedInputFilename = normalizePathToDoubleBackslashes(inputFilename);

		if (normalizedOutputFilename.empty() || normalizedInputFilename.empty())
		{
			return ERROR_INVALID_PARAMETERS; // код ошибки: некорректные параметры
		}

		// Проверка входного файла
		FILE *file = fopen(normalizedInputFilename.c_str(), "rb");
		if (file == NULL)
		{
			return ERROR_FILE_OPEN_FAILED;
		}

		// Проверяем, является ли файл форматом Excellon/Drill
		if (!Excellon::isExcellonFile(file)) 
		{
			fclose(file);
			std::cerr << "Error: The file is not recognized as an Excellon/Drill format." << std::endl;
			return ERROR_EXCELLON_PROCESSING; // код ошибки: ошибка обработки Excellon
		}

		if (normalizedOutputFilename.empty())
			normalizedOutputFilename = normalizedInputFilename + ".tiff";

		std::ostringstream excellonParamsLog;
		excellonParamsLog << "file: " << normalizedInputFilename << "\n"
						<< "imageDPI: " << imageDPI << "\n"
						<< "optGrowSize: " << optGrowSize << "\n"
						<< "optScaleX: " << optScaleX << "\n"
						<< "optScaleY: " << optScaleY;

		if (uniformDrills) {
			excellonParamsLog << "\n" << "uniformDrillDiameter: " << uniformDrillDiameter;
		}

		// Корректировка единиц измерения (используем один параметр)
		if (unitsMillimeters) {
			optGrowSize *= imageDPI / 25.4;  // Преобразование мм в пиксели
			optBoarder *= imageDPI / 25.4;   // Преобразование мм в пиксели
			// НЕ преобразуем uniformDrillDiameter здесь, это будет сделано внутри класса Excellon
		}

		// Создаем объект Excellon для обработки файла сверловки
		std::list<Excellon *> excellonList;
		try
		{
			excellonList.push_back(new Excellon(file, imageDPI, optGrowSize, optScaleX, optScaleY, 
                                              uniformDrills ? uniformDrillDiameter : 0,
                                              uniformDrillsMillimeters)); // Передаем флаг единиц измерения
		}
		catch (const std::exception &e)
		{
			fclose(file);
			return ERROR_EXCELLON_PROCESSING; // код ошибки: ошибка обработки Excellon
		}
		catch (...)
		{
			fclose(file);
			return ERROR_EXCELLON_PROCESSING; // код ошибки: ошибка обработки Excellon
		}
		fclose(file);

		// Вывод предупреждений
		for (std::size_t i = 0; i < excellonList.back()->messages.size(); i++)
		{
			if (i == 0)
				std::cout << "\n";
			std::cout << "(" << normalizedInputFilename << ") " << excellonList.back()->messages[i] << std::endl;
		}

		// Вывод ошибок
		if (excellonList.back()->isError)
		{
			return ERROR_EXCELLON_PROCESSING; // код ошибки: ошибка обработки Excellon
		}

		// Создание таблицы для подсчета битов (если ещё не создана)
		static bool nbitsTableInitialized = false;
		if (!nbitsTableInitialized)
		{
			for (int i = 0; i < 256; i++)
			{
				nbitsTable[i] = 0;
				if ((i & 0x01)) nbitsTable[i]++;
				if ((i & 0x02)) nbitsTable[i]++;
				if ((i & 0x04)) nbitsTable[i]++;
				if ((i & 0x08)) nbitsTable[i]++;
				if ((i & 0x10)) nbitsTable[i]++;
				if ((i & 0x20)) nbitsTable[i]++;
				if ((i & 0x40)) nbitsTable[i]++;
				if ((i & 0x80)) nbitsTable[i]++;
			}
			nbitsTableInitialized = true;
		}

		if (imageDPI < 1 || optBoarder < 0)
		{
			std::cerr << "Error: invalid DPI or border parameters." << std::endl;
			return ERROR_INVALID_PARAMETERS; // код ошибки: некорректные параметры
		}

		int miny = INT_MAX; // Минимальные и максимальные координаты для отверстий
		int minx = INT_MAX;
		int maxy = INT_MIN;
		int maxx = INT_MIN;
		std::list<Polygon> globalPolygons; // Полигоны, созданные для всех отверстий

		// Объединение полигонов из всех Excellon файлов
		for (std::list<Excellon *>::iterator it = excellonList.begin(); it != excellonList.end(); it++)
		{
			globalPolygons.merge((*it)->polygons);
		}

		if (globalPolygons.size() == 0)
		{ // Если нет полигонов, завершаем с ошибкой
			return ERROR_NO_IMAGE; // код ошибки: нет изображения
		}

		// Находим крайние координаты для всех полигонов
		for (std::list<Polygon>::iterator it = globalPolygons.begin(); it != globalPolygons.end(); it++)
		{
			it->initialise(); // Инициализируем полигон, если ещё не инициализирован
			
			if (minx > it->pixelMinX)
				minx = it->pixelMinX;
			if (maxx < it->pixelMaxX)
				maxx = it->pixelMaxX;
			if (miny > it->pixelMinY)
				miny = it->pixelMinY;
			if (maxy < it->pixelMaxY)
				maxy = it->pixelMaxY;
		}

		// Определяем размеры области изображения
		unsigned imageWidth = unsigned(std::ceil((maxx - minx) + 2 * optBoarder + 1));
		unsigned imageHeight = unsigned(std::ceil((maxy - miny) + 2 * optBoarder + 1));
		int xOffset = int(std::floor(optBoarder));
		int yOffset = xOffset;

		// Получаем базовую полярность из настроек Excellon
		bool isPolarityDark = true;
		isPolarityDark = (optInvertPolarity ^ excellonList.front()->imagePolarityDark);
		if (rowsPerStrip > static_cast<unsigned>(imageHeight) || rowsPerStrip == 0)
		{
			rowsPerStrip = imageHeight;
		}

		// Проверяем формат выходного файла (BMP или TIFF)
		std::string outputLower = normalizedOutputFilename;
		std::transform(outputLower.begin(), outputLower.end(), outputLower.begin(), ::tolower);
		bool isBMP = (outputLower.find(".bmp") != std::string::npos);

		if (isBMP)
		{
			// Создаем BMP с использованием EasyBMP
			BMP output;
			output.SetSize(imageWidth, imageHeight);
			output.SetBitDepth(1); // Монохромный BMP

			// Устанавливаем DPI информацию
			output.SetDPI(int(imageDPI), int(imageDPI));

			// Цвета для пикселей
			RGBApixel white;
			white.Red = white.Green = white.Blue = white.Alpha = 255;
			RGBApixel black;
			black.Red = black.Green = black.Blue = black.Alpha = 0;

			// В GERBER файлах фон всегда противоположен полярности полигона:
			// - Для DARK полигонов (основной слой платы) фон всегда БЕЛЫЙ
			// - Для CLEAR полигонов (прорези и отверстия) фон всегда ЧЕРНЫЙ
			// НО в BMP форматe белый = 255, черный = 0, что инвертировано по отношению к TIFF
			// Очищаем холст с учетом запрошенной полярности
			RGBApixel bgColor = isPolarityDark ? white : black;
			for (unsigned y = 0; y < imageHeight; y++)
			{
				for (unsigned x = 0; x < imageWidth; x++)
				{
					output.SetPixel(x, y, bgColor);
				}
			}

			// Рисуем полигоны отверстий
			xOffset -= minx;
			for (std::list<Polygon>::iterator it = globalPolygons.begin(); it != globalPolygons.end(); it++)
			{
				Polarity_t pol = it->polarity;
				if ((pol == DARK) && !isPolarityDark)
					pol = CLEAR;
				if ((pol == CLEAR) && isPolarityDark)
				pol = DARK;

				int sliCount;
				int *sliTable;
				for (int y = it->pixelMinY; y <= it->pixelMaxY; y++)
				{
					it->getNextLineX1X2Pairs(sliTable, sliCount);
					for (int i = 0; i < sliCount; i += 2)
					{
						for (int x = xOffset + it->pixelOffsetX + sliTable[i];
							 x <= xOffset + it->pixelOffsetX + sliTable[i + 1];
							 x++)
						{
							if (x >= 0 && x < (int)imageWidth &&
								(y - miny + yOffset) >= 0 &&
								(y - miny + yOffset) < (int)imageHeight)
							{
								// Инвертируем Y-координату для правильного отображения Excellon координат
								// Это необходимо, чтобы соответствовать BMP спецификации, где строки идут снизу вверх
								int invY = imageHeight - 1 - (y - miny + yOffset);
								output.SetPixel(x, invY, (pol == DARK) ? black : white);
							}
						}
					}
				}
			}

			// Записываем BMP файл
			if (!output.WriteToFile(normalizedOutputFilename.c_str()))
			{
				return ERROR_OUTPUT_FILE_CREATION; // код ошибки: ошибка создания выходного файла
			}
		}
		else
		{
			// По умолчанию создаем TIFF файл
			TIFF *tif = TIFFOpen(normalizedOutputFilename.c_str(), "w");
			if (tif == NULL)
			{
				return ERROR_OUTPUT_FILE_CREATION; // код ошибки: ошибка создания выходного файла
			}

			// Настраиваем TIFF параметры
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_CCITTRLE);
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, imageHeight);
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, imageWidth);
			TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, 2); // Единицы измерения - дюймы
			TIFFSetField(tif, TIFFTAG_YRESOLUTION, imageDPI);
			TIFFSetField(tif, TIFFTAG_XRESOLUTION, imageDPI);
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rowsPerStrip);

			// Рассчитываем размер буфера для рисования
			bytesPerScanline = ((imageWidth + 7) >> 3);
			bitmapBytes = bytesPerScanline * rowsPerStrip;
			bitmap = (unsigned char *)std::malloc(bitmapBytes);
			if (bitmap == 0)
			{
				std::cerr << "Error: memory allocation failed." << std::endl;
				return ERROR_MEMORY_ALLOCATION; // код ошибки: ошибка выделения памяти
			}

			// Рисуем полигоны отверстий
			xOffset -= minx;

			int stripCounter = 0;
			std::list<Polygon>::iterator polyIterator = globalPolygons.begin();
			std::list<PolygonReference> activePolys;

			// Проходим по полосам изображения
			for (int ystart = miny - yOffset; ystart < (int(imageHeight) + miny - yOffset); ystart += rowsPerStrip)
			{
				// Очищаем буфер в зависимости от полярности
				if (isPolarityDark)
					memset(bitmap, 0x00, bitmapBytes);
				else
					memset(bitmap, 0xff, bitmapBytes);

				// Проходим по каждой строке в полосе
				for (int y = ystart; (y - ystart) < static_cast<int>(rowsPerStrip) && (y <= maxy); y++)
				{
					// Инвертируем положение строки в буфере (от нижней к верхней)
					// Это необходимо, чтобы соответствовать TIFF спецификации, где строки идут снизу вверх
    				unsigned char *bufferLine = bitmap + (rowsPerStrip - 1 - (y - ystart)) * bytesPerScanline;
					
					while (polyIterator != globalPolygons.end() && y == (polyIterator->pixelMinY))
					{
						activePolys.push_back(PolygonReference());
						activePolys.back().polygon = &(*polyIterator);
						activePolys.sort();
						polyIterator++;
					}

					for (std::list<PolygonReference>::iterator it = activePolys.begin(); it != activePolys.end();)
					{
						if (y > it->polygon->pixelMaxY)
						{
							it = activePolys.erase(it);
							continue;
						}

						int sliCount;
						int *sliTable;
						it->polygon->getNextLineX1X2Pairs(sliTable, sliCount);

						Polarity_t pol = it->polygon->polarity;
						if ((pol == DARK) && !isPolarityDark)
							pol = CLEAR;
						if ((pol == CLEAR) && isPolarityDark)
							pol = DARK;

						for (int i = 0; i < sliCount; i += 2)
						{
							horizontalLine(xOffset + it->polygon->pixelOffsetX + sliTable[i],
										   xOffset + it->polygon->pixelOffsetX + sliTable[i + 1],
										   bufferLine, pol);
						}
						it++;
					}
				}

				// Записываем полосу в TIFF файл
				unsigned lines = std::min(rowsPerStrip, imageHeight - rowsPerStrip * stripCounter);
				TIFFWriteEncodedStrip(tif, stripCounter++, bitmap, bytesPerScanline * lines);
			}

			TIFFClose(tif);
			std::free(bitmap);
		}

		// Освобождаем ресурсы
		for (std::list<Excellon *>::iterator it = excellonList.begin(); it != excellonList.end(); it++)
		{
			delete *it;
		}

		double elapsed_time = static_cast<double>(std::clock() - start_time) / CLOCKS_PER_SEC;
		std::cout << "Processing time: " << elapsed_time << " seconds." << std::endl;

		return NO_ERROR;
	}
	catch (const std::exception &e)
	{
		return ERROR_UNKNOWN; // код ошибки: неизвестная ошибка
	}
	catch (...)
	{
		return ERROR_UNKNOWN; // код ошибки: неизвестная ошибка
	}
}

extern "C" __declspec(dllexport) int __stdcall processExcellonJSON(const char *jsonParams)
{
    try
    {
        // Десериализация JSON в параметры
        json j = json::parse(jsonParams);

        double imageDPI = j.value("imageDPI", 2400.0);
        bool unitsMillimeters = j.value("unitsMillimeters", false);  // Объединенный параметр единиц измерения
        double optBoarder = j.value("optBoarder", 0.0);
        bool optInvertPolarity = j.value("optInvertPolarity", false);
        double optGrowSize = j.value("optGrowSize", 0.0);
        double optScaleX = j.value("optScaleX", 1.0);
        double optScaleY = j.value("optScaleY", 1.0);
        bool uniformDrills = j.value("uniformDrills", false);  // Использовать одинаковый диаметр для всех отверстий		
        bool uniformDrillsMillimeters = j.value("uniformDrillsMillimeters", true);
        double uniformDrillDiameter = j.value("uniformDrillDiameter", 0.5);  // Значение диаметра для всех отверстий
        std::string outputFilename = j.value("outputFilename", "");
        std::string inputFilename = j.value("inputFilename", "");

        // Вызов основного процесса с новыми параметрами
        return processExcellon(
            imageDPI,
            unitsMillimeters,
            optBoarder,
            optInvertPolarity,
            optGrowSize,
            optScaleX,
            optScaleY,
            uniformDrills,			
            uniformDrillsMillimeters,
            uniformDrillDiameter,
            outputFilename.c_str(),
            inputFilename.c_str());
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error processing JSON: " << e.what() << std::endl;
        return ERROR_JSON_PROCESSING; // код ошибки: ошибка обработки JSON
    }
}
