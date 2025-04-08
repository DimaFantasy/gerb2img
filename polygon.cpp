#define _USE_MATH_DEFINES
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <list>
#include <cctype>
#include <math.h>

using namespace std;

#include "polygon.h"

// #define DEBUG

/*
 * High speed double to integer conversion replacement for int(floor(0.5 + x))
 * This function is used when converting real coordinates to pixel coordinates.
 */
inline int roundDot(double x)
{
	if (x < 0)
	{
		return int(x - 0.5);
	}
	return int(x + 0.5);
}

inline Point roundDot(Point &p)
{
	return Point(roundDot(p.x), roundDot(p.y));
}

/*
 * The Edge object for defining information for an edge of a polygon.
 * Edge class is used by polygon scan line filling algorithm as implimented in Polygon::initialise().
 */
class Edge
{
public:
	double delta_x;
	double delta_y;
	double C;
	bool includeBottom;
	double ymin;
	double ymax;
	int number; // used for debugging

	// Define < to be used for sorting edges in a list of ascending ymin.
	bool operator<(const Edge &rhs) const
	{
		return (ymin < rhs.ymin);
	}

	// Define a Edge line from point p1 to point p2.
	Edge(const Point &p1, const Point &p2)
		: includeBottom(false)
	{

		ymin = min(p1.y, p2.y);
		ymax = max(p1.y, p2.y);

		delta_x = p2.x - p1.x;
		delta_y = p2.y - p1.y;

		C = p1.x * delta_y - p1.y * delta_x;
	}

	// Line equation to return x coordinate from y.
	// Used by polygon fill algorithm to get the x coordinate of scan line intersect.
	inline double x(double y)
	{
		return (y * delta_x + C) / delta_y; // delta_y should never be zero, as such Edges are excluded from Edge table
	}
};

/*
 *  Polygon initialisation.
 *   - Sets min and max variables from vertex data.
 *   - Creates scan line intercept X data used for filling the polygon by scan line method.
 *
 *   This function shall be called after polygon vertex data has been created.
 */
void Polygon::initialise()
{
	// the screen minimum and maximum values are the pixel ranges of the polygon when plotted to a bitmap.
	pixelMinX = roundDot(vdata->minx + offset.x);
	pixelMaxX = pixelMinX + vdata->pixelWidth;
	pixelMinY = roundDot(vdata->miny + offset.y);
	pixelMaxY = pixelMinY + vdata->pixelHeigth;
	pixelOffsetX = roundDot(offset.x);
}

/*
 *  VertexData initialisation.
 *   - Sets min and max variables from vertex data.
 *   - Creates scan line intercept X data used for filling the polygon by scan line method.
 */
