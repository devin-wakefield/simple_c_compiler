/*
 * File:	generator.cpp
 *
 * Description:	This file contains the public and member function
 *		definitions for the code generator for Simple C.
 *
 *		Extra functionality:
 *		- putting all the global declarations at the end
 */

# include <sstream>
# include <iostream>
# include <map>
# include "generator.h"
# include "machine.h"

using namespace std;
extern vector<fLabel> fLabels;

/*
 * Function:	assigntemp (allocator)
 *
 * Description:	Store values into a temporary variable location on the stack.
 * 		One does not simply do this. You DO IT AHDHFAJKSDAFKSDA:
 */
static int tempoffset;
int minoffset;
int maxoffset;

struct Label{
    int number;
    static int counter;
    Label(){
	number = counter++;
    }
};

int Label::counter = 0;
map<string, Label> Labels;
Label returnLab;

void assigntemp(Expression *e)
{
    stringstream ss;

    tempoffset -=  e->type().size();
    ss <<  tempoffset << "(%ebp)";
    
    e->operand(ss.str());
}

/*
 * Function:	operator <<
 *
 * Description:	Convenience function for writing the operand of an
 *		expression.
 */

ostream &operator <<(ostream &ostr, Expression *expr)
{
    return ostr << expr->operand();
}

/*
 * Function:	operator <<
 *
 * Description:	Convenience function for writing the operand of a fLabel.
 */

ostream &operator << (ostream &ostr, fLabel &lbl)
{
    return ostr << ".fp" << lbl.number; 
}

/*
 * Function:	operator <<
 *
 * Description:	Convenience function for writing the operand of a Label.
 */

ostream &operator << (ostream &ostr, Label &lbl)
{
    return ostr << ".L" << lbl.number;
}

/*
 * Function:	Expression::operand (accessor)
 *
 * Description:	Return the operand for an expression as a string.
 */

const string &Expression::operand() const
{
    return _operand;
}


/*
 * Function:	Expression::operand (mutator)
 *
 * Description:	Update the operand string for an expression.
 */

void Expression::operand(const string &operand)
{
    _operand = operand;
}


/*
 * Function:	Identifier::generate
 *
 * Description:	Generate code for an identifier.  Since there is really no
 *		code to generate, we simply update our operand.
 */

void Identifier::generate()
{
    stringstream ss;


    if (_symbol->offset() != 0) {
	ss << _symbol->offset() << "(%ebp)";
	_operand = ss.str();
    } else
	_operand = _symbol->name();
}


/*
 * Function:	Integer::generate
 *
 * Description:	Generate code for an integer literal.  Since there is
 *		really no code to generate, we simply update our operand.
 */

void Integer::generate()
{
    stringstream ss;


    ss << "$" << _value;
    _operand = ss.str();
}


/*
 * Function:	Call::generate
 *
 * Description:	Generate code for a function call expression, in which each
 *		argument is simply a variable or an integer literal.
 */

void Call::generate()
{
    unsigned numBytes = 0;


    for (int i = _args.size() - 1; i >= 0; i --) {
	_args[i]->generate();
	if(_args[i]->type().isReal()){
	    cout << "\tsubl\t$8, %esp\n";
	    cout << "\tfldl\t" << _args[i] << endl;
	    cout << "\tfstpl\t(%esp)\n";
	}else{
    	    cout << "\tpushl\t" << _args[i] << endl;
	}

	numBytes += _args[i]->type().size();
	
    }

    cout << "\tcall\t" << _id->name() << endl;

    if (numBytes > 0)
	cout << "\taddl\t$" << numBytes << ", %esp" << endl;
    assigntemp(this);
    if(_type.isReal()){
	cout << "#getting a double from a returned statement thing from the function up thereish^\n";
	cout << "\tfstpl\t" << this << endl;
    }else{
	cout << "#getting an int from a return.\n";
    	cout << "\tmovl\t%eax, " << this << endl;
    }
    stringstream ss;
    ss << this;
    _operand = ss.str();
}


/*
 * Function:	Assign::generate
 *
 * Description:	Generate code for this assignment statement, in which the
 *		right-hand side is an integer literal and the left-hand
 *		side is an integer scalar variable.  Actually, the way
 *		we've written things, the right-side can be a variable too.
 */

