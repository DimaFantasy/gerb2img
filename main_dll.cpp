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
	double imageDPI,
	bool optGrowUnitsMillimeters,
	bool optBoarderUnitsMillimeters,
	double optBoarder,
	bool optInvertPolarity,
	unsigned rowsPerStrip,
	double optGrowSize,
	double optScaleX,
	double optScaleY,
	const char *outputFilename,
	const char *inputFilename)
{
	try
	{
		clock_t start_time = std::clock(); // Начало измерения времени

		if (!outputFilename || !inputFilename)
		{
			return ERROR_INVALID_PARAMETERS; // код ошибки: некорректные параметры
		}

		// Логирование входных параметров
		std::ostringstream paramsLog;
		paramsLog << "Called processGerber with parameters:\n"
				  << "imageDPI: " << imageDPI << "\n"
				  << "optGrowUnitsMillimeters: " << (optGrowUnitsMillimeters ? "true" : "false") << "\n"
				  << "optBoarderUnitsMillimeters: " << (optBoarderUnitsMillimeters ? "true" : "false") << "\n"
				  << "optBoarder: " << optBoarder << "\n"
				  << "optInvertPolarity: " << (optInvertPolarity ? "true" : "false") << "\n"
				  << "rowsPerStrip: " << rowsPerStrip << "\n"
				  << "optGrowSize: " << optGrowSize << "\n"
				  << "optScaleX: " << optScaleX << "\n"
				  << "optScaleY: " << optScaleY << "\n"
				  << "outputFilename: " << (outputFilename ? outputFilename : "null") << "\n"
				  << "inputFilename: " << (inputFilename ? inputFilename : "null");

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

		bool isPolarityDark = true;
		isPolarityDark = (optInvertPolarity ^ gerbers.front()->imagePolarityDark); // polarity is relative to 1st gerber file
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

			// Initialize all pixels based on polarity
			RGBApixel white;
			white.Red = white.Green = white.Blue = white.Alpha = 255;
			RGBApixel black;
			black.Red = black.Green = black.Blue = black.Alpha = 0;

			// Fill background based on polarity
			for (unsigned y = 0; y < imageHeight; y++)
			{
				for (unsigned x = 0; x < imageWidth; x++)
				{
					output.SetPixel(x, y, isPolarityDark ? white : black);
				}
			}

			// Draw polygons
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
		unsigned rowsPerStrip = j.value("rowsPerStrip", 512);
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
			rowsPerStrip,
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
