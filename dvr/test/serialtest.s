	.file	"serialtest.c"
	.section	.rodata
.LC0:
	.string	"hello"
.LC1:
	.string	"a=%d, b=%d\n"
	.text
.globl main
	.type	main, @function
main:
	pushl	%ebp
	movl	%esp, %ebp
	andl	$-16, %esp
	subl	$32, %esp
	movl	$0, 24(%esp)
	movl	$.LC0, (%esp)
	call	puts
	leal	24(%esp), %eax
	movl	$1, %edx
	lock xaddl	%edx, (%eax)
	movl	%edx, 28(%esp)
	movl	24(%esp), %edx
	movl	$.LC1, %eax
	movl	28(%esp), %ecx
	movl	%ecx, 8(%esp)
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	printf
	leal	24(%esp), %eax
	movl	$1, %edx
	lock xaddl	%edx, (%eax)
	movl	%edx, 28(%esp)
	movl	24(%esp), %edx
	movl	$.LC1, %eax
	movl	28(%esp), %ecx
	movl	%ecx, 8(%esp)
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	printf
	movl	$0, %eax
	leave
	ret
	.size	main, .-main
	.ident	"GCC: (Ubuntu/Linaro 4.5.2-8ubuntu4) 4.5.2"
	.section	.note.GNU-stack,"",@progbits
