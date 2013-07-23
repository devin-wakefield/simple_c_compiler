/*
 * File:	parser.c
 *
 * Description:	This file contains the public and private function and
 *		variable definitions for the recursive-descent parser for
 *		Simple C.
 */

# include <cstdlib>
# include <iostream>
# include "generator.h"
# include "checker.h"
# include "tokens.h"
# include "lexer.h"

using namespace std;

static int lookahead, nexttoken;
static string lexbuf, nextbuf;
static Type returnType;

static Expression *expression();
static Statement *statement();

static Symbols globals;


/*
 * Function:	error
 *
 * Description:	Report a syntax error to standard error.
 */

static void error()
{
    if (lookahead == DONE)
	report("syntax error at end of file");
    else
	report("syntax error at '%s'", lexbuf);

    exit(EXIT_FAILURE);
}


/*
 * Function:	match
 *
 * Description:	Match the next token against the specified token.  A
 *		failure indicates a syntax error and will terminate the
 *		program since our parser does not do error recovery.
 */

static void match(int t)
{
    if (lookahead != t)
	error();

    if (nexttoken) {
	lookahead = nexttoken;
	lexbuf = nextbuf;
	nexttoken = 0;
    } else
	lookahead = lexan(lexbuf);
}


/*
 * Function:	peek
 *
 * Description:	Return the next token in the input stream and save it so
 *		that match() will later return it.
 */

static int peek()
{
    if (!nexttoken)
	nexttoken = lexan(nextbuf);

    return nexttoken;
}


/*
 * Function:	expect
 *
 * Description:	Match the next token against the specified token, and
 *		return its lexeme.  We must save the contents of the buffer
 *		from the lexical analyzer before matching, since matching
 *		will advance to the next token.
 */

static string expect(int t)
{
    string buf = lexbuf;
    match(t);
    return buf;
}


/*
 * Function:	specifier
 *
 * Description:	Parse a type specifier.  Simple C has only ints and
 *		doubles.
 *
 *		specifier:
 *		  int
 *		  double
 */

static int specifier()
{
    if (lookahead == INT) {
	match(INT);
	return INT;
    }

    if (lookahead == DOUBLE) {
	match(DOUBLE);
	return DOUBLE;
    }

    error();
    return ERROR;
}


/*
 * Function:	pointers
 *
 * Description:	Parse pointer declarators (i.e., zero or more asterisks).
 *
 *		pointers:
 *		  empty
 *		  * pointers
 */

static unsigned pointers()
{
    unsigned count = 0;


    while (lookahead == '*') {
	match('*');
	count ++;
    }

    return count;
}


/*
 * Function:	declarator
 *
 * Description:	Parse a declarator, which in Simple C is either a scalar
 *		variable or an array, with optional pointer declarators.
 *
 *		declarator:
 *		  pointers identifier
 *		  pointers identifier [ integer ]
 */

static void declarator(int typespec)
{
    unsigned indirection, length;
    string name;


    indirection = pointers();
    name = expect(ID);

    if (lookahead == '[') {
	match('[');
	length = strtoul(expect(INTEGER).c_str(), NULL, 0);
	declareVariable(name, Type(typespec, indirection, length));
	match(']');

    } else
	declareVariable(name, Type(typespec, indirection));
}


/*
 * Function:	declaration
 *
 * Description:	Parse a local variable declaration.  Global declarations
 *		are handled separately since we need to detect a function
 *		as a special case.
 *
 *		declaration:
 *		  specifier declarator-list ';'
 *
 *		declarator-list:
 *		  declarator
 *		  declarator , declarator-list
 */

static void declaration()
{
    int typespec;


    typespec = specifier();
    declarator(typespec);

    while (lookahead == ',') {
	match(',');
	declarator(typespec);
    }

    match(';');
}


/*
 * Function:	declarations
 *
 * Description:	Parse a possibly empty sequence of declarations.
 *
 *		declarations:
 *		  empty
 *		  declaration declarations
 */

static void declarations()
{
    while (lookahead == INT || lookahead == DOUBLE)
	declaration();
}


/*
 * Function:	argument
 *
 * Description:	Parse an argument to a function call.  The only place
 *		string literals are allowed in Simple C is here, to
 *		facilitate calling printf(), scanf(), and the like.
 *
 *		argument:
 *		  string
 *		  expression
 */

static Expression *argument()
{
    if (lookahead == STRING)
	return new String(expect(STRING));

    return expression();
}


/*
 * Function:	primaryExpression
 *
 * Description:	Parse a primary expression.
 *
 *		primary-expression:
 *		  ( expression )
 *		  identifier ( argument-list )
 *		  identifier ( )
 *		  identifier
 *		  integer
 *		  real
 *
 *		argument-list:
 *		  argument
 *		  argument , argument-list
 */

