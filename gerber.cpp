// This file is distributed under the terms of the GNU General Public License v3.

#define _USE_MATH_DEFINES
#include <vector>
#include <list>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <fstream>

using namespace ::std;

#include "polygon.h"
#include "apertures.h"
#include "gerber.h"
// #include "gerber_flex.h"

void yyerror(Gerber *g, const char *bisonMessage)
{
	string str = bisonMessage;
	size_t found = str.find("$undefined"); // replace the "$undefined" word in bison error string
	if (found != string::npos)
		str.replace(found, string::npos, "'" + string(yytext) + "'");
	found = str.find("syntax error");
	if (found != string::npos)
	{
		throw(str);
	}
	(void)g;
	throw(str);
}

/*
void Debug_print_aperture_info(Aperture *p, bool flag_composites=false)
{

	printf ("- Aperture Definition -\n");
	if (p->DCode == 0 && flag_composites == true) return;
	if (p->composite)printf ("(Macro is Composite)");
	printf ("  macro name 	= \"%s\"\n", p->nameMacro.c_str());
	printf ("  object ptr 	= %p\n", p);

	while(p)
	{
		printf ("  type 		= <%s>\n", p->rs274x_name());
		printf ("  primitive 	= %d\n", p->primitive);
		printf ("  D-code    	= %d\n", p->DCode);
		printf ("  isPolarityDark = %d\n", p->polygons.front().isPolarityDark);
//		for (int i=0; i < p->modifier.size(); i++)
//		{
			//printf ("   modifier %d 	= ($%d) %f\n", i, p->modifier[i].variableIndex, p->modifier[i].value);
//		}
		if (flag_composites == false) break;
		p = p->composite;
		if (p) {
		  printf("    |\n");
		  printf("   \\|/\n");
		  printf("    ` \n");
		}
	}
}

*/

/*
 * Add a warning message to the general message string stream
 */

void Gerber::warning(const char *format, ...)
{
	char buffer[256];
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

	warningCount++;
	ostringstream oss;
	if (warningCount < 30)
		oss << "Warning: " << buffer << " at line " << currentLine << "";
	if (warningCount == 30)
		oss << "Too many warnings, suppressing.";
	if (warningCount <= 30)
		messages.push_back(oss.str());
}

/*
 *  Calculate pixels per Gerber dimensional unit
 *  Unit are set from %MOIN*% and %MOMM*% parameters. See flex rules in gerber.lex
 */
double Gerber::dotsPerUnit()
{
	switch (units)
	{
	case MILLIMETER:
		return dotsPerInch / 25.4;
	case INCH:
		return dotsPerInch;
	default:
		warning("Dimension specified without units. Setting units to inches.");
		units = INCH;
		return dotsPerInch;
	}
}

//
// Flash Aperture. Adds the polygon of the currently selected aperture to the polygon list, including sub aperture.
//
void Gerber::flashAperture(double x, double y)
{
	// Aperture::composite points to aperture in the link list that must be plotted in succession.
	Aperture *arp = &*(apertureSelect);
	while (arp)
	{
		for (list<Polygon>::iterator it = arp->polygons.begin(); it != arp->polygons.end(); it++)
		{
			polygons.push_back(*it); // Copy polygon from aperture
			polygons.back().offset.x = x * scaleFactor[0];
			polygons.back().offset.y = -y * scaleFactor[1];
			// invert all sub polygons polarity when %PLC*% parameter specified.
			if (layerPolarityClear)
			{
				polygons.back().polarity = CLEAR;
			}
		}
		arp = arp->composite;
	}
}

/*
 *  Calculate a arc data from Gerber file Circular Interpolation information: (oldX,oldY), (I,J) ,(X,Y) and single or quadrant mode
 *
 */
