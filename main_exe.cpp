// This file is distributed under the terms of the GNU General Public License v3.

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
#include <tiffio.h>
#include <stdarg.h>
#include <string.h>
#include "config.h"

#include "polygon.h"
#include "apertures.h"
#include "gerber.h"

unsigned char nbitsTable[256];

const char *help_message =
	"Gerber RS-274X file to raster graphics converter \n"
	"\n"
	"Usage: gerb2img [OPTIONS] [file1] [file2]...\n"
	"\n"
	"Output control: \n"
	"  -a, --area           Show total dark area of TIFF in square centimeters.\n"
	"  -q, --quiet          Suppress warnings and non critical messages.\n"
	"  -t                   Test only. Process Gerber file without writing TIFF.\n"
	"  -o, --output=FILE    Set name of output TIFF to FILE. If gerber-file is\n"
	"                       specified then default is <file1>.tiff\n"
	"                       This option is required when no gerber-file specified.\n"
	"  -v                   Verbose mode, display information while processing\n"
	"                       multiple -v increases verbosity. Disables --quiet\n"
	"  --help               This help screen\n"
	"\n"
	"Image options: \n"
	"  --boarder-pixels=X   Add a boarder of X pixels around image. Default 0\n"
	"  -b, --boarder-mm=X   same as --boarder except X is in millimeters\n"
	"  -p, --dpi=X          Number of dots per inch X. Default 2400\n"
	"  -n, --negative       Negate image polarity\n"
	"  --grow-pixels=X      Expand perimeter of all aperture features by X pixels.\n"
	"                       Negative values shrink. Fractional pixels allowed.\n"
	"  --grow-mm=X          Same as --grow-pixels except X is in unit millimeters.\n"
	"  --strip-rows=N       Specify N rows per strip in TIFF. Default 512\n"
	"  --scale-y=FACTOR     Scale image in Y axis by FACTOR. Default 1\n"
	"  --scale-x=FACTOR     Scale image in X axis by FACTOR. Default 1\n"
	"\n"
	"Where file1 file2... are gerber files rendered as overlays to a single bitmap.\n"
	"Standard input is read if no gerber files specified and --output is specified.\n"
	"Output bitmap is compressed monochrome TIFF"


void show_interval(const char *msg = "")
{
	static clock_t start_clock = clock();
	double cpu_time_used = ((double)(clock() - start_clock)) / CLOCKS_PER_SEC;
	std::printf("time: %.3f s (%s)\n", cpu_time_used, msg);
	start_clock = clock();
}

void error(const std::string &message)
{
	std::cerr << "gerb2img: error: " << message << std::endl;
	std::exit(1);
}

//***************************************************
// Global variables of plotting parameters
//**************************************************
double imageDPI = 2400;
double optRotation;
bool optGrowUnitsMillimeters = false;
bool optBoarderUnitsMillimeters = false;
double optBoarder = 0;
bool optInvertPolarity = false;
bool optTestOnly = false;
int optVerbose = 0;
unsigned rowsPerStrip = 512;
bool optShowArea = false;
bool optQuiet = false;
double total_area_cmsq = 0;
double optGrowSize = 0;
double optScaleX = 1;
double optScaleY = 1;
unsigned int bytesPerScanline;
unsigned int bitmapBytes;
unsigned char *bitmap;

//***********************************************************

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

	const unsigned char b1 = static_cast<unsigned char>(x1 & 7); // Исправлено: char -> unsigned char
	const unsigned char b2 = static_cast<unsigned char>(x2 & 7); // Исправлено: char -> unsigned char

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