static Expression *primaryExpression()
{
    Expressions args;
    Expression *expr;
    Symbol *symbol;


    if (lookahead == '(') {
	match('(');
	expr = expression();
	match(')');

    } else if (lookahead == INTEGER) {
	expr = new Integer(expect(INTEGER));

    } else if (lookahead == REAL) {
	expr = new Real(expect(REAL));

    } else if (lookahead == ID) {
	symbol = checkIdentifier(expect(ID));

	if (lookahead == '(') {
	    match('(');

	    if (lookahead != ')') {
		args.push_back(argument());

		while (lookahead == ',') {
		    match(',');
		    args.push_back(argument());
		}
	    }

	    expr = checkCall(symbol, args);
	    match(')');

	} else
	    expr = new Identifier(symbol);

    } else {
	expr = nullptr;
	error();
    }

    return expr;
}


/*
 * Function:	postfixExpression
 *
 * Description:	Parse a postfix expression.
 *
 *		postfix-expression:
 *		  primary-expression
 *		  postfix-expression [ expression ]
 */

static Expression *postfixExpression()
{
    Expression *left, *right;


    left = primaryExpression();

    while (lookahead == '[') {
	match('[');
	right = expression();
	left = checkArray(left, right);
	match(']');
    }

    return left;
}


/*
 * Function:	unaryExpression
 *
 * Description:	Parse a unary expression.
 *
 *		unary-expression:
 *		  postfix-expression
 *		  ! unary-expression
 *		  - unary-expression
 *		  * unary-expression
 *		  & unary-expression
 *		  sizeof unary-expression
 *		  sizeof ( specifier pointers )
 */

static Expression *unaryExpression()
{
    Expression *expr;
    unsigned indirection;
    int typespec;
    Type type;


    if (lookahead == '!') {
	match('!');
	expr = unaryExpression();
	expr = checkNot(expr);

    } else if (lookahead == '-') {
	match('-');
	expr = unaryExpression();
	expr = checkNegate(expr);

    } else if (lookahead == '*') {
	match('*');
	expr = unaryExpression();
	expr = checkDereference(expr);

    } else if (lookahead == '&') {
	match('&');
	expr = unaryExpression();
	expr = checkAddress(expr);

    } else if (lookahead == SIZEOF) {
	match(SIZEOF);

	if (lookahead == '(' && (peek() == INT || peek() == DOUBLE)) {
	    match('(');
	    typespec = specifier();
	    indirection = pointers();
	    match(')');
	    type = Type(typespec, indirection);

	} else {
	    expr = unaryExpression();
	    type = expr->type();
	}

	expr = new Integer(type.size());

    } else
	expr = postfixExpression();

    return expr;
}


/*
 * Function:	castExpression
 *
 * Description:	Parse a cast expression.  If the token after the opening
 *		parenthesis is not a type specifier, we could have a
 *		parenthesized expression instead.
 *
 *		cast-expression:
 *		  unary-expression
 *		  ( specifier pointers ) cast-expression
 */

static Expression *castExpression()
{
    Expression *expr;
    unsigned indirection;
    int typespec;


    if (lookahead == '(' && (peek() == INT || peek() == DOUBLE)) {
	match('(');
	typespec = specifier();
	indirection = pointers();
	match(')');
	expr = castExpression();
	expr = checkCast(Type(typespec, indirection), expr);

    } else
	expr = unaryExpression();

    return expr;
}


/*
 * Function:	multiplicativeExpression
 *
 * Description:	Parse a multiplicative expression.
 *
 *		multiplicative-expression:
 *		  cast-expression
 *		  multiplicative-expression * cast-expression
 *		  multiplicative-expression / cast-expression
 *		  multiplicative-expression % cast-expression
 */

static Expression *multiplicativeExpression()
{
    Expression *left, *right;


    left = castExpression();

    while (1) {
	if (lookahead == '*') {
	    match('*');
	    right = castExpression();
	    left = checkMultiply(left, right);

	} else if (lookahead == '/') {
	    match('/');
	    right = castExpression();
	    left = checkDivide(left, right);

	} else if (lookahead == '%') {
	    match('%');
	    right = castExpression();
	    left = checkRemainder(left, right);

	} else
	    break;
    }

    return left;
}


/*
 * Function:	additiveExpression
 *
 * Description:	Parse an additive expression.
 *
 *		additive-expression:
 *		  multiplicative-expression
 *		  additive-expression + multiplicative-expression
 *		  additive-expression - multiplicative-expression
 */