void VertexData::initialise()
{
	if (vertices.size() == 0) // nothing to do with no vertices
		return;

	minx = miny = INT_MAX;
	maxx = maxy = INT_MIN;

	for (int i = 0; i < static_cast<int>(vertices.size()); i++)
	{
		Point p = vertices[i];

		if (p.x < minx)
		{
			minx = p.x;
		}
		if (p.y < miny)
		{
			miny = p.y;
		}
		if (p.x > maxx)
		{
			maxx = p.x;
		}
		if (p.y > maxy)
		{
			maxy = p.y;
		}
	}

	pixelHeigth = roundDot(maxy - miny);
	pixelWidth = roundDot(maxx - minx);

	list<Edge> edges;
	list<Edge *> active; // Active edge list. Points to the  edges that intersect the current scan line

	Point p1 = vertices.back();

	// Build Global Edges Table
	// Edges are initially stored in list #edge in same order as polygon boundary path.
	// All horizontal edges are excluded from table.
	for (int i = 0; i < static_cast<int>(vertices.size()); i++)
	{
		Point p2 = vertices[i];
		if (p1.y != p2.y)
		{
			edges.push_back(Edge(p1, p2));
		}
		p1 = p2;
	}

	//
	// Determine edges that have a bottom vertex which has higher y value than either of it's neighbouring vertices.
	// Such edges are flagged includeBottom, so in the scan line loop, the edge is not removed from the active list
	// until the very bottom of the edge is scanned. This prevents vertices and bottom horizontal lines being missed in the plot.
	list<Edge>::iterator pit = edges.end();
	pit--;
	for (list<Edge>::iterator it = edges.begin(); it != edges.end(); it++)
	{
		// When this edge is pointing up (y1 > y2) and the previous edge pointing down (y2 > y1) then its a bottom
		if ((it->delta_y < 0) && (pit->delta_y > 0))
		{
			pit->includeBottom = true;
			it->includeBottom = true;
		}
		pit = it;
	}

	// Special case with  < 1 pixel high polygon that is assumed to be a single horizontal line.
	// Action: A single x1 x2 pair for horizontal line from polygon's minx to maxx at where y coordinate is at miny = maxy
	if (pixelHeigth == 0)
	{
		linesInCounts.push_back(2);
		gxIntersects.push_back(roundDot(minx));
		gxIntersects.push_back(roundDot(maxx));
		return;
	}

	// All edges in table are to be sorted with ascending ymin points.
	edges.sort();
	list<Edge>::iterator currentEdge = edges.begin();

	// Run through the scan lines
	double y = roundDot(miny) + 0.5;
	for (int linedc = pixelHeigth; linedc >= 0; linedc--, y += 1.0)
	{
		// Добавляем активные ребра в список
		while (currentEdge != edges.end() && y >= (currentEdge->ymin))
		{
			active.push_back(&(*currentEdge));
			currentEdge++;
		}

		// Remove edges from active list
		// When the scan line is equal to or greater than the bottom of the edge then it shall be removed.
		// This avoids double counting due to a joining edge below this edge.
		for (list<Edge *>::iterator it = active.begin(); it != active.end();)
		{
			if (y > ((*it)->ymax) || (y == ((*it)->ymax) && !(*it)->includeBottom))
			{
				it = active.erase(it);
				continue;
			}
			it++;
		}
		// printf(" y(%3f)  ", y);
		for (list<Edge *>::iterator it = active.begin(); it != active.end(); it++)
		{
			int x = roundDot((*it)->x(y));
			gxIntersects.push_back(x); // Store intersect X point as integer
									   //			printf("%4d ", x );
		}
		//		printf("\n");

		int sliCount = active.size();
		linesInCounts.push_back(sliCount);

		if (sliCount & 1)
			throw string("Execution error. (polygon scan line data not even)");

		// Sort all x intersections for this scan line
		sort(gxIntersects.end() - sliCount, gxIntersects.end());
	}
}

#ifdef DEBUG
printf("draw offset (%f, %f) \n", xOffsetDraw, yOffsetDraw);
printf("pixelMinY %d, pixelMaxY %d \n", pixelMinY, pixelMaxY);
printf("pixelMinx %d, pixelMaxx %d \n", pixelMinX, pixelMaxX);
printf("x range %f  %f  y range %f %f \n", vdata->minx, vdata->maxx, vdata->miny, vdata->maxy);
printf("polygon vertices %d, edge table size %d\n", vdata->vertices.size(), edges.size());
#endif
#ifdef DEBUG
it->number = pit->number + 1;
printf(" edge %3d (%5.3f  %5.3f) -> (%5.3f  %5.3f) %s %s\n", it->number,
	   it->x1, it->y1, it->x2, it->y2, it->includeBottom ? "bot" : "   ",
	   it->startAtVertex ? "vert" : "   ");
#endif
#ifdef DEBUG
printf("horizontal line polygon %d, %d\n", pixelMinX, pixelMaxX);
#endif
#ifdef DEBUG
printf(" remove %d\n", (*it)->number);
#endif
#ifdef DEBUG
printf(" add %d\n", active.back()->number);
#endif
#ifdef DEBUG
printf("\n");
#endif

