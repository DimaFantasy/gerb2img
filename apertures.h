// This file is distributed under the terms of the GNU General Public License v3.

#ifndef APERTURES_H_
#define APERTURES_H_

#include <string>
#include <map>
#include <sstream>
#include <list>



class NodeT
{
public:
	const enum NodeType { VAR, CONSTANT, OPMUL ,OPDIV, OPADD, OPSUB, OPNEG } type;

	union {
		struct NodeT *node;
		double value;
		int	varID;
	} arg1;

	union {
		std::vector<double> * variables;
		struct NodeT *node;
	} arg2;

	NodeT(enum NodeType type, const void * parg1=0, const void * parg2=0)
		: type(type)
	{
		switch (type)
		{
		case OPNEG:
		case OPADD:
		case OPSUB:
		case OPMUL:
		case OPDIV:
			arg1.node = (NodeT*)parg1;
			arg2.node = (NodeT*)parg2;
			break;
		case CONSTANT:
			arg1.value = *(double*)parg1;
			break;
		case VAR:
			arg1.varID = *(int*)parg1;
			arg2.variables = (std::vector<double> *)parg2;
			break;
		}
	}
	double evaluate()
	{
		switch (type)
		{
		case OPNEG:		return -arg1.node->evaluate();
		case OPADD:		return arg1.node->evaluate() + arg2.node->evaluate();
		case OPSUB:		return arg1.node->evaluate() - arg2.node->evaluate();
		case OPMUL:		return arg1.node->evaluate() * arg2.node->evaluate();
		case OPDIV:
			{
				double d = arg2.node->evaluate();
				if (d == 0) throw std::string("division by zero");
				return arg1.node->evaluate() / d;
			}
		case CONSTANT:	return arg1.value;
		case VAR:
			if (arg1.varID >= static_cast<int>(arg2.variables->size()))
			{
				std::ostringstream oss;
				oss<<"variable $"<<(arg1.varID+1)<<" has not been assigned";
				throw oss.str();
			}
			return (*arg2.variables)[arg1.varID];
		}
		return 0; // Возврат значения по умолчанию
	}
};

class Gerber;

// abstract class; defines base members for an aperture object.
// Pure virtual functions are defined. Derived classes (one for each
// type of aperture) initialize the aperture pattern for plotting to the
// monochrome bitmap buffer
//
class Aperture
{
	public:
		enum PRIMITIVE {STANDARD_CIRCLE = 'C',
						STANDARD_RECTANGLE = 'R',
						STANDARD_ORBROUND = 'O',
						STANDARD_POLYGON = 'P',
						SPECIAL_CIRCLE = 1,
						SPECIAL_LINE_VECTOR  = 2,
						SPECIAL_LINE_VECTOR2 = 20,
						SPECIAL_LINE_CENTER = 21,
						SPECIAL_LINE_LOLEFT = 22,
						SPECIAL_OUTLINE = 4,
						SPECIAL_POLYGON = 5,
						SPECIAL_MOIRE = 6,
						SPECIAL_THERMAL = 7,
						PRIMITIVE_INVALID = -1
						} primitive;

        Aperture * composite;					// points to the next Aperture object for a composite macro aperture
        int linenum_at_definition;				// line in Gerber file where aperture was defined
        const char * rs274x_name();				// name of aperture type described in RS-274X
		double grow_size;						// number of pixels to expand all object outlines
		double standardApWidth;					// Holds the width and height for a standard type aperture (equals 0 for Special Ap.)
		double standardApHeight;				//  used to determine track width when linear and arc drawing plotting


		std::vector< NodeT  * > parameter;

        int   		DCode;
        std::string nameMacro;
        std::list<Polygon> 	polygons;			// A list of polygons that making up this aperture. Created by member render()

        double getParameter(int index);
		void render(const double dots_per_unit, const double grow_size, int ADmodifierCount );

        Aperture()
        {
        	standardApWidth = 0;
        	standardApHeight = 0;
            composite = 0;
            primitive = PRIMITIVE_INVALID;
            DCode = -1;
        }
};


#endif  // APERTURES_H_
