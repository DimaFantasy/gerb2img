// This file is distributed under the terms of the GNU General Public License v3.

#define _USE_MATH_DEFINES
#include <string>
#include <math.h>
#include <vector>
#include <list>
#include <map>

using namespace std;

#include "polygon.h"
#include "apertures.h"
#include "gerber.h"



#define LOLIMIT(a,b) if (a < b) a = b;
#define UPLIMIT(a,b) if (a > b) a = b;



const char * Aperture::rs274x_name()
{
	static string name;
	switch (primitive) {
	case STANDARD_CIRCLE : 		return "standard circle";
	case STANDARD_RECTANGLE : 	return "standard rectangle";
	case STANDARD_ORBROUND :	return "standard orbround";
	case STANDARD_POLYGON : 	return "standard polygon";
	case SPECIAL_CIRCLE : 		return "special circle";
	case SPECIAL_LINE_VECTOR :
	case SPECIAL_LINE_VECTOR2 : return "special line vector";
	case SPECIAL_LINE_CENTER : 	return "special line centre";
	case SPECIAL_LINE_LOLEFT : 	return "special lower left";
	case SPECIAL_OUTLINE : 		return "special outline";
	case SPECIAL_POLYGON : 		return "special polygon";
	case SPECIAL_MOIRE : 		return "special moire";
	case SPECIAL_THERMAL : 		return "special thermal";
	case PRIMITIVE_INVALID: return "Invalid primitive";
	}
	return "Invalid primitive";
}


//
// Safely gets value from Aperture::modifier at element number idx
//
double Aperture::getParameter(int idx)
{
	ostringstream oss;
	if (idx >= static_cast<int>(parameter.size())) // Исправлено сравнение типов
	{
		oss	<<"modifier expected at position "<<(idx+1);
		throw oss.str();
	}
	try
	{
		return parameter[idx]->evaluate();
	}
	catch (string msg)
	{
		oss <<msg<<" at parameter "<<idx+1;
		throw oss.str();
	}

}