void Assign::generate()
{
    bool indirect;
    _left->generate(indirect);
    _right->generate();
    assigntemp(this);

    cout << "#we are teh assignzorzezvillez\n";
    if(_left->type().isReal()){
	if(indirect){
	    cout << "#INDIRECT REAL ASSIGNMENT OH MY GOODIENESS\n";
	    cout << "\tfldl\t" << _right << endl;
	    cout << "\tmovl\t" << _left << ", %eax\n";
	    cout << "\tfstl\t(%eax)\n";
	    cout << "\tfstpl\t" << _left << endl;
	    cout << "\tfstpl\t" << this << endl;
	}else{
    	    cout << "\tfldl\t" << _right << endl;
    	    cout << "\tfstpl\t" << _left << endl;
	    cout << "\tfstpl\t" << this << endl;
	}
    }else{
	if(indirect){
	    cout << "#INDIRECT ASSIGNMENT OH MY GOODIENESS\n";
	    cout << "\tmovl\t" << _right << ", %eax\n";
	    cout << "\tmovl\t" << _left << ", %ecx\n";
	    cout << "\tmovl\t%eax, (%ecx)\n";
	    cout << "\tmovl\t%eax, " << _left << endl;
	    cout << "\tmovl\t%eax, " << this << endl;
	}else{
    	    cout << "\tmovl\t" << _right << ", %eax" << endl;
    	    cout << "\tmovl\t%eax, " << _left << endl;
	    cout << "\tmovl\t%eax, " << this << endl;
	}
    }
}


/*
 * Function:	Block::generate
 *
 * Description:	Generate code for this block, which simply means we
 *		generate code for each statement within the block.
 */

void Block::generate()
{
    for (unsigned i = 0; i < _stmts.size(); i ++){
	_stmts[i]->generate();
	if(tempoffset < maxoffset)
	    maxoffset = tempoffset;
	tempoffset = minoffset;
    }
}


/*
 * Function:	Function::generate
 *
 * Description:	Generate code for this function, which entails allocating
 *		space for local variables, then emitting our prologue, the
 *		body of the function, and the epilogue.
 */

void Function::generate()
{
    int offset = 0;
    Label ret;
    returnLab = ret;

    /* Generate our prologue. */

    allocate(offset);
    tempoffset = offset;
    minoffset = offset;
    maxoffset = offset;
    cout << _id->name() << ":" << endl;
    cout << "\tpushl\t%ebp" << endl;
    cout << "\tmovl\t%esp, %ebp" << endl;

    cout << "\tsubl\t$" << _id->name() << ".size, %esp" << endl;

    /* Generate the body of this function. */

    _body->generate();


    /* Generate our epilogue. */

    cout << returnLab << ":\n";
    cout << "\tmovl\t%ebp, %esp" << endl;
    cout << "\tpopl\t%ebp" << endl;
    cout << "\tret" << endl << endl;

    cout << "\t.global\t" << _id->name() << endl;
    cout << "\t.set\t" << _id->name() << ".size, " << -maxoffset << endl;
    /*cout << "#HELP WHY IS MAXOFFSET NOT RIGHT :C" << maxoffset << endl;
    cout << "#minoffset: " << minoffset << endl;
    cout << "#tempoffset: " << tempoffset << endl;*/

    cout << endl;
}


/*
 * Function:	generateGlobals
 *
 * Description:	Generate code for any global variable declarations.
 */

void generateGlobals(const Symbols &globals)
{
    if ((globals.size() + fLabels.size() + Labels.size()) > 0)
	cout << "\t.data" << endl;

    for (unsigned i = 0; i < globals.size(); i ++) {
	cout << "\t.comm\t" << globals[i]->name();
	cout << ", " << globals[i]->type().size() << ", 4" << endl;
    }

    for (unsigned i = 0; i<fLabels.size(); i ++) {
	cout << fLabels[i] << ":\t.double\t" << fLabels[i].value << endl;
    }
    
    map<string, Label>::iterator it;

    for (it = Labels.begin(); it != Labels.end(); it++) {
	Label lab = it->second;
	string value = it->first;
	cout << lab << ":\t.asciz\t" << value << endl;
    }
}

/*
 * Function:	Add::generate
 *
 * Description:	Generate code for Addition :)
 */

void Add::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "\n#addition time y'all! :) <3\n";
    if(_type.isReal()){
	//do floating point shizzzzz
	cout << "#float the boat with plussesssszzzz <3<3\n";
	cout << "\tfldl\t" << _left << endl;
	cout << "\tfaddl\t" << _right << endl;
	cout << "\tfstpl\t" << this << endl;
    }
    else{
	cout << "\t#integerz adding plus! <3<3\n";
    	cout << "\tmovl\t" << _left << ", %eax\n";
    	cout << "\taddl\t" << _right << ", %eax\n";
    	cout << "\tmovl\t%eax, " << this << endl;
    }
}