void Gerber::calculateArc(Arc &arc)
{
	Point pntStart(oldX, oldY);
	Point pntEnd(X, Y);
	arc.isTooSmall = false;

	if ((I < 0 || J < 0) && isCircular360 == false)
	{
		warning("negative I or J found in single quadrant mode. Forcing to 360 degree mode.");
		isCircular360 = true;
	}

	if (isCircular360 == false && abs(Point(I, J)) >= 0.01)
	{ // Determine the correct signedness of I J coordinates in single quadrant mode.
		// Let:	S = start point
		// 		C = centre point, having one of 4 possible centre points (I,J) (-I,J) (I,-J) (-I,-J)
		// 		E = end point
		// The algorithm chooses C by following method:
		// If clockwise angle between the lines S.E and S.C is positive (or negative for anti clockwise) then
		// test all 4 possible combinations of C such that length differences between S.E and S.C is a minimum.
		double wi, wj, minDeltaR = 1E100;
		Point SE(pntEnd - pntStart);
		for (int i = 0; i < 4; i++)
		{
			double deltaR = fabs(abs(Point(I, J)) - abs(Point(I, J) + pntStart - pntEnd));
			double theta = arg(SE) - arg(Point(I, J));
			if (theta > M_PI)
			{
				theta -= 2 * M_PI;
			} // Wrap theta between -pi and +pi
			if (theta < -M_PI)
			{
				theta += 2 * M_PI;
			}
			if (drawingMode == CIRCLE_ANTICLOCKWISE)
			{
				theta *= -1;
			}
			if (theta >= 0 && deltaR < minDeltaR)
			{
				minDeltaR = deltaR;
				wi = I;
				wj = J;
			}
			// printf("theta=%0.f dR=%0.f I,J %.0f, %.0f\n",theta*180/M_PI,deltaR,I,J);
			I *= -1;
			if (i == 1)
				J *= -1;
		}
		I = wi;
		J = wj;
	}

	arc.centre = Point(I, J) + pntStart; // get the specified arc centre point
										 //	printf("initial centre=(%.0f,%.0f) \n",  arc.centre.real(),arc.centre.imag() );

	// Adjust the centre point or arc so end and start points always match old and current tool locations
	// only for 360 degree mode. Adjustment is impossible when start and end points are equal.
	if (isCircular360 == true && (pntStart != pntEnd))
	{
		Line L1(pntStart, pntEnd);
		Line L2 = L1;
		L2.moveParalell(arc.centre);
		L1.movePerpendicular((pntStart + pntEnd) / 2.0);
		arc.centre = L1.intersect(L2);
	}

	double coord_precision = pow(10.0, -min(coordsDecimals[0], coordsDecimals[1])) * dotsPerUnit();

	//	printf("final centre %.0f, %.0f\n",arc.centre.real(),arc.centre.imag());

	arc.radius = abs(arc.centre - pntStart); // Radius is always distance between start and centre points of arc

	arc.start = arg(pntStart - arc.centre);
	arc.end = arg(pntEnd - arc.centre);
	arc.stopped = arc.centre + polar(arc.radius, arc.end); // end point where arc finished (not always at current tool position)
	if (fabs(arc.start - arc.end) < 1e-10)
		arc.end += 2 * M_PI; // handle condition of equal start and end angles, assume full circle
							 //	printf("arc radius=%.0f  angles %f -> %f  end=(%.0f,%.0f)  start=(%.0f,%.0f)  centre=(%.0f,%.0f) stop=(%.0f,%.0f)\n",
							 //			arc.radius, arc.start*180/M_PI, arc.end*180/M_PI, pntEnd.x, pntEnd.y, pntStart.x, pntStart.y, arc.centre.x, arc.centre.y, arc.stopped.x, arc.stopped.y );

	// flag as a zero radius arc
	if (arc.radius < 2 * coord_precision || (I == 0 && J == 0))
	{
		warning("Zero arc radius - replacing with line segment.");
		arc.isTooSmall = true;
		return;
	}
	// report centre mismatch. Allow a mismatch up to 5 times the precision of coordinate data.
	double mismatch = abs(arc.centre - (Point(I, J) + pntStart));
	if (mismatch > 5 * coord_precision)
		warning("Adjusting arc centre mismatch by %0.4f%s", mismatch / dotsPerUnit(), unitText());
}

