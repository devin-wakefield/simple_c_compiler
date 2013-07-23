/*
 * File:	checker.cpp
 *
 * Description:	This file contains the public and private function and
 *		variable definitions for the semantic checker for Simple C.
 *
 *		Extra functionality:
 *		- the global error and integer types for convenience
 *		- scaling the operands and results of pointer arithmetic
 *		- explicit type conversions and promotions
 */

# include <iostream>
# include "lexer.h"
# include "checker.h"
# include "nullptr.h"
# include "tokens.h"
# include "Symbol.h"
# include "Scope.h"
# include "Type.h"

using namespace std;

static Scope *outermost, *toplevel;
static const Type error, integer(INT), real(DOUBLE);

static string invalid_return = "invalid return type";
static string invalid_test = "invalid type for test expression";
static string invalid_lvalue = "invalid lvalue in expression";
static string invalid_operands = "invalid operands to binary %s";
static string invalid_operand = "invalid operand to unary %s";
static string invalid_cast = "invalid operand in cast expression";
static string invalid_function = "called object is not a function";
static string invalid_arguments = "invalid arguments to called function";

static string redeclared_function = "function %s is previously declared";
static string redeclared_variable = "variable %s is previously declared";
static string redeclared_parameter = "parameter %s is previously declared";
static string undeclared_identifier = "%s is undeclared";


/*
 * Function:	promote
 *
 * Description:	Attempt to promote an array expression to a pointer.
 */

static Type promote(Expression *&expr)
{
    if (expr->type().isArray()) {
	// cout << "promoting array to pointer" << endl;
	expr = new Address(expr, expr->type().promote());
    }

    return expr->type();
}


/*
 * Function:	promote
 *
 * Description:	Attempt to promote the given expression to the specified
 *		type.  If necessary, an array is also promoted.
 */

static Type promote(Expression *&expr, Type type)
{
    if (expr->type() == integer && type == real) {
	// cout << "promoting int to double" << endl;
	//
	if (dynamic_cast<Integer *>(expr))
	    expr = new Real(((Integer *) expr)->value());
	else
	    expr = new Cast(real, expr);
    }

    return promote(expr);
}


/*
 * Function:	convert
 *
 * Description:	Attempt to convert the given expression to the given
 *		type by truncation or promotion.
 */

static Type convert(Expression *&expr, Type type)
{
    if (expr->type() == real && type == integer) {
	// cout << "converting double to int" << endl;
	expr = new Cast(integer, expr);
    }

    return promote(expr, type);
}


/*
 * Function:	openScope
 *
 * Description:	Create a scope and make it the new top-level scope.
 */

Scope *openScope()
{
    toplevel = new Scope(toplevel);

    if (outermost == nullptr)
	outermost = toplevel;

    return toplevel;
}


/*
 * Function:	closeScope
 *
 * Description:	Remove the top-level scope, and make its enclosing scope
 *		the new top-level scope.
 */

Scope *closeScope()
{
    Scope *old = toplevel;
    toplevel = toplevel->enclosing();
    return old;
}


/*
 * Function:	declareFunction
 *
 * Description:	Declare a function with the specified NAME and TYPE.  A
 *		function is always declared in the outermost scope.
 */

Symbol *declareFunction(const string &name, const Type &type)
{
    Symbol *symbol = outermost->find(name);

    if (symbol != nullptr) {
	report(redeclared_function, name);
	outermost->remove(name);
	delete symbol;
    }

    symbol = new Symbol(name, type);
    outermost->insert(symbol);
    return symbol;
}


/*
 * Function:	declareVariable
 *
 * Description:	Declare a variable with the specified NAME and TYPE.
 */

Symbol *declareVariable(const string &name, const Type &type)
{
    Symbol *symbol = toplevel->find(name);

    if (symbol != nullptr) {
	report(redeclared_variable, name);
	toplevel->remove(name);
	delete symbol;
    }

    symbol = new Symbol(name, type);
    toplevel->insert(symbol);
    return symbol;
}


/*
 * Function:	declareParameter
 *
 * Description:	Declare a parameter with the specified NAME and TYPE.
 */

Symbol *declareParameter(const string &name, const Type &type)
{
    Symbol *symbol = toplevel->find(name);

    if (symbol != nullptr) {
	report(redeclared_parameter, name);
	toplevel->remove(name);
	delete symbol;
    }

    symbol = new Symbol(name, type);
    toplevel->insert(symbol);
    return symbol;
}


/*
 * Function:	checkIdentifier
 *
 * Description:	Check if NAME is declared.  If it is undeclared, then
 *		declare it as having the error type in order to eliminate
 *		future error messages.
 */