/*
 * Function:	Multiply::generate
 *
 * Description:	Generate code for MULTIPLICATION YEAHHH DAWGI
 */

void Multiply::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "\n#multiplication time y'all! :) <3\n";
    if(_type.isReal()){
	cout << "#floating point multiplication! <3 y'all :)\n";
	cout << "\tfldl\t" << _left << endl;
	cout << "\tfmull\t" << _right << endl;
	cout << "\tfstpl\t" << this << endl;
    }else{
	cout << "#integer multiplication! fuck the floats! <3\n";
    	cout << "\tmovl\t" << _left << ", %eax\n";
    	cout << "\timull\t" << _right << ", %eax\n";
    	cout << "\tmovl\t%eax, " << this << endl;
    }
}

/*
 * Function:	Divide::generate
 *
 * Description:	Generate assembly for division with EAX RAH
 */

void Divide::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "\n#division time y'all! :) <3\n";
    if(_type.isReal()){
	cout << "\tfldl\t" << _left << endl;
	cout << "\tfdivl\t" << _right << endl;
	cout << "\tfstpl\t" << this << endl;
    }else{
    	cout << "\tmovl\t" << _left << ", %eax\n";
    	cout << "\tcltd\n";
    	cout << "\tmovl\t" << _right << ", %ecx\n";
    	cout << "\tidivl\t%ecx\n";
    	cout << "\tmovl\t%eax, " << this << endl;
    }
}

/*
 * Function:	Subtract::generate
 *
 * Description:	Generate assembly language shit for subtracting shit BLAH
 */

void Subtract::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "\n#Subtacting things from things! <3<3<3\n";
    if(_type.isReal()){
	cout << "\tfldl\t" << _left << endl;
	cout << "\tfsubl\t" << _right << endl;
	cout << "\tfstpl\t" << this << endl;
    }else{
    	cout << "\tmovl\t" << _left << ", %eax\n";    
	cout << "\tsubl\t" << _right << ", %eax\n";
    	cout << "\tmovl\t%eax, " << this << endl;
    }
}

void Cast::generate()
{
    _expr->generate();
    assigntemp(this);

    cout << "\n\t#Time to cast things!!! <3\n";

    if(_type.isReal()){
	if(_expr->type().isReal()){
	    _operand = _expr->operand();
	}else{
    	    cout << "\tfildl\t" << _expr << endl;
	    cout << "\tfstpl\t" << this << endl;
	}
    }else{
	if(_expr->type().isReal()){
    	    cout << "\tfldl\t" << _expr << endl;
    	    cout << "\tfistpl\t" << this << endl;
	}else
	    _operand = _expr->operand();
    }
}

/*
 * Function:	NotEqual::generate
 *
 * Description:	Generate the assembly language yeah donny finn fin we can do it
 */

void NotEqual::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "#Equal time hashtag all the ballin'! <3\n";
    if(_left->type().isReal()){
	cout << "\tfldl\t" << _left << endl;
	cout << "\tfcompl\t" << _right << endl;
	cout << "\tfnstsw\t%ax\n";
	cout << "\tsahf\n";
	cout << "\tsene\t%al\n";
	cout << "\tmovzbl\t%al, %eax\n";
	cout << "\tmovl\t%eax, " << this << endl;
    }else{
    	cout << "\tmovl\t" << _left << ", %eax\n";
    	cout << "\tcmpl\t" << _right << ", %eax\n";
    	cout << "\tsetne\t%al\n";
    	cout << "\tmovzbl\t%al, %eax\n";
    	cout << "\tmovl\t%eax, " << this << endl << endl;
    }
}



/*
 * Function:	Equal::generate
 *
 * Description:	Generate the assembly language code jizz shitasdjfksladjfklsad help
 */