// Gets coordinated value an formatted RS274X coordinate string contained in /text.
// Returns coordinate value pixel units.
// If is_I_J is true then the coordinate data is for I J, and is therefore absolute,
//  and unaffected by image offset parameter.
//  This function is called from yylex() on any X, Y, I, J commands, and shall enable
//  the drawing
double Gerber::getCoordinate(char *text, int axis, bool is_I_J)
{
	isDrawingEnabled = true; // any X, Y, I, J command shall enable the drawing

	if (coordsInts[0] < 0)
	{
		warning("FS parameter missing, defaulting to FSLAX23Y23");
		coordsInts[0] = coordsInts[1] = 2;
		coordsDecimals[0] = coordsDecimals[1] = 3;
		isOmitLeadingZeroes = true;
		isCoordsAbsolute = true;
	}

	double value = atoi(text);
	if (text[0] == '-' || text[0] == '+')
		text++;

	int excess_digits = (int)strlen(text) - (coordsDecimals[axis] + coordsInts[axis]);
	if (excess_digits > 0)
		warning("found %d surplus digits in coordinate ", excess_digits);

	if (isOmitLeadingZeroes)
		value /= pow(10, double(coordsDecimals[axis]));
	else
		value /= pow(10, double((int)strlen(text) - coordsInts[axis])); // bug fixed: added int cast to strlen()

	value *= dotsPerUnit(); // convert to pixels
	if (!is_I_J)
		value += imageOffsetPixels[axis]; // Offset adjustment,

	if (is_I_J || isCoordsAbsolute)
		return value;

	// if coordinates are specified incremental then return new value - previous value
	coordPrevious[axis] += value;
	return coordPrevious[axis];
}

//--------------------------------------------------------------------------------------------------
//						Process AD data block
//--------------------------------------------------------------------------------------------------
void Gerber::process_AD_block(int DCode)
{

	int composite_count = 0;
	Aperture *arp = 0;

	// if aperture D code has already been defined then replace it
	for (list<Aperture>::iterator it = ad_apertures.begin(); it != ad_apertures.end(); it++)
	{
		if (it->DCode == DCode)
		{
			arp = &(*it);
			break;
		}
	}
	// Create a new aperture object for this AD code.
	if (arp == 0)
	{
		ad_apertures.push_back(Aperture()); // blank aperture
		arp = &ad_apertures.back();
	}

	// Aperture type references a previously defined macro aperture. We find a match
	// to the macro aperture of this name by searching the list.
	// Search the aperture macro list for matching macro names.
	// Multiple name matches will occur when there is a Macro Aperture defined
	// in the Gerber file which has multiple special aperture primitives in the one AM block.
	// When searching through the link list of Aperture objects we look for all matches of
	// macro_name. We set Aperture::composite of the current matching object to point to the next
	// matching Aperture object. Using the Aperture::composite member allows plotting of all primitives when only
	// only the first primitive in the AM block list is known.
	// note: apertures having single character names, C, R, O or P have been predefined.
	for (list<Aperture>::iterator amacro = macro_apertures.begin(); amacro != macro_apertures.end(); amacro++)
	{
		if (temporaryNameMacro != amacro->nameMacro)
			continue; // search for a macro name match.

		// Subsequent macro name matches mean this aperture is built up of multiple aperture primitives.
		// The <composite> member points to the subsequent primitive Aperture object as a link list.
		if (composite_count >= 1)
		{
			ad_apertures.push_back(*amacro);
			arp->composite = &ad_apertures.back(); // set composite data member of previous Aperture in list to point to here.
			arp = &ad_apertures.back();
		}
		composite_count++;

		*arp = *amacro;							  // Copy entire Macro Aperture object
		arp->DCode = DCode;						  // Assigned D number found in this ADD block
		arp->linenum_at_definition = currentLine; // record line in Gerber file

		try
		{
			arp->render(dotsPerUnit(), growSize, variables.size());
			// New polygons object for this aperture have been created, we can now scale the vertices, and save pointer to new vertex data.
			for (list<Polygon>::iterator it = arp->polygons.begin(); it != arp->polygons.end(); it++)
			{
				it->vdata->scale(scaleFactor[0], -scaleFactor[1]);
				vertexdata.push_back(it->vdata);
			}
			if (arp->polygons.size() == 0) // if aperture is blank then don't add it to the list
				ad_apertures.pop_back();   // this should never happen ???????
		}
		catch (string msg)
		{
			oss << msg << " in primitive index " << composite_count << " (" << arp->rs274x_name() << ") in macro '" << amacro->nameMacro << "' mapped from D" << DCode;
			throw oss.str();
		}

#ifdef DEBBUG
		Debug_print_aperture_info(&(*arp));
#endif

	} // end of macro name search and finished with

	variables.clear();

	if (composite_count == 0)
	{
		oss << "the referring macro aperture name '" << temporaryNameMacro << "' is undefined";
		throw oss.str();
	}

} // end of AD command block  processing