Symbol *checkIdentifier(const string &name)
{
    Symbol *symbol = toplevel->lookup(name);

    if (symbol == nullptr) {
	report(undeclared_identifier, name);
	symbol = new Symbol(name, error);
	toplevel->insert(symbol);
    }

    return symbol;
}


/*
 * Function:	checkCall
 *
 * Description:	Check a function call expression: the type of the object
 *		being called must be a function type, and the number and
 *		types of arguments must agree.
 */

Expression *checkCall(const Symbol *id, Expressions &args)
{
    const Type &t = id->type();
    Type result = error;


    if (t != error) {
	if (!t.isFunction())
	    report(invalid_function);

    	else {
	    Parameters *params = t.parameters();
	    result = Type(t.specifier(), t.indirection());

	    if (params != nullptr) {
		if (params->size() != args.size())
		    report(invalid_arguments);

		else {
		    for (unsigned i = 0; i < args.size(); i ++)
			if (convert(args[i], (*params)[i]) != (*params)[i]) {
			    report(invalid_arguments);
			    result = error;
			    break;
			}
		}
	    } else
		for (unsigned i = 0; i < args.size(); i ++)
		    promote(args[i]);
	}
    }

    return new Call(id, args, result);
}


/*
 * Function:	checkArray
 *
 * Description:	Check an array index expression: the left operand must have
 *		type "pointer to T" and the right operand must have type
 *		int, and the result has type T.
 *
 *		pointer(T) x int -> T
 */

Expression *checkArray(Expression *left, Expression *right)
{
    const Type &t1 = promote(left);
    const Type &t2 = right->type();
    Type result = error;

    right = new Multiply(right, new Integer(t1.deref().size()), integer);
    Expression *expr = new Add(left, right, t1);

    if (t1 != error && t2 != error) {
	if (t1.isPointer() && t2 == integer)
	    result = t1.deref();
	else
	    report(invalid_operands, "[]");
    }

    return new Dereference(expr, result);
}


/*
 * Function:	checkNot
 *
 * Description:	Check a logical negation expression: the operand must have a
 *		value type, and the result has type int.
 *
 *		int -> int
 *		double -> int
 *		pointer(T) -> int
 */

Expression *checkNot(Expression *expr)
{
    const Type &t = promote(expr);
    Type result = error;


    if (t != error) {
	if (t.isValue())
	    result = integer;
	else
	    report(invalid_operand, "!");
    }

    return new Not(expr, result);
}


/*
 * Function:	checkNegate
 *
 * Description:	Check an arithmetic negation expression: the operand must
 *		have a numeric type, and the result has that type.
 *
 *		int -> int
 *		double -> double
 */

Expression *checkNegate(Expression *expr)
{
    const Type &t = expr->type();
    Type result = error;


    if (t != error) {
	if (t.isNumeric())
	    result = t;
	else
	    report(invalid_operand, "-");
    }

    return new Negate(expr, result);
}


/*
 * Function:	checkDereference
 *
 * Description:	Check a dereference expression: the operand must have type
 *		"pointer to T," and the result has type T.
 *
 *		pointer(T) -> T
 */

Expression *checkDereference(Expression *expr)
{
    const Type &t = promote(expr);
    Type result = error;


    if (t != error) {
	if (t.isPointer())
	    result = t.deref();
	else
	    report(invalid_operand, "*");
    }

    return new Dereference(expr, result);
}


/*
 * Function:	checkAddress
 *
 * Description:	Check an address expression: the operand must be an lvalue,
 *		and if the operand has type T, then the result has type
 *		"pointer to T."
 *
 *		T -> pointer(T)
 */

Expression *checkAddress(Expression *expr)
{
    const Type &t = expr->type();
    Type result = error;


    if (t != error) {
	if (expr->lvalue())
	    result = Type(t.specifier(), t.indirection() + 1);
	else
	    report(invalid_lvalue);
    }

    return new Address(expr, result);
}


/*
 * Function:	checkCast
 *
 * Description:	Check a cast expression: the result and operand must either
 *		both have numeric types, both have pointer types, or one
 *		has the type integer and the other has a pointer type.
 *
 *		int -> int
 *		double -> int
 *		pointer(T) -> int
 *		int -> double
 *		double -> double
 *		int -> pointer(T)
 *		pointer(S) -> pointer(T)
 */

Expression *checkCast(const Type &type, Expression *expr)
{
    const Type &t = promote(expr);
    Type result = error;

    if (t != error) {
	if (type.isNumeric() && t.isNumeric())
	    result = type;
	else if (type.isPointer() && (t.isPointer() || t == integer))
	    result = type;
	else if (t.isPointer() && type == integer)
	    result = type;
	else
	    report(invalid_cast);
    }

    return new Cast(result, expr);
}