void Equal::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "#Equal time hashtag all the ballin'! <3\n";
    if(_left->type().isReal()){
	cout << "\tfldl\t" << _left << endl;
	cout << "\tfcompl\t" << _right << endl;
	cout << "\tfnstsw\t%ax\n";
	cout << "\tsahf\n";
	cout << "\tsete\t%al\n";
	cout << "\tmovzbl\t%al, %eax\n";
	cout << "\tmovl\t%eax, " << this << endl;
    }else{
    	cout << "\tmovl\t" << _left << ", %eax\n";
    	cout << "\tcmpl\t" << _right << ", %eax\n";
    	cout << "\tsete\t%al\n";
    	cout << "\tmovzbl\t%al, %eax\n";
    	cout << "\tmovl\t%eax, " << this << endl << endl;
    }
}

/*
 * Function:	Not::generate
 *
 * Description: generate all KINDS OF ASSEMBLY then run around naked outside
 */

void Not::generate()
{
    _expr->generate();
    assigntemp(this);

    cout << "#NOT THE END OF THE WORLD MAYBE \n";
    if(_expr->type().isReal()){
	
    	cout << "\tfldl\t" << _expr << endl;
	cout << "\tftst\n";
    	cout << "\tfstp\t%st(0)\n";
	cout << "\tfnstsw\t%ax\n";
    	cout << "\tsahf\n";
    	cout << "\tsete\t%al\n";
    	cout << "\tmovzbl\t%al, %eax\n";
    	cout << "\tmovl\t%eax, " << this << endl;

    }else{

	cout << "\tmovl\t" << _expr << ", %eax" << endl;
	cout << "\ttestl\t%eax, %eax\n";
	cout << "\tsete\t%al\n";
	cout << "\tmovzbl\t%al, %eax\n";
	cout << "\tmovl\t%eax, " << this << endl;

    }
}

/*
 * Function:	I don't fucking know why are you asking the comment? look at it yourself! gosh!
 *
 * Description:	You can't figure it out? Why, when I was your age, we figured out ALLLL the shit.
 */

void Real::generate()
{
    stringstream ss;
    ss << _label;
    _operand = ss.str();
}

/*
 * Function:	LessOrEqual::generate
 *
 * Description:	Output the assembly language for the happiness of the LessOrEqual stuff!
 */

void LessOrEqual::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "#LessOrEqual time hashtag ballin'! <3\n";
    if(_left->type().isReal()){
	cout << "\tfldl\t" << _left << endl;
	cout << "\tfcompl\t" << _right << endl;
	cout << "\tfnstsw\t%ax\n";
	cout << "\tsahf\n";
	cout << "\tsetbe\t%al\n";
	cout << "\tmovzbl\t%al, %eax\n";
	cout << "\tmovl\t%eax, " << this << endl;
    }else{
    	cout << "\tmovl\t" << _left << ", %eax\n";
    	cout << "\tcmpl\t" << _right << ", %eax\n";
    	cout << "\tsetle\t%al\n";
    	cout << "\tmovzbl\t%al, %eax\n";
    	cout << "\tmovl\t%eax, " << this << endl << endl;
    }
}

/*
 * Function:	LessThan::generate
 *
 * Description:	Generate the assembly code for the LessThan operator.
 */

void LessThan::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "#LessThan time! hashtag sexy pieces ;)\n";
    if(_left->type().isReal()){
	cout << "\tfldl\t" << _left << endl;
	cout << "\tfcompl\t" << _right << endl;
	cout << "\tfnstsw\t%ax\n";
	cout << "\tsahf\n";
	cout << "\tsetb\t%al\n";
	cout << "\tmovzbl\t%al, %eax\n";
	cout << "\tmovl\t%eax, " << this << endl << endl;
    }else{
    	cout << "\tmovl\t" << _left << ", %eax\n";
    	cout << "\tcmpl\t" << _right << ", %eax\n";
    	cout << "\tsetl\t%al\n";
    	cout << "\tmovzbl\t%al, %eax\n";
    	cout << "\tmovl\t%eax, " << this << endl << endl;
    }
}

/*
 * Function:	GreaterThan::generate
 *
 * Description:	Generate the assembly language for the GreaterThan operator >.
 */

void GreaterThan::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "#Time to do a GreaterThan calculation. amirite? <3 all y'all :) \n";

    if(_left->type().isReal()){
	cout << "\tfldl\t" << _left << endl;
	cout << "\tfcompl\t" << _right << endl;
	cout << "\tfnstsw\t%ax\n";
	cout << "\tsahf\n";
	cout << "\tseta\t%al\n";
	cout << "\tmovzbl\t%al, %eax\n";
	cout << "\tmovl\t%eax, " << this << endl << endl;
    }else{
    	cout << "\tmovl\t" << _left << ", %eax\n";
    	cout << "\tcmpl\t" << _right << ", %eax\n";
    	cout << "\tsetg\t%al\n";
    	cout << "\tmovzbl\t%al, %eax\n";
    	cout << "\tmovl\t%eax, " << this << endl << endl;
    }
}