void Gerber::process_G_command(int code)
{

	// G codes (general functions)
	if (code == 1)
		drawingMode = LINEAR_1X;
	if (code == 10)
		drawingMode = LINEAR_10X;
	if (code == 11)
		drawingMode = LINEAR_01X;
	if (code == 12)
		drawingMode = LINEAR_001X;
	if (code == 2)
		drawingMode = CIRCLE_CLOCKWISE;
	if (code == 3)
		drawingMode = CIRCLE_ANTICLOCKWISE;
	if (code == 74)
		isCircular360 = false;
	if (code == 75)
		isCircular360 = true;
	if (code == 70)
		units = INCH; // assign dimensions to inches (synonymous to %MOIN)
	if (code == 71)
		units = MILLIMETER; // assign dimensions to millimetres (synonymous to %MOMM)
	if (code == 90)
		isCoordsAbsolute = true; // (synonymous to %FS)
	if (code == 91)
		isCoordsAbsolute = false; // (synonymous to %FS)
	if (code == 37)
	{
		isPolygonFill = false;
		isDrawingEnabled = false; // don't draw after polygon exit within current command block
		polygons.back().vdata->scale(scaleFactor[0], -scaleFactor[1]);
	}
	if (code == 36 && isPolygonFill == false) // new polygon for the current polygon fill command
	{
		isLampOn = false; // Always start with lamp off so tool can be positioned after a G36 command with lamp off
		isPolygonFill = true;
		polygons.push_back(Polygon());
		vertexdata.push_back(polygons.back().vdata); // Save pointer to vertex data for the newly created Polygon
		if (layerPolarityClear)
		{
			polygons.back().polarity = CLEAR;
		} // polygon polarity dependent on PLC / PLD parameters
	}
}

void Gerber::process_D_command(int code)
{

	if (code >= 10)
	{
		// search for a Aperture list with a matching D_number, use default aperture if not found.
		apertureSelect = ad_apertures.begin();
		apertureSelect++;
		for (; apertureSelect != ad_apertures.end(); apertureSelect++)
		{
			if (code == apertureSelect->DCode)
				break;
		}
		if (apertureSelect == ad_apertures.end())
		{ // default to first in list
			apertureSelect = ad_apertures.begin();
			warning("Aperture D%d has not been defined", code);
		}
	}
	if (code == 3) // flash
	{
		flashAperture(X, Y);
	}
	if (code == 1) // drawing with light on;
	{
		isLampOn = true;
		isDrawingEnabled = true;
	}
	else // turn light off
	{
		isLampOn = false;
	}
}