/*
 * Function:	checkMult
 *
 * Description:	Check a multiplication expression: both operands must have
 *		a numeric type, and the result has type double if either
 *		has type double and has type int otherwise.
 *
 *		int x int -> int
 *		double x double -> double;
 */

static Type checkMult(Expression *&left, Expression *&right, const string &op)
{
    const Type &t1 = promote(left, right->type());
    const Type &t2 = promote(right, left->type());
    Type result = error;


    if (t1 != error && t2 != error) {
	if (t1.isNumeric() && t2.isNumeric())
	    result = t1;
	else
	    report(invalid_operands, op);
    }

    return result;
}


/*
 * Function:	checkMultiply
 *
 * Description:	Check a multiplication expression.
 */

Expression *checkMultiply(Expression *left, Expression *right)
{
    Type t = checkMult(left, right, "*");
    return new Multiply(left, right, t);
}


/*
 * Function:	checkDivide
 *
 * Description:	Check a division expression.
 */

Expression *checkDivide(Expression *left, Expression *right)
{
    Type t = checkMult(left, right, "/");
    return new Divide(left, right, t);
}


/*
 * Function:	checkRemainder
 *
 * Description:	Check a remainder expression: both operands must have type
 *		int, and the result has type int.
 *
 *		int x int -> int
 */

Expression *checkRemainder(Expression *left, Expression *right)
{
    const Type &t1 = left->type();
    const Type &t2 = right->type();
    Type result = error;


    if (t1 != error && t2 != error) {
	if (t1 == integer && t2 == integer)
	    result = integer;
	else
	    report(invalid_operands, "%");
    }

    return new Remainder(left, right, result);
}


/*
 * Function:	checkAdd
 *
 * Description:	Check an addition expression: if both operands have numeric
 *		types, then the result has type int if both have type int
 *		and has type double otherwise; if one operand has a pointer
 *		type and the other operand has type int, then the result
 *		has that pointer type.
 *
 *		int x int -> int
 *		double x double -> double
 *		pointer(T) x int -> pointer(T)
 *		int x pointer(T) -> pointer(T)
 */

Expression *checkAdd(Expression *left, Expression *right)
{
    const Type &t1 = promote(left, right->type());
    const Type &t2 = promote(right, left->type());
    Type result = error;


    if (t1 != error && t2 != error) {
	if (t1.isNumeric() && t2.isNumeric())
	    result = t1;

	else if (t1.isPointer() && t2 == integer) {
	    right = new Multiply(right, new Integer(t1.deref().size()), integer);
	    result = t1;

	} else if (t1 == integer && t2.isPointer()) {
	    left = new Multiply(left, new Integer(t2.deref().size()), integer);
	    result = t2;

	} else
	    report(invalid_operands, "+");
    }

    return new Add(left, right, result);
}


/*
 * Function:	checkSubtract
 *
 * Description:	Check a subtraction expression: if both operands have
 *		types, then the result has type int if both have type int
 *		and has type double otherwise; if the left operand has a
 *		pointer type and the right operand has type int, then the
 *		result has that pointer type; if both operands are
 *		identical pointer types, then the result has type int.
 *
 *		int x int -> int
 *		double x double -> double
 *		pointer(T) x int -> pointer(T)
 *		pointer(T) x pointer(T) -> int
 */

Expression *checkSubtract(Expression *left, Expression *right)
{
    Expression *tree;
    const Type &t1 = promote(left, right->type());
    const Type &t2 = promote(right, left->type());
    Type result = error;
    Type deref;


    if (t1 != error && t2 != error) {
	if (t1.isNumeric() && t2.isNumeric())
	    result = t1;

	else if (t1.isPointer() && t1 == t2)
	    result = integer;

	else if (t1.isPointer() && t2 == integer) {
	    right = new Multiply(right, new Integer(t1.deref().size()), integer);
	    result = t1;

	} else
	    report(invalid_operands, "-");
    }

    tree = new Subtract(left, right, result);

    if (t1.isPointer() && t1 == t2)
	tree = new Divide(tree, new Integer(t1.deref().size()), integer);

    return tree;
}


/*
 * Function:	checkCompare
 *
 * Description:	Check an equality or relational expression: the types of
 *		both operands much be compatible, and the result has type
 *		int.
 *
 *		int x int -> int
 *		double x double -> int
 *		pointer(T) x pointer(T) -> int
 */

static Type checkCompare(Expression *&left, Expression *&right, const string &op)
{
    const Type &t1 = promote(left, right->type());
    const Type &t2 = promote(right, left->type());
    Type result = error;


    if (t1 != error && t2 != error) {
	if (t1 == t2 && t1.isValue())
	    result = integer;
	else
	    report(invalid_operands, op);
    }

    return result;
}