/*
 * Function:	GreaterOrEqual::generate
 *
 * Description:	Generate the Assembly code for the operator >=.
 *
 * I love you :) <3
 */

void GreaterOrEqual::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "#Let us do all of the greater than or equal to operations! yes :)\n";


    if(_left->type().isReal()){
	cout << "\tfldl\t" << _left << endl;
	cout << "\tfcompl\t" << _right << endl;
	cout << "\tfnstsw\t%ax\n";
	cout << "\tsahf\n";
	cout << "\tsetbe\t%al\n";
	cout << "\tmovzbl\t%al, %eax\n";
	cout << "\tmovl\t%eax, " << this << endl << endl;
    }else{
    	cout << "\tmovl\t" << _left << ", %eax\n";
    	cout << "\tcmpl\t" << _right << ", %eax\n";
    	cout << "\tsetle\t%al\n";
    	cout << "\tmovzbl\t%al, %eax\n";
    	cout << "\tmovl\t%eax, " << this << endl << endl;
    }
}

/*
 * Function:	String::generate
 *
 * Please help me... I'm trapped! :(
 *
 * Description:	I'm not actually sure what should happen here, so... yeah! :)
 */

void String::generate()
{
    pair<map<string, Label>::iterator, bool> ret;
    stringstream ss;
    Label lab;
    ret = Labels.insert(pair<string, Label>(_value, lab) );
    if(ret.second){
	ss << lab;
    }else{
	Label tempie = ret.first->second;
	ss << tempie;
    }
    _operand = ss.str();
}

/*
 * Function:	Remainder::generate
 *
 * Description:	Output the assembly language for Remainder (%).
 */

void Remainder::generate()
{
    _left->generate();
    _right->generate();
    assigntemp(this);

    cout << "\n\t#Remainder time y'all! :) <3\n";
    
    cout << "\tmovl\t" << _left << ", %eax\n";
    cout << "\tcltd\n";
    cout << "\tmovl\t" << _right << ", %ecx\n";
    cout << "\tidivl\t%ecx\n";
    cout << "\tmovl\t%edx, " << this << endl;
}

/*
 * Function:	Negate::generate
 *
 * Description:	output the assembly crapshitz for negating numberz. LBAH!
 */

void Negate::generate()
{
    _expr->generate();
    assigntemp(this);

    cout << "#all of the negationz <3 <3 <3 :) KSDFAJFKLDJSDFAKL\n";

    if(_type.isReal()){
	//how do I negate a real? just subtract it from 0! yeah! :)
	cout << "\tfldl\t" << _expr << endl;
	cout << "\tfchs\t\n";
	cout << "\tfstpl\t" << this << endl;
    }else{
	cout << "\tmovl\t" << _expr << ", %eax\n";
	cout << "\tnegl\t%eax\n";
	cout << "\tmovl\t%eax, " << this << endl;
    }
}

/*
 * Function:	Address::generate
 *
 * Description:	Get the Address of an Array :)
 */

void Address::generate()
{
    bool indirect;
    _expr->generate(indirect);

    if(!indirect)
    {
	if(_expr->operand()[0] == '.')
	{
	    stringstream ss;
	    ss << "$" << _expr->operand();
	    _operand = ss.str();
	}
	else
	{
	    assigntemp(this);
    	    cout << "#time to ADDRESS THINGS TO THINGS AND OH MY GOODINESS!\n";
    	    cout << "\tleal\t" << _expr << ", %eax\n";
    	    cout << "\tmovl\t%eax, " << this << endl;
	}
    }
    else _operand = _expr->operand();
}

/*
 * Function:	Expression::generate
 *
 * Description:	Let's me know that I have not written this... xD
 */

void Expression::generate()
{
    cerr << "woopsiez!\n";
}

/*
 * Function:	Expression::generate(bool &indirect)
 *
 * Description:	Makes things work well :)
 */

void Expression::generate(bool &indirect)
{
    indirect = false;
    generate();
}

/*
 * Function:	Dereference::generate(bool &indirect)
 *
 * Description:	Makes bools happen well xD (I don't know don't ask me AHHHHH)
 */

void Dereference::generate(bool &indirect)
{
    indirect = true;
    _expr->generate();
    _operand = _expr->operand();
}