static Expression *additiveExpression()
{
    Expression *left, *right;


    left = multiplicativeExpression();

    while (1) {
	if (lookahead == '+') {
	    match('+');
	    right = multiplicativeExpression();
	    left = checkAdd(left, right);

	} else if (lookahead == '-') {
	    match('-');
	    right = multiplicativeExpression();
	    left = checkSubtract(left, right);

	} else
	    break;
    }

    return left;
}


/*
 * Function:	relationalExpression
 *
 * Description:	Parse a relational expression.  Note that Simple C does not
 *		have shift operators, so we go immediately to additive
 *		expressions.
 *
 *		relational-expression:
 *		  additive-expression
 *		  relational-expression < additive-expression
 *		  relational-expression > additive-expression
 *		  relational-expression <= additive-expression
 *		  relational-expression >= additive-expression
 */

static Expression *relationalExpression()
{
    Expression *left, *right;


    left = additiveExpression();

    while (1) {
	if (lookahead == '<') {
	    match('<');
	    right = additiveExpression();
	    left = checkLessThan(left, right);

	} else if (lookahead == '>') {
	    match('>');
	    right = additiveExpression();
	    left = checkGreaterThan(left, right);

	} else if (lookahead == LEQ) {
	    match(LEQ);
	    right = additiveExpression();
	    left = checkLessOrEqual(left, right);

	} else if (lookahead == GEQ) {
	    match(GEQ);
	    right = additiveExpression();
	    left = checkGreaterOrEqual(left, right);

	} else
	    break;
    }

    return left;
}


/*
 * Function:	equalityExpression
 *
 * Description:	Parse an equality expression.
 *
 *		equality-expression:
 *		  relational-expression
 *		  equality-expression == relational-expression
 *		  equality-expression != relational-expression
 */

static Expression *equalityExpression()
{
    Expression *left, *right;


    left = relationalExpression();

    while (1) {
	if (lookahead == EQL) {
	    match(EQL);
	    right = relationalExpression();
	    left = checkEqual(left, right);

	} else if (lookahead == NEQ) {
	    match(NEQ);
	    right = relationalExpression();
	    left = checkNotEqual(left, right);

	} else
	    break;
    }

    return left;
}


/*
 * Function:	logicalAndExpression
 *
 * Description:	Parse a logical-and expression.  Note that Simple C does
 *		not have bitwise-and expressions.
 *
 *		logical-and-expression:
 *		  equality-expression
 *		  logical-and-expression && equality-expression
 */

static Expression *logicalAndExpression()
{
    Expression *left, *right;


    left = equalityExpression();

    while (lookahead == AND) {
	match(AND);
	right = equalityExpression();
	left = checkLogicalAnd(left, right);
    }

    return left;
}


/*
 * Function:	logicalOrExpression
 *
 * Description:	Parse a logical-or expression.  Note that Simple C does not
 *		have bitwise-or expressions.
 *
 *		logical-or-expression:
 *		  logical-and-expression
 *		  logical-or-expression || logical-and-expression
 */

static Expression *logicalOrExpression()
{
    Expression *left, *right;


    left = logicalAndExpression();

    while (lookahead == OR) {
	match(OR);
	right = logicalAndExpression();
	left = checkLogicalOr(left, right);
    }

    return left;
}


/*
 * Function:	expression
 *
 * Description:	Parse an expression, or more specifically, an assignment
 *		expression, since Simple C does not allow comma as an
 *		expression operator.
 *
 *		expression:
 *		  logical-or-expression
 *		  logical-or-expression = expression
 */

static Expression *expression()
{
    Expression *left, *right;


    left = logicalOrExpression();

    if (lookahead == '=') {
	match('=');
	right = expression();
	left = checkAssign(left, right);
    }

    return left;
}


/*
 * Function:	statements
 *
 * Description:	Parse a possibly empty sequence of statements.  Rather than
 *		checking if the next token starts a statement, we check if
 *		the next token ends the sequence, since a sequence of
 *		statements is always terminated by a closing brace.
 *
 *		statements:
 *		  empty
 *		  statement statements
 */

static Statements statements()
{
    Statements stmts;


    while (lookahead != '}')
	stmts.push_back(statement());

    return stmts;
}


/*
 * Function:	statement
 *
 * Description:	Parse a statement.  Note that Simple C has so few
 *		statements that we handle them all in this one function.
 *
 *		statement:
 *		  { declarations statements }
 *		  return expression ;
 *		  while ( expression ) statement
 *		  if ( expression ) statement
 *		  if ( expression ) statement else statement
 *		  expression ;
 */

