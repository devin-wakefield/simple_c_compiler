main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$main.size, %esp
#we are teh assignzorzezvillez
	movl	$1, %eax
	movl	%eax, -4(%ebp)
	movl	%eax, -20(%ebp)
#we are teh assignzorzezvillez
	movl	$2, %eax
	movl	%eax, -8(%ebp)
	movl	%eax, -20(%ebp)
#we are teh assignzorzezvillez
	movl	$3, %eax
	movl	%eax, -12(%ebp)
	movl	%eax, -20(%ebp)
#we are teh assignzorzezvillez
	movl	$4, %eax
	movl	%eax, -16(%ebp)
	movl	%eax, -20(%ebp)

#Subtacting things from things! <3<3<3
	movl	-8(%ebp), %eax
	subl	-12(%ebp), %eax
	movl	%eax, -20(%ebp)

#multiplication time y'all! :) <3
#integer multiplication! fuck the floats! <3
	movl	-16(%ebp), %eax
	imull	-4(%ebp), %eax
	movl	%eax, -24(%ebp)

#division time y'all! :) <3
	movl	-24(%ebp), %eax
	cltd
	movl	-16(%ebp), %ecx
	idivl	%ecx
	movl	%eax, -28(%ebp)

#Subtacting things from things! <3<3<3
	movl	-20(%ebp), %eax
	subl	-28(%ebp), %eax
	movl	%eax, -32(%ebp)
#we are teh assignzorzezvillez
	movl	-32(%ebp), %eax
	movl	%eax, -4(%ebp)
	movl	%eax, -36(%ebp)
#LessThan time! hashtag sexy pieces ;)
	movl	-4(%ebp), %eax
	cmpl	-8(%ebp), %eax
	setl	%al
	movzbl	%al, %eax
	movl	%eax, -20(%ebp)

#iffin and iffin and yeah! C> ice cream! C>
	movl	-20(%ebp), %eax
	testl	%eax, %eax
	je	.L2
	pushl	$.L4
	call	printf
	addl	$4, %esp
#getting an int from a return.
	movl	%eax, -24(%ebp)
	jmp	.L3
.L2:
	pushl	$.L5
	call	printf
	addl	$4, %esp
#getting an int from a return.
	movl	%eax, -20(%ebp)
.L3:
	pushl	-4(%ebp)
	pushl	$.L6
	call	printf
	addl	$8, %esp
#getting an int from a return.
	movl	%eax, -20(%ebp)
.L1:
	movl	%ebp, %esp
	popl	%ebp
	ret

	.global	main
	.set	main.size, 36

	.data
.L6:	.asciz	"a: %d\n"
.L5:	.asciz	"good riddance\n"
.L4:	.asciz	"hello\n"