void Gerber::processDataBlock()
{

	// special variables containing arc information when in circular mode

	double dX = (X - oldX);
	double dY = (Y - oldY);
	double toolShift = sqrt(dX * dX + dY * dY); // calculate relative tool shift distance

	// printf("block process %s old(%f,%f) -> (%f,%f)\n",(isLampOn) ? "on " : "off", oldX,oldY,X,Y);

	//--------------------------------------------------------------
	// draw if the lamp is currently on
	//--------------------------------------------------------------
	if (isLampOn && isDrawingEnabled)
	{
		if (isPolygonFill) // Add vertices to polygon
		{
			if (polygons.back().empty())
			{
				polygons.back().vdata->add(oldX, oldY);
			}
			if ((drawingMode == CIRCLE_CLOCKWISE || drawingMode == CIRCLE_ANTICLOCKWISE))
			{
				Arc arc;
				calculateArc(arc);
				if (!arc.isTooSmall)
					polygons.back().vdata->addArc(arc.start, arc.end, arc.radius, arc.centre.x, arc.centre.y, (drawingMode == CIRCLE_CLOCKWISE));
				else
					polygons.back().vdata->add(X, Y);
			}
			else
			{
				polygons.back().vdata->add(X, Y);
			}
		}
		// draw a trace (line or arc)
		else // Draw to current tool position
		{
			// Warn user we are about to use default aperture
			if (!isWarnNoApertureSelect && (apertureSelect == ad_apertures.begin()))
			{
				warning("Drawing started without aperture select. Using default");
				isWarnNoApertureSelect = true;
			}
			// Check if a suitable aperture is selected for the drawing mode.
			if (((drawingMode == CIRCLE_CLOCKWISE || drawingMode == CIRCLE_ANTICLOCKWISE) && apertureSelect->primitive != Aperture::STANDARD_CIRCLE) || (drawingMode == LINEAR_1X && (apertureSelect->primitive != Aperture::STANDARD_CIRCLE && apertureSelect->primitive != Aperture::STANDARD_RECTANGLE)))
			{
				oss << "D" << apertureSelect->DCode << " mapped to (" << apertureSelect->rs274x_name() << ") aperture which is not supported for drawing traces\n"
					<< "Supported shapes are:\n"
					   " C or R     for linear traces\n"
					   " C          for arc traces\n";
				throw oss.str();
			}

			// Flash at start of line or arc if last draw was at a different position or different aperture
			if (lastDrawnApertureSelect != apertureSelect || lastDrawnX != oldX || lastDrawnY != oldY)
			{
				//			printf("init flashed %d (%f,%f)\n",apertureSelect->DCode, oldX,oldY );
				flashAperture(oldX, oldY);
			}

			// Get size of the circle (C) or rectangle (R) type polygon.
			double polygon_width = apertureSelect->standardApWidth;
			double polygon_heigth = apertureSelect->standardApHeight;

			// A dirty fix to avoid polygon slivers narrower than 1 pixel, as the polygon filling routines currently do not
			// correctly plot such slivers. The aperture height is limited to minimum value so that after scaling,
			// the trace width is always >= 1 pixel
			double f = fabs(scaleFactor[1]);
			if (f > 1e-10 && polygon_heigth * f < 1.1)
				polygon_heigth = 1.1 / f;

			if (drawingMode == LINEAR_1X)
			{
				if (toolShift > 1) // don't bother drawing traces of tiny length
				{
					double sy, sx;
					// width of line or arc draw by using height of the polygon for this aperture.
					if (apertureSelect->primitive == Aperture::STANDARD_CIRCLE)
					{
						// assume trance width is diameter of circle or polygon height.
						double traceWidth = max(polygon_heigth, polygon_width);
						traceWidth -= 0.05; // subtract a small fraction to fix problem with arc polygons boundary aligning with the trace
						sy = (traceWidth * dX) / toolShift;
						sx = traceWidth * traceWidth - sy * sy;
						if (sx < 0)
							sx = 0; // avoid  slightly negative sx values due to rounding off errors.
						sx = sqrt(sx) / 2;
						sy = sy / 2;
						if (dY > 0)
							sy *= -1;
					}
					else // set the linear polygon to corner points of rectangle aperture.
					{
						sx = polygon_width / 2;
						sy = -polygon_heigth / 2;
						if ((dX * dY) < 0)
							sx *= -1;
					}
					polygons.push_back(Polygon());
					vertexdata.push_back(polygons.back().vdata); // Save pointer to vertex data for the newly created Polygon
					if (layerPolarityClear)
					{
						polygons.back().polarity = CLEAR;
					} // polygon polarity dependent on PLC / PLD parameters
					polygons.back().vdata->add(oldX + sx, oldY + sy);
					polygons.back().vdata->add(oldX - sx, oldY - sy);
					polygons.back().vdata->add(X - sx, Y - sy);
					polygons.back().vdata->add(X + sx, Y + sy);
					polygons.back().vdata->scale(scaleFactor[0], -scaleFactor[1]);
				}
				if (toolShift > 0) // don't flash when line length is exactly zero because the initial flash is acceptable.
				{
					flashAperture(X, Y); // flashes aperture at very end of line.
				}
			}
			// Draw ARC -----------------------------------------------------------------------
			else if (drawingMode == CIRCLE_CLOCKWISE || drawingMode == CIRCLE_ANTICLOCKWISE)
			{
				Arc arc;
				calculateArc(arc);
				if (!arc.isTooSmall)
				{
					polygons.push_back(Polygon());
					vertexdata.push_back(polygons.back().vdata); // Save pointer to vertex data for the newly created Polygon
					polygons.back().vdata->addArc(arc.start, arc.end, arc.radius - (polygon_heigth) / 2, arc.centre.x, arc.centre.y, (drawingMode == CIRCLE_CLOCKWISE));
					polygons.back().vdata->addArc(arc.end, arc.start, arc.radius + (polygon_heigth) / 2, arc.centre.x, arc.centre.y, (drawingMode != CIRCLE_CLOCKWISE));
					polygons.back().vdata->scale(scaleFactor[0], -scaleFactor[1]);
					if (layerPolarityClear)
					{
						polygons.back().polarity = CLEAR;
					} // polygon polarity dependent on PLC / PLD parameters
					oldX = arc.stopped.x; // set oldX,oldY to stopped point of arc
					oldY = arc.stopped.y;
				}

				// The RS274X standard does not restrict an arc ending point to match current tool point.
				// When mismatched, other Gerber software draws a line between these two points.
				enum APETURE_DRAWING_MODE tmp = drawingMode;
				drawingMode = LINEAR_1X;
				lastDrawnApertureSelect = apertureSelect;
				processDataBlock();
				drawingMode = tmp;
			}
			else
			{
				throw string("drawing mode unsupported in current version, please contact author");
			}
			lastDrawnX = X;
			lastDrawnY = Y;
			lastDrawnApertureSelect = apertureSelect;
			isDrawingEnabled = false;
		}

	} // end if (isLampOn) -------------------------------------------------------------

	I = J = 0; // Undocumented RS-274X(D) feature: If either I or J coordinates missing from block then assume zero.
	oldX = X;
	oldY = Y;
	isDrawingEnabled = false; // disable drawing after each data block. re-enabled on any D1 or a X, Y command
}

