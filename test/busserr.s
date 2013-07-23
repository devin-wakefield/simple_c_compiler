roar:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$roar.size, %esp
	movl	-4(%ebp), %eax
	jmp	.L1
.L1:
	movl	%ebp, %esp
	popl	%ebp
	ret

	.global	roar
	.set	roar.size, 4

main:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$main.size, %esp
#time to ADDRESS THINGS TO THINGS AND OH MY GOODINESS!
	leal	-12(%ebp), %eax
	movl	%eax, -16(%ebp)
#we are teh assignzorzezvillez
	movl	-16(%ebp), %eax
	movl	%eax, -4(%ebp)
	movl	%eax, -20(%ebp)

#multiplication time y'all! :) <3
#integer multiplication! fuck the floats! <3
	movl	$3, %eax
	imull	$4, %eax
	movl	%eax, -16(%ebp)

#addition time y'all! :) <3
	#integerz adding plus! <3<3
	movl	-4(%ebp), %eax
	addl	-16(%ebp), %eax
	movl	%eax, -20(%ebp)
#we are teh assignzorzezvillez
#INDIRECT ASSIGNMENT OH MY GOODIENESS
	movl	$4, %eax
	movl	-20(%ebp), %ecx
	movl	%eax, (%ecx)
	movl	%eax, -20(%ebp)
	movl	%eax, -24(%ebp)
	call	roar
#getting an int from a return.
	movl	%eax, -16(%ebp)
#we are teh assignzorzezvillez
	movl	-16(%ebp), %eax
	movl	%eax, -12(%ebp)
	movl	%eax, -20(%ebp)
	pushl	-12(%ebp)
	pushl	$.L3
	call	printf
	addl	$8, %esp
#getting an int from a return.
	movl	%eax, -16(%ebp)
.L2:
	movl	%ebp, %esp
	popl	%ebp
	ret

	.global	main
	.set	main.size, 24

	.data
.L3:	.asciz	"i: %d\n"