//---------------------------------------------------------------------------------
int main(int argc, char **argv)
{
	clock_t start_clock = clock();
	std::string inputfile;
	std::string outputFilename;

	// create 256 element look up table for fast retrieve of
	// number of bits set in numbers 0 to 255.
	// used for calculating positive pixels in the drawn bitmap

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

	//
	// parse the command line
	//
	while (1)
	{
		static struct option long_options[] =
			{
				// These options don't set a flag.
				// We distinguish them by their indices.
				{"dpi", LOCAL_REQUIRED_ARGUMENT, 0, 'p'},
				{"output", LOCAL_REQUIRED_ARGUMENT, 0, 'o'},
				{"negative", LOCAL_NO_ARGUMENT, 0, 'n'},
				{"area", LOCAL_NO_ARGUMENT, 0, 'a'},
				{"test", LOCAL_NO_ARGUMENT, 0, 't'},
				{"quiet", LOCAL_NO_ARGUMENT, 0, 'q'},
				{"verbose", LOCAL_NO_ARGUMENT, 0, 'v'},
				{"scale-x", LOCAL_REQUIRED_ARGUMENT, 0, 1},
				{"scale-y", LOCAL_REQUIRED_ARGUMENT, 0, 2},
				{"help", LOCAL_NO_ARGUMENT, 0, 3},
				{"grow-pixels", LOCAL_REQUIRED_ARGUMENT, 0, 4},
				{"grow-mm", LOCAL_REQUIRED_ARGUMENT, 0, 5},
				{"strip-rows", LOCAL_REQUIRED_ARGUMENT, 0, 6},
				{"boarder-mm", LOCAL_REQUIRED_ARGUMENT, 0, 'b'},
				{"boarder-pixels", LOCAL_REQUIRED_ARGUMENT, 0, 7},
				{"rotation", LOCAL_REQUIRED_ARGUMENT, 0, 8},
				{0, 0, 0, 0}};
		// getopt_long stores the option index here.
		int option_index = 0;

		int c = getopt_long(argc, argv, "G:b:o:p:atvnq",
							long_options, &option_index);

		if (c == EOF)
			break;

		switch (c)
		{

		case 8:
			optRotation = atof(optarg);
			break;
		case 7:
			optBoarder = atof(optarg);
			optBoarderUnitsMillimeters = false;
			break;
		case 6:
			rowsPerStrip = atoi(optarg);
			break;
		case 5:
			optGrowSize = atof(optarg);
			optGrowUnitsMillimeters = true;
			break;
		case 4:
			optGrowSize = atof(optarg);
			optGrowUnitsMillimeters = false;
			break;
		case 3:
			fprintf(stdout, "%s", help_message);
			exit(0);
		case 2:
			optScaleY = atof(optarg);
			break;
		case 1:
			optScaleX = atof(optarg);
			break;
		case 'p':
			imageDPI = atof(optarg);
			break;
		case 'b':
			optBoarder = atof(optarg);
			optBoarderUnitsMillimeters = true;
			break;
		case 'n':
			optInvertPolarity = true;
			break;
		case 't':
			optTestOnly = true;
			break;
		case 'a':
			optShowArea = true;
			break;
		case 'v':
			optVerbose++;
			break;
		case 'q':
			optQuiet = true;
			break;
		case 'o':
			outputFilename = optarg;
			break;
		case '?':
		case ':':
			fprintf(stderr, "Try 'gerb2img --help' for more information.\n");
			return 1;
			break;
		}
	}

	if (optVerbose > 0)
		optQuiet = false; // if user wants verbose, then cancel quiet option

	if (imageDPI < 1)
		error(std::string("DPI setting must be >= 1"));
	if (optBoarder < 0)
		error(std::string("boarder setting must be >= 0"));

	// correct the units for some options
	if (optGrowUnitsMillimeters)
		optGrowSize *= imageDPI / 25.4;
	if (optBoarderUnitsMillimeters)
		optBoarder *= imageDPI / 25.4;

	std::list<Gerber *> gerbers; // pointer to the list of Gerber object

	bool isStandardInput = false;
	if (optind == argc)
		isStandardInput = true;

	int first_optind = optind;

	for (; optind < argc || isStandardInput; optind++)
	{
		FILE *file = 0;
		if (isStandardInput)
		{
			file = stdin;
			if (!optTestOnly && outputFilename.empty())
			{
				std::cerr << "no output or input file specified.\n"
						  << "Try 'gerb2img --help' for more information.\n";
				return 1;
			}
		}
		else
		{
			inputfile = argv[optind];
			if (outputFilename.empty())
				outputFilename = inputfile + ".tiff";
			file = fopen(argv[optind], "rb");
			if (file == NULL)
				error(std::string("cannot open input file ") + inputfile);
			if (!optQuiet)
			{
				if (optind == first_optind)
					std::cout << "gerb2img: ";
				if (!optQuiet && optind > first_optind)
					std::cout << "+ ";
				std::cout << inputfile << " " << std::flush;
			}
		}

		gerbers.push_back(new Gerber(file, imageDPI, optGrowSize, optScaleX, optScaleY));

		if (!isStandardInput)
			fclose(file);

		// print all warning messages
		for (size_t i = 0; i < gerbers.back()->messages.size() && !optQuiet; i++)
		{
			if (i == 0)
				std::cout << "\n";
			std::cout << "(" << inputfile << ") " << gerbers.back()->messages[i] << std::endl; // print messages if any
		}
		// print error messages and abort
		if (gerbers.back()->isError)
		{
			std::cout << "\n(" << inputfile << ") " << gerbers.back()->errorMessage.str() << std::endl;
			return 1; // exit program
		}
		if (isStandardInput)
			break;
	}

	if (!optTestOnly && !optQuiet)
		std::cout << "-> " << outputFilename;
	if (!optQuiet)
		std::cout << std::endl;

	int miny = INT_MAX; // holds min and max dimentions of the occupied gerber images (superimposed)
	int minx = INT_MAX;
	int maxy = INT_MIN;
	int maxx = INT_MIN;
	std::list<Polygon> globalPolygons; // Contains polygons created by the all gerbers.

	// group all the polygons
	for (std::list<Gerber *>::iterator it = gerbers.begin(); it != gerbers.end(); it++)
	{
		globalPolygons.merge((*it)->polygons);
	}

	//    for (int i=0; i < 100; i++)
	//    {
	//    	double radius = 50.0 * rand() / double(RAND_MAX);
	//    	double x0  = 2000.0 * rand() / double(RAND_MAX);
	//    	double y0  = 2000.0 * rand() / double(RAND_MAX);
	//        globalPolygons.push_back(Polygon() );
	//        globalPolygons.back().number = 1;
	//        globalPolygons.back().vdata->add( 0,0);
	//        globalPolygons.back().vdata->add(100,0);
	//        globalPolygons.back().vdata->add(100,50);
	//        globalPolygons.back().vdata->add( 0,50);
	//        globalPolygons.back().vdata->rotate(222 *M_PI/180);
	//        globalPolygons.back().vdata->initialise();
	//        globalPolygons.back().initialise();
	//    }

	//    globalPolygons.sort();
	//        globalPolygons.push_back(Polygon() );
	//        globalPolygons.back().addArc(0, 2*M_PI, 10, 0, 0, false );
	//        globalPolygons.back().number = 0;
	//        globalPolygons.back().initialise();

	if (globalPolygons.size() == 0) // If nothing to draw then abort with error
		error("no image");

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
	unsigned imageWidth = unsigned(ceil((maxx - minx) + 2 * optBoarder + 1));
	unsigned imageHeight = unsigned(ceil((maxy - miny) + 2 * optBoarder + 1));
	int xOffset = int(floor(optBoarder));
	int yOffset = xOffset;

	bool isPolarityDark = true;
	isPolarityDark = (optInvertPolarity ^ gerbers.front()->imagePolarityDark); // polarity is relative to 1st gerber file
	if (rowsPerStrip > unsigned(imageHeight) || rowsPerStrip == 0)
		rowsPerStrip = imageHeight;
	unsigned darkPixelsCount = 0;

	//
	// Eye candy
	//
	if (optVerbose >= 2)
	{
		std::printf("polygon count:               %llu\n", static_cast<unsigned long long>(globalPolygons.size())); // Исправлено: %d -> %llu
		std::printf("grow option:                 %.1f pixels , %.3f mm\n", optGrowSize, optGrowSize / imageDPI * 25.4);
	}
	if (optVerbose >= 1)
	{
		std::printf("Image data\n"
					"  origin (mm):               %.3f x %.3f\n"
					"  size (mm):                 %.3f x %.3f\n"
					"  size (pixels):             %u x %d\n"
					"  uncompressed size (MB):    %.1f\n"
					"  dots per inch:             %u\n"
					"  TIFF rows per strip        %u\n",
					(-xOffset + minx) / imageDPI * 25.4, (-yOffset + miny) / imageDPI * 25.4, imageWidth / imageDPI * 25.4, imageHeight / imageDPI * 25.4, imageWidth, imageHeight, float((((imageWidth + 7) / 8) * imageHeight) / 0x100000), int(imageDPI), rowsPerStrip);
	}
	fflush(stdout);

	if (optTestOnly) // stop here on testing or if no Polygons to draw
	{
		if (optVerbose)
			std::printf("  time (sec):                %.2f\n", ((double)(clock() - start_clock)) / CLOCKS_PER_SEC);
		return 0;
	}

	// Initialise TIFF with the libtiff library
	//
	TIFF *tif = TIFFOpen(outputFilename.c_str(), "w");
	if (tif == NULL)
	{
		std::cout << "error creating output file '" << outputFilename << "\n";
		;
		return 1;
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
	bitmap = (unsigned char *)malloc(bitmapBytes);
	if (bitmap == 0)
		error("cannot allocate memory");

	//-----------------------------------------------------------------------
	// Draw polygons
	//-----------------------------------------------------------------------
	xOffset -= minx;

	int stripCounter = 0;
	std::list<Polygon>::iterator polyIterator = globalPolygons.begin();
	std::list<PolygonReference> activePolys;

	// The bitmap will be divided into strips, of height rowsPerStrip.
	// Polygons are plotted for each strip consecutively in a loop, where the strip y coordinate equals ystart
	for (unsigned int ystart = static_cast<unsigned int>(miny - yOffset); ystart < (static_cast<unsigned int>(imageHeight) + static_cast<unsigned int>(miny - yOffset)); ystart += rowsPerStrip) // Исправлено: int -> unsigned int
	{
		// blank entire strip buffer, set pixels on/off depending on polarity of the 1st Gerber.
		if (isPolarityDark)
			memset(bitmap, 0x00, bitmapBytes);
		else
			memset(bitmap, 0xff, bitmapBytes);

		unsigned char *bufferLine = bitmap;

		// Loop over each row of the strip and fill with horizontal lines from the polygon raster data.
		// All polygon are sorted in the list globalPolygons. Iterating each polygon for raster data will guarantee no missing lines.
		for (unsigned int y = ystart; (y - ystart) < rowsPerStrip && (y <= static_cast<unsigned int>(maxy)); y++, bufferLine += bytesPerScanline) // Исправлено: maxy -> unsigned int
		{
			while (polyIterator != globalPolygons.end() && y == static_cast<unsigned int>(polyIterator->pixelMinY)) // Исправлено: pixelMinY -> unsigned int
			{
				activePolys.push_back(PolygonReference());
				activePolys.back().polygon = &(*polyIterator);
				activePolys.sort();
				//				printf("added poly %d (y=%d)\n", activePolys.back().polygon->number, y);
				polyIterator++;
			}

			for (std::list<PolygonReference>::iterator it = activePolys.begin(); it != activePolys.end();)
			{
				if (y > static_cast<unsigned int>(it->polygon->pixelMaxY)) // Исправлено: pixelMaxY -> unsigned int
				{
					//					printf("erased poly %d (y=%d)\n", activePolys.back().polygon->number, y);
					it = activePolys.erase(it);
					continue;
				}
				int sliCount;
				int *sliTable;
				it->polygon->getNextLineX1X2Pairs(sliTable, sliCount);

				//				printf("p %2d y:%d (x cnt %d) |",it->polygon->number, y, sliCount); fflush(stdout);

				Polarity_t pol = it->polygon->polarity;
				if ((pol == DARK) && !isPolarityDark)
					pol = CLEAR;
				if ((pol == CLEAR) && isPolarityDark)
					pol = DARK;

				for (int i = 0; i < sliCount; i += 2)
				{
					//					printf(" %d~%d ",sliTable[i], sliTable[i+1] ); fflush(stdout);
					horizontalLine(xOffset + it->polygon->pixelOffsetX + sliTable[i],
								   xOffset + it->polygon->pixelOffsetX + sliTable[i + 1],
								   bufferLine, pol);
				}
				//				printf("\n");
				it++;
			}
		}

		//
		// Write strip buffer to TIFF
		//
		unsigned lines = std::min(rowsPerStrip, imageHeight - rowsPerStrip * stripCounter);
		int percentComplete = (100 * (stripCounter * rowsPerStrip + lines)) / imageHeight;
		if (optVerbose)
		{
			static int last = percentComplete;
			if (percentComplete != last)
				std::cout << "Rendering " << percentComplete << "%  \r" << std::flush;
			last = percentComplete;
		}
		TIFFWriteEncodedStrip(tif, stripCounter++, bitmap, bytesPerScanline * lines);

		// Calculate positive area information
		if (optShowArea)
		{
			for (unsigned int i = 0; i < lines; i++)
			{
				unsigned char *pbitmaprow = bitmap + bytesPerScanline * i;
				for (unsigned int x = 0; x < bytesPerScanline; x++)
					darkPixelsCount += nbitsTable[*pbitmaprow];
				pbitmaprow++;
			}
		}
	}
	TIFFClose(tif);

	if (optVerbose)
		std::cout << "\n";

	if (optShowArea)
	{
		std::printf("  dark  area (sq.cm):        %0.1f\n", darkPixelsCount * 2.54 * 2.54 / (imageDPI * imageDPI));
		std::printf("  clear area (sq.cm):        %0.1f\n", ((imageHeight * imageWidth) - darkPixelsCount * 2.54 * 2.54) / (imageDPI * imageDPI));
	}

	if (optVerbose)
		std::printf("  time (sec):                %.2f\n", ((double)(clock() - start_clock)) / CLOCKS_PER_SEC);
}