/*
 * Initialize all Gerber parameters to default values;
 * this function is called in Gerber constructor and M02 (End of program) commands encounted.
 * Experimentation with ViewMate v10.6.0 and GC-Preview 17.2.2 commercial Gerber viewers showed the following:
 *
 * M0 and M1: both views effectively ignored.
 * M02: both views reset coordinate data, but aperture and FS definitions remained.
 * M3: ViewMate was stop reading data after this code, GC-Preview gave error.
 */
void Gerber::loadDefaults()
{
	isDrawingEnabled = false;
	lastDrawnApertureSelect = ad_apertures.begin();
	isWarnNoApertureSelect = false;
	layerPolarityClear = false;
	scaleFactor[0] = optScaleX;
	scaleFactor[1] = optScaleY;
	imageOffsetPixels[0] = imageOffsetPixels[1] = 0;
	repeat.X = 1;
	repeat.Y = 1;
	repeat.I = 0;
	repeat.J = 0;
	isLampOn = false;
	isMirrorAaxis = false;
	isMirrorBaxis = false;
	isAxisSwapped = false;
	drawingMode = LINEAR_1X;
	isCircular360 = false; // safest to assume single quadrant mode.
	isPolygonFill = false;
	// must initialize plotter coordinates to zero, (also undocumented in RS-274X)
	// Some gerber files can start drawing without setting both or either X and Y coordinates !
	oldX = oldY = X = Y = 0;
	coordPrevious[0] = coordPrevious[1] = 0;
	// Generate artificial macros in the list that represent each of the standard apertures, C, R, O and P.
	// The macro modifiers will be of variable type, listed from $1 to $5. Note, the P aperture can have up to 5 modifiers.
	macro_apertures.clear();
	ad_apertures.clear();
	Aperture arp;
	arp.parameter.resize(5);
	for (int i = 0; i < 5; i++)
	{
		arp.parameter[i] = new NodeT(NodeT::VAR, &i, &variables);
	}
	arp.nameMacro = "C";
	arp.primitive = Aperture::STANDARD_CIRCLE;
	macro_apertures.push_back(arp);

	arp.nameMacro = "R";
	arp.primitive = Aperture::STANDARD_RECTANGLE;
	macro_apertures.push_back(arp);

	arp.nameMacro = "O";
	arp.primitive = Aperture::STANDARD_ORBROUND;
	macro_apertures.push_back(arp);

	arp.nameMacro = "P";
	arp.primitive = Aperture::STANDARD_POLYGON;
	macro_apertures.push_back(arp);

	// Create an aperture definition consisting of single pixel wide circle.
	// This will be the first aperture in the list and is to be the default aperture.
	temporaryNameMacro = "C";
	Units_t ut = units;
	units = INCH; // Temporarily set inches just to construct this aperture
	variables.push_back(1.5 / dotsPerUnit());
	process_AD_block(-1);
	apertureSelect = ad_apertures.begin(); // select to default aperture
	units = ut;							   // restore previous units
}

