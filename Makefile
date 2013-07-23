CXXFLAGS	= -g -Wall
OBJS		= Scope.o Symbol.o Tree.o Type.o allocator.o checker.o \
		  generator.o lexer.o parser.o
PROG		= scc

all:		clean $(PROG)

$(PROG):	$(OBJS)
		$(CXX) -o $(PROG) $(OBJS)

clean:;		$(RM) -f $(PROG) core *.o