static Statement *statement()
{
    Scope *decls;
    Statements stmts;
    Expression *expr;
    Statement *stmt;


    if (lookahead == '{') {
	match('{');
	decls = openScope();
	declarations();
	stmts = statements();
	closeScope();
	match('}');
	return new Block(decls, stmts);

    }
    
    if (lookahead == RETURN) {
	match(RETURN);
	expr = expression();
	checkReturn(expr, returnType);
	match(';');
	return new Return(expr);
    }
    
    if (lookahead == WHILE) {
	match(WHILE);
	match('(');
	expr = expression();
	checkTest(expr);
	match(')');
	stmt = statement();
	return new While(expr, stmt);
    }
    
    if (lookahead == IF) {
	match(IF);
	match('(');
	expr = expression();
	checkTest(expr);
	match(')');
	stmt = statement();

	if (lookahead != ELSE)
	    return new If(expr, stmt, nullptr);

	match(ELSE);
	return new If(expr, stmt, statement());
    } 

    expr = expression();
    match(';');
    return expr;
}


/*
 * Function:	parameter
 *
 * Description:	Parse a parameter, which in Simple C is always a scalar
 *		variable with optional pointer declarators.
 *
 *		parameter:
 *		  specifier pointers ID
 */

static Type parameter()
{
    unsigned indirection;
    int typespec;
    string name;


    typespec = specifier();
    indirection = pointers();
    name = expect(ID);

    Type type = Type(typespec, indirection);
    declareParameter(name, type);
    return type;
}


/*
 * Function:	parameters
 *
 * Description:	Parse the parameters of a function, but not the opening or
 *		closing parentheses.
 *
 *		parameters:
 *		  void
 *		  parameter-list
 *
 *		parameter-list:
 *		  parameter
 *		  parameter , parameter-list
 */

static Parameters *parameters()
{
    Parameters *params = new Parameters();

    if (lookahead == VOID)
	match(VOID);
    else {
	params->push_back(parameter());

	while (lookahead == ',') {
	    match(',');
	    params->push_back(parameter());
	}
    }

    return params;
}


/*
 * Function:	globalDeclaration
 *
 * Description:	Parse a global variable declaration, function declaration,
 *		or function definition.
 *
 *		global-declaration:
 *		  specifier global-declarator-list ;
 *
 *		global-declarator-list:
 *		  global-declarator
 *		  global-declarator global-declarator-list
 *
 *		global-declarator:
 *		  pointers ID
 *		  pointers ID ( )
 *		  pointers ID [ integer ]
 *
 *		function-definition:
 *		  specifier pointers identifier ( parameters ) { ... }
 */

static void globalDeclaration()
{
    Symbol *symbol;
    unsigned indirection, length;
    int typespec;
    string name;


    typespec = specifier();
    indirection = pointers();
    name = expect(ID);

    if (lookahead == '[') {
	match('[');
	length = strtoul(expect(INTEGER).c_str(), NULL, 0);
	symbol = declareVariable(name, Type(typespec, indirection, length));
	globals.push_back(symbol);
	match(']');

    } else if (lookahead == '(') {
	match('(');

	if (lookahead == ')') {
	    match(')');
	    declareFunction(name, Type(typespec, indirection, nullptr));

	} else {
	    Scope *decls;
	    Function *function;
	    Statements stmts;
	    Parameters *params;

	    decls = openScope();
	    params = parameters();
	    returnType = Type(typespec, indirection);
	    symbol = declareFunction(name, Type(typespec, indirection, params));
	    match(')');
	    match('{');
	    declarations();
	    stmts = statements();
	    closeScope();
	    match('}');

	    function = new Function(symbol, new Block(decls, stmts));

	    if (numErrors == 0)
		function->generate();

	    return;
	}

    } else {
	symbol = declareVariable(name, Type(typespec, indirection));
	globals.push_back(symbol);
    }

    while (lookahead == ',') {
	match(',');
	indirection = pointers();
	name = expect(ID);

	if (lookahead == '[') {
	    match('[');
	    length = strtoul(expect(INTEGER).c_str(), NULL, 0);
	    symbol = declareVariable(name, Type(typespec, indirection, length));
	    globals.push_back(symbol);
	    match(']');

	} else if (lookahead == '(') {
	    match('(');
	    match(')');
	    declareFunction(name, Type(typespec, indirection, nullptr));

	} else {
	    symbol = declareVariable(name, Type(typespec, indirection));
	    globals.push_back(symbol);
	}
    }

    match(';');
}


/*
 * Function:	main
 *
 * Description:	Analyze the standard input stream.
 */

int main()
{
    openScope();
    lookahead = lexan(lexbuf);

    while (lookahead != DONE)
	globalDeclaration();

    closeScope();

    if (numErrors == 0)
	generateGlobals(globals);

    exit(EXIT_SUCCESS);
}