// ***********************************************************************
// The Gerber constructor. This is the function called by the user to parse
// the Gerber RS-274X file and create drawing information so the image can
// be plotted with the Draw() member function. see below.
//
// entry -
//
// fp_gerb must point to file name of the Gerber file.
//
// return -
//
// If error occurred then ErrorFlag = true and errormessage will point to
// error string containing a short description of the problem encounted.
// If no errors then ErrorFlag = 0 and the Draw() functions may be called.
// On success the public data members <x_max, x_min, y_max, y_min> will now
// contain useful information.
//
// *****************************************************************************
Gerber::Gerber(FILE *fp_gerb, const double dotsPerInch, const double growSize, double optScaleX, double optScaleY)
	: dotsPerInch(dotsPerInch), growSize(growSize), optScaleX(optScaleX), optScaleY(optScaleY)
{

	if (!fp_gerb)
	{
		throw std::runtime_error("File pointer is null");
	}

	// Проверка размера файла
	fseek(fp_gerb, 0, SEEK_END);
	long fileSize = ftell(fp_gerb);
	rewind(fp_gerb);

	if (fileSize <= 0)
	{
		throw std::runtime_error("File is empty or invalid");
	}

	rewind(fp_gerb);

	try
	{
		imageRotate = 0;
		imagePolarityDark = true;
		isError = false;
		warningCount = 0;
		currentLine = 1;
		coordsInts[0] = -1; // assign to negative value until FS parameter encounter
		units = UNDEFINED;

		loadDefaults();

		yyrestart(fp_gerb); // set a new input file for FLEX, flushes input buffer.

		yyparse(this);

		// Modify then Initialise all vertices used by the polygons
		for (list<VertexData *>::iterator it = vertexdata.begin(); it != vertexdata.end(); it++)
		{

			(*it)->rotate(imageRotate); // Rotate the vertices specified by the Image Rotate parameter.
			(*it)->initialise();
		}

		// Initialise the polygons
		int k = 0;
		for (list<Polygon>::iterator it = polygons.begin(); it != polygons.end();)
		{
			if (it->empty())
			{

				it = polygons.erase(it);
				continue;
			}

			// Rotate entire gerber image as specified by IR parameter
			it->offset.rotate(imageRotate);

			it->initialise(); // Initialise to calculate raster x1,x2 data.

			// Identify each polygon with a drawing order number.
			it->number = k;
			k++;
			it++;
		}

		if (polygons.size() == 0)
			warning("nothing to draw");

		// Sort all polygons object so they have ascending miny values.
		polygons.sort();
	}
	catch (const string &msg)
	{
		isError = true;
		errorMessage << "error: " << msg << ". stopped at line " << currentLine;
	}
	catch (const std::exception &e)
	{
		isError = true;
		errorMessage << "error: " << e.what() << ". stopped at line " << currentLine;
	}
	catch (...)
	{
		isError = true;
		errorMessage << "error: Unknown exception. stopped at line " << currentLine;
	}
}
