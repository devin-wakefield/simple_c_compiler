/*
 * File:	Symbol.cpp
 *
 * Description:	This file contains the member function definitions for
 *		symbols in Simple C.  A symbol consists of a name, a type,
 *		and an offset.
 */

# include "Symbol.h"

using std::string;


/*
 * Function:	Symbol::Symbol (constructor)
 *
 * Description:	Initialize a symbol object.
 */

Symbol::Symbol(const string &name, const Type &type)
    : _name(name), _type(type), _offset(0)
{
}


/*
 * Function:	Symbol::name (accessor)
 *
 * Description:	Return the name of this symbol.
 */

const string &Symbol::name() const
{
    return _name;
}


/*
 * Function:	Symbol::type (accessor)
 *
 * Description:	Return the type of this symbol.
 */

const Type &Symbol::type() const
{
    return _type;
}


/*
 * Function:	Symbol::offset (accessor)
 *
 * Description:	Return the offset of this symbol.
 */

int Symbol::offset() const
{
    return _offset;
}


/*
 * Function:	Symbol::offset (mutator)
 *
 * Description:	Update the offset of this symbol.
 */

Symbol &Symbol::offset(int offset)
{
    _offset = offset;
    return *this;
}