/*
 * Append a vertex to polygon's vertices list.
 */
void VertexData::add(const Point &P)
{
	if ((vertices.size() == 0) || abs_sq(lastVertex - P) > 0.25)
	{
		vertices.push_back(P);
		lastVertex = P;
	}
}
void VertexData::add(double x, double y)
{
	add(Point(x, y));
}

/*
 *  Add vertices that follow an arc approximation
 */
void VertexData::addArc(double start_angle, double end_angle, double radius, double x0, double y0, bool clockwise)
{

	double deviaion = 0.01;
	if (radius < 0.5)
		radius = 0.5;
	if (radius < 150)
		deviaion *= (radius / 150);
	if (deviaion < 0.01)
		deviaion = 0.01;
	double step = 2 * acos(1 - deviaion / radius); // calculate minimum step magnitude to satisfy maximum deviation

	if (start_angle < 0)
		start_angle += 2 * M_PI;
	if (end_angle < 0)
		end_angle += 2 * M_PI;

	double theta = start_angle;
	double arc = end_angle - start_angle;
	if (arc < 0)
		arc += 2 * M_PI;
	if (clockwise)
		arc = 2 * M_PI - arc;

	int N = int(ceil(arc / step)); // get integer number of arc divisions
	step = arc / (N - 1);		   // re-calculate the step angle for integer divisions.
	if (N < 2)
		return;
	if (clockwise)
		step *= -1;

	for (int i = 0; i < N; i++)
	{
		double const x = radius * cos(theta) + x0;
		double const y = radius * sin(theta) + y0;
		theta += step;
		add(x, y);
	}
}

/*
 * Add vertices for a regular N sided polygon
 */
void VertexData::addRegularPolygon(double vertex_radius, double start_angle, int num_sides, double x0, double y0)
{
	if (num_sides < 3)
		return;
	double step = 2 * M_PI / double(num_sides);
	double theta = start_angle;
	// double radius = face_radius/cos(theta);  // radius is from centre to normal of face.
	double radius = vertex_radius; // radius defined from centre to a vertex

	for (int i = 0; i < num_sides; i++)
	{
		add(radius * cos(theta) + x0, radius * sin(theta) + y0);
		theta += step;
	}
}

/*
 * Add vertices for a rectangle, of width x_size, and height y_size and centre position at x0, y0.
 */
void VertexData::addRectangle(double x_size, double y_size, double x0, double y0)
{
	const double x1 = x0 - x_size / 2;
	const double y1 = y0 - y_size / 2;
	const double x2 = x1 + x_size;
	const double y2 = y1 + y_size;
	add(x1, y1);
	add(x2, y1);
	add(x2, y2);
	add(x1, y2);
}

/*
 * Rotate vertices of polygon about origin in the counter clockwise direction.
 * by <radian> radian.
 */
void VertexData::rotate(double theta)
{
	for (int i = 0; i < static_cast<int>(vertices.size()); i++)
	{
		vertices[i].rotate(theta);
	}
}

/*
 * Scale vertices of the polygon by multiplying all x coordinates by scaleX, and all y coordinates by scaleY
 */
void VertexData::scale(double scaleX, double scaleY)
{
	int N = vertices.size();
	if (N == 0)
		return;
	for (int i = 0; i < N; i++)
	{
		vertices[i].x *= scaleX;
		vertices[i].y *= scaleY;
	}
}

/*
 * Group shifting of all vertices
 */
void VertexData::shift(double x_shift, double y_shift)
{
	for (vector<Point>::iterator it = vertices.begin(); it != vertices.end(); it++)
	{
		it->x += x_shift;
		it->y += y_shift;
	}
}

/*
 * Rotate coordinates Point about origin with an angle <radian> in anti clockwise direction.
 */
void Point::rotate(const double &radian)
{
	double _x = x * cos(radian) - y * sin(radian);
	double _y = y * cos(radian) + x * sin(radian);
	x = _x;
	y = _y;
}