//------------------------------------------------------------
// function for adding a new element to the link list of
// Aperture objects.
void Aperture::render(const double dots_per_unit, const double grow_size, int ADmodifierCount)
{
	double rotation = 0;
	double standardHoleX = 0;
	double standardHoleY = 0;

	switch (primitive)
	{
	// ******************************************************************************
	//		 STANDARD APERTURE 'O' ORBROUND
	//		 STANDARD APERTURE 'C' CIRCLE
	//		 SPECIAL  APERTURE primitive #1  Circle
	// ******************************************************************************
	case STANDARD_CIRCLE :
	case STANDARD_ORBROUND :
	case SPECIAL_CIRCLE :
	{
		double  ysize, xsize, x_center=0, y_center=0;
		polygons.push_back(Polygon());		// Create instance of empty polygon

		if (primitive == STANDARD_CIRCLE)
		{
			ysize = xsize = getParameter(0) * dots_per_unit;
			if ( ADmodifierCount > 1) 	standardHoleX = getParameter(1)*dots_per_unit;
			if ( ADmodifierCount > 2) 	standardHoleY = getParameter(2)*dots_per_unit;
		}
		else if (primitive  == STANDARD_ORBROUND)
		{
			xsize 		= getParameter(0) * dots_per_unit;
			ysize 		= getParameter(1) * dots_per_unit;
			if ( ADmodifierCount > 2) 	standardHoleX = getParameter(2)*dots_per_unit;
			if ( ADmodifierCount > 3) 	standardHoleY = getParameter(3)*dots_per_unit;
		}
		else if (primitive == SPECIAL_CIRCLE)
		{
			if (getParameter(0) == 1) {	polygons.back().polarity = CLEAR; }
			ysize = xsize 	= getParameter(1) * dots_per_unit;
			x_center 		= getParameter(2) * dots_per_unit;
			y_center 		= getParameter(3) * dots_per_unit;
		}
		else throw;
		standardApWidth = xsize;		// Save size of Standard C or R for trace plotting use.
		standardApHeight = ysize;

	    if (xsize<0 || ysize<0) throw std::string("dimension must be > 0");

	    xsize += grow_size;
	    ysize += grow_size;

	    if (xsize < 1) xsize = 1;		// handle zero radius circles as single pixel.
	    if (ysize < 1) ysize = 1;

    	double arc_offset = (xsize - ysize)/2;

	    if (xsize > ysize )	// horizontal orbround
	    {
	    	polygons.back().vdata->addArc(0.5*M_PI, 1.5*M_PI, ysize/2, x_center - arc_offset , y_center);
	    	polygons.back().vdata->addArc(1.5*M_PI, 2.5*M_PI, ysize/2, x_center + arc_offset, y_center);
	    }
	    else				// vertical orbround or circle
	    {
	    	polygons.back().vdata->addArc(0*M_PI, 1*M_PI, xsize/2, x_center , y_center - arc_offset);
	    	polygons.back().vdata->addArc(1*M_PI, 2*M_PI, xsize/2, x_center , y_center + arc_offset);
	    }
	    break;
	} // end of case

	// ******************************************************************************
	//		 Build STANDARD APERTURE 'R' RECTANGLE
	// ******************************************************************************
	case STANDARD_RECTANGLE :
	{
		double y_size, x_size;
		polygons.push_back(Polygon());		// Create instance of empty polygon

		y_size = x_size = getParameter(0) * dots_per_unit - 0.5  + grow_size;
		// (RS274X  botch) If only 1 modifier given then assume square.
		if ( ADmodifierCount > 1)  	y_size = getParameter(1) * dots_per_unit - 0.5  + grow_size;
		if ( ADmodifierCount > 2) 	standardHoleX = getParameter(2)*dots_per_unit;
		if ( ADmodifierCount > 3) 	standardHoleY = getParameter(3)*dots_per_unit;

		if (x_size <= 0) x_size = 1;
		if (y_size <= 0) y_size = 1;

		standardApWidth = x_size;		// Save size of Standard C or R for trace plotting use.
		standardApHeight = y_size;

	    polygons.back().vdata->addRectangle(x_size, y_size);
	    break;

	} // end of case

	// ******************************************************************************
	//		 STANDARD APERTURE 'P' POLYGON
	//		 SPECIAL  APERTURE #5 POLYGON
	//
	// notes:	diameter parameter represents the 2 * distance from a line that
	// intersects the normal to a side and centre point.
	//
	// ******************************************************************************
	case STANDARD_POLYGON :
	case SPECIAL_POLYGON :
	{
		double diameter;
		int nsides;
		double x_centre = 0;
		double y_centre = 0;
		polygons.push_back(Polygon());		// Create instance of empty polygon

		if ( primitive == STANDARD_POLYGON)
		{
			diameter 	= getParameter(0) * dots_per_unit;
			nsides 	= int(getParameter(1));
			if (ADmodifierCount > 2)	rotation = getParameter(2) * M_PI/180;
			if (ADmodifierCount > 3) 	standardHoleX = getParameter(3)*dots_per_unit;
			if (ADmodifierCount > 4) 	standardHoleY = getParameter(4)*dots_per_unit;

		}
		else if ( primitive == SPECIAL_POLYGON)
		{
			if (getParameter(0) == 1) {	polygons.back().polarity = CLEAR; }
			nsides 		= int(getParameter(1));
			x_centre 	= getParameter(2) * dots_per_unit;
			y_centre 	= getParameter(3) * dots_per_unit;
			diameter 	= getParameter(4) * dots_per_unit;
			rotation 	= getParameter(5) * M_PI/180;
		}
		else throw;

		diameter += grow_size;
		// adjust diameter
		if (nsides < 3 || nsides > 24 ) throw  std::string("number of sides out of range 3 to 24");
		if (diameter < 1) diameter = 1;

	    polygons.back().vdata->addRegularPolygon(diameter/2.0, rotation,  nsides, x_centre, y_centre);

	    break;
	} //end of case

	// ******************************************************************************
	//		 SPECAL APERTURE primitive #7  : THERMAL CROSS HAIR
	// ******************************************************************************
	case SPECIAL_THERMAL :
	{
		rotation = fmod( getParameter(5) * M_PI/180, M_PI/2);    // get radians
		double hair_thickness		= getParameter(4) * dots_per_unit + grow_size ;
		double inside_radius		= getParameter(3)/2 * dots_per_unit - grow_size/2.0;
		double outside_radius		= getParameter(2)/2 * dots_per_unit + grow_size/2.0;
		double y_centre     		= getParameter(1) * dots_per_unit;
		double x_centre     		= getParameter(0) * dots_per_unit;

		if ( hair_thickness  >= 2.4*inside_radius) 	hair_thickness = 2.4*inside_radius;
		if ( hair_thickness  < 1) hair_thickness = 1;
		if ( inside_radius   < 1) inside_radius  = 1;
		if ( outside_radius  < 2) break;

		double argout = M_PI/2 - acos( hair_thickness/2 / outside_radius);
		double argin =  M_PI/2 - acos( hair_thickness/2 / inside_radius);

		if (inside_radius >= outside_radius )
			throw  std::string("inside radius >= outside radius " );

		double theta = rotation;
		for (int i=0; i < 4; i++)
		{
			polygons.push_back(Polygon());		// Create instance of empty polygon
		    polygons.back().vdata->addArc( theta + argout, theta+(M_PI/2 - argout), outside_radius, x_centre, y_centre, false);
		    polygons.back().vdata->addArc( theta + ( M_PI/2 - argin), theta + argin, inside_radius, x_centre, y_centre, true);
		    theta += M_PI/2;
		}
	    break;
	} // end of case

	// ******************************************************************************
	//		 A base class member to initialise special apertures
	//  primitive #21 		,LINE CENTER
	//  primitive #2 or 20 	,LINE VECTOR
	//  primitive #22		,LINE LOWWER LEFT
	//
	// A solid rectangle defined by width, height, and centre point.
	// The end points are rectangular. Rotation point is ambiguous
	// in the RS-274X standard. GerbTool V13 and Altium handle
	// equivalently. Rotation point is at the origin (0,0) of aperture data points.
	//
	// ******************************************************************************
	case SPECIAL_LINE_VECTOR :
	case SPECIAL_LINE_VECTOR2 :
	case SPECIAL_LINE_CENTER :
	case SPECIAL_LINE_LOLEFT :
	{

		double rectangle_length, rectangle_height, theta;
		Point start, end, centre;
		polygons.push_back(Polygon());		// Create instance of empty polygon

		if (getParameter(0) == 1) {	polygons.back().polarity = CLEAR; }

		switch(primitive)
		{
		case SPECIAL_LINE_CENTER:
			rectangle_length= getParameter(1) * dots_per_unit;
			rectangle_height= getParameter(2) * dots_per_unit;
			centre.x 		= getParameter(3) * dots_per_unit;
			centre.y 		= -getParameter(4) * dots_per_unit;
			theta			= getParameter(5) / 180*M_PI;   	 // rotation radians (+ = counterclockwise, - = clockwise)
			centre.rotate( theta );
			break;

		case SPECIAL_LINE_VECTOR:
		case SPECIAL_LINE_VECTOR2:
			rectangle_height = getParameter(1) * dots_per_unit;
			start.x			 = getParameter(2) * dots_per_unit;
			start.y			 = getParameter(3) * dots_per_unit;
			end.x			 = getParameter(4) * dots_per_unit;
			end.y		     = getParameter(5) * dots_per_unit;
			theta		  	 = getParameter(6)*M_PI/180;   	 	// radians (+ = counterclockwise, - = clockwise)
			start.rotate( theta );	// rotation about origin point
			end.rotate( theta );
			rectangle_length = abs(start-end);
			centre = (start + end) / 2;
			theta = arg( end - start );
			break;

		case SPECIAL_LINE_LOLEFT:
			rectangle_length = getParameter(1) * dots_per_unit;
			rectangle_height = getParameter(2) * dots_per_unit;
			centre.x 		 = +rectangle_length /2 + getParameter(3) * dots_per_unit;
			centre.y 		 = +rectangle_height /2 - getParameter(4) * dots_per_unit;
			theta			 = getParameter(5)*M_PI/180;   	 // rotation radians (+ = counterclockwise, - = clockwise)
			centre.rotate( theta );
			break;
			
		default:
			throw std::string("Invalid primitive type for line vector");			
		}

		rectangle_length  += grow_size;
		rectangle_height += grow_size;
		if (rectangle_length  <= 0 ) throw  std::string("Illegal line width, <= 0");
		if (rectangle_height <= 0 ) throw  std::string("Illegal line height, <= 0");
		if (rectangle_length < 1) rectangle_length = 1;
		if (rectangle_height < 1) rectangle_height = 1;

		polygons.back().vdata->addRectangle(rectangle_length, rectangle_height);
		polygons.back().vdata->rotate( theta );
		polygons.back().vdata->shift(centre.x, centre.y);
	    break;
	} // end of case

	// ******************************************************************************
	//		 A Special Aperture Primitive #4 "Outline"
	// ******************************************************************************
	case SPECIAL_OUTLINE :
	{
		polygons.push_back(Polygon());		// Create instance of empty polygon
		if (getParameter(0) == 1) {	polygons.back().polarity = CLEAR; }

		int num_points		= int ( getParameter(1) );

		// rotation radians (+ = counterclockwise, - = clockwise)
		// last parameter is always specifies rotation regardless of number of surplus vertices.
		rotation = parameter.back()->evaluate() * M_PI / 180.0;

		if (num_points*2 + 3 > static_cast<int>(parameter.size())) // Исправлено сравнение типов
			throw string("specified number of points exceeds number of vertices listed");

		for (int i=0; i < num_points; i++ )
		{
			Point p( getParameter(i*2+2) * dots_per_unit, getParameter(i*2+3) * dots_per_unit );
			p.rotate(rotation);
			polygons.back().vdata->add(p);
		}
	    break;
	} // end of case

	// ******************************************************************************
	//		 A Special Aperture Primitive #6 "Moiré"
	// ******************************************************************************
	case SPECIAL_MOIRE :
	{
		double x_centre     	= getParameter(0) * dots_per_unit;
		double y_centre     	= getParameter(1) * dots_per_unit;
		double diameter			= getParameter(2) * dots_per_unit + grow_size;
		double thickenss		= getParameter(3) * dots_per_unit + grow_size;
		double gap				= getParameter(4) * dots_per_unit - grow_size;
		int num_circles			= int(getParameter(5));
		double hair_thickness	= getParameter(6) * dots_per_unit + grow_size;
		double hair_length		= getParameter(7) * dots_per_unit + grow_size;
		rotation = fmod( getParameter(8) * M_PI/180, M_PI/2);    				// get radians

		LOLIMIT( hair_thickness, 1);
		LOLIMIT( hair_length, 1);
		LOLIMIT( gap, 1);

		for (int i=0; i < num_circles; i++)
		{
		    if (diameter < 1) break;
		    UPLIMIT(thickenss, diameter/2);
			polygons.push_back(Polygon());		// Create instance of empty polygon
		    polygons.back().vdata->addArc( 0, 2*M_PI, diameter/2, x_centre, y_centre, false);
		    polygons.back().vdata->addArc( 2*M_PI, 0, diameter/2 - thickenss, x_centre, y_centre, true);
		    polygons.back().vdata->rotate(rotation);
		    diameter -= 2*(thickenss + gap);
		}
		polygons.push_back(Polygon());
	    polygons.back().vdata->addRectangle(hair_thickness, hair_length, x_centre, y_centre);
	    polygons.back().vdata->rotate(rotation);
		polygons.push_back(Polygon());
	    polygons.back().vdata->addRectangle(hair_length, hair_thickness, x_centre, y_centre);
	    polygons.back().vdata->rotate(rotation);
		break;
	} // end of case
	case PRIMITIVE_INVALID:
		throw std::string("Invalid primitive type");

	default:
		// Обработчик по умолчанию для необработанных значений.
		throw std::string("Unhandled primitive type");
	} // end of switch
	//-----------------------------------------------------------------------------------------------------

	// If one of the hole dimensions were present in a standard R, C or O aperture definition
	// then add a hole by adding one final clear circle or rectangle polygon to end of the list.
	if ( standardHoleX > 0.5 )
	{
		polygons.push_back(Polygon());
		polygons.back().polarity = CLEAR;
		if ( standardHoleY > 0.5 )
			polygons.back().vdata->addRectangle(standardHoleX, standardHoleY);
		else
		    polygons.back().vdata->addArc( 0, 2*M_PI, standardHoleX / 2, 0, 0, false);
	}
}