/*
 * Function:	Dereference::generate()
 *
 * Description:	Wheeeee the other thing.
 */

void Dereference::generate()
{
    _expr->generate();
    assigntemp(this);
    cout << "#Dereferencing this here expression\n";
    if(_type.isReal()){
	cout << "\tmovl\t" << _expr << ", %eax\n";
	cout << "\tfldl\t(%eax)\n";
	cout << "\tfstpl\t" << this << endl;
    }else{
	cout << "\tmovl\t" << _expr << ", %eax\n";
	cout << "\tmovl\t(%eax), %eax\n";
	cout << "\tmovl\t%eax, " << this << endl;
    }
}

/*
 * Function:	Dereference::isPoint
 *
 * Description:	Return true.
 */

bool Dereference::isPoint()
{
    return true;
}

/*
 * Function:	Expression::isPoint
 *
 * Description:	Return false.
 */

bool Expression::isPoint()
{
    return false;
}

/*
 * Function:	Return::generate()
 *
 * Description:	Put the return value into the right register, in assembly language. Yeah.
 */

void Return::generate()
{
    _expr->generate();
    if(_expr->type().isReal()){
	cout << "\tfldl\t" << _expr << endl;
    }else{
    	cout << "\tmovl\t" << _expr << ", %eax\n";
    }
    cout << "\tjmp\t" << returnLab << endl;
}

/*
 * Function: 	If::generate
 *
 * Description:	Generate the assembly language code for an IF statement.
 */

void If::generate()
{
    _expr->generate();
    Label skip, lou;
    cout << "#iffin and iffin and yeah! C> ice cream! C>\n";
    cout << "\tmovl\t" << _expr << ", %eax" << endl;
    cout << "\ttestl\t%eax, %eax\n";
    cout << "\tje\t" << skip << endl;
    _thenStmt->generate();
    if(_elseStmt != nullptr){
	cout << "\tjmp\t" << lou << endl;
	cout << skip << ":\n";
	_elseStmt->generate();
	cout << lou << ":\n";
    }else{
    	cout << skip << ":\n";
    }

}

/*
 * Function:	LogicalAnd::generate
 *
 * Description:	Generate assembly for LogicalAnd
 */

void LogicalAnd::generate()
{
    cout << "#all of the logical, AND THEN scream for all the ice and cream C> C> C> C> C> \n";
    _left->generate();
    assigntemp(this);
    Label lab;
    cout << "\tmovl\t" << _left << ", %eax" << endl;
    cout << "\ttestl\t%eax, %eax\n";
    cout << "\tje\t" << lab << endl;
    _right->generate();
    cout << "\tmovl\t" << _right << ", %eax" << endl;
    cout << "\ttestl\t%eax, %eax\n";
    cout << lab << ":\n";
    cout << "\tsetne\t%al\n";
    cout << "\tmovzbl\t%al, %eax\n";
    cout << "\tmovl\t%eax, " << this << endl;
} 

/*
 * Function:	LogicalOr::generate
 *
 * Description:	Generate assembly code for LogicalOr
 */

void LogicalOr::generate()
{
    cout << "#all of the logical, or scream for all the ice and cream C> C> C> C> C> \n";
    _left->generate();
    assigntemp(this);
    Label lab;
    cout << "\tmovl\t" << _left << ", %eax" << endl;
    cout << "\ttestl\t%eax, %eax\n";
    cout << "\tjne\t" << lab << endl;
    _right->generate();
    cout << "\tmovl\t" << _right << ", %eax" << endl;
    cout << "\ttestl\t%eax, %eax\n";
    cout << lab << ":\n";
    cout << "\tsetne\t%al\n";
    cout << "\tmovzbl\t%al, %eax\n";
    cout << "\tmovl\t%eax, " << this << endl;
} 

/*
 * Function:	While::generate
 *
 * Description:	Generate assembly frab for While loops :) <3
 */

void While::generate()
{
    cout << "#while time! and i scream for all the ice and cream C> C> C> C> C> \n";
    Label loop, exit;
    cout << loop << ":\n";
    _expr->generate();
    cout << "\tmovl\t" << _expr << ", %eax" << endl;
    cout << "\ttestl\t%eax, %eax\n";
    cout << "\tje\t" << exit << endl;
    _stmt->generate();
    cout << "\tjmp\t" << loop << endl;
    cout << exit << ":\n";
}