/*
 * Function:	checkEqual
 *
 * Description:	Check an equality expression: left == right.
 */

Expression *checkEqual(Expression *left, Expression *right)
{
    Type t = checkCompare(left, right, "==");
    return new Equal(left, right, t);
}


/*
 * Function:	checkNotEqual
 *
 * Description:	Check an inequality expression: left != right.
 */

Expression *checkNotEqual(Expression *left, Expression *right)
{
    Type t = checkCompare(left, right, "!=");
    return new NotEqual(left, right, t);
}


/*
 * Function:	checkLessThan
 *
 * Description:	Check a less-than expression: left < right.
 */

Expression *checkLessThan(Expression *left, Expression *right)
{
    Type t = checkCompare(left, right, "<");
    return new LessThan(left, right, t);
}


/*
 * Function:	checkGreaterThan
 *
 * Description:	Check a greater-than expression: left > right.
 */

Expression *checkGreaterThan(Expression *left, Expression *right)
{
    Type t = checkCompare(left, right, ">");
    return new GreaterThan(left, right, t);
}


/*
 * Function:	checkLessOrEqual
 *
 * Description:	Check a less-than-or-equal expression: left <= right.
 */

Expression *checkLessOrEqual(Expression *left, Expression *right)
{
    Type t = checkCompare(left, right, "<=");
    return new LessOrEqual(left, right, t);
}


/*
 * Function:	checkGreaterOrEqual
 *
 * Description:	Check a greater-than-or-equal expression: left >= right.
 */

Expression *checkGreaterOrEqual(Expression *left, Expression *right)
{
    Type t = checkCompare(left, right, ">=");
    return new GreaterOrEqual(left, right, t);
}


/*
 * Function:	checkLogical
 *
 * Description:	Check a logical-or or logical-and expression: the types of
 *		both operands must be value types and the result has type
 *		int.
 *
 *		int x int -> int
 *		int x double -> int
 *		int x pointer(T) -> int
 *		double x int -> int
 *		double x double -> int
 *		double x pointer(T) -> int
 *		pointer(T) x int -> int
 *		pointer(T) x double -> int
 *		pointer(S) x pointer(T) -> int
 */

static Type checkLogical(Expression *&left, Expression *&right, const string &op)
{
    const Type &t1 = promote(left);
    const Type &t2 = promote(right);
    Type result = error;


    if (t1 != error && t2 != error) {
	if (t1.isValue() && t2.isValue())
	    result = integer;
	else
	    report(invalid_operands, op);
    }

    return result;
}


/*
 * Function:	checkLogicalAnd
 *
 * Description:	Check a logical-and expression: left && right.
 */

Expression *checkLogicalAnd(Expression *left, Expression *right)
{
    Type t = checkLogical(left, right, "&&");
    return new LogicalAnd(left, right, t);
}


/*
 * Function:	checkLogicalOr
 *
 * Description:	Check a logical-or expression: left || right.
 */

Expression *checkLogicalOr(Expression *left, Expression *right)
{
    Type t = checkLogical(left, right, "||");
    return new LogicalOr(left, right, t);
}


/*
 * Function:	checkAssign
 *
 * Description:	Check an assignment expression: the left operand must be an
 *		lvalue and the types of the operands must be compatible,
 *		and the result has the type of the left operand.
 *
 *		int x int -> int
 *		int x double -> int
 *		double x int -> double
 *		double x double -> double
 *		pointer(T) x pointer(T) -> pointer(T)
 */

Expression *checkAssign(Expression *left, Expression *right)
{
    const Type &t1 = left->type();
    const Type &t2 = convert(right, left->type());
    Type result = error;


    if (t1 != error && t2 != error) {
	if (!left->lvalue())
	    report(invalid_lvalue);

	else if (t1 == t2 && t1.isValue())
	    result = t1;

	else
	    report(invalid_operands, "=");
    }

    return new Assign(left, right, result);
}


/*
 * Function:	checkReturn
 *
 * Description:	Check a return statement: the type of the expression must
 *		be compatible with the given type, which should be the
 *		return type of the enclosing function.
 */

void checkReturn(Expression *&expr, const Type &type)
{
    const Type &t = convert(expr, type);

    if (t != error && t != type)
	report(invalid_return);
}


/*
 * Function:	checkTest
 *
 * Description:	Check if the type is a legal type in a test expression in a
 *		while, if-then, or if-then-else statement: the type must be
 *		a value type.
 */

void checkTest(Expression *&expr)
{
    const Type &type = promote(expr);


    if (type != error && !type.isValue())
	report(invalid_test);
}
