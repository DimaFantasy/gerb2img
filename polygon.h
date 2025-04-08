#ifndef POLYGON_H_
#define POLYGON_H_

#include <limits.h>
#include <algorithm>

/*  Constants used to specify how objects are to be plotted to the bitmap */
typedef  enum {DARK, CLEAR, XOR} Polarity_t;


/*
 *  Point class - Defines a point with x,y real coordinates and methods for point(s) calculations.
 */
class Point
{
public:
	double x;
	double y;
	Point(double X, double Y) : x(X), y(Y) { }
	Point() { }
	Point operator*(const double &fac) { return Point(x*fac, y*fac); }
	Point operator/(const double &den) { return Point(x/den, y/den); }
	Point operator-(const Point &rhs) { return Point(x-rhs.x, y-rhs.y); }
	Point operator+(const Point &rhs) { return Point(x+rhs.x, y+rhs.y); }
	bool operator!=(const Point &rhs) { return x != rhs.x || y != rhs.y; }
	bool operator==(const Point &rhs) { return x == rhs.x && y == rhs.y; }
	void rotate(const double &radians_anticlockwise);
};

inline double abs(const Point &P) { return sqrt(P.x*P.x + P.y*P.y); }
inline double abs_sq(const Point &P) { return (P.x*P.x + P.y*P.y); }
inline double arg(const Point &P) { return atan2(P.y, P.x); }
inline Point polar(const double &rho, const double &theta) { return Point( rho*cos(theta), rho*sin(theta)); }

//
// A general line equation object
//
class Line
{
	double A;
	double B;
	double C;
public:
	// assign the line to points P1 and P2
	Line(const Point &P1, const Point &P2)
	{
		double dx = P2.x - P1.x;
		double dy = P2.y - P1.y;;
		if (fabs(dy) > fabs(dx))
		{
			A = -1;
			B = dx/dy;
			C = P1.x - P1.y*B;
		} else
		{
			A = -dy/dx;
			B = 1;
			C = - P1.y - P1.x*A;
		}
	}
	// moves line to intersect P leaving slope unchanged
	void moveParalell(const Point &P)
	{
		C = -A*P.x - B*P.y;
	}
	// moves line to intersect P and changes slope perpendicular
	void movePerpendicular(const Point &P)
	{
		C = B*P.x - A*P.y;
		std::swap(A,B);
		A = -A;
	}
	// return the point where #test will intersect
	// If #test is parallel then returns the point (0,0)
	Point intersect(const Line &test)
	{
		Point P(0,0);
		if (fabs(test.A) > fabs(test.B))
		{
			double b = B - (A/test.A) * test.B;
			double c = C -(A/test.A) * test.C;
			if (b != 0)
			{
				P.y = -c/b;
				P.x = -(test.C + P.y * test.B) / test.A;
			}
			return P;
		}
		else
		{
			double a = A - (B/test.B) * test.A;
			double c = C - (B/test.B) * test.C;
			if (a != 0)
			{
				P.x = -c/a;
				P.y = -(test.C + P.x * test.A) / test.B;
			}
			return P;
		}
	}
};


/*
 * VertexData set of vertices and handles scan line filling.
 *
 * Polygons can have a common set of vertices and therefore much processing time is saved by sharing the vertices and
 * scan line filling process. Each polygon has individual data for screen positioning, screen limits and rotation.
 */
class VertexData
{
private:
	std::vector<int> gxIntersects;	// Vector of x coordinates that intersect each edge of polygon on consecutive scan lines
    std::vector<int> linesInCounts;	// For each scan line, linesInCounts holds number of x intersections.
	Point lastVertex;
	friend class Polygon;
	int pixelHeigth;
	int pixelWidth;

public:
	std::vector<Point> vertices;	// All vertices in polygon
	double minx, miny, maxx, maxy;

	bool empty()   	{ return (vertices.size()==0); }
	void scale(double scaleX,  double scaleY );
	void rotate( double radian);
	void shift( double x_offset, double y_offset);
	void add( double  x, double y );
	void add( const Point &P );
	void addArc( double start_angle, double end_angle, double radius, double x0=0, double y0=0, bool clockwise=false);
	void addRegularPolygon( double face_radius, double start_angle, int num_sides, double x0=0, double y0=0);
	void addRectangle( double x_size, double y_size, double x0=0, double y0=0);
	void initialise();
};


/*
 * The Polygon class.
 * Contains methods to created vertices that make up a general complex polygon.
 * Once vertices are created, the initialise() function is called to create the scan line filling data
 */
class Polygon
{
private:
	int * nextInTable;
	int * nextInCount;

public:
	VertexData * vdata;
	int pixelMinX, pixelMinY, pixelMaxX, pixelMaxY;
	int pixelOffsetX;
	Point offset;

	int number;										// used to sort order of creation of polygons
	Polarity_t polarity;							// The plotting polarity

	Polygon () 
		: nextInTable(0)
		, nextInCount(0)
		, vdata(new VertexData)
		, pixelMinX(0), pixelMinY(0), pixelMaxX(0), pixelMaxY(0)
		, pixelOffsetX(0)
		, offset(0,0)
		, number(0)
		, polarity(DARK) // default to a dark polarity
	{ }

	void initialise();
	std::vector<int> const & getNextLineX1X2Pairs();
	bool empty()   	{ return vdata->empty(); }
	bool operator<(const Polygon &rhs) const
	{
		return pixelMinY < rhs.pixelMinY;
	}

	/*
	 * Each call will return the polygon edge intercepting data for the next scan line. The first call will be for the first scan line of
	 * the polygon.    Used for plotting filled polygon to a bitmap. Horizontal lines are to be drawn between each odd and even pair in the
	 * x intercept data pointed to by sliTable.
	 */
	void getNextLineX1X2Pairs(int * &sliTable, int &sliCount )
	{
		 // Resets the scan line counters to zero  on first call to this function
		if (nextInCount == 0)
		{
			nextInCount = &vdata->linesInCounts.front();
			nextInTable = &vdata->gxIntersects.front();
		}

		sliCount = *nextInCount;
		sliTable = nextInTable;
		nextInTable += sliCount;
		nextInCount++;
	}
};



class PolygonReference
{
public:
	Polygon *polygon;
	bool operator<( const PolygonReference &rhs) const
	{
		return polygon->number < rhs.polygon->number;
	}
};



#endif /*POLYGON_H_*/
