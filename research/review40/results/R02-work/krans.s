	.file	"krans.c"
	.text
	.p2align 4
	.type	kr_ctx_init, @function
kr_ctx_init:
.LFB31:
	.cfi_startproc
	cmpl	$5, (%rdi)
	ja	.L94
	movl	4(%rdi), %edx
	movq	%rdi, %rsi
	leal	-1(%rdx), %eax
	cmpl	$7, %eax
	ja	.L94
	movl	8(%rdi), %eax
	cmpl	$3, %eax
	ja	.L94
	movl	12(%rdi), %r10d
	cmpl	$2, %r10d
	ja	.L94
	movl	20(%rdi), %r8d
	cmpl	$2, %r8d
	ja	.L94
	movl	28(%rdi), %ecx
	cmpl	$4, %ecx
	ja	.L94
	testl	%r10d, %r10d
	sete	%r9b
	testl	%ecx, %ecx
	sete	%dil
	cmpb	%dil, %r9b
	jne	.L94
	movl	24(%rsi), %r11d
	movl	16(%rsi), %edi
	testl	%r11d, %r11d
	jne	.L100
.L5:
	pushq	%r13
	.cfi_def_cfa_offset 16
	.cfi_offset 13, -16
	movl	$17, %r9d
	pushq	%r12
	.cfi_def_cfa_offset 24
	.cfi_offset 12, -24
	pushq	%rbp
	.cfi_def_cfa_offset 32
	.cfi_offset 6, -32
	pushq	%rbx
	.cfi_def_cfa_offset 40
	.cfi_offset 3, -40
	cmpl	$1, %r10d
	je	.L6
	xorl	%r9d, %r9d
	cmpl	$2, %r10d
	sete	%r9b
	leal	1(,%r9,8), %r9d
.L6:
	leaq	kr_mbits(%rip), %rbp
	leal	-1(%rdi), %ebx
	movzbl	0(%rbp,%rax), %ebp
	cmpl	$2, %ebx
	ja	.L101
	leaq	CSWTCH.114(%rip), %rax
	movl	%ebp, 52(%rsi)
	movl	(%rax,%rbx,4), %ebx
	movl	%edx, %eax
	movl	$1, 56(%rsi)
	imull	%ebx, %eax
	imull	%r9d, %eax
	movl	%eax, 48(%rsi)
	cmpl	$4096, %eax
	ja	.L4
	testl	%ecx, %ecx
	je	.L26
.L8:
	movl	32(%rsi), %eax
	leal	-1(%rax), %r9d
	cmpl	$16777215, %r9d
	ja	.L4
	cmpl	%eax, 56(%rsi)
	jnb	.L20
	movl	%eax, 56(%rsi)
.L20:
	cmpl	$1, %ecx
	je	.L26
	movl	36(%rsi), %eax
	leal	-1(%rax), %r9d
	cmpl	$16777215, %r9d
	ja	.L4
	cmpl	%eax, 56(%rsi)
	jnb	.L22
	movl	%eax, 56(%rsi)
.L22:
	cmpl	$2, %ecx
	je	.L26
	movl	40(%rsi), %eax
	leal	-1(%rax), %r9d
	cmpl	$16777215, %r9d
	ja	.L4
	cmpl	%eax, 56(%rsi)
	jnb	.L23
	movl	%eax, 56(%rsi)
.L23:
	cmpl	$4, %ecx
	jne	.L26
	movl	44(%rsi), %eax
	leal	-1(%rax), %ecx
	cmpl	$16777215, %ecx
	ja	.L4
	cmpl	%eax, 56(%rsi)
	jnb	.L26
	movl	%eax, 56(%rsi)
.L26:
	movl	%edx, %r9d
	cmpl	$1, %edi
	je	.L10
.L9:
	leal	0(,%r9,4), %ebp
	xorl	%eax, %eax
	leal	0(%rbp,%r9), %r12d
	.p2align 4,,10
	.p2align 3
.L11:
	movl	%eax, %edx
	testl	%r8d, %r8d
	jne	.L27
	leal	1(%rax), %ecx
	shrl	%ecx
	movl	%ecx, %r13d
.L28:
	movb	%r13b, 60(%rsi,%rax)
	cmpl	$2, %edi
	je	.L102
.L29:
	cmpl	$3, %edi
	je	.L103
.L34:
	xorl	%edx, %edx
.L31:
	movw	%dx, 2358(%rsi,%rax,2)
	addq	$1, %rax
	cmpq	$256, %rax
	jne	.L11
.L96:
	movdqa	.LC50(%rip), %xmm4
.L18:
	imull	%r9d, %ebx
	cmpl	$1, %r10d
	je	.L35
	cmpl	$2, %r10d
	je	.L36
	leaq	324(%rsi), %rdi
	movl	%esi, %eax
	movq	$0, 316(%rsi)
	movq	$0, 2350(%rsi)
	andq	$-8, %rdi
	subl	%edi, %eax
	leal	2358(%rax), %ecx
	xorl	%eax, %eax
	shrl	$3, %ecx
	rep stosq
.L37:
	negl	%r11d
	pxor	%xmm2, %xmm2
	sbbl	%eax, %eax
	movd	%eax, %xmm0
	xorl	%eax, %eax
	pshufd	$0, %xmm0, %xmm0
	pand	.LC58(%rip), %xmm0
	pand	%xmm4, %xmm0
	movdqa	%xmm0, %xmm1
	punpcklwd	%xmm2, %xmm0
	punpckhwd	%xmm2, %xmm1
	movdqa	%xmm0, %xmm2
	punpckhwd	%xmm1, %xmm2
	punpcklwd	%xmm1, %xmm0
	pcmpeqd	%xmm1, %xmm1
	punpcklwd	%xmm2, %xmm0
	paddw	%xmm1, %xmm0
	movups	%xmm0, 2870(%rsi)
	popq	%rbx
	.cfi_def_cfa_offset 32
	popq	%rbp
	.cfi_def_cfa_offset 24
	popq	%r12
	.cfi_def_cfa_offset 16
	popq	%r13
	.cfi_def_cfa_offset 8
	ret
.L100:
	.cfi_restore 3
	.cfi_restore 6
	.cfi_restore 12
	.cfi_restore 13
	cmpl	$1, %edx
	je	.L94
	testl	%edi, %edi
	jne	.L5
.L94:
	movl	$-1, %eax
	ret
	.p2align 4,,10
	.p2align 3
.L103:
	.cfi_def_cfa_offset 40
	.cfi_offset 3, -40
	.cfi_offset 6, -32
	.cfi_offset 12, -24
	.cfi_offset 13, -16
	shrl	$4, %edx
	imull	%r9d, %edx
	jmp	.L31
.L101:
	imull	%edx, %r9d
	movl	%ebp, 52(%rsi)
	movl	$1, %ebx
	movl	$1, 56(%rsi)
	movl	%r9d, 48(%rsi)
	movl	%edx, %r9d
	testl	%ecx, %ecx
	jne	.L8
	jmp	.L9
	.p2align 4,,10
	.p2align 3
.L27:
	movl	%eax, %r13d
	movl	%eax, %ecx
	cmpl	$1, %r8d
	jne	.L28
	cmpq	$127, %rax
	jbe	.L28
	negl	%r13d
	movb	%r13b, 60(%rsi,%rax)
	movzbl	%r13b, %ecx
	cmpl	$2, %edi
	jne	.L29
	cmpl	$2, %ecx
	jbe	.L72
	.p2align 4,,10
	.p2align 3
.L104:
	leal	(%r9,%r9), %edx
	cmpl	$5, %ecx
	jbe	.L31
	cmpl	$25, %ecx
	movl	%r12d, %edx
	leal	(%r9,%r9,2), %r13d
	cmovb	%ebp, %edx
	cmpl	$12, %ecx
	movl	%r13d, %ecx
	cmovb	%ecx, %edx
	jmp	.L31
	.p2align 4,,10
	.p2align 3
.L102:
	testl	%ecx, %ecx
	je	.L34
	cmpl	$2, %ecx
	ja	.L104
.L72:
	movl	%r9d, %edx
	jmp	.L31
.L4:
	popq	%rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 32
	movl	$-1, %eax
	popq	%rbp
	.cfi_def_cfa_offset 24
	popq	%r12
	.cfi_def_cfa_offset 16
	popq	%r13
	.cfi_def_cfa_offset 8
	ret
.L10:
	.cfi_restore_state
	testl	%r8d, %r8d
	je	.L12
	cmpl	$1, %r8d
	je	.L69
	movdqa	.LC1(%rip), %xmm1
	movd	%edx, %xmm0
	punpcklwd	%xmm0, %xmm0
	movups	%xmm1, 60(%rsi)
	movdqa	.LC2(%rip), %xmm1
	pshufd	$0, %xmm0, %xmm0
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2358(%rsi)
	movdqa	.LC3(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2374(%rsi)
	movdqa	.LC4(%rip), %xmm1
	movups	%xmm1, 76(%rsi)
	movdqa	.LC5(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2390(%rsi)
	movdqa	.LC6(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2406(%rsi)
	movdqa	.LC7(%rip), %xmm1
	movups	%xmm1, 92(%rsi)
	movdqa	.LC8(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2422(%rsi)
	movdqa	.LC9(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2438(%rsi)
	movdqa	.LC10(%rip), %xmm1
	movups	%xmm1, 108(%rsi)
	movdqa	.LC11(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2454(%rsi)
	movdqa	.LC12(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2470(%rsi)
	movdqa	.LC13(%rip), %xmm1
	movups	%xmm1, 124(%rsi)
	movdqa	.LC14(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2486(%rsi)
	movdqa	.LC15(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2502(%rsi)
	movdqa	.LC16(%rip), %xmm1
	movups	%xmm1, 140(%rsi)
	movdqa	.LC17(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2518(%rsi)
	movdqa	.LC18(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2534(%rsi)
	movdqa	.LC19(%rip), %xmm1
	movups	%xmm1, 156(%rsi)
	movdqa	.LC20(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2550(%rsi)
	movdqa	.LC21(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2566(%rsi)
	movdqa	.LC22(%rip), %xmm1
	movups	%xmm1, 172(%rsi)
	movdqa	.LC23(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2582(%rsi)
	movdqa	.LC24(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2598(%rsi)
	movdqa	.LC25(%rip), %xmm1
	movups	%xmm1, 188(%rsi)
	movdqa	.LC26(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2614(%rsi)
	movdqa	.LC27(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2630(%rsi)
	movdqa	.LC28(%rip), %xmm1
	movups	%xmm1, 204(%rsi)
	movdqa	.LC29(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2646(%rsi)
	movdqa	.LC30(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2662(%rsi)
	movdqa	.LC31(%rip), %xmm1
	movups	%xmm1, 220(%rsi)
	movdqa	.LC32(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2678(%rsi)
	movdqa	.LC33(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2694(%rsi)
	movdqa	.LC34(%rip), %xmm1
	movups	%xmm1, 236(%rsi)
	movdqa	.LC35(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2710(%rsi)
	movdqa	.LC36(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2726(%rsi)
	movdqa	.LC37(%rip), %xmm1
	movups	%xmm1, 252(%rsi)
	movdqa	.LC38(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2742(%rsi)
	movdqa	.LC39(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2758(%rsi)
	movdqa	.LC40(%rip), %xmm1
	movups	%xmm1, 268(%rsi)
	movdqa	.LC41(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2774(%rsi)
	movdqa	.LC42(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2790(%rsi)
	movdqa	.LC43(%rip), %xmm1
	movups	%xmm1, 284(%rsi)
	movdqa	.LC44(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2806(%rsi)
	movdqa	.LC45(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2822(%rsi)
	movdqa	.LC46(%rip), %xmm1
	movups	%xmm1, 300(%rsi)
	movdqa	.LC47(%rip), %xmm1
	pmullw	%xmm0, %xmm1
	pmullw	.LC48(%rip), %xmm0
	movups	%xmm0, 2854(%rsi)
	movups	%xmm1, 2838(%rsi)
	movdqa	.LC50(%rip), %xmm4
	jmp	.L18
.L35:
	movl	%ebx, %ecx
	xorl	%eax, %eax
	sall	$4, %ecx
	jmp	.L65
	.p2align 4,,10
	.p2align 3
.L48:
	cmpl	$1, %eax
	je	.L105
	cmpl	$3, %eax
	jbe	.L106
	cmpl	$5, %eax
	jbe	.L107
	cmpl	$7, %eax
	jbe	.L108
	cmpl	$10, %eax
	jbe	.L109
	cmpl	$13, %eax
	jbe	.L110
	cmpl	$17, %eax
	jbe	.L111
	cmpl	$22, %eax
	jbe	.L112
	cmpl	$28, %eax
	jbe	.L113
	cmpl	$36, %eax
	jbe	.L114
	cmpl	$46, %eax
	jbe	.L115
	cmpl	$59, %eax
	jbe	.L116
	cmpl	$79, %eax
	jbe	.L117
	cmpl	$109, %eax
	jbe	.L118
	cmpl	$159, %eax
	jbe	.L119
	movw	%cx, 316(%rsi,%rax,2)
	addq	$1, %rax
	cmpq	$1021, %rax
	je	.L37
.L65:
	testl	%eax, %eax
	jne	.L48
	xorl	%edx, %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L12:
	movd	%edx, %xmm0
	movdqa	.LC0(%rip), %xmm3
	movdqa	.LC49(%rip), %xmm12
	xorl	%eax, %eax
	punpcklwd	%xmm0, %xmm0
	movdqa	.LC50(%rip), %xmm4
	movdqa	.LC51(%rip), %xmm11
	movdqa	.LC52(%rip), %xmm10
	pshufd	$0, %xmm0, %xmm0
	movdqa	.LC53(%rip), %xmm9
	movdqa	.LC54(%rip), %xmm5
	movdqa	.LC56(%rip), %xmm7
	movdqa	.LC55(%rip), %xmm8
	movdqa	.LC57(%rip), %xmm6
.L17:
	movdqa	%xmm3, %xmm13
	paddd	%xmm12, %xmm3
	movdqa	%xmm13, %xmm1
	movdqa	%xmm13, %xmm14
	movdqa	%xmm13, %xmm15
	paddd	%xmm11, %xmm14
	paddd	%xmm4, %xmm1
	movdqa	%xmm1, %xmm2
	punpcklwd	%xmm14, %xmm1
	paddd	%xmm9, %xmm15
	punpckhwd	%xmm14, %xmm2
	movdqa	%xmm1, %xmm14
	punpckhwd	%xmm2, %xmm14
	punpcklwd	%xmm2, %xmm1
	punpcklwd	%xmm14, %xmm1
	psrlw	$1, %xmm1
	movdqa	%xmm1, %xmm14
	movdqa	%xmm13, %xmm1
	paddd	%xmm10, %xmm1
	movdqa	%xmm1, %xmm2
	punpcklwd	%xmm15, %xmm1
	punpckhwd	%xmm15, %xmm2
	movdqa	%xmm1, %xmm15
	punpckhwd	%xmm2, %xmm15
	punpcklwd	%xmm2, %xmm1
	punpcklwd	%xmm15, %xmm1
	movdqa	%xmm1, %xmm2
	movdqa	%xmm14, %xmm1
	movdqa	%xmm13, %xmm14
	psrlw	$1, %xmm2
	pand	%xmm5, %xmm1
	paddd	%xmm8, %xmm14
	pand	%xmm5, %xmm2
	packuswb	%xmm2, %xmm1
	movdqa	%xmm13, %xmm2
	movups	%xmm1, 60(%rsi,%rax)
	movdqa	%xmm13, %xmm1
	punpckhwd	%xmm14, %xmm2
	punpcklwd	%xmm14, %xmm1
	movdqa	%xmm1, %xmm14
	punpcklwd	%xmm2, %xmm1
	punpckhwd	%xmm2, %xmm14
	movdqa	%xmm13, %xmm2
	punpcklwd	%xmm14, %xmm1
	paddd	%xmm7, %xmm2
	pmullw	%xmm0, %xmm1
	movups	%xmm1, 2358(%rsi,%rax,2)
	movdqa	%xmm13, %xmm1
	movdqa	%xmm2, %xmm13
	paddd	%xmm6, %xmm1
	punpcklwd	%xmm1, %xmm2
	punpckhwd	%xmm1, %xmm13
	movdqa	%xmm2, %xmm1
	punpcklwd	%xmm13, %xmm2
	punpckhwd	%xmm13, %xmm1
	punpcklwd	%xmm1, %xmm2
	pmullw	%xmm0, %xmm2
	movups	%xmm2, 2374(%rsi,%rax,2)
	addq	$16, %rax
	cmpq	$256, %rax
	jne	.L17
	jmp	.L18
.L36:
	leal	0(,%rbx,8), %edx
	xorl	%eax, %eax
	jmp	.L40
	.p2align 4,,10
	.p2align 3
.L38:
	cmpq	$2, %rax
	jbe	.L92
	cmpq	$5, %rax
	jbe	.L42
	cmpq	$10, %rax
	jbe	.L43
	cmpq	$17, %rax
	jbe	.L44
	cmpq	$28, %rax
	jbe	.L45
	cmpq	$46, %rax
	jbe	.L46
	cmpq	$79, %rax
	jbe	.L120
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	cmpq	$1021, %rax
	je	.L37
.L40:
	testq	%rax, %rax
	jne	.L38
	xorl	%ecx, %ecx
	movl	$1, %eax
	movw	%cx, 316(%rsi)
.L92:
	movw	%bx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L40
.L105:
	movw	%bx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L106:
	leal	(%rbx,%rbx), %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L108:
	leal	0(,%rbx,4), %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L107:
	leal	(%rbx,%rbx,2), %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L110:
	leal	(%rbx,%rbx,2), %edx
	addl	%edx, %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L109:
	leal	(%rbx,%rbx,4), %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L111:
	leal	0(,%rbx,8), %edx
	subl	%ebx, %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L43:
	leal	(%rbx,%rbx,2), %ecx
	movw	%cx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L40
.L42:
	leal	(%rbx,%rbx), %ecx
	movw	%cx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L40
.L113:
	leal	(%rbx,%rbx,8), %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L112:
	leal	0(,%rbx,8), %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L114:
	leal	(%rbx,%rbx,4), %edx
	addl	%edx, %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L115:
	leal	(%rbx,%rbx,4), %edx
	leal	(%rbx,%rdx,2), %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L117:
	leal	(%rbx,%rbx,2), %edx
	leal	(%rbx,%rdx,4), %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L116:
	leal	(%rbx,%rbx,2), %edx
	sall	$2, %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L120:
	leal	0(,%rbx,8), %ecx
	subl	%ebx, %ecx
	movw	%cx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L40
.L46:
	leal	(%rbx,%rbx,2), %ecx
	addl	%ecx, %ecx
	movw	%cx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L40
.L45:
	leal	(%rbx,%rbx,4), %ecx
	movw	%cx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L40
.L44:
	leal	0(,%rbx,4), %ecx
	movw	%cx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L40
.L69:
	xorl	%edx, %edx
	xorl	%eax, %eax
	.p2align 4,,10
	.p2align 3
.L13:
	cmpq	$127, %rax
	jbe	.L15
.L121:
	movl	%eax, %ecx
	negl	%ecx
	movb	%cl, 60(%rsi,%rax)
	movw	%dx, 2358(%rsi,%rax,2)
	addq	$1, %rax
	addl	%r9d, %edx
	cmpq	$256, %rax
	je	.L96
	cmpq	$127, %rax
	ja	.L121
	.p2align 4,,10
	.p2align 3
.L15:
	movb	%al, 60(%rsi,%rax)
	movw	%dx, 2358(%rsi,%rax,2)
	addq	$1, %rax
	addl	%r9d, %edx
	jmp	.L13
.L119:
	movl	%ebx, %edx
	sall	$4, %edx
	subl	%ebx, %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
.L118:
	leal	0(,%rbx,8), %edx
	subl	%ebx, %edx
	addl	%edx, %edx
	movw	%dx, 316(%rsi,%rax,2)
	addq	$1, %rax
	jmp	.L65
	.cfi_endproc
.LFE31:
	.size	kr_ctx_init, .-kr_ctx_init
	.p2align 4
	.type	kr_cmp64, @function
kr_cmp64:
.LFB54:
	.cfi_startproc
	endbr64
	movq	(%rdi), %rdx
	movq	(%rsi), %rcx
	movl	$1, %eax
	cmpq	%rcx, %rdx
	jb	.L122
	cmpq	%rdx, %rcx
	sbbl	%eax, %eax
.L122:
	ret
	.cfi_endproc
.LFE54:
	.size	kr_cmp64, .-kr_cmp64
	.p2align 4
	.type	kr_reassign, @function
kr_reassign:
.LFB56:
	.cfi_startproc
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	subq	$1336, %rsp
	.cfi_def_cfa_offset 1392
	movl	48(%rsi), %r14d
	movq	%rdi, 24(%rsp)
	movq	%fs:40, %rax
	movq	%rax, 1320(%rsp)
	xorl	%eax, %eax
	movl	2888(%rsi), %eax
	movl	%eax, 20(%rsp)
	testl	%r14d, %r14d
	je	.L125
	movl	%r14d, %r11d
	xorl	%r14d, %r14d
	movq	%rsi, %r15
	xorl	%ecx, %ecx
	leaq	32(%rsp), %r9
	movl	%eax, %r12d
	movq	%r11, %rdi
	movl	%r14d, %ebx
.L139:
	xorl	%esi, %esi
	xorl	%eax, %eax
	.p2align 4,,10
	.p2align 3
.L130:
	movl	(%rdx,%rsi,4), %r8d
	testl	%r8d, %r8d
	je	.L127
	pxor	%xmm0, %xmm0
	movl	%eax, %r10d
	addl	$1, %eax
	cvtsi2ssq	%r8, %xmm0
	movb	%sil, 1056(%rsp,%r10)
	movss	%xmm0, 32(%rsp,%r10,4)
.L127:
	addq	$1, %rsi
	cmpq	$256, %rsi
	jne	.L130
	testl	%eax, %eax
	jne	.L159
.L131:
	addq	$1, %rcx
	addq	$1024, %rdx
	cmpq	%rcx, %rdi
	jne	.L139
	movl	%ebx, %r14d
.L125:
	movq	1320(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L160
	addq	$1336, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	movl	%r14d, %eax
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L159:
	.cfi_restore_state
	movq	2904(%r15), %rsi
	movq	24(%rsp), %r14
	xorl	%r11d, %r11d
	pxor	%xmm2, %xmm2
	leaq	1056(%rsp), %r10
	addq	%rcx, %rsi
	movq	8024(%r14), %r8
	movzbl	(%rsi), %ebp
	movzbl	%bpl, %r14d
	salq	$10, %r14
	.p2align 4,,10
	.p2align 3
.L132:
	movzbl	(%r10,%r11), %r13d
	leaq	(%r8,%r13,4), %r13
	movss	0(%r13,%r14), %xmm0
	mulss	(%r9,%r11,4), %xmm0
	addq	$1, %r11
	addss	%xmm0, %xmm2
	cmpq	%rax, %r11
	jne	.L132
	movl	20(%rsp), %r11d
	testl	%r11d, %r11d
	je	.L131
	movq	%rdi, (%rsp)
	movl	%ebp, %r14d
	xorl	%r11d, %r11d
	movq	%rdx, 8(%rsp)
	.p2align 4,,10
	.p2align 3
.L138:
	cmpl	%r11d, %ebp
	je	.L134
	movq	%r11, %rdi
	movl	%r11d, 16(%rsp)
	xorl	%edx, %edx
	pxor	%xmm1, %xmm1
	salq	$10, %rdi
	jmp	.L135
	.p2align 4,,10
	.p2align 3
.L137:
	movzbl	(%r10,%rdx), %r13d
	leaq	(%r8,%r13,4), %r13
	movss	0(%r13,%rdi), %xmm0
	mulss	(%r9,%rdx,4), %xmm0
	addq	$1, %rdx
	addss	%xmm0, %xmm1
	cmpq	%rax, %rdx
	je	.L136
.L135:
	comiss	%xmm1, %xmm2
	ja	.L137
.L134:
	addq	$1, %r11
	cmpq	%r12, %r11
	jne	.L138
.L161:
	movq	(%rsp), %rdi
	movq	8(%rsp), %rdx
	cmpl	%r14d, %ebp
	je	.L131
	movb	%r14b, (%rsi)
	addl	$1, %ebx
	jmp	.L131
.L136:
	comiss	%xmm1, %xmm2
	movl	16(%rsp), %r13d
	minss	%xmm2, %xmm1
	cmova	%r13d, %r14d
	addq	$1, %r11
	movaps	%xmm1, %xmm2
	cmpq	%r12, %r11
	jne	.L138
	jmp	.L161
.L160:
	call	*__stack_chk_fail@GOTPCREL(%rip)
	.cfi_endproc
.LFE56:
	.size	kr_reassign, .-kr_reassign
	.p2align 4
	.type	kr_enc_free, @function
kr_enc_free:
.LFB46:
	.cfi_startproc
	pushq	%r12
	.cfi_def_cfa_offset 16
	.cfi_offset 12, -16
	leaq	8840(%rdi), %r12
	pushq	%rbp
	.cfi_def_cfa_offset 24
	.cfi_offset 6, -24
	movq	%rdi, %rbp
	pushq	%rbx
	.cfi_def_cfa_offset 32
	.cfi_offset 3, -32
	leaq	2936(%rdi), %rbx
.L163:
	movq	(%rbx), %rdi
	addq	$2952, %rbx
	call	*free@GOTPCREL(%rip)
	movq	-2944(%rbx), %rdi
	call	*free@GOTPCREL(%rip)
	movq	-2936(%rbx), %rdi
	call	*free@GOTPCREL(%rip)
	movq	-2928(%rbx), %rdi
	call	*free@GOTPCREL(%rip)
	movq	-2912(%rbx), %rdi
	call	*free@GOTPCREL(%rip)
	cmpq	%r12, %rbx
	jne	.L163
	movq	5936(%rbp), %rdi
	call	*free@GOTPCREL(%rip)
	movq	5952(%rbp), %rdi
	call	*free@GOTPCREL(%rip)
	movq	8000(%rbp), %rdi
	call	*free@GOTPCREL(%rip)
	movq	8008(%rbp), %rdi
	call	*free@GOTPCREL(%rip)
	movq	8024(%rbp), %rdi
	call	*free@GOTPCREL(%rip)
	movq	8032(%rbp), %rdi
	call	*free@GOTPCREL(%rip)
	movq	8040(%rbp), %rdi
	call	*free@GOTPCREL(%rip)
	movq	8048(%rbp), %rdi
	call	*free@GOTPCREL(%rip)
	popq	%rbx
	.cfi_def_cfa_offset 24
	movq	8056(%rbp), %rdi
	popq	%rbp
	.cfi_def_cfa_offset 16
	popq	%r12
	.cfi_def_cfa_offset 8
	jmp	*free@GOTPCREL(%rip)
	.cfi_endproc
.LFE46:
	.size	kr_enc_free, .-kr_enc_free
	.p2align 4
	.type	kr_cluster, @function
kr_cluster:
.LFB55:
	.cfi_startproc
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	movq	%rdx, %r15
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	movq	%rdi, %r14
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	subq	$3528, %rsp
	.cfi_def_cfa_offset 3584
	movl	48(%rsi), %ebp
	movq	2904(%rsi), %rdi
	movq	%rsi, 152(%rsp)
	xorl	%esi, %esi
	movq	%rdx, 168(%rsp)
	movq	%rbp, %rdx
	movsd	%xmm0, 120(%rsp)
	movq	%fs:40, %rax
	movq	%rax, 3512(%rsp)
	xorl	%eax, %eax
	call	*memset@GOTPCREL(%rip)
	movq	8032(%r14), %rdi
	testl	%ebp, %ebp
	je	.L167
	movq	%r15, %rsi
	leaq	1024(%r15), %rdx
	xorl	%ecx, %ecx
	xorl	%ebx, %ebx
	pxor	%xmm2, %xmm2
.L170:
	movq	%rsi, %rax
	pxor	%xmm1, %xmm1
	.p2align 4,,10
	.p2align 3
.L168:
	movdqu	(%rax), %xmm0
	addq	$16, %rax
	movdqa	%xmm0, %xmm3
	punpckldq	%xmm2, %xmm0
	punpckhdq	%xmm2, %xmm3
	paddq	%xmm3, %xmm0
	paddq	%xmm0, %xmm1
	cmpq	%rax, %rdx
	jne	.L168
	movdqa	%xmm1, %xmm0
	psrldq	$8, %xmm0
	paddq	%xmm0, %xmm1
	movq	%xmm1, %rax
	testq	%rax, %rax
	je	.L169
	salq	$12, %rax
	movl	%ebx, %r8d
	addl	$1, %ebx
	orq	%rcx, %rax
	movq	%rax, (%rdi,%r8,8)
.L169:
	addq	$1, %rcx
	addq	$1024, %rsi
	addq	$1024, %rdx
	cmpq	%rcx, %rbp
	jne	.L170
	movl	%ebx, %ebp
	movl	$8, %edx
	leaq	kr_cmp64(%rip), %rcx
	movq	%rbp, %rsi
	call	*qsort@GOTPCREL(%rip)
	leaq	0(,%rbp,8), %rax
	xorl	%edx, %edx
	movq	$0, 128(%rsp)
	movq	%rax, 160(%rsp)
	leaq	176(%rsp), %r9
	testl	%ebx, %ebx
	je	.L172
	movl	%edx, 140(%rsp)
.L209:
	movq	128(%rsp), %rdi
	xorl	%ebx, %ebx
	xorl	%ebp, %ebp
	movq	%r9, %r15
	movq	8032(%r14), %rax
	movq	(%rax,%rdi), %rax
	movq	%rax, %rdi
	andl	$4095, %eax
	movq	%rax, %r13
	movq	%rax, 144(%rsp)
	shrq	$12, %rdi
	movq	168(%rsp), %rax
	salq	$10, %r13
	movq	%rdi, 96(%rsp)
	addq	%rax, %r13
	jmp	.L180
	.p2align 4,,10
	.p2align 3
.L238:
	movq	8008(%r14), %rax
	movsd	(%rax,%rdx,8), %xmm0
.L177:
	movsd	%xmm0, 1200(%rsp,%r12,8)
	addl	$1, %ebp
.L175:
	addq	$1, %rbx
	cmpq	$256, %rbx
	je	.L237
.L180:
	movl	0(%r13,%rbx,4), %eax
	testl	%eax, %eax
	je	.L175
	movl	%ebp, %r12d
	movl	%eax, %edx
	movb	%bl, 3248(%rsp,%r12)
	movl	%eax, 176(%rsp,%r12,4)
	cmpl	$4095, %eax
	jbe	.L238
	pxor	%xmm1, %xmm1
	cvtsi2sdq	%rdx, %xmm1
	movapd	%xmm1, %xmm0
	movsd	%xmm1, 8(%rsp)
	call	*log2@GOTPCREL(%rip)
	movsd	8(%rsp), %xmm1
	mulsd	%xmm1, %xmm0
	jmp	.L177
.L237:
	movsd	.LC60(%rip), %xmm7
	movl	140(%rsp), %eax
	movq	%r15, %r9
	movsd	%xmm7, 80(%rsp)
	cmpl	20(%r14), %eax
	jnb	.L181
	movl	%ebp, %eax
	pxor	%xmm0, %xmm0
	cvtsi2sdq	%rax, %xmm0
	mulsd	120(%rsp), %xmm0
	addsd	.LC62(%rip), %xmm0
	movsd	%xmm0, 80(%rsp)
.L181:
	movq	96(%rsp), %rdi
	cmpq	$4095, %rdi
	ja	.L184
	movq	8008(%r14), %rax
	movsd	(%rax,%rdi,8), %xmm3
.L185:
	movl	140(%rsp), %edx
	movq	5952(%r14), %r15
	testl	%edx, %edx
	je	.L239
	movl	140(%rsp), %eax
	xorl	%r12d, %r12d
	movq	%r15, %rdx
	movq	%r14, %rcx
	movl	$-1, 76(%rsp)
	movq	8000(%r14), %rdi
	leaq	1200(%rsp), %r11
	movq	%rax, 104(%rsp)
	movl	%ebp, 136(%rsp)
	movq	%r12, %rbp
	movsd	%xmm3, 112(%rsp)
	.p2align 4,,10
	.p2align 3
.L206:
	movq	5960(%rcx,%rbp,8), %rbx
	movq	96(%rsp), %rax
	movq	%rbp, %r12
	movq	%rbp, %rsi
	salq	$10, %r12
	salq	$11, %rsi
	addq	%rbx, %rax
	cmpq	$4095, %rax
	ja	.L188
	movq	8008(%rcx), %r8
	movsd	(%r8,%rax,8), %xmm2
.L189:
	cmpq	$4095, %rbx
	ja	.L192
	movq	8008(%rcx), %rax
	movsd	(%rax,%rbx,8), %xmm0
.L193:
	movl	136(%rsp), %eax
	subsd	%xmm0, %xmm2
	subsd	112(%rsp), %xmm2
	testl	%eax, %eax
	je	.L218
	leaq	3248(%rsp), %r14
	movl	%eax, %r15d
	xorl	%ebx, %ebx
	xorl	%r13d, %r13d
	movq	%rbp, 88(%rsp)
	movq	%r14, %r10
	movq	%rdi, %rbp
	movq	%r12, %r14
	jmp	.L201
	.p2align 4,,10
	.p2align 3
.L241:
	movl	(%r9,%rbx,4), %r8d
	addq	%rax, %r8
	cmpq	$4095, %r8
	ja	.L198
	movq	8008(%rcx), %rax
	movsd	(%rax,%r8,8), %xmm0
.L199:
	leaq	0(%rbp,%r12,8), %rax
	subsd	(%rax,%rsi), %xmm0
	subsd	(%r11,%rbx,8), %xmm0
	addq	$1, %rbx
	subsd	%xmm0, %xmm2
	cmpq	%r15, %rbx
	je	.L240
.L201:
	movzbl	(%r10,%rbx), %r12d
	leaq	(%rdx,%r12,4), %rax
	movl	(%rax,%r14), %eax
	testl	%eax, %eax
	jne	.L241
	addq	$1, %rbx
	addl	$1, %r13d
	cmpq	%r15, %rbx
	jne	.L201
.L240:
	pxor	%xmm0, %xmm0
	movq	%rbp, %rdi
	movq	88(%rsp), %rbp
	cvtsi2sdq	%r13, %xmm0
.L196:
	mulsd	120(%rsp), %xmm0
	movsd	80(%rsp), %xmm4
	movl	76(%rsp), %eax
	addsd	%xmm2, %xmm0
	comisd	%xmm0, %xmm4
	minsd	%xmm4, %xmm0
	cmova	%ebp, %eax
	addq	$1, %rbp
	movsd	%xmm0, 80(%rsp)
	movl	%eax, 76(%rsp)
	movq	104(%rsp), %rax
	cmpq	%rax, %rbp
	jne	.L206
	movl	76(%rsp), %eax
	movl	136(%rsp), %ebp
	movq	%rdx, %r15
	movq	%rcx, %r14
	testl	%eax, %eax
	js	.L207
.L187:
	movslq	76(%rsp), %rax
	movl	%ebp, %ebx
	xorl	%r12d, %r12d
	leaq	3248(%rsp), %r13
	movq	%rax, %r8
	movq	%rax, 48(%rsp)
	salq	$11, %rax
	salq	$10, %r8
	movq	%rax, %rsi
	testl	%ebp, %ebp
	je	.L212
	.p2align 4,,10
	.p2align 3
.L215:
	movzbl	0(%r13,%r12), %ecx
	movl	(%r9,%r12,4), %eax
	leaq	(%r8,%rcx,4), %rdx
	leaq	(%rsi,%rcx,8), %rbp
	addq	%r15, %rdx
	addq	%rdi, %rbp
	addl	(%rdx), %eax
	movl	%eax, (%rdx)
	movl	%eax, %edx
	cmpl	$4095, %eax
	ja	.L210
	movq	8008(%r14), %rax
	addq	$1, %r12
	movsd	(%rax,%rdx,8), %xmm0
	movsd	%xmm0, 0(%rbp)
	cmpq	%r12, %rbx
	jne	.L215
.L212:
	movq	48(%rsp), %rax
	movq	96(%rsp), %rdi
	addq	$8, 128(%rsp)
	movzbl	76(%rsp), %esi
	addq	%rdi, 5960(%r14,%rax,8)
	movq	152(%rsp), %rax
	movq	144(%rsp), %rdi
	movq	2904(%rax), %rax
	movb	%sil, (%rax,%rdi)
	movq	128(%rsp), %rax
	movq	160(%rsp), %rdi
	cmpq	%rdi, %rax
	jne	.L209
	movl	140(%rsp), %edx
.L174:
	movq	152(%rsp), %rax
	movl	%edx, 2888(%rax)
	movq	3512(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L242
	addq	$3528, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
	.p2align 4,,10
	.p2align 3
.L198:
	.cfi_restore_state
	pxor	%xmm1, %xmm1
	movq	%r9, 64(%rsp)
	cvtsi2sdq	%r8, %xmm1
	movq	%r11, 56(%rsp)
	movq	%r10, 48(%rsp)
	movq	%rcx, 40(%rsp)
	movq	%rdx, 32(%rsp)
	movq	%rsi, 24(%rsp)
	movapd	%xmm1, %xmm0
	movsd	%xmm2, 16(%rsp)
	movsd	%xmm1, 8(%rsp)
	call	*log2@GOTPCREL(%rip)
	movsd	8(%rsp), %xmm1
	movq	64(%rsp), %r9
	movq	56(%rsp), %r11
	movq	48(%rsp), %r10
	mulsd	%xmm1, %xmm0
	movq	40(%rsp), %rcx
	movq	32(%rsp), %rdx
	movq	24(%rsp), %rsi
	movsd	16(%rsp), %xmm2
	jmp	.L199
	.p2align 4,,10
	.p2align 3
.L210:
	pxor	%xmm1, %xmm1
	movq	%r9, 40(%rsp)
	addq	$1, %r12
	cvtsi2sdq	%rdx, %xmm1
	movq	%rdi, 32(%rsp)
	movq	%rsi, 24(%rsp)
	movq	%r8, 16(%rsp)
	movapd	%xmm1, %xmm0
	movsd	%xmm1, 8(%rsp)
	call	*log2@GOTPCREL(%rip)
	movsd	8(%rsp), %xmm1
	cmpq	%rbx, %r12
	movq	16(%rsp), %r8
	movq	24(%rsp), %rsi
	movq	32(%rsp), %rdi
	mulsd	%xmm1, %xmm0
	movq	40(%rsp), %r9
	movsd	%xmm0, 0(%rbp)
	jne	.L215
	jmp	.L212
	.p2align 4,,10
	.p2align 3
.L192:
	testq	%rbx, %rbx
	js	.L194
	pxor	%xmm1, %xmm1
	cvtsi2sdq	%rbx, %xmm1
.L195:
	movq	%r9, 64(%rsp)
	movapd	%xmm1, %xmm0
	movq	%r11, 56(%rsp)
	movq	%rcx, 48(%rsp)
	movq	%rdx, 40(%rsp)
	movq	%rdi, 32(%rsp)
	movq	%rsi, 24(%rsp)
	movsd	%xmm2, 16(%rsp)
	movsd	%xmm1, 8(%rsp)
	call	*log2@GOTPCREL(%rip)
	movsd	8(%rsp), %xmm1
	movq	64(%rsp), %r9
	movq	56(%rsp), %r11
	movq	48(%rsp), %rcx
	mulsd	%xmm1, %xmm0
	movq	40(%rsp), %rdx
	movq	32(%rsp), %rdi
	movq	24(%rsp), %rsi
	movsd	16(%rsp), %xmm2
	jmp	.L193
	.p2align 4,,10
	.p2align 3
.L188:
	testq	%rax, %rax
	js	.L190
	pxor	%xmm2, %xmm2
	cvtsi2sdq	%rax, %xmm2
.L191:
	movq	%r9, 56(%rsp)
	movapd	%xmm2, %xmm0
	movq	%r11, 48(%rsp)
	movq	%rcx, 40(%rsp)
	movq	%rdx, 32(%rsp)
	movq	%rdi, 24(%rsp)
	movq	%rsi, 16(%rsp)
	movsd	%xmm2, 8(%rsp)
	call	*log2@GOTPCREL(%rip)
	movsd	8(%rsp), %xmm2
	movq	56(%rsp), %r9
	movq	48(%rsp), %r11
	movq	40(%rsp), %rcx
	mulsd	%xmm0, %xmm2
	movq	32(%rsp), %rdx
	movq	24(%rsp), %rdi
	movq	16(%rsp), %rsi
	jmp	.L189
	.p2align 4,,10
	.p2align 3
.L194:
	movq	%rbx, %rax
	andl	$1, %ebx
	pxor	%xmm0, %xmm0
	shrq	%rax
	orq	%rbx, %rax
	cvtsi2sdq	%rax, %xmm0
	movapd	%xmm0, %xmm1
	addsd	%xmm0, %xmm1
	jmp	.L195
	.p2align 4,,10
	.p2align 3
.L190:
	movq	%rax, %r8
	andl	$1, %eax
	pxor	%xmm2, %xmm2
	shrq	%r8
	orq	%rax, %r8
	cvtsi2sdq	%r8, %xmm2
	addsd	%xmm2, %xmm2
	jmp	.L191
.L218:
	pxor	%xmm0, %xmm0
	jmp	.L196
.L184:
	pxor	%xmm3, %xmm3
	movq	%r9, 16(%rsp)
	cvtsi2sdq	96(%rsp), %xmm3
	movapd	%xmm3, %xmm0
	movsd	%xmm3, 8(%rsp)
	call	*log2@GOTPCREL(%rip)
	movsd	8(%rsp), %xmm3
	movq	16(%rsp), %r9
	mulsd	%xmm0, %xmm3
	jmp	.L185
.L239:
	movq	$0, 104(%rsp)
.L207:
	movq	104(%rsp), %rsi
	movl	140(%rsp), %ebx
	xorl	%eax, %eax
	movq	%rsi, %rdx
	movl	%ebx, 76(%rsp)
	salq	$11, %rsi
	salq	$10, %rdx
	addq	%r15, %rdx
	leaq	8(%rdx), %rdi
	movq	$0, (%rdx)
	movq	$0, 1016(%rdx)
	andq	$-8, %rdi
	subq	%rdi, %rdx
	leal	1024(%rdx), %ecx
	shrl	$3, %ecx
	rep stosq
	movq	8000(%r14), %rdx
	addq	%rsi, %rdx
	leaq	8(%rdx), %rdi
	movq	$0, (%rdx)
	movq	$0, 2040(%rdx)
	andq	$-8, %rdi
	subq	%rdi, %rdx
	leal	2048(%rdx), %ecx
	shrl	$3, %ecx
	rep stosq
	movslq	%ebx, %rax
	movq	$0, 5960(%r14,%rax,8)
	leal	1(%rbx), %eax
	movl	%eax, 140(%rsp)
	movq	5952(%r14), %r15
	movq	8000(%r14), %rdi
	jmp	.L187
.L167:
	leaq	kr_cmp64(%rip), %rcx
	movl	$8, %edx
	xorl	%esi, %esi
	call	*qsort@GOTPCREL(%rip)
.L172:
	movl	$1, %edx
	jmp	.L174
.L242:
	call	*__stack_chk_fail@GOTPCREL(%rip)
	.cfi_endproc
.LFE55:
	.size	kr_cluster, .-kr_cluster
	.p2align 4
	.type	kr_dec_table, @function
kr_dec_table:
.LFB40:
	.cfi_startproc
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	movl	%esi, %r11d
	movq	%rdi, %rax
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	movq	%rdx, %r14
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	subq	$568, %rsp
	.cfi_def_cfa_offset 624
	movl	24(%rdi), %esi
	movq	%fs:40, %rdx
	movq	%rdx, 552(%rsp)
	xorl	%edx, %edx
	movq	16(%rdi), %rdx
	cmpl	$7, %esi
	ja	.L345
	movq	(%rdi), %rcx
	cmpq	8(%rdi), %rcx
	jnb	.L247
	leaq	1(%rcx), %r8
	movq	%r8, (%rdi)
	movzbl	(%rcx), %edi
	movl	%esi, %ecx
	salq	%cl, %rdi
	movq	%r8, %rcx
	orq	%rdi, %rdx
.L248:
	movzbl	%dl, %r8d
	shrq	$8, %rdx
	movl	%esi, 24(%rax)
	movq	%rdx, 16(%rax)
.L245:
	cmpq	8(%rax), %rcx
	jnb	.L250
	leaq	1(%rcx), %rdi
	movq	%rdi, (%rax)
	movzbl	(%rcx), %edi
	movl	%esi, %ecx
	salq	%cl, %rdi
	orq	%rdi, %rdx
.L249:
	movzbl	%dl, %r15d
	shrq	$8, %rdx
	movl	%esi, 24(%rax)
	movq	%rdx, 16(%rax)
	cmpl	%r8d, %r15d
	jb	.L252
	movl	%r8d, %r10d
	movl	$-1, 4(%rsp)
	xorl	%edi, %edi
	xorl	%ebx, %ebx
	movq	%r10, 8(%rsp)
	movq	%r10, %r8
	movq	%r14, 16(%rsp)
	movq	%r10, 24(%rsp)
	jmp	.L253
	.p2align 4,,10
	.p2align 3
.L349:
	leaq	1(%rcx), %rsi
	movq	%rsi, (%rax)
	movzbl	(%rcx), %ecx
	orq	%rcx, %rdx
.L256:
	movq	%rdx, %r10
	movl	$7, 24(%rax)
	movl	$7, %esi
	shrq	%r10
	movq	%r10, 16(%rax)
	testb	$1, %dl
	jne	.L346
.L257:
	movl	%ebx, %r9d
	movq	%r10, %rdx
.L268:
	cmpl	$12, %r9d
	ja	.L252
	leaq	32(%rsp), %rbp
	xorl	%ecx, %ecx
	movw	%cx, 0(%rbp,%r8,2)
	testl	%ebx, %ebx
	jne	.L347
.L288:
	addq	$1, %r8
	cmpl	%r8d, %r15d
	jb	.L294
.L253:
	testl	%esi, %esi
	jne	.L348
	movq	(%rax), %rcx
	cmpq	8(%rax), %rcx
	jb	.L349
	movl	$1, 28(%rax)
	jmp	.L256
	.p2align 4,,10
	.p2align 3
.L347:
	subl	$1, %r9d
	movl	%r11d, %r10d
	leal	-1(%rbx), %ecx
	movl	$1, %r13d
	cmpl	%r11d, %r9d
	cmovbe	%r9d, %r10d
	sall	%cl, %r13d
	cmpl	%r10d, %esi
	jnb	.L289
	movq	8(%rax), %r14
	movq	(%rax), %r12
	movl	%esi, %ecx
	movq	%r14, %rsi
.L291:
	cmpq	%rsi, %r12
	jb	.L350
	movl	%ecx, %esi
	leal	8(%rcx), %ecx
	cmpl	%r10d, %ecx
	jnb	.L351
	movl	$1, 28(%rax)
	addl	$16, %esi
.L289:
	movl	%r10d, %ecx
	movl	$-1, %r12d
	subl	%r10d, %esi
	sall	%cl, %r12d
	movl	%esi, 24(%rax)
	notl	%r12d
	andl	%edx, %r12d
	shrq	%cl, %rdx
	movl	%r9d, %ecx
	subl	%r10d, %ecx
	movq	%rdx, 16(%rax)
	sall	%cl, %r12d
	orl	%r12d, %r13d
	movw	%r13w, 0(%rbp,%r8,2)
	movzwl	%r13w, %r13d
	addq	$1, %r8
	addl	%r13d, %edi
	cmpl	%r8d, %r15d
	jnb	.L253
.L294:
	movl	28(%rax), %r13d
	movq	8(%rsp), %r12
	movq	16(%rsp), %r14
	movq	24(%rsp), %r10
	testl	%r13d, %r13d
	jne	.L252
	movl	4(%rsp), %ebx
	testl	%ebx, %ebx
	js	.L252
	cmpl	$4095, %edi
	ja	.L252
	cmpw	$0, 32(%rsp,%r10,2)
	je	.L252
	movl	%r15d, %eax
	cmpw	$0, 32(%rsp,%rax,2)
	je	.L252
	movl	$4096, %edx
	movslq	%ebx, %rax
	movl	%r13d, 4(%rsp)
	xorl	%ebx, %ebx
	subl	%edi, %edx
	movq	%r14, %r13
	movw	%dx, 32(%rsp,%rax,2)
	jmp	.L298
	.p2align 4,,10
	.p2align 3
.L297:
	addq	$1, %r12
	cmpl	%r12d, %r15d
	jb	.L352
.L298:
	movzwl	0(%rbp,%r12,2), %eax
	testw	%ax, %ax
	je	.L297
	movl	%ebx, %edx
	movzwl	%ax, %r14d
	movl	%ebx, %edi
	movl	%r12d, %esi
	sall	$16, %edx
	addq	%r13, %rdi
	addl	%r14d, %ebx
	orl	%r14d, %edx
	movl	%edx, 4096(%r13,%r12,4)
	addq	$1, %r12
	movzwl	%ax, %edx
	call	*memset@GOTPCREL(%rip)
	cmpl	%r12d, %r15d
	jnb	.L298
.L352:
	movl	4(%rsp), %r13d
	jmp	.L243
	.p2align 4,,10
	.p2align 3
.L350:
	leaq	1(%r12), %r14
	movq	%r14, (%rax)
	movzbl	(%r12), %r12d
	salq	%cl, %r12
	addl	$8, %ecx
	orq	%r12, %rdx
	movl	%ecx, 24(%rax)
	movq	%rdx, 16(%rax)
	cmpl	%r10d, %ecx
	jb	.L353
	movl	%ecx, %esi
	jmp	.L289
	.p2align 4,,10
	.p2align 3
.L353:
	movq	%r14, %r12
	jmp	.L291
	.p2align 4,,10
	.p2align 3
.L250:
	movl	$1, 28(%rax)
	jmp	.L249
	.p2align 4,,10
	.p2align 3
.L247:
	movl	$1, 28(%rdi)
	jmp	.L248
	.p2align 4,,10
	.p2align 3
.L348:
	movq	%rdx, %r10
	leal	-1(%rsi), %ecx
	shrq	%r10
	movl	%ecx, 24(%rax)
	movq	%r10, 16(%rax)
	testb	$1, %dl
	jne	.L354
	movl	%ecx, %esi
	jmp	.L257
.L351:
	movl	$1, 28(%rax)
	movl	%ecx, %esi
	jmp	.L289
.L252:
	movl	$-1, %r13d
.L243:
	movq	552(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L355
	addq	$568, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	movl	%r13d, %eax
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
	.p2align 4,,10
	.p2align 3
.L346:
	.cfi_restore_state
	shrq	$2, %rdx
	andl	$1, %r10d
	movl	$6, 24(%rax)
	movq	%rdx, 16(%rax)
	jne	.L356
	movl	$6, %esi
	.p2align 4,,10
	.p2align 3
.L260:
	subl	$1, %ebx
	movl	%ebx, %r9d
	jmp	.L268
.L345:
	leal	-8(%rsi), %ecx
	movzbl	%dl, %r8d
	shrq	$8, %rdx
	movq	%rdx, 16(%rdi)
	movl	%ecx, 24(%rdi)
	cmpl	$7, %ecx
	jbe	.L357
	subl	$16, %esi
	jmp	.L249
.L357:
	movl	%ecx, %esi
	movq	(%rdi), %rcx
	jmp	.L245
.L355:
	call	*__stack_chk_fail@GOTPCREL(%rip)
.L354:
	testl	%ecx, %ecx
	jne	.L358
	movq	(%rax), %rdx
	cmpq	8(%rax), %rdx
	jnb	.L262
	leaq	1(%rdx), %rcx
	movq	%rcx, (%rax)
	movzbl	(%rdx), %edx
	orq	%rdx, %r10
.L263:
	movq	%r10, %rdx
	movl	$7, 24(%rax)
	movl	$7, %esi
	shrq	%rdx
	movq	%rdx, 16(%rax)
	testb	$1, %r10b
	je	.L260
	movl	%edx, %ecx
	movq	%r10, %rdx
	movl	$6, 24(%rax)
	movl	$6, %esi
	shrq	$2, %rdx
	andl	$1, %ecx
	movq	%rdx, 16(%rax)
	testl	%ecx, %ecx
	jne	.L299
	.p2align 4,,10
	.p2align 3
.L266:
	addl	$1, %ebx
	movl	%ebx, %r9d
	jmp	.L268
	.p2align 4,,10
	.p2align 3
.L262:
	movl	$1, 28(%rax)
	jmp	.L263
.L358:
	movq	%rdx, %rcx
	subl	$2, %esi
	shrq	$2, %rcx
	andb	$1, %r10b
	movl	%esi, 24(%rax)
	movq	%rcx, 16(%rax)
	movq	%rcx, %rdx
	je	.L260
	testl	%esi, %esi
	jne	.L300
	movq	(%rax), %rsi
	cmpq	8(%rax), %rsi
	jnb	.L269
	leaq	1(%rsi), %rcx
	movq	%rcx, (%rax)
	movzbl	(%rsi), %ecx
	orq	%rdx, %rcx
.L270:
	movq	%rcx, %rdx
	movl	$7, 24(%rax)
	movl	$7, %esi
	shrq	%rdx
	movq	%rdx, 16(%rax)
	testb	$1, %cl
	je	.L266
	movl	%edx, %esi
	shrq	$2, %rcx
	movl	$6, 24(%rax)
	andl	$1, %esi
	movq	%rcx, 16(%rax)
	movq	%rcx, %rdx
	testl	%esi, %esi
	movl	$6, %esi
	jne	.L277
	.p2align 4,,10
	.p2align 3
.L273:
	subl	$2, %ebx
	movl	%ebx, %r9d
	jmp	.L268
	.p2align 4,,10
	.p2align 3
.L269:
	movl	$1, 28(%rax)
	jmp	.L270
.L356:
	movq	%rdx, %rcx
	movl	$6, %esi
.L300:
	movq	%rcx, %rdx
	subl	$1, %esi
	shrq	%rdx
	andb	$1, %cl
	movl	%esi, 24(%rax)
	movq	%rdx, 16(%rax)
	je	.L266
	testl	%esi, %esi
	jne	.L299
	movq	(%rax), %rcx
	cmpq	8(%rax), %rcx
	jnb	.L275
	leaq	1(%rcx), %rsi
	movq	%rsi, (%rax)
	movzbl	(%rcx), %ecx
	orq	%rcx, %rdx
.L276:
	movl	%edx, %ecx
	shrq	%rdx
	movl	$7, 24(%rax)
	movl	$7, %esi
	andl	$1, %ecx
	movq	%rdx, 16(%rax)
	testl	%ecx, %ecx
	je	.L273
.L277:
	movl	%edx, %r9d
	leal	-1(%rsi), %ecx
	shrq	%rdx
	andl	$1, %r9d
	movq	%rdx, 16(%rax)
	movl	%ecx, 24(%rax)
	testl	%r9d, %r9d
	jne	.L359
	movl	%ecx, %esi
.L279:
	addl	$2, %ebx
	movl	%ebx, %r9d
	jmp	.L268
.L359:
	cmpl	$3, %ecx
	ja	.L283
	movq	(%rax), %r9
	cmpq	8(%rax), %r9
	jnb	.L285
	leaq	1(%r9), %r10
	movq	%r10, (%rax)
	movzbl	(%r9), %r9d
	salq	%cl, %r9
	orq	%r9, %rdx
.L286:
	leal	7(%rsi), %ecx
.L283:
	movl	%edx, %r9d
	leal	-4(%rcx), %esi
	shrq	$4, %rdx
	andl	$15, %r9d
	movq	%rdx, 16(%rax)
	movl	%esi, 24(%rax)
	cmpl	$14, %r9d
	je	.L287
	movl	%r9d, %ebx
	jmp	.L268
.L285:
	movl	$1, 28(%rax)
	jmp	.L286
.L287:
	movl	4(%rsp), %r10d
	testl	%r10d, %r10d
	jns	.L252
	leaq	32(%rsp), %rbp
	movl	$1, %r9d
	movl	%r8d, 4(%rsp)
	movw	%r9w, 0(%rbp,%r8,2)
	jmp	.L288
	.p2align 4,,10
	.p2align 3
.L275:
	movl	$1, 28(%rax)
	jmp	.L276
.L299:
	movl	%edx, %ecx
	subl	$1, %esi
	shrq	%rdx
	andl	$1, %ecx
	movq	%rdx, 16(%rax)
	movl	%esi, 24(%rax)
	testl	%ecx, %ecx
	je	.L273
	testl	%esi, %esi
	jne	.L277
	movq	(%rax), %rcx
	cmpq	8(%rax), %rcx
	jnb	.L281
	leaq	1(%rcx), %rsi
	movq	%rsi, (%rax)
	movzbl	(%rcx), %ecx
	orq	%rcx, %rdx
.L282:
	movl	%edx, %ecx
	shrq	%rdx
	movl	$7, 24(%rax)
	movl	$7, %esi
	andl	$1, %ecx
	movq	%rdx, 16(%rax)
	testl	%ecx, %ecx
	je	.L279
	movl	$7, %ecx
	jmp	.L283
	.p2align 4,,10
	.p2align 3
.L281:
	movl	$1, 28(%rax)
	jmp	.L282
	.cfi_endproc
.LFE40:
	.size	kr_dec_table, .-kr_dec_table
	.p2align 4
	.type	kr_put_header, @function
kr_put_header:
.LFB52:
	.cfi_startproc
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	movq	%rsi, %rax
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	movq	%rdi, %r14
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	subq	$40, %rsp
	.cfi_def_cfa_offset 96
	movl	4(%rsi), %ecx
	movq	8(%rdi), %rbp
	movq	%fs:40, %rdx
	movq	%rdx, 24(%rsp)
	movl	8(%rsi), %edx
	leal	-8(,%rcx,8), %ecx
	sall	$6, %edx
	orl	(%rsi), %edx
	orl	%ecx, %edx
	leaq	8(%rbp), %rcx
	movq	%rcx, 8(%rdi)
	movq	24(%rdi), %rdi
	testq	%rdi, %rdi
	je	.L361
	movl	16(%r14), %ecx
	movq	%rdi, %rsi
	salq	%cl, %rdx
	addl	$8, %ecx
	orq	(%r14), %rdx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L362
.L364:
	cmpq	32(%r14), %rdi
	jb	.L653
	movq	%rdx, %r9
	leal	-8(%rcx), %r8d
	shrq	$8, %r9
	movl	%r8d, 16(%r14)
	movq	%r9, (%r14)
	cmpl	$7, %r8d
	jbe	.L365
	testb	$8, %cl
	jne	.L366
	shrq	$16, %rdx
	leal	-16(%rcx), %r8d
	movq	%rdx, %r9
	cmpl	$7, %r8d
	jbe	.L635
.L366:
	subl	$16, %r8d
	shrq	$16, %r9
	cmpl	$7, %r8d
	ja	.L366
.L635:
	movq	%r9, (%r14)
	movl	%r8d, 16(%r14)
	jmp	.L365
	.p2align 4,,10
	.p2align 3
.L653:
	leaq	1(%rdi), %rcx
	movq	%rcx, 24(%r14)
	movb	%dl, (%rdi)
	movq	(%r14), %rdx
	movl	16(%r14), %ebx
	movq	24(%r14), %rdi
	shrq	$8, %rdx
	leal	-8(%rbx), %ecx
	movq	%rdx, (%r14)
	movq	%rdi, %rsi
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L364
.L365:
	movl	16(%rax), %ebx
	movl	20(%rax), %ecx
	movl	12(%rax), %r8d
	leal	0(,%rbx,4), %edx
	sall	$4, %ecx
	movq	8(%r14), %rbx
	orl	%ecx, %edx
	movl	24(%rax), %ecx
	orl	%r8d, %edx
	leaq	8(%rbx), %rbp
	sall	$6, %ecx
	movq	%rbp, 8(%r14)
	orl	%ecx, %edx
	testq	%rdi, %rdi
	je	.L516
.L368:
	movl	16(%r14), %ecx
	salq	%cl, %rdx
	addl	$8, %ecx
	orq	(%r14), %rdx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L371
.L373:
	cmpq	32(%r14), %rdi
	jb	.L654
	movq	%rdx, %r9
	leal	-8(%rcx), %r8d
	shrq	$8, %r9
	movl	%r8d, 16(%r14)
	movq	%r9, (%r14)
	cmpl	$7, %r8d
	jbe	.L374
	testb	$8, %cl
	jne	.L375
	shrq	$16, %rdx
	leal	-16(%rcx), %r8d
	movq	%rdx, %r9
	cmpl	$7, %r8d
	jbe	.L634
.L375:
	subl	$16, %r8d
	shrq	$16, %r9
	cmpl	$7, %r8d
	ja	.L375
.L634:
	movl	12(%rax), %edx
	movq	8(%r14), %rbp
	movq	%r9, (%r14)
	movl	%r8d, 16(%r14)
	testl	%edx, %edx
	je	.L377
.L656:
	addq	$8, %rbp
	movl	28(%rax), %r9d
	movq	%rbp, 8(%r14)
	testq	%rdi, %rdi
	je	.L385
.L378:
	movl	16(%r14), %edx
	movl	%r9d, %r8d
	movl	%edx, %ecx
	addl	$8, %edx
	salq	%cl, %r8
	movq	(%r14), %rcx
	movl	%edx, 16(%r14)
	orq	%r8, %rcx
	movq	%rcx, (%r14)
	cmpl	$7, %edx
	jbe	.L385
.L381:
	cmpq	32(%r14), %rdi
	jb	.L655
	movq	%rcx, %r9
	leal	-8(%rdx), %r8d
	shrq	$8, %r9
	movl	%r8d, 16(%r14)
	movq	%r9, (%r14)
	cmpl	$7, %r8d
	jbe	.L382
	testb	$8, %dl
	jne	.L383
	shrq	$16, %rcx
	leal	-16(%rdx), %r8d
	movq	%rcx, %r9
	cmpl	$7, %r8d
	jbe	.L633
.L383:
	subl	$16, %r8d
	shrq	$16, %r9
	cmpl	$7, %r8d
	ja	.L383
.L633:
	movq	%r9, (%r14)
	movl	%r8d, 16(%r14)
	jmp	.L382
	.p2align 4,,10
	.p2align 3
.L654:
	leaq	1(%rdi), %rcx
	movq	%rcx, 24(%r14)
	movb	%dl, (%rdi)
	movq	(%r14), %rdx
	movl	16(%r14), %ebx
	movq	24(%r14), %rdi
	shrq	$8, %rdx
	leal	-8(%rbx), %ecx
	movq	%rdx, (%r14)
	movq	%rdi, %rsi
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L373
.L374:
	movl	12(%rax), %edx
	movq	8(%r14), %rbp
	testl	%edx, %edx
	jne	.L656
.L377:
	movl	2888(%rax), %r9d
	addq	$8, %rbp
	movq	%rbp, 8(%r14)
	leal	-1(%r9), %r10d
	testq	%rsi, %rsi
	je	.L401
.L396:
	movl	16(%r14), %ecx
	movl	%r10d, %r8d
	salq	%cl, %r8
	leal	8(%rcx), %edx
	orq	(%r14), %r8
	movq	%r8, (%r14)
	movl	%edx, 16(%r14)
	cmpl	$7, %edx
	ja	.L402
	jmp	.L401
	.p2align 4,,10
	.p2align 3
.L657:
	leaq	1(%rsi), %rdx
	movq	%rdx, 24(%r14)
	movb	%r8b, (%rsi)
	movq	(%r14), %r8
	movl	16(%r14), %ebx
	shrq	$8, %r8
	leal	-8(%rbx), %edx
	movq	%r8, (%r14)
	movl	%edx, 16(%r14)
	cmpl	$7, %edx
	jbe	.L400
	movq	24(%r14), %rdi
.L399:
	movq	%rdi, %rsi
.L402:
	cmpq	32(%r14), %rsi
	jb	.L657
	shrq	$8, %r8
	subl	$8, %edx
	movq	%r8, (%r14)
	movl	%edx, 16(%r14)
	cmpl	$7, %edx
	ja	.L399
.L400:
	movl	2888(%rax), %r9d
	movq	8(%r14), %rbp
	leal	-1(%r9), %r10d
.L401:
	testl	%r10d, %r10d
	je	.L403
	bsrl	%r10d, %r10d
	leal	1(%r10), %r11d
	movl	48(%rax), %r10d
	testl	%r10d, %r10d
	je	.L403
	movl	%r11d, %ebx
	xorl	%r9d, %r9d
	xorl	%r8d, %r8d
	.p2align 4,,10
	.p2align 3
.L412:
	movq	2904(%rax), %rdx
	movq	24(%r14), %rsi
	addq	%rbx, %rbp
	movzbl	(%rdx,%r9), %edx
	movq	%rbp, 8(%r14)
	testq	%rsi, %rsi
	je	.L404
	movl	16(%r14), %ecx
	salq	%cl, %rdx
	addl	%r11d, %ecx
	orq	(%r14), %rdx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L405
	.p2align 4,,10
	.p2align 3
.L409:
	cmpq	32(%r14), %rsi
	jnb	.L406
.L658:
	leaq	1(%rsi), %rcx
	movq	%rcx, 24(%r14)
	movb	%dl, (%rsi)
	movq	(%r14), %rdx
	movl	16(%r14), %esi
	shrq	$8, %rdx
	leal	-8(%rsi), %ecx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L408
	movq	24(%r14), %rsi
	cmpq	32(%r14), %rsi
	jb	.L658
.L406:
	shrq	$8, %rdx
	subl	$8, %ecx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L409
.L408:
	movl	48(%rax), %r10d
	movq	8(%r14), %rbp
.L405:
	addl	$1, %r8d
	cmpl	%r10d, %r8d
	jnb	.L410
	addq	$1, %r9
	jmp	.L412
.L404:
	addl	$1, %r8d
	cmpl	%r10d, %r8d
	jnb	.L410
	leaq	0(%rbp,%rbx), %rdx
.L411:
	addl	$1, %r8d
	movq	%rdx, %rbp
	leaq	(%rdx,%rbx), %rdx
	cmpl	%r10d, %r8d
	jb	.L411
	movq	%rbp, 8(%r14)
.L410:
	movl	2888(%rax), %r9d
.L403:
	testl	%r9d, %r9d
	je	.L413
	xorl	%r10d, %r10d
	leaq	.L452(%rip), %r15
	movq	%r10, %rdx
	.p2align 4,,10
	.p2align 3
.L508:
	movq	%rdx, %r9
	movq	2920(%rax), %rcx
	movl	52(%rax), %r13d
	salq	$9, %r9
	addq	2912(%rax), %r9
	cmpw	$0, (%r9)
	movzbl	(%rcx,%rdx), %r12d
	jne	.L520
	xorl	%esi, %esi
	.p2align 4,,10
	.p2align 3
.L415:
	leal	1(%rsi), %r11d
	cmpw	$0, (%r9,%r11,2)
	movq	%r11, %rsi
	je	.L415
.L414:
	cmpw	$0, 510(%r9)
	jne	.L521
	movl	$255, %r8d
	.p2align 4,,10
	.p2align 3
.L417:
	leal	-1(%r8), %edi
	cmpw	$0, (%r9,%rdi,2)
	movq	%rdi, %r8
	je	.L417
.L416:
	movq	24(%r14), %r10
	leaq	8(%rbp), %rcx
	movq	%rcx, 8(%r14)
	testq	%r10, %r10
	je	.L659
	movl	16(%r14), %ecx
	salq	%cl, %r11
	addl	$8, %ecx
	orq	(%r14), %r11
	movq	%r11, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L420
.L422:
	cmpq	32(%r14), %r10
	jb	.L660
	movq	%r11, %rbp
	leal	-8(%rcx), %ebx
	shrq	$8, %rbp
	movl	%ebx, 16(%r14)
	movq	%rbp, (%r14)
	cmpl	$7, %ebx
	jbe	.L423
	testb	$8, %cl
	jne	.L424
	shrq	$16, %r11
	leal	-16(%rcx), %ebx
	movq	%r11, %rbp
	cmpl	$7, %ebx
	jbe	.L632
	.p2align 4,,10
	.p2align 3
.L424:
	subl	$16, %ebx
	shrq	$16, %rbp
	cmpl	$7, %ebx
	ja	.L424
.L632:
	movq	%rbp, (%r14)
	movl	%ebx, 16(%r14)
	jmp	.L423
	.p2align 4,,10
	.p2align 3
.L660:
	leaq	1(%r10), %rcx
	movq	%rcx, 24(%r14)
	movb	%r11b, (%r10)
	movq	(%r14), %r11
	movl	16(%r14), %ebx
	movq	24(%r14), %r10
	shrq	$8, %r11
	leal	-8(%rbx), %ecx
	movq	%r11, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L422
.L423:
	movq	8(%r14), %rbx
	leaq	8(%rbx), %rbp
	movq	%rbp, 8(%r14)
	testq	%r10, %r10
	je	.L432
.L426:
	movl	16(%r14), %ecx
	salq	%cl, %rdi
	addl	$8, %ecx
	orq	(%r14), %rdi
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L432
	.p2align 4,,10
	.p2align 3
.L433:
	cmpq	32(%r14), %r10
	jnb	.L429
.L661:
	leaq	1(%r10), %rcx
	movq	%rcx, 24(%r14)
	movb	%dil, (%r10)
	movq	(%r14), %rdi
	movl	16(%r14), %ebx
	shrq	$8, %rdi
	leal	-8(%rbx), %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L431
	movq	24(%r14), %r10
	cmpq	32(%r14), %r10
	jb	.L661
.L429:
	shrq	$8, %rdi
	subl	$8, %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L433
.L431:
	movq	8(%r14), %rbp
.L432:
	xorl	%r10d, %r10d
	cmpl	%esi, %r8d
	jb	.L428
	movq	%rdx, (%rsp)
	movq	%rax, 8(%rsp)
.L427:
	movq	24(%r14), %rax
	cmpl	%esi, %r12d
	je	.L501
	movl	%esi, %edx
	xorl	%ebx, %ebx
	leaq	(%r9,%rdx,2), %r11
	movzwl	(%r11), %edx
	testl	%edx, %edx
	je	.L449
.L502:
	bsrl	%edx, %ecx
	movl	$32, %edx
	xorl	$31, %ecx
	subl	%ecx, %edx
	movl	%edx, %ebx
.L449:
	movl	%ebx, %ecx
	subl	%r10d, %ecx
	addl	$2, %ecx
	cmpl	$4, %ecx
	ja	.L450
	movslq	(%r15,%rcx,4), %rcx
	addq	%r15, %rcx
	notrack jmp	*%rcx
	.section	.rodata
	.align 4
	.align 4
.L452:
	.long	.L456-.L452
	.long	.L455-.L452
	.long	.L454-.L452
	.long	.L453-.L452
	.long	.L451-.L452
	.text
	.p2align 4,,10
	.p2align 3
.L453:
	addq	$3, %rbp
	movq	%rbp, 8(%r14)
	testq	%rax, %rax
	je	.L458
	movl	16(%r14), %ecx
	movl	$3, %edi
	salq	%cl, %rdi
	addl	$3, %ecx
	orq	(%r14), %rdi
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L458
	.p2align 4,,10
	.p2align 3
.L475:
	cmpq	32(%r14), %rax
	jnb	.L472
.L662:
	leaq	1(%rax), %rcx
	movq	%rcx, 24(%r14)
	movb	%dil, (%rax)
	movq	(%r14), %rdi
	movl	16(%r14), %eax
	shrq	$8, %rdi
	leal	-8(%rax), %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L498
	movq	24(%r14), %rax
	cmpq	32(%r14), %rax
	jb	.L662
.L472:
	shrq	$8, %rdi
	subl	$8, %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L475
	.p2align 4,,10
	.p2align 3
.L498:
	movq	8(%r14), %rbp
.L458:
	testl	%edx, %edx
	jne	.L500
	addl	$1, %esi
	cmpl	%esi, %r8d
	jb	.L644
	movq	24(%r14), %rax
	cmpl	%esi, %r12d
	je	.L524
	movl	%esi, %edx
	xorl	%r10d, %r10d
	xorl	%ebx, %ebx
	leaq	(%r9,%rdx,2), %r11
	movzwl	(%r11), %edx
	testl	%edx, %edx
	jne	.L502
.L454:
	addq	$1, %rbp
	movq	%rbp, 8(%r14)
	testq	%rax, %rax
	je	.L458
	movl	16(%r14), %edi
	leal	1(%rdi), %ecx
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L458
	movq	(%r14), %rdi
	.p2align 4,,10
	.p2align 3
.L463:
	cmpq	32(%r14), %rax
	jnb	.L460
.L663:
	leaq	1(%rax), %rcx
	movq	%rcx, 24(%r14)
	movb	%dil, (%rax)
	movq	(%r14), %rdi
	movl	16(%r14), %eax
	shrq	$8, %rdi
	leal	-8(%rax), %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L498
	movq	24(%r14), %rax
	cmpq	32(%r14), %rax
	jb	.L663
.L460:
	shrq	$8, %rdi
	subl	$8, %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L463
	movq	8(%r14), %rbp
	jmp	.L458
	.p2align 4,,10
	.p2align 3
.L455:
	addq	$2, %rbp
	movq	%rbp, 8(%r14)
	testq	%rax, %rax
	je	.L458
	movl	16(%r14), %ecx
	movl	$1, %edi
	salq	%cl, %rdi
	addl	$2, %ecx
	orq	(%r14), %rdi
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L458
	.p2align 4,,10
	.p2align 3
.L469:
	cmpq	32(%r14), %rax
	jnb	.L466
.L664:
	leaq	1(%rax), %rcx
	movq	%rcx, 24(%r14)
	movb	%dil, (%rax)
	movq	(%r14), %rdi
	movl	16(%r14), %eax
	shrq	$8, %rdi
	leal	-8(%rax), %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L498
	movq	24(%r14), %rax
	cmpq	32(%r14), %rax
	jb	.L664
.L466:
	shrq	$8, %rdi
	subl	$8, %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L469
	movq	8(%r14), %rbp
	jmp	.L458
	.p2align 4,,10
	.p2align 3
.L456:
	addq	$4, %rbp
	movq	%rbp, 8(%r14)
	testq	%rax, %rax
	je	.L458
	movl	16(%r14), %ecx
	movl	$7, %edi
	salq	%cl, %rdi
	addl	$4, %ecx
	orq	(%r14), %rdi
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L458
	.p2align 4,,10
	.p2align 3
.L481:
	cmpq	32(%r14), %rax
	jnb	.L478
.L665:
	leaq	1(%rax), %rcx
	movq	%rcx, 24(%r14)
	movb	%dil, (%rax)
	movq	(%r14), %rdi
	movl	16(%r14), %eax
	shrq	$8, %rdi
	leal	-8(%rax), %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L498
	movq	24(%r14), %rax
	cmpq	32(%r14), %rax
	jb	.L665
.L478:
	shrq	$8, %rdi
	subl	$8, %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L481
	movq	8(%r14), %rbp
	jmp	.L458
	.p2align 4,,10
	.p2align 3
.L451:
	addq	$5, %rbp
	movq	%rbp, 8(%r14)
	testq	%rax, %rax
	je	.L458
	movl	16(%r14), %ecx
	movl	$15, %edi
	salq	%cl, %rdi
	addl	$5, %ecx
	orq	(%r14), %rdi
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L458
	.p2align 4,,10
	.p2align 3
.L487:
	cmpq	32(%r14), %rax
	jnb	.L484
.L666:
	leaq	1(%rax), %rcx
	movq	%rcx, 24(%r14)
	movb	%dil, (%rax)
	movq	(%r14), %rdi
	movl	16(%r14), %eax
	shrq	$8, %rdi
	leal	-8(%rax), %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L498
	movq	24(%r14), %rax
	cmpq	32(%r14), %rax
	jb	.L666
.L484:
	shrq	$8, %rdi
	subl	$8, %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L487
	movq	8(%r14), %rbp
	jmp	.L458
	.p2align 4,,10
	.p2align 3
.L500:
	leal	-1(%rdx), %ecx
	movzwl	(%r11), %eax
	movl	$-1, %edx
	cmpl	%ecx, %r13d
	movl	%ecx, %edi
	cmovbe	%r13d, %edi
	subl	%edi, %ecx
	shrl	%cl, %eax
	movl	%edi, %ecx
	sall	%cl, %edx
	notl	%edx
	andl	%eax, %edx
	movl	%edi, %eax
	addq	%rax, %rbp
	movq	24(%r14), %rax
	movq	%rbp, 8(%r14)
	testq	%rax, %rax
	je	.L503
	movl	16(%r14), %ecx
	salq	%cl, %rdx
	addl	%ecx, %edi
	orq	(%r14), %rdx
	movq	%rdx, (%r14)
	movl	%edi, 16(%r14)
	cmpl	$7, %edi
	jbe	.L503
	.p2align 4,,10
	.p2align 3
.L507:
	cmpq	32(%r14), %rax
	jnb	.L504
.L667:
	leaq	1(%rax), %rcx
	movq	%rcx, 24(%r14)
	movb	%dl, (%rax)
	movq	(%r14), %rdx
	movl	16(%r14), %eax
	shrq	$8, %rdx
	leal	-8(%rax), %edi
	movq	%rdx, (%r14)
	movl	%edi, 16(%r14)
	cmpl	$7, %edi
	jbe	.L506
	movq	24(%r14), %rax
	cmpq	32(%r14), %rax
	jb	.L667
.L504:
	shrq	$8, %rdx
	subl	$8, %edi
	movq	%rdx, (%r14)
	movl	%edi, 16(%r14)
	cmpl	$7, %edi
	ja	.L507
.L506:
	movq	8(%r14), %rbp
	movl	%ebx, %r10d
.L444:
	addl	$1, %esi
	cmpl	%esi, %r8d
	jnb	.L427
.L644:
	movq	(%rsp), %rdx
	movq	8(%rsp), %rax
.L428:
	addq	$1, %rdx
	cmpl	2888(%rax), %edx
	jb	.L508
.L413:
	movl	%ebp, %eax
	negl	%eax
	andl	$7, %eax
	jne	.L668
.L360:
	movq	24(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L669
	addq	$40, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L450:
	.cfi_restore_state
	leaq	5(%rbp), %rcx
	movq	%rcx, 8(%r14)
	testq	%rax, %rax
	je	.L670
	movl	16(%r14), %ecx
	movl	$31, %edi
	salq	%cl, %rdi
	addl	$5, %ecx
	orq	(%r14), %rdi
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L490
.L492:
	cmpq	32(%r14), %rax
	jb	.L671
	movq	%rdi, %rbp
	leal	-8(%rcx), %r10d
	shrq	$8, %rbp
	movl	%r10d, 16(%r14)
	movq	%rbp, (%r14)
	cmpl	$7, %r10d
	jbe	.L493
	testb	$8, %cl
	jne	.L494
	shrq	$16, %rdi
	leal	-16(%rcx), %r10d
	movq	%rdi, %rbp
	cmpl	$7, %r10d
	jbe	.L631
	.p2align 4,,10
	.p2align 3
.L494:
	subl	$16, %r10d
	shrq	$16, %rbp
	cmpl	$7, %r10d
	ja	.L494
.L631:
	movq	%rbp, (%r14)
	movl	%r10d, 16(%r14)
	jmp	.L493
	.p2align 4,,10
	.p2align 3
.L671:
	leaq	1(%rax), %rcx
	movq	%rcx, 24(%r14)
	movb	%dil, (%rax)
	movq	(%r14), %rdi
	movl	16(%r14), %ecx
	movq	24(%r14), %rax
	shrq	$8, %rdi
	subl	$8, %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L492
.L493:
	movq	8(%r14), %rdi
	leaq	4(%rdi), %rbp
	movq	%rbp, 8(%r14)
	testq	%rax, %rax
	je	.L458
.L513:
	movl	16(%r14), %ecx
	movl	%edx, %edi
	salq	%cl, %rdi
	addl	$4, %ecx
	orq	(%r14), %rdi
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L458
	.p2align 4,,10
	.p2align 3
.L499:
	cmpq	32(%r14), %rax
	jnb	.L496
.L672:
	leaq	1(%rax), %rcx
	movq	%rcx, 24(%r14)
	movb	%dil, (%rax)
	movq	(%r14), %rdi
	movl	16(%r14), %eax
	shrq	$8, %rdi
	leal	-8(%rax), %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L498
	movq	24(%r14), %rax
	cmpq	32(%r14), %rax
	jb	.L672
.L496:
	shrq	$8, %rdi
	subl	$8, %ecx
	movq	%rdi, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L499
	movq	8(%r14), %rbp
	jmp	.L458
	.p2align 4,,10
	.p2align 3
.L524:
	xorl	%r10d, %r10d
.L501:
	leaq	5(%rbp), %rdx
	movq	%rdx, 8(%r14)
	testq	%rax, %rax
	je	.L673
	movl	16(%r14), %ecx
	movl	$31, %edx
	salq	%cl, %rdx
	addl	$5, %ecx
	orq	(%r14), %rdx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L437
.L439:
	cmpq	32(%r14), %rax
	jb	.L674
	movq	%rdx, %r11
	leal	-8(%rcx), %edi
	shrq	$8, %r11
	movl	%edi, 16(%r14)
	movq	%r11, (%r14)
	cmpl	$7, %edi
	jbe	.L440
	testb	$8, %cl
	jne	.L441
	shrq	$16, %rdx
	leal	-16(%rcx), %edi
	movq	%rdx, %r11
	cmpl	$7, %edi
	jbe	.L630
	.p2align 4,,10
	.p2align 3
.L441:
	subl	$16, %edi
	shrq	$16, %r11
	cmpl	$7, %edi
	ja	.L441
.L630:
	movq	%r11, (%r14)
	movl	%edi, 16(%r14)
	jmp	.L440
	.p2align 4,,10
	.p2align 3
.L674:
	leaq	1(%rax), %rcx
	movq	%rcx, 24(%r14)
	movb	%dl, (%rax)
	movq	(%r14), %rdx
	movl	16(%r14), %ebx
	movq	24(%r14), %rax
	shrq	$8, %rdx
	leal	-8(%rbx), %ecx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L439
.L440:
	movq	8(%r14), %rbx
	leaq	4(%rbx), %rbp
	movq	%rbp, 8(%r14)
	testq	%rax, %rax
	je	.L444
.L443:
	movl	16(%r14), %ecx
	movl	$14, %edx
	salq	%cl, %rdx
	addl	$4, %ecx
	orq	(%r14), %rdx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L444
	.p2align 4,,10
	.p2align 3
.L448:
	cmpq	32(%r14), %rax
	jnb	.L445
.L675:
	leaq	1(%rax), %rcx
	movq	%rcx, 24(%r14)
	movb	%dl, (%rax)
	movq	(%r14), %rdx
	movl	16(%r14), %eax
	shrq	$8, %rdx
	leal	-8(%rax), %ecx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L447
	movq	24(%r14), %rax
	cmpq	32(%r14), %rax
	jb	.L675
.L445:
	shrq	$8, %rdx
	subl	$8, %ecx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L448
.L447:
	movq	8(%r14), %rbp
	jmp	.L444
	.p2align 4,,10
	.p2align 3
.L503:
	movl	%ebx, %r10d
	jmp	.L444
	.p2align 4,,10
	.p2align 3
.L670:
	addq	$9, %rbp
	movq	%rbp, 8(%r14)
	jmp	.L458
	.p2align 4,,10
	.p2align 3
.L659:
	addq	$16, %rbp
	movq	%rbp, 8(%r14)
	jmp	.L432
.L673:
	addq	$9, %rbp
	movq	%rbp, 8(%r14)
	jmp	.L444
.L521:
	movl	$255, %edi
	movl	$255, %r8d
	jmp	.L416
.L520:
	xorl	%r11d, %r11d
	xorl	%esi, %esi
	jmp	.L414
.L668:
	movl	%eax, %edx
	movq	24(%r14), %rdi
	addq	%rbp, %rdx
	movq	%rdx, 8(%r14)
	testq	%rdi, %rdi
	je	.L360
	addl	16(%r14), %eax
	movl	%eax, 16(%r14)
	cmpl	$7, %eax
	jbe	.L360
	movq	(%r14), %rdx
.L512:
	cmpq	32(%r14), %rdi
	jnb	.L510
.L676:
	leaq	1(%rdi), %rax
	movq	%rax, 24(%r14)
	movb	%dl, (%rdi)
	movq	(%r14), %rdx
	movl	16(%r14), %eax
	shrq	$8, %rdx
	subl	$8, %eax
	movq	%rdx, (%r14)
	movl	%eax, 16(%r14)
	cmpl	$7, %eax
	jbe	.L360
	movq	24(%r14), %rdi
	cmpq	32(%r14), %rdi
	jb	.L676
.L510:
	shrq	$8, %rdx
	subl	$8, %eax
	movq	%rdx, (%r14)
	movl	%eax, 16(%r14)
	cmpl	$7, %eax
	ja	.L512
	jmp	.L360
.L655:
	leaq	1(%rdi), %rdx
	movq	%rdx, 24(%r14)
	movb	%cl, (%rdi)
	movq	(%r14), %rcx
	movl	16(%r14), %ebx
	movq	24(%r14), %rdi
	shrq	$8, %rcx
	leal	-8(%rbx), %edx
	movq	%rcx, (%r14)
	movq	%rdi, %rsi
	movl	%edx, 16(%r14)
	cmpl	$7, %edx
	ja	.L381
.L382:
	movl	28(%rax), %r9d
	movq	8(%r14), %rbp
.L385:
	xorl	%ebx, %ebx
	leaq	16(%rsp), %r11
	leaq	15(%rsp), %r10
	testl	%r9d, %r9d
	je	.L377
	movl	32(%rax,%rbx,4), %edx
	movl	%edx, %ecx
	cmpl	$127, %edx
	jbe	.L386
.L679:
	xorl	%r8d, %r8d
	.p2align 4,,10
	.p2align 3
.L387:
	orl	$-128, %ecx
	leaq	1(%r8), %r9
	shrq	$7, %rdx
	movb	%cl, (%r10,%r9)
	movl	%edx, %ecx
	cmpq	$127, %rdx
	jbe	.L677
	movq	%r9, %r8
	jmp	.L387
	.p2align 4,,10
	.p2align 3
.L677:
	movb	%dl, 16(%rsp,%r9)
	addq	$2, %r8
.L389:
	movq	%r11, %r9
	addq	%r11, %r8
.L395:
	addq	$8, %rbp
	movzbl	(%r9), %edx
	movq	%rbp, 8(%r14)
	testq	%rsi, %rsi
	je	.L390
	movl	16(%r14), %ecx
	salq	%cl, %rdx
	addl	$8, %ecx
	orq	(%r14), %rdx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L393
	jmp	.L390
	.p2align 4,,10
	.p2align 3
.L678:
	leaq	1(%rsi), %rcx
	movq	%rcx, 24(%r14)
	movb	%dl, (%rsi)
	movq	(%r14), %rdx
	movl	16(%r14), %esi
	movq	24(%r14), %rdi
	shrq	$8, %rdx
	leal	-8(%rsi), %ecx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	jbe	.L390
.L392:
	movq	%rdi, %rsi
.L393:
	cmpq	32(%r14), %rsi
	jb	.L678
	shrq	$8, %rdx
	subl	$8, %ecx
	movq	%rdx, (%r14)
	movl	%ecx, 16(%r14)
	cmpl	$7, %ecx
	ja	.L392
.L390:
	addq	$1, %r9
	movq	8(%r14), %rbp
	movq	%rdi, %rsi
	cmpq	%r8, %r9
	jne	.L395
	addq	$1, %rbx
	cmpl	28(%rax), %ebx
	jnb	.L377
	movl	32(%rax,%rbx,4), %edx
	movl	%edx, %ecx
	cmpl	$127, %edx
	ja	.L679
.L386:
	movb	%dl, 16(%rsp)
	movl	$1, %r8d
	jmp	.L389
.L361:
	movl	12(%rsi), %r8d
	addq	$16, %rbp
.L516:
	testl	%r8d, %r8d
	jne	.L680
	movl	2888(%rax), %r9d
	addq	$8, %rbp
	movq	%rbp, 8(%r14)
	leal	-1(%r9), %r10d
	jmp	.L401
.L680:
	addq	$8, %rbp
	movl	28(%rax), %r9d
	xorl	%esi, %esi
	movq	%rbp, 8(%r14)
	jmp	.L385
.L437:
	addq	$9, %rbp
	movq	%rbp, 8(%r14)
	jmp	.L443
.L669:
	call	*__stack_chk_fail@GOTPCREL(%rip)
.L371:
	testl	%r8d, %r8d
	jne	.L681
	movl	2888(%rax), %r9d
	addq	$8, %rbp
	movq	%rbp, 8(%r14)
	leal	-1(%r9), %r10d
	jmp	.L396
.L681:
	addq	$8, %rbp
	movl	28(%rax), %r9d
	movq	%rbp, 8(%r14)
	jmp	.L378
.L490:
	addq	$9, %rbp
	movq	%rbp, 8(%r14)
	jmp	.L513
.L362:
	movl	20(%rax), %edx
	movl	16(%rax), %ebx
	addq	$16, %rbp
	movl	12(%rax), %r8d
	leal	0(,%rbx,4), %ecx
	sall	$4, %edx
	orl	%ecx, %edx
	movl	24(%rax), %ecx
	movq	%rbp, 8(%r14)
	orl	%r8d, %edx
	sall	$6, %ecx
	orl	%ecx, %edx
	jmp	.L368
.L420:
	addq	$16, %rbp
	movq	%rbp, 8(%r14)
	jmp	.L426
	.cfi_endproc
.LFE52:
	.size	kr_put_header, .-kr_put_header
	.p2align 4
	.type	kr_eval, @function
kr_eval:
.LFB53:
	.cfi_startproc
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	movq	%rdi, %r15
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	movq	%rdx, %r13
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	subq	$376, %rsp
	.cfi_def_cfa_offset 432
	movl	2888(%rsi), %ebx
	movl	48(%rsi), %r12d
	movq	%rsi, 16(%rsp)
	movq	5952(%rdi), %rdi
	xorl	%esi, %esi
	movq	%rbx, %rdx
	movq	%rbx, %rbp
	salq	$10, %rdx
	movq	%fs:40, %rax
	movq	%rax, 360(%rsp)
	xorl	%eax, %eax
	call	*memset@GOTPCREL(%rip)
	xorl	%esi, %esi
	leaq	0(,%rbx,8), %rdx
	leaq	5960(%r15), %rdi
	call	*memset@GOTPCREL(%rip)
	testl	%r12d, %r12d
	je	.L693
	movq	16(%rsp), %rax
	movl	%r12d, %r9d
	movq	%r13, %rsi
	pxor	%xmm2, %xmm2
	movq	5952(%r15), %r8
	movq	2904(%rax), %rdi
	leaq	1024(%r8), %r10
	addq	%rdi, %r9
.L692:
	movzbl	(%rdi), %r13d
	salq	$10, %r13
	leaq	16(%r8,%r13), %rdx
	leaq	(%r8,%r13), %rax
	cmpq	%rdx, %rsi
	jnb	.L767
	leaq	16(%rsi), %rdx
	cmpq	%rdx, %rax
	jb	.L686
.L767:
	xorl	%edx, %edx
	pxor	%xmm1, %xmm1
	.p2align 4,,10
	.p2align 3
.L688:
	movdqu	(%rax,%rdx), %xmm0
	movdqu	(%rsi,%rdx), %xmm5
	paddd	%xmm5, %xmm0
	movups	%xmm0, (%rax,%rdx)
	movdqu	(%rsi,%rdx), %xmm0
	addq	$16, %rdx
	movdqa	%xmm0, %xmm3
	punpckldq	%xmm2, %xmm0
	punpckhdq	%xmm2, %xmm3
	paddq	%xmm3, %xmm0
	paddq	%xmm0, %xmm1
	cmpq	$1024, %rdx
	jne	.L688
	movdqa	%xmm1, %xmm0
	psrldq	$8, %xmm0
	paddq	%xmm0, %xmm1
	movq	%xmm1, %rcx
.L691:
	movzbl	(%rdi), %eax
	addq	$1, %rdi
	addq	$1024, %rsi
	addq	%rcx, 5960(%r15,%rax,8)
	cmpq	%r9, %rdi
	jne	.L692
.L693:
	xorl	%eax, %eax
	xorl	%edx, %edx
	leaq	96(%rsp), %r8
	testl	%ebp, %ebp
	jne	.L684
	jmp	.L685
	.p2align 4,,10
	.p2align 3
.L695:
	cmpl	%eax, %edx
	je	.L697
	movl	%edx, %r9d
	movq	5952(%r15), %rsi
	movq	%rax, %rdi
	movq	%r9, %rcx
	salq	$10, %rdi
	salq	$10, %rcx
	addq	%rsi, %rcx
	addq	%rdi, %rsi
	movq	(%rsi), %rdi
	movq	%rdi, (%rcx)
	movq	1016(%rsi), %rdi
	movq	%rdi, 1016(%rcx)
	leaq	8(%rcx), %rdi
	andq	$-8, %rdi
	subq	%rdi, %rcx
	subq	%rcx, %rsi
	addl	$1024, %ecx
	shrl	$3, %ecx
	rep movsq
	movq	5960(%r15,%rax,8), %rcx
	movq	%rcx, 5960(%r15,%r9,8)
.L697:
	addq	$1, %rax
	addl	$1, %edx
	cmpq	%rax, %rbx
	je	.L807
.L684:
	cmpq	$0, 5960(%r15,%rax,8)
	movb	%dl, (%r8,%rax)
	jne	.L695
	movb	$0, (%r8,%rax)
	addq	$1, %rax
	cmpq	%rax, %rbx
	jne	.L684
.L807:
	cmpl	%ebp, %edx
	je	.L698
	movq	16(%rsp), %r8
	movl	%r12d, %edi
	xorl	%eax, %eax
	testl	%r12d, %r12d
	je	.L701
.L700:
	movq	2904(%r8), %rcx
	addq	%rax, %rcx
	addq	$1, %rax
	movzbl	(%rcx), %esi
	movzbl	96(%rsp,%rsi), %esi
	movb	%sil, (%rcx)
	cmpq	%rax, %rdi
	jne	.L700
.L701:
	movq	16(%rsp), %rax
	movl	%edx, 2888(%rax)
	testl	%edx, %edx
	je	.L685
.L698:
	movl	%edx, %eax
	movq	%r15, 32(%rsp)
	xorl	%edx, %edx
	xorl	%r14d, %r14d
	movq	%rax, 24(%rsp)
	pxor	%xmm3, %xmm3
	movq	%rdx, %r12
.L748:
	movq	16(%rsp), %rdi
	movq	%r14, %rbp
	movq	32(%rsp), %rax
	xorl	%r10d, %r10d
	salq	$9, %rbp
	addq	2912(%rdi), %rbp
	movq	2920(%rdi), %r8
	movl	52(%rdi), %r9d
	leaq	8(%rbp), %rdi
	movq	%rbp, %rcx
	movq	5952(%rax), %rbx
	andq	$-8, %rdi
	movq	8024(%rax), %rsi
	movq	5960(%rax,%r14,8), %rdx
	xorl	%eax, %eax
	subq	%rdi, %rcx
	movq	$0, 0(%rbp)
	addq	%r12, %rbx
	addq	%r14, %r8
	addl	$512, %ecx
	movq	$0, 504(%rbp)
	shrl	$3, %ecx
	rep stosq
	xorl	%edi, %edi
	.p2align 4,,10
	.p2align 3
.L703:
	movl	(%rbx,%rax,4), %r11d
	movl	%edi, %ecx
	testl	%r11d, %r11d
	je	.L702
	addl	$1, %r10d
	cmpl	%r11d, (%rbx,%rcx,4)
	cmovb	%eax, %edi
	cmovb	%rax, %rcx
.L702:
	addq	$1, %rax
	cmpq	$256, %rax
	jne	.L703
	movb	%dil, (%r8)
	leaq	0(%rbp,%rcx,2), %r11
	movl	$4096, %eax
	cmpl	$1, %r10d
	jbe	.L704
	testq	%rdx, %rdx
	js	.L705
	pxor	%xmm0, %xmm0
	cvtsi2sdq	%rdx, %xmm0
.L706:
	movsd	.LC65(%rip), %xmm5
	movl	(%rbx,%rcx,4), %eax
	pxor	%xmm1, %xmm1
	movapd	%xmm5, %xmm2
	cvtsi2sdq	%rax, %xmm1
	divsd	%xmm0, %xmm2
	mulsd	%xmm2, %xmm1
	cmpl	$-1, %r9d
	jne	.L758
	movsd	.LC63(%rip), %xmm4
	xorl	%ecx, %ecx
	movsd	.LC66(%rip), %xmm9
	movapd	%xmm4, %xmm7
.L726:
	xorl	%eax, %eax
	xorl	%r8d, %r8d
	jmp	.L720
	.p2align 4,,10
	.p2align 3
.L800:
	comisd	%xmm9, %xmm0
	movl	$4095, %edx
	jb	.L808
.L715:
	movw	%dx, 0(%rbp,%rax,2)
	movzwl	%dx, %edx
	addl	%edx, %r8d
.L710:
	addq	$1, %rax
	cmpq	$256, %rax
	je	.L809
.L720:
	movl	(%rbx,%rax,4), %edx
	testl	%edx, %edx
	je	.L710
	cmpl	%eax, %edi
	je	.L710
	pxor	%xmm0, %xmm0
	cvtsi2sdq	%rdx, %xmm0
	mulsd	%xmm2, %xmm0
	mulsd	%xmm7, %xmm0
	comisd	%xmm0, %xmm4
	jbe	.L800
	movl	$1, %r9d
	movl	$2, %edx
	movapd	%xmm4, %xmm0
.L713:
	movl	%r9d, %r10d
	pxor	%xmm8, %xmm8
	pxor	%xmm6, %xmm6
	cvtsi2sdq	%r10, %xmm8
	movapd	%xmm0, %xmm10
	cvtsi2sdl	%edx, %xmm6
	subsd	%xmm8, %xmm10
.L719:
	subsd	%xmm0, %xmm6
	comisd	%xmm6, %xmm10
	cmovbe	%r9d, %edx
	jmp	.L715
.L686:
	addq	%r10, %r13
	movq	%rsi, %rdx
	xorl	%ecx, %ecx
	.p2align 4,,10
	.p2align 3
.L690:
	movl	(%rdx), %r11d
	addl	%r11d, (%rax)
	addq	$4, %rax
	addq	$4, %rdx
	movl	-4(%rdx), %r11d
	addq	%r11, %rcx
	cmpq	%r13, %rax
	jne	.L690
	jmp	.L691
	.p2align 4,,10
	.p2align 3
.L808:
	cvttsd2siq	%xmm0, %r10
	movl	%r10d, %r9d
	testl	%r10d, %r10d
	jne	.L810
	movapd	%xmm0, %xmm10
	movapd	%xmm4, %xmm6
	movl	$1, %edx
	jmp	.L719
.L810:
	leal	1(%r10), %edx
	cmpl	$4095, %edx
	jbe	.L713
	movl	%r10d, %edx
	jmp	.L715
	.p2align 4,,10
	.p2align 3
.L809:
	cmpl	$3, %ecx
	jg	.L721
	movapd	%xmm1, %xmm0
	pxor	%xmm6, %xmm6
	mulsd	%xmm7, %xmm0
	cvtsi2sdq	%r8, %xmm6
	addsd	%xmm6, %xmm0
	movapd	%xmm5, %xmm6
	divsd	%xmm0, %xmm6
	mulsd	%xmm6, %xmm7
.L724:
	addl	$1, %ecx
	jmp	.L726
.L721:
	cmpl	$4095, %r8d
	ja	.L811
.L725:
	movl	$4096, %eax
	subl	%r8d, %eax
.L704:
	movw	%ax, (%r11)
	movss	.LC64(%rip), %xmm2
	xorl	%r13d, %r13d
	leaq	(%rsi,%r12), %r15
	jmp	.L747
	.p2align 4,,10
	.p2align 3
.L743:
	movl	(%rbx,%r13,4), %eax
	movss	%xmm0, (%r15,%r13,4)
	testl	%eax, %eax
	je	.L744
	pxor	%xmm1, %xmm1
	cvtss2sd	%xmm0, %xmm0
	cvtsi2sdq	%rax, %xmm1
	mulsd	%xmm1, %xmm0
	addsd	%xmm0, %xmm3
.L744:
	addq	$1, %r13
	cmpq	$256, %r13
	je	.L812
.L747:
	movzwl	0(%rbp,%r13,2), %eax
	movaps	%xmm2, %xmm0
	testw	%ax, %ax
	je	.L743
	pxor	%xmm0, %xmm0
	movsd	%xmm3, 8(%rsp)
	cvtsi2sdl	%eax, %xmm0
	call	*log2@GOTPCREL(%rip)
	movss	.LC64(%rip), %xmm2
	movsd	8(%rsp), %xmm3
	movapd	%xmm0, %xmm1
	movsd	.LC68(%rip), %xmm0
	subsd	%xmm1, %xmm0
	cvtsd2ss	%xmm0, %xmm0
	jmp	.L743
.L812:
	addq	$1, %r14
	addq	$1024, %r12
	cmpq	%r14, 24(%rsp)
	jne	.L748
.L694:
	movq	16(%rsp), %rbx
	pxor	%xmm0, %xmm0
	leaq	48(%rsp), %rdi
	movsd	%xmm3, 8(%rsp)
	movq	$0, 80(%rsp)
	movq	%rbx, %rsi
	movaps	%xmm0, 48(%rsp)
	movaps	%xmm0, 64(%rsp)
	call	kr_put_header
	movl	(%rbx), %ecx
	movl	$1, %eax
	pxor	%xmm1, %xmm1
	movsd	8(%rsp), %xmm3
	sall	%cl, %eax
	cvtsi2sdq	%rax, %xmm1
	movq	56(%rsp), %rax
	mulsd	.LC69(%rip), %xmm1
	testq	%rax, %rax
	js	.L751
	pxor	%xmm0, %xmm0
	cvtsi2sdq	%rax, %xmm0
.L752:
	addsd	%xmm3, %xmm0
	addsd	%xmm1, %xmm0
	movq	360(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L813
	addq	$376, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L758:
	.cfi_restore_state
	movq	%rsi, 40(%rsp)
	movl	%r9d, %edx
	xorl	%r13d, %r13d
	movsd	.LC63(%rip), %xmm4
	movsd	.LC66(%rip), %xmm7
	movl	$1, %r10d
	notl	%edx
.L709:
	movq	%r11, 8(%rsp)
	xorl	%eax, %eax
	xorl	%r8d, %r8d
	jmp	.L738
	.p2align 4,,10
	.p2align 3
.L816:
	comisd	%xmm7, %xmm0
	jb	.L814
	movl	$12, %ecx
	movl	$4095, %esi
	cmpl	$10, %r9d
	ja	.L735
.L754:
	subl	%r9d, %ecx
	subl	$1, %ecx
	shrl	%cl, %esi
	sall	%cl, %esi
.L753:
	movl	%r10d, %r15d
	sall	%cl, %r15d
	movl	%r15d, %ecx
.L733:
	addl	%esi, %ecx
	cmpl	$4095, %ecx
	jbe	.L730
.L735:
	movw	%si, 0(%rbp,%rax,2)
	movzwl	%si, %esi
	addl	%esi, %r8d
.L727:
	addq	$1, %rax
	cmpq	$256, %rax
	je	.L815
.L738:
	movl	(%rbx,%rax,4), %ecx
	testl	%ecx, %ecx
	je	.L727
	cmpl	%eax, %edi
	je	.L727
	pxor	%xmm0, %xmm0
	movsd	.LC63(%rip), %xmm6
	cvtsi2sdq	%rcx, %xmm0
	mulsd	%xmm2, %xmm0
	mulsd	%xmm4, %xmm0
	comisd	%xmm0, %xmm6
	jbe	.L816
	movapd	%xmm6, %xmm0
	movl	$1, %esi
	movl	$2, %ecx
.L730:
	pxor	%xmm6, %xmm6
	movl	%esi, %r15d
	cvtsi2sdl	%ecx, %xmm6
	subsd	%xmm0, %xmm6
	movapd	%xmm6, %xmm11
	pxor	%xmm6, %xmm6
	cvtsi2sdq	%r15, %xmm6
	subsd	%xmm6, %xmm0
	ucomisd	%xmm11, %xmm0
	cmova	%ecx, %esi
	jmp	.L735
	.p2align 4,,10
	.p2align 3
.L814:
	cvttsd2siq	%xmm0, %rcx
	movl	%ecx, %esi
	testl	%ecx, %ecx
	jne	.L817
	movl	%edx, %ecx
	jmp	.L753
.L815:
	movq	8(%rsp), %r11
	cmpl	$3, %r13d
	jle	.L739
	cmpl	$4095, %r8d
	ja	.L818
	movq	40(%rsp), %rsi
	jmp	.L725
.L739:
	movapd	%xmm4, %xmm0
	pxor	%xmm6, %xmm6
	mulsd	%xmm1, %xmm0
	cvtsi2sdq	%r8, %xmm6
	addsd	%xmm6, %xmm0
	movapd	%xmm5, %xmm6
	divsd	%xmm0, %xmm6
	mulsd	%xmm6, %xmm4
.L740:
	addl	$1, %r13d
	jmp	.L709
.L705:
	movq	%rdx, %rax
	andl	$1, %edx
	pxor	%xmm0, %xmm0
	shrq	%rax
	orq	%rdx, %rax
	cvtsi2sdq	%rax, %xmm0
	addsd	%xmm0, %xmm0
	jmp	.L706
.L818:
	mulsd	.LC67(%rip), %xmm4
	jmp	.L740
.L811:
	mulsd	.LC67(%rip), %xmm7
	jmp	.L724
.L751:
	movq	%rax, %rdx
	andl	$1, %eax
	pxor	%xmm0, %xmm0
	shrq	%rdx
	orq	%rax, %rdx
	cvtsi2sdq	%rdx, %xmm0
	addsd	%xmm0, %xmm0
	jmp	.L752
.L685:
	pxor	%xmm3, %xmm3
	jmp	.L694
.L813:
	call	*__stack_chk_fail@GOTPCREL(%rip)
.L817:
	bsrl	%ecx, %ecx
	movl	$31, %r15d
	xorl	$31, %ecx
	movl	%ecx, %r11d
	movl	$32, %ecx
	subl	%r11d, %r15d
	subl	%r11d, %ecx
	cmpl	%r15d, %r9d
	jb	.L754
	movl	$1, %ecx
	jmp	.L733
	.cfi_endproc
.LFE53:
	.size	kr_eval, .-kr_eval
	.p2align 4
	.type	kr_plan_build, @function
kr_plan_build:
.LFB58:
	.cfi_startproc
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	movslq	%r8d, %r15
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	movq	%rdx, %r14
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	movq	%rcx, %r12
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	movq	%rdi, %rbp
	movq	%rsi, %rdi
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	movq	%rsi, %rbx
	subq	$136, %rsp
	.cfi_def_cfa_offset 192
	movq	%fs:40, %rax
	movq	%rax, 120(%rsp)
	xorl	%eax, %eax
	call	kr_ctx_init
	testl	%eax, %eax
	jne	.L820
	movl	48(%rbx), %eax
	cmpl	$1, %eax
	jbe	.L821
	movq	5936(%rbp), %rdi
	cmpq	%rax, 5944(%rbp)
	jb	.L912
	salq	$10, %rax
	movq	%rax, %r13
.L824:
	movq	%r13, %rdx
	xorl	%esi, %esi
	movq	%r12, %r13
	call	*memset@GOTPCREL(%rip)
	movl	(%rbx), %ecx
	movl	$1, %eax
	movl	56(%rbx), %edi
	movq	2944(%rbx), %r10
	movq	%r12, (%rsp)
	xorl	%r9d, %r9d
	xorl	%r11d, %r11d
	sall	%cl, %eax
	shrq	%cl, %r13
	movq	%rdi, 24(%rsp)
	movl	4(%rbx), %edi
	movl	%eax, 20(%rsp)
	subl	$1, %eax
	movl	%eax, 72(%rsp)
	leaq	(%r13,%r13), %rax
	movq	%rdi, %r8
	movq	%r10, 8(%rsp)
	movq	%rax, 40(%rsp)
	movq	%rbp, 48(%rsp)
	movq	%r14, 56(%rsp)
	movq	%r12, 64(%rsp)
	movl	%r15d, 76(%rsp)
	movq	%rdi, 32(%rsp)
	movq	%r14, %rdi
	.p2align 4,,10
	.p2align 3
.L840:
	movl	72(%rsp), %eax
	movq	(%rsp), %r12
	cmpl	%eax, %r11d
	movq	24(%rsp), %rax
	cmovne	%r13, %r12
	movq	%rax, %rbp
	cmpq	%r12, %rax
	movq	%r9, %rax
	cmova	%r12, %rbp
	xorl	%edx, %edx
	divq	32(%rsp)
	testq	%rbp, %rbp
	je	.L827
	movl	28(%rbx), %r14d
	testl	%r14d, %r14d
	je	.L828
	xorl	%eax, %eax
	jmp	.L837
	.p2align 4,,10
	.p2align 3
.L914:
	cmpq	%rbp, %rax
	je	.L827
.L836:
	testl	%r14d, %r14d
	je	.L913
.L837:
	movl	32(%rbx), %esi
	xorl	%ecx, %ecx
	cmpq	%rsi, %rax
	jb	.L829
	movq	%rax, %rcx
	subq	%rsi, %rcx
	movzbl	(%rdi,%rcx), %ecx
	movzbl	60(%rbx,%rcx), %ecx
.L829:
	cmpl	$1, %r14d
	je	.L830
	movl	36(%rbx), %esi
	cmpq	%rsi, %rax
	jb	.L831
	movq	%rax, %r15
	subq	%rsi, %r15
	movzbl	(%rdi,%r15), %esi
	movzbl	60(%rbx,%rsi), %esi
	addl	%esi, %ecx
.L831:
	cmpl	$2, %r14d
	je	.L830
	movl	40(%rbx), %esi
	cmpq	%rsi, %rax
	jb	.L832
	movq	%rax, %r15
	subq	%rsi, %r15
	movzbl	(%rdi,%r15), %esi
	movzbl	60(%rbx,%rsi), %esi
	addl	%esi, %ecx
.L832:
	cmpl	$3, %r14d
	je	.L830
	movl	44(%rbx), %esi
	cmpq	%rsi, %rax
	jb	.L830
	movq	%rax, %r15
	subq	%rsi, %r15
	movzbl	(%rdi,%r15), %esi
	movzbl	60(%rbx,%rsi), %esi
	addl	%esi, %ecx
.L830:
	movzwl	316(%rbx,%rcx,2), %esi
	xorl	%ecx, %ecx
	testq	%rax, %rax
	jne	.L838
.L834:
	movl	%edx, %r15d
	movzwl	2358(%rbx,%rcx,2), %ecx
	addl	%edx, %esi
	addl	$1, %edx
	andw	2870(%rbx,%r15,2), %cx
	movzwl	%cx, %ecx
	addl	%esi, %ecx
	movw	%cx, (%r10,%rax,2)
	addq	$1, %rax
	cmpl	%edx, %r8d
	jne	.L914
	xorl	%edx, %edx
	cmpq	%rax, %rbp
	jne	.L836
.L827:
	cmpq	%r12, %rbp
	jnb	.L846
	movq	8(%rsp), %rax
	leaq	(%rdi,%rbp), %rsi
	movl	28(%rbx), %r14d
	addq	%r9, %rbp
	addq	%rdi, %r12
	leaq	(%rax,%rbp,2), %rbp
	.p2align 4,,10
	.p2align 3
.L847:
	testl	%r14d, %r14d
	je	.L867
.L918:
	movl	32(%rbx), %ecx
	movq	%rsi, %rax
	subq	%rcx, %rax
	movzbl	(%rax), %eax
	movzbl	60(%rbx,%rax), %eax
	cmpl	$1, %r14d
	je	.L842
	movl	36(%rbx), %r15d
	movq	%rsi, %rcx
	subq	%r15, %rcx
	movzbl	(%rcx), %ecx
	movzbl	60(%rbx,%rcx), %ecx
	addl	%ecx, %eax
	cmpl	$2, %r14d
	je	.L842
	movl	40(%rbx), %r15d
	movq	%rsi, %rcx
	subq	%r15, %rcx
	movzbl	(%rcx), %ecx
	movzbl	60(%rbx,%rcx), %ecx
	addl	%ecx, %eax
	cmpl	$3, %r14d
	je	.L842
	movl	44(%rbx), %r15d
	movq	%rsi, %rcx
	subq	%r15, %rcx
	movzbl	(%rcx), %ecx
	movzbl	60(%rbx,%rcx), %ecx
	addl	%ecx, %eax
.L842:
	movzbl	-1(%rsi), %ecx
	movl	%edx, %r15d
	addq	$1, %rsi
	addq	$2, %rbp
	movzwl	316(%rbx,%rax,2), %eax
	movzwl	2358(%rbx,%rcx,2), %ecx
	andw	2870(%rbx,%r15,2), %cx
	movzwl	%cx, %ecx
	addl	%edx, %ecx
	addl	$1, %edx
	addl	%eax, %ecx
	movw	%cx, -2(%rbp)
	cmpl	%edx, %r8d
	je	.L844
	cmpq	%rsi, %r12
	jne	.L847
.L846:
	movq	40(%rsp), %rax
	subq	%r13, (%rsp)
	addl	$1, %r11d
	addq	%r13, %r9
	addq	%r13, %rdi
	addq	%rax, %r10
	cmpl	%r11d, 20(%rsp)
	jne	.L840
	movq	48(%rsp), %rbp
	movq	64(%rsp), %r12
	xorl	%edx, %edx
	movq	56(%rsp), %r14
	movslq	76(%rsp), %r15
	movq	5936(%rbp), %r13
	movq	8(%rsp), %rsi
	testq	%r12, %r12
	je	.L851
	.p2align 4,,10
	.p2align 3
.L848:
	movzwl	(%rsi,%rdx,2), %eax
	movzbl	(%r14,%rdx), %ecx
	addq	$1, %rdx
	salq	$8, %rax
	addq	%rcx, %rax
	addl	$1, 0(%r13,%rax,4)
	cmpq	%rdx, %r12
	jne	.L848
.L851:
	movl	8016(%rbp), %eax
	testl	%eax, %eax
	jne	.L850
	movq	8008(%rbp), %r14
	testq	%r14, %r14
	je	.L915
.L852:
	movq	$0x000000000, (%r14)
	movl	$1, %r12d
	.p2align 4,,10
	.p2align 3
.L853:
	pxor	%xmm1, %xmm1
	cvtsi2sdl	%r12d, %xmm1
	movapd	%xmm1, %xmm0
	movsd	%xmm1, (%rsp)
	call	*log2@GOTPCREL(%rip)
	movsd	(%rsp), %xmm1
	mulsd	%xmm1, %xmm0
	movsd	%xmm0, (%r14,%r12,8)
	addq	$1, %r12
	cmpq	$4096, %r12
	jne	.L853
	movl	$1, 8016(%rbp)
.L850:
	movl	52(%rbx), %eax
	movsd	.LC71(%rip), %xmm0
	cmpl	$6, %eax
	ja	.L854
	pxor	%xmm0, %xmm0
	cvtsi2sdl	%eax, %xmm0
	mulsd	.LC72(%rip), %xmm0
	addsd	.LC73(%rip), %xmm0
.L854:
	movq	%r13, %rdx
	movq	%rbx, %rsi
	movq	%rbp, %rdi
	call	kr_cluster
.L855:
	movq	%r13, %rdx
	movq	%rbx, %rsi
	movq	%rbp, %rdi
	call	kr_eval
	movsd	%xmm0, 2896(%rbx)
	testl	%r15d, %r15d
	je	.L858
	xorl	%r12d, %r12d
	jmp	.L857
	.p2align 4,,10
	.p2align 3
.L861:
	movq	8040(%rbp), %rdi
	movl	48(%rbx), %edx
	movq	2904(%rbx), %rsi
	call	*memcpy@GOTPCREL(%rip)
	movq	%r13, %rdx
	movq	%rbx, %rsi
	movq	%rbp, %rdi
	call	kr_reassign
	testl	%eax, %eax
	je	.L858
	movq	%r13, %rdx
	movq	%rbx, %rsi
	movq	%rbp, %rdi
	call	kr_eval
	comisd	2896(%rbx), %xmm0
	jnb	.L916
	addq	$1, %r12
	movsd	%xmm0, 2896(%rbx)
	cmpq	%r15, %r12
	je	.L858
.L857:
	movl	2888(%rbx), %r14d
	cmpl	$1, %r14d
	ja	.L861
.L858:
	movq	8(%rbp), %rdx
	pxor	%xmm0, %xmm0
	leaq	80(%rsp), %rdi
	movq	%rbx, %rsi
	movq	2928(%rbx), %rax
	movaps	%xmm0, 80(%rsp)
	movl	$0, 96(%rsp)
	addq	%rax, %rdx
	movq	%rax, %xmm0
	movq	%rdx, %xmm2
	punpcklqdq	%xmm2, %xmm0
	movups	%xmm0, 104(%rsp)
	call	kr_put_header
	movq	88(%rsp), %rax
	shrq	$3, %rax
	cmpq	%rax, 8(%rbp)
	movq	%rax, 2936(%rbx)
	sbbl	%eax, %eax
.L819:
	movq	120(%rsp), %rdx
	subq	%fs:40, %rdx
	jne	.L917
	addq	$136, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
	.p2align 4,,10
	.p2align 3
.L913:
	.cfi_restore_state
	movzwl	316(%rbx), %esi
.L838:
	movzbl	-1(%rdi,%rax), %ecx
	jmp	.L834
.L912:
	call	*free@GOTPCREL(%rip)
	movl	48(%rbx), %edx
	movq	%rdx, %r13
	movq	%rdx, (%rsp)
	salq	$10, %r13
	movq	%r13, %rdi
	call	*malloc@GOTPCREL(%rip)
	movq	%rax, 5936(%rbp)
	movq	%rax, %rdi
	testq	%rax, %rax
	je	.L823
	movq	(%rsp), %rdx
	movq	%rdx, 5944(%rbp)
	jmp	.L824
	.p2align 4,,10
	.p2align 3
.L844:
	cmpq	%r12, %rsi
	je	.L846
	xorl	%edx, %edx
	testl	%r14d, %r14d
	jne	.L918
.L867:
	xorl	%eax, %eax
	jmp	.L842
.L828:
	movzwl	316(%rbx), %esi
	xorl	%ecx, %ecx
	xorl	%eax, %eax
	jmp	.L834
.L821:
	movq	2904(%rbx), %rax
	leaq	8064(%rbp), %r13
	movb	$0, (%rax)
	movl	$1, 2888(%rbx)
	jmp	.L855
.L916:
	movq	2904(%rbx), %rdi
	movl	48(%rbx), %edx
	movq	8040(%rbp), %rsi
	call	*memcpy@GOTPCREL(%rip)
	movq	%r13, %rdx
	movq	%rbx, %rsi
	movq	%rbp, %rdi
	movl	%r14d, 2888(%rbx)
	call	kr_eval
	movsd	%xmm0, 2896(%rbx)
	jmp	.L858
.L915:
	movl	$32768, %edi
	call	*malloc@GOTPCREL(%rip)
	movq	%rax, 8008(%rbp)
	movq	%rax, %r14
	testq	%rax, %rax
	jne	.L852
.L911:
	movl	$1, 24(%rbp)
.L820:
	orl	$-1, %eax
	jmp	.L819
.L917:
	call	*__stack_chk_fail@GOTPCREL(%rip)
.L823:
	xorl	%edx, %edx
	movq	%rdx, 5944(%rbp)
	jmp	.L911
	.cfi_endproc
.LFE58:
	.size	kr_plan_build, .-kr_plan_build
	.p2align 4
	.type	kr_dec_syms, @function
kr_dec_syms:
.LFB42:
	.cfi_startproc
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	movq	%rcx, %rbx
	subq	$840, %rsp
	.cfi_def_cfa_offset 896
	movl	28(%rdi), %ebp
	movl	4(%rdi), %r14d
	movq	%rsi, 8(%rsp)
	movq	%rdx, %rsi
	movq	%rcx, 32(%rsp)
	movl	(%rdi), %ecx
	movq	%r8, 16(%rsp)
	movq	%r9, 88(%rsp)
	movq	%fs:40, %rax
	movq	%rax, 824(%rsp)
	xorl	%eax, %eax
	movq	%rbx, %rax
	subq	%rdx, %rax
	cmpl	$3, %ecx
	je	.L1355
	cmpl	$2, %ecx
	je	.L1356
.L983:
	movl	$1, %edx
	movdqa	.LC75(%rip), %xmm0
	movq	88(%rsp), %r11
	sall	%cl, %edx
	movl	%edx, %r10d
	shrq	%cl, %r11
	movaps	%xmm0, 272(%rsp)
	leaq	0(,%r10,4), %rbx
	movaps	%xmm0, 288(%rsp)
	movq	%rbx, 24(%rsp)
	cmpq	%rbx, %rax
	jb	.L1101
	movl	%r10d, 40(%rsp)
	leaq	(%rsi,%rbx), %r9
	xorl	%r8d, %r8d
	xorl	%ecx, %ecx
	movq	%r10, 48(%rsp)
	leaq	304(%rsp), %r12
	movq	16(%rsp), %r10
	movl	%r14d, %r13d
	jmp	.L1102
	.p2align 4,,10
	.p2align 3
.L1358:
	leaq	(%r10,%r8), %rax
	leaq	560(%rsp), %rbx
	xorl	%edx, %edx
	movq	%rax, (%rbx,%rcx,2)
	movq	%r8, %rax
	leaq	432(%rsp), %r15
	addq	%r11, %r8
	divq	%r13
	movl	%edx, 432(%rsp,%rcx)
	addq	$4, %rcx
	cmpq	%r9, %rsi
	je	.L1357
.L1102:
	movl	(%rsi), %eax
	addq	$4, %rsi
	movl	%eax, (%rcx,%r12)
	cmpl	$65535, %eax
	ja	.L1358
.L1101:
	movl	$-1, %eax
.L919:
	movq	824(%rsp), %rdx
	subq	%fs:40, %rdx
	jne	.L1359
	addq	$840, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L1356:
	.cfi_restore_state
	testl	%ebp, %ebp
	jne	.L983
	movq	%r9, %rcx
	shrq	$2, %rcx
	cmpq	$15, %rax
	jbe	.L1101
	movl	(%rdx), %r12d
	movl	%r12d, 304(%rsp)
	cmpl	$65535, %r12d
	jbe	.L1101
	movl	4(%rdx), %r13d
	movq	%r8, 560(%rsp)
	movl	%r14d, %r11d
	movl	$0, 432(%rsp)
	movl	%r13d, 308(%rsp)
	cmpl	$65535, %r13d
	jbe	.L1101
	movq	%rcx, %rax
	xorl	%edx, %edx
	movl	8(%rsi), %r9d
	leaq	(%r8,%rcx), %rbx
	divq	%r11
	movq	%rbx, 48(%rsp)
	movq	%rbx, 568(%rsp)
	movl	%r9d, 312(%rsp)
	movl	%edx, 104(%rsp)
	movl	%edx, 436(%rsp)
	cmpl	$65535, %r9d
	jbe	.L1101
	leaq	(%rcx,%rcx), %r10
	xorl	%edx, %edx
	movl	12(%rsi), %r8d
	addq	%rcx, %rbx
	movq	%r10, %rax
	movq	%rbx, 40(%rsp)
	leaq	16(%rsi), %r15
	divq	%r11
	movq	%rbx, 576(%rsp)
	movl	%r8d, 316(%rsp)
	movl	%edx, 96(%rsp)
	movl	%edx, 440(%rsp)
	cmpl	$65535, %r8d
	jbe	.L1101
	leaq	(%r10,%rcx), %rax
	xorl	%edx, %edx
	addq	%rcx, %rbx
	movq	%rax, 112(%rsp)
	divq	%r11
	movq	%rbx, 56(%rsp)
	movq	%rbx, 584(%rsp)
	cmpl	$1, 48(%rdi)
	movl	%edx, 80(%rsp)
	movq	%rdx, %rbp
	movl	%edx, 444(%rsp)
	je	.L1360
	movl	56(%rdi), %eax
	leaq	2870(%rdi), %rbx
	cmpq	%rcx, %rax
	cmova	%rcx, %rax
	movq	%rax, 72(%rsp)
	leaq	2358(%rdi), %rax
	movq	%rax, 64(%rsp)
	testq	%rcx, %rcx
	je	.L1045
	xorl	%r12d, %r12d
	xorl	%edx, %edx
.L1087:
	movq	72(%rsp), %rax
	cmpq	%rax, %rdx
	jb	.L1064
	movq	32(%rsp), %rax
	subq	%r15, %rax
	cmpq	$7, %rax
	jbe	.L1064
	movl	%r12d, %esi
	movq	16(%rsp), %rax
	movq	64(%rsp), %r9
	subl	%r14d, %esi
	cmpl	%r14d, %r12d
	movl	304(%rsp), %r13d
	cmovb	%r12d, %esi
	movzbl	-1(%rax,%rdx), %eax
	movl	%r13d, %r10d
	movl	%esi, %r8d
	movzwl	(%r9,%rax,2), %eax
	andl	$4095, %r10d
	andw	(%rbx,%r8,2), %ax
	movzwl	316(%rdi), %r8d
	movzwl	%ax, %eax
	addl	%esi, %r8d
	movq	8(%rsp), %rsi
	addl	%r8d, %eax
	movl	%r13d, %r8d
	movl	%eax, %eax
	shrl	$12, %r8d
	movq	(%rsi,%rax,8), %r9
	movl	%r10d, %eax
	movzbl	(%r9,%rax), %esi
	movq	16(%rsp), %rax
	movb	%sil, (%rax,%rdx)
	movzwl	4096(%r9,%rsi,4), %eax
	movzwl	4098(%r9,%rsi,4), %esi
	imull	%r8d, %eax
	subl	%esi, %eax
	movzwl	(%r15), %esi
	addl	%r10d, %eax
	cmpl	$65535, %eax
	setbe	%r8b
	ja	.L1066
	sall	$16, %eax
	orl	%esi, %eax
.L1066:
	movl	%eax, 304(%rsp)
	movl	104(%rsp), %eax
	leaq	(%r8,%r8), %rsi
	andl	$2, %esi
	movq	48(%rsp), %r9
	movq	64(%rsp), %r10
	addl	%r12d, %eax
	addq	%r15, %rsi
	movl	308(%rsp), %r15d
	movl	%eax, %r8d
	subl	%r14d, %r8d
	cmpl	%r14d, %eax
	movl	%r15d, %r13d
	cmovnb	%r8d, %eax
	movzbl	-1(%r9,%rdx), %r8d
	andl	$4095, %r13d
	shrl	$12, %r15d
	movl	%eax, %r9d
	movzwl	(%r10,%r8,2), %r8d
	andw	(%rbx,%r9,2), %r8w
	movzwl	316(%rdi), %r9d
	movzwl	%r8w, %r8d
	addl	%eax, %r9d
	leal	(%r8,%r9), %eax
	movq	8(%rsp), %r8
	movq	48(%rsp), %r9
	movq	(%r8,%rax,8), %r10
	movl	%r13d, %eax
	movzbl	(%r10,%rax), %r8d
	movb	%r8b, (%r9,%rdx)
	movzwl	4096(%r10,%r8,4), %eax
	movzwl	4098(%r10,%r8,4), %r8d
	imull	%r15d, %eax
	subl	%r8d, %eax
	movzwl	(%rsi), %r8d
	addl	%r13d, %eax
	cmpl	$65535, %eax
	setbe	%r9b
	ja	.L1068
	sall	$16, %eax
	orl	%r8d, %eax
.L1068:
	movl	%eax, 308(%rsp)
	leaq	(%r9,%r9), %rax
	movq	40(%rsp), %r9
	andl	$2, %eax
	movq	64(%rsp), %r10
	movl	312(%rsp), %r15d
	addq	%rax, %rsi
	movl	96(%rsp), %eax
	movl	%r15d, %r13d
	addl	%r12d, %eax
	movl	%eax, %r8d
	subl	%r14d, %r8d
	cmpl	%r14d, %eax
	cmovnb	%r8d, %eax
	movzbl	-1(%r9,%rdx), %r8d
	andl	$4095, %r13d
	shrl	$12, %r15d
	movl	%eax, %r9d
	movzwl	(%r10,%r8,2), %r8d
	andw	(%rbx,%r9,2), %r8w
	movzwl	316(%rdi), %r9d
	movzwl	%r8w, %r8d
	addl	%eax, %r9d
	leal	(%r8,%r9), %eax
	movq	8(%rsp), %r8
	movq	40(%rsp), %r9
	movq	(%r8,%rax,8), %r10
	movl	%r13d, %eax
	movzbl	(%r10,%rax), %r8d
	movb	%r8b, (%r9,%rdx)
	movzwl	4096(%r10,%r8,4), %eax
	movzwl	4098(%r10,%r8,4), %r8d
	imull	%r15d, %eax
	subl	%r8d, %eax
	movzwl	(%rsi), %r8d
	addl	%r13d, %eax
	cmpl	$65535, %eax
	setbe	%r9b
	ja	.L1070
	sall	$16, %eax
	orl	%r8d, %eax
.L1070:
	movl	%eax, 312(%rsp)
	movl	80(%rsp), %eax
	leaq	(%r9,%r9), %r15
	andl	$2, %r15d
	movq	64(%rsp), %r9
	movl	316(%rsp), %r13d
	addl	%r12d, %eax
	addq	%rsi, %r15
	movl	%eax, %esi
	movl	%r13d, %r10d
	subl	%r14d, %esi
	cmpl	%r14d, %eax
	cmovnb	%esi, %eax
	movq	56(%rsp), %rsi
	andl	$4095, %r10d
	movzbl	-1(%rsi,%rdx), %esi
	movl	%eax, %r8d
	movzwl	(%r9,%rsi,2), %esi
	andw	(%rbx,%r8,2), %si
	movzwl	316(%rdi), %r8d
	movzwl	%si, %esi
	movq	8(%rsp), %r9
	addl	%eax, %r8d
	leal	(%rsi,%r8), %eax
	movl	%r13d, %r8d
	movq	(%r9,%rax,8), %r9
	movl	%r10d, %eax
	shrl	$12, %r8d
	movzbl	(%r9,%rax), %esi
	movq	56(%rsp), %rax
	movb	%sil, (%rax,%rdx)
	movzwl	4096(%r9,%rsi,4), %eax
	movzwl	4098(%r9,%rsi,4), %esi
	imull	%r8d, %eax
	subl	%esi, %eax
	movzwl	(%r15), %esi
	addl	%r10d, %eax
	cmpl	$65535, %eax
	setbe	%r8b
	ja	.L1072
	sall	$16, %eax
	orl	%esi, %eax
.L1072:
	movl	%eax, 316(%rsp)
	leaq	(%r8,%r8), %rax
	andl	$2, %eax
	addq	%rax, %r15
.L1073:
	addl	$1, %r12d
	addq	$1, %rdx
	cmpl	%r14d, %r12d
	je	.L1084
	cmpq	%rdx, %rcx
	jne	.L1087
.L1086:
	movl	316(%rsp), %r8d
.L1045:
	movq	88(%rsp), %rsi
	movq	112(%rsp), %rax
	subq	%rax, %rsi
	cmpq	%rsi, %rcx
	jnb	.L1088
	movq	56(%rsp), %r9
	movq	8(%rsp), %r10
	jmp	.L1098
.L1361:
	movl	32(%rdi), %r12d
	xorl	%eax, %eax
	cmpq	%r12, %rcx
	jb	.L1091
	movq	%rcx, %rax
	subq	%r12, %rax
	movzbl	(%r9,%rax), %eax
	movzbl	60(%rdi,%rax), %eax
.L1091:
	cmpl	$1, %ebx
	je	.L1090
	movl	36(%rdi), %r12d
	cmpq	%r12, %rcx
	jb	.L1093
	movq	%rcx, %r13
	subq	%r12, %r13
	movzbl	(%r9,%r13), %r12d
	movzbl	60(%rdi,%r12), %r12d
	addl	%r12d, %eax
.L1093:
	cmpl	$2, %ebx
	je	.L1090
	movl	40(%rdi), %r12d
	cmpq	%r12, %rcx
	jb	.L1094
	movq	%rcx, %r13
	subq	%r12, %r13
	movzbl	(%r9,%r13), %r12d
	movzbl	60(%rdi,%r12), %r12d
	addl	%r12d, %eax
.L1094:
	cmpl	$3, %ebx
	je	.L1090
	movl	44(%rdi), %ebx
	cmpq	%rbx, %rcx
	jb	.L1090
	movq	%rcx, %r12
	subq	%rbx, %r12
	movzbl	(%r9,%r12), %ebx
	movzbl	60(%rdi,%rbx), %ebx
	addl	%ebx, %eax
.L1090:
	movzwl	316(%rdi,%rax,2), %ebx
	xorl	%eax, %eax
	testq	%rcx, %rcx
	je	.L1096
	movzbl	-1(%r9,%rcx), %eax
.L1096:
	movzwl	2358(%rdi,%rax,2), %eax
	andw	2870(%rdi,%rdx,2), %ax
	addl	%edx, %ebx
	movzwl	%ax, %eax
	addl	%ebx, %eax
	movl	%eax, %eax
	leaq	(%r10,%rax,8), %rax
.L1089:
	movl	%r8d, %ebx
	movq	(%rax), %r12
	shrl	$12, %r8d
	andl	$4095, %ebx
	movl	%ebx, %eax
	movzbl	(%r12,%rax), %edx
	movb	%dl, (%r9,%rcx)
	movzwl	4096(%r12,%rdx,4), %eax
	movzwl	4098(%r12,%rdx,4), %edx
	imull	%r8d, %eax
	subl	%edx, %eax
	leal	(%rbx,%rax), %r8d
	cmpl	$65535, %r8d
	ja	.L1097
	movq	32(%rsp), %rax
	subq	%r15, %rax
	cmpq	$1, %rax
	jle	.L1101
	movzwl	(%r15), %eax
	sall	$16, %r8d
	addq	$2, %r15
	orl	%eax, %r8d
.L1097:
	addq	$1, %rcx
	cmpq	%rcx, %rsi
	je	.L1088
.L1098:
	cmpl	$1, 48(%rdi)
	movq	%r10, %rax
	je	.L1089
	movl	28(%rdi), %ebx
	leaq	0(%rbp,%rcx), %rax
	xorl	%edx, %edx
	divq	%r11
	testl	%ebx, %ebx
	jne	.L1361
	xorl	%eax, %eax
	jmp	.L1090
	.p2align 4,,10
	.p2align 3
.L1357:
	movq	%r15, 16(%rsp)
	movq	48(%rsp), %r10
	movl	40(%rsp), %r15d
	testl	%ebp, %ebp
	je	.L1107
	leal	-1(%rbp), %eax
	cmpl	$2, %eax
	jbe	.L1182
	movdqu	32(%rdi), %xmm0
	pxor	%xmm1, %xmm1
	movl	%ebp, %eax
	andl	$-4, %eax
	movdqa	%xmm0, %xmm2
	punpckhdq	%xmm1, %xmm0
	punpckldq	%xmm1, %xmm2
	movaps	%xmm0, 288(%rsp)
	movaps	%xmm2, 272(%rsp)
	testb	$3, %bpl
	je	.L1107
.L1106:
	movl	%eax, %edx
	addl	$1, %eax
	movl	32(%rdi,%rdx,4), %ecx
	movq	%rcx, 272(%rsp,%rdx,8)
	cmpl	%ebp, %eax
	jnb	.L1107
	movl	32(%rdi,%rax,4), %edx
	movq	%rdx, 272(%rsp,%rax,8)
	cmpl	$2, %ebp
	je	.L1107
	movl	40(%rdi), %eax
	movq	%rax, 288(%rsp)
.L1107:
	cmpl	$1, 48(%rdi)
	je	.L1362
	movl	56(%rdi), %eax
	cmpq	%r11, %rax
	cmova	%r11, %rax
	movq	%rax, %r8
	leaq	60(%rdi), %rax
	movq	%rax, 72(%rsp)
	leaq	316(%rdi), %rax
	movq	%rax, 64(%rsp)
	leaq	2358(%rdi), %rax
	movq	%rax, 48(%rsp)
	leaq	2870(%rdi), %rax
	movq	%rax, 56(%rsp)
	testq	%r11, %r11
	je	.L1185
	leaq	(%r10,%r10), %rax
	movq	%r11, 136(%rsp)
	xorl	%edx, %edx
	movq	%rax, 144(%rsp)
	movq	272(%rsp), %rax
	movq	%r8, 128(%rsp)
	negq	%rax
	movq	%rdi, 152(%rsp)
	movq	%rax, 80(%rsp)
	movq	280(%rsp), %rax
	movq	%r13, 168(%rsp)
	negq	%rax
	movl	%r15d, 164(%rsp)
	movl	%ebp, %r15d
	movq	%rax, 96(%rsp)
	movq	288(%rsp), %rax
	movq	%r12, 120(%rsp)
	negq	%rax
	movq	%rax, 104(%rsp)
	movq	296(%rsp), %rax
	negq	%rax
	movq	%rax, 112(%rsp)
	xorl	%eax, %eax
	movl	%eax, %r12d
.L1140:
	movq	128(%rsp), %rax
	cmpq	%rax, %rdx
	jb	.L1120
	movq	32(%rsp), %rax
	movq	144(%rsp), %rdi
	subq	%rsi, %rax
	cmpq	%rdi, %rax
	jb	.L1120
	movq	%rdx, 40(%rsp)
	movq	120(%rsp), %r13
	xorl	%ecx, %ecx
	.p2align 4,,10
	.p2align 3
.L1127:
	movq	16(%rsp), %rax
	movq	40(%rsp), %r8
	addq	(%rbx,%rcx,2), %r8
	movl	(%rax,%rcx), %edi
	addl	%r12d, %edi
	movl	%edi, %eax
	subl	%r14d, %eax
	cmpl	%r14d, %edi
	cmovnb	%eax, %edi
	testl	%r15d, %r15d
	je	.L1186
	movq	80(%rsp), %rax
	movq	72(%rsp), %r11
	movzbl	(%r8,%rax), %eax
	movzbl	(%r11,%rax), %eax
	cmpl	$1, %r15d
	je	.L1123
	movq	96(%rsp), %rdx
	movzbl	(%r8,%rdx), %r9d
	movzbl	(%r11,%r9), %r9d
	addl	%r9d, %eax
	cmpl	$2, %r15d
	je	.L1123
	movq	104(%rsp), %rdx
	movzbl	(%r8,%rdx), %r9d
	movzbl	(%r11,%r9), %r9d
	addl	%r9d, %eax
	cmpl	$3, %r15d
	je	.L1123
	movq	112(%rsp), %rdx
	movzbl	(%r8,%rdx), %r9d
	movzbl	(%r11,%r9), %r9d
	addl	%r9d, %eax
.L1123:
	movq	64(%rsp), %rdx
	leaq	(%rdx,%rax,2), %rbp
.L1122:
	movzbl	-1(%r8), %eax
	movq	48(%rsp), %rdx
	movl	%edi, %r11d
	movl	(%rcx,%r13), %r9d
	movzwl	(%rdx,%rax,2), %eax
	movq	56(%rsp), %rdx
	movl	%r9d, %r10d
	shrl	$12, %r9d
	andw	(%rdx,%r11,2), %ax
	movq	8(%rsp), %rdx
	andl	$4095, %r10d
	movzwl	%ax, %eax
	addl	%edi, %eax
	movzwl	0(%rbp), %edi
	addl	%edi, %eax
	movl	%eax, %eax
	movq	(%rdx,%rax,8), %rdi
	movl	%r10d, %eax
	movzbl	(%rdi,%rax), %eax
	movb	%al, (%r8)
	movzbl	%al, %r8d
	movzwl	4096(%rdi,%r8,4), %eax
	movzwl	4098(%rdi,%r8,4), %edi
	imull	%r9d, %eax
	subl	%edi, %eax
	movzwl	(%rsi), %edi
	addl	%r10d, %eax
	cmpl	$65535, %eax
	ja	.L1124
	sall	$16, %eax
	addq	$2, %rsi
	addq	$4, %rcx
	orl	%edi, %eax
	movl	%eax, -4(%rcx,%r13)
	cmpq	%rcx, 24(%rsp)
	jne	.L1127
.L1353:
	movq	40(%rsp), %rdx
	addl	$1, %r12d
	addq	$1, %rdx
	cmpl	%r14d, %r12d
	je	.L1138
.L1367:
	cmpq	%rdx, 136(%rsp)
	jne	.L1140
.L1190:
	movq	136(%rsp), %r11
	movl	164(%rsp), %r15d
	movq	152(%rsp), %rdi
	movq	120(%rsp), %r12
	movq	168(%rsp), %r13
	movq	%r11, %rbp
.L1109:
	leal	-1(%r15), %ebx
	movq	88(%rsp), %r10
	imulq	%rbx, %r11
	movq	560(%rsp,%rbx,8), %r9
	movl	304(%rsp,%rbx,4), %ecx
	subq	%r11, %r10
	cmpq	%r10, %rbp
	jnb	.L1141
	movq	8(%rsp), %r11
	movq	%r12, 8(%rsp)
	movq	32(%rsp), %r12
	jmp	.L1151
	.p2align 4,,10
	.p2align 3
.L1364:
	movl	32(%rdi), %r14d
	xorl	%eax, %eax
	cmpq	%r14, %rbp
	jb	.L1144
	movq	%rbp, %rax
	subq	%r14, %rax
	movzbl	(%r9,%rax), %eax
	movzbl	60(%rdi,%rax), %eax
.L1144:
	cmpl	$1, %r8d
	je	.L1143
	movl	36(%rdi), %r14d
	cmpq	%r14, %rbp
	jb	.L1146
	movq	%rbp, %r15
	subq	%r14, %r15
	movzbl	(%r9,%r15), %r14d
	movzbl	60(%rdi,%r14), %r14d
	addl	%r14d, %eax
.L1146:
	cmpl	$2, %r8d
	je	.L1143
	movl	40(%rdi), %r14d
	cmpq	%r14, %rbp
	jb	.L1147
	movq	%rbp, %r15
	subq	%r14, %r15
	movzbl	(%r9,%r15), %r14d
	movzbl	60(%rdi,%r14), %r14d
	addl	%r14d, %eax
.L1147:
	cmpl	$3, %r8d
	je	.L1143
	movl	44(%rdi), %r8d
	cmpq	%r8, %rbp
	jb	.L1143
	movq	%rbp, %r14
	subq	%r8, %r14
	movzbl	(%r9,%r14), %r8d
	movzbl	60(%rdi,%r8), %r8d
	addl	%r8d, %eax
.L1143:
	movzwl	316(%rdi,%rax,2), %r8d
	xorl	%eax, %eax
	testq	%rbp, %rbp
	je	.L1149
	movzbl	-1(%r9,%rbp), %eax
.L1149:
	movzwl	2358(%rdi,%rax,2), %eax
	andw	2870(%rdi,%rdx,2), %ax
	addl	%edx, %r8d
	movzwl	%ax, %eax
	addl	%r8d, %eax
	movl	%eax, %eax
	leaq	(%r11,%rax,8), %rax
.L1142:
	movl	%ecx, %edx
	movq	(%rax), %r8
	shrl	$12, %ecx
	andl	$4095, %edx
	movl	%edx, %eax
	movzbl	(%r8,%rax), %eax
	movb	%al, (%r9,%rbp)
	movzwl	4096(%r8,%rax,4), %r14d
	movzwl	4098(%r8,%rax,4), %eax
	imull	%r14d, %ecx
	subl	%eax, %ecx
	addl	%edx, %ecx
	cmpl	$65535, %ecx
	ja	.L1150
	movq	%r12, %rax
	subq	%rsi, %rax
	cmpq	$1, %rax
	jle	.L1101
	movzwl	(%rsi), %eax
	sall	$16, %ecx
	addq	$2, %rsi
	orl	%eax, %ecx
.L1150:
	addq	$1, %rbp
	cmpq	%rbp, %r10
	je	.L1363
.L1151:
	cmpl	$1, 48(%rdi)
	movq	%r11, %rax
	je	.L1142
	movl	432(%rsp,%rbx,4), %eax
	movl	28(%rdi), %r8d
	xorl	%edx, %edx
	addq	%rbp, %rax
	divq	%r13
	testl	%r8d, %r8d
	jne	.L1364
	xorl	%eax, %eax
	jmp	.L1143
	.p2align 4,,10
	.p2align 3
.L1124:
	movl	%eax, (%rcx,%r13)
	addq	$4, %rcx
	cmpq	%rcx, 24(%rsp)
	jne	.L1127
	jmp	.L1353
	.p2align 4,,10
	.p2align 3
.L1120:
	movl	%r15d, 40(%rsp)
	movq	120(%rsp), %r10
	xorl	%r9d, %r9d
	movq	152(%rsp), %rdi
	jmp	.L1137
	.p2align 4,,10
	.p2align 3
.L1366:
	movl	32(%rdi), %ebp
	xorl	%ecx, %ecx
	cmpq	%rbp, %rdx
	jb	.L1130
	movq	%rdx, %rcx
	subq	%rbp, %rcx
	movzbl	(%r11,%rcx), %ecx
	movzbl	60(%rdi,%rcx), %ecx
.L1130:
	cmpl	$1, %r8d
	je	.L1129
	movl	36(%rdi), %ebp
	cmpq	%rbp, %rdx
	jb	.L1132
	movq	%rdx, %r13
	subq	%rbp, %r13
	movzbl	(%r11,%r13), %ebp
	movzbl	60(%rdi,%rbp), %ebp
	addl	%ebp, %ecx
.L1132:
	cmpl	$2, %r8d
	je	.L1129
	movl	40(%rdi), %ebp
	cmpq	%rbp, %rdx
	jb	.L1133
	movq	%rdx, %r13
	subq	%rbp, %r13
	movzbl	(%r11,%r13), %ebp
	movzbl	60(%rdi,%rbp), %ebp
	addl	%ebp, %ecx
.L1133:
	cmpl	$3, %r8d
	je	.L1129
	movl	44(%rdi), %r8d
	cmpq	%r8, %rdx
	jb	.L1129
	movq	%rdx, %rbp
	subq	%r8, %rbp
	movzbl	(%r11,%rbp), %r8d
	movzbl	60(%rdi,%r8), %r8d
	addl	%r8d, %ecx
.L1129:
	movzwl	316(%rdi,%rcx,2), %r13d
	xorl	%ecx, %ecx
	testq	%rdx, %rdx
	je	.L1135
	movzbl	-1(%r11,%rdx), %ecx
.L1135:
	movl	%eax, %r15d
	movzwl	2358(%rdi,%rcx,2), %ecx
	movl	(%r10), %r8d
	addl	%r13d, %eax
	andw	2870(%rdi,%r15,2), %cx
	movzwl	%cx, %ecx
	movl	%r8d, %ebp
	shrl	$12, %r8d
	addl	%ecx, %eax
	movq	8(%rsp), %rcx
	andl	$4095, %ebp
	movl	%eax, %eax
	movq	(%rcx,%rax,8), %rcx
	movl	%ebp, %eax
	movzbl	(%rcx,%rax), %eax
	movb	%al, (%r11,%rdx)
	movzbl	%al, %r11d
	movzwl	4096(%rcx,%r11,4), %eax
	movzwl	4098(%rcx,%r11,4), %ecx
	imull	%r8d, %eax
	subl	%ecx, %eax
	addl	%ebp, %eax
	movl	%eax, (%r10)
	cmpl	$65535, %eax
	ja	.L1136
	movq	32(%rsp), %rcx
	subq	%rsi, %rcx
	cmpq	$1, %rcx
	jle	.L1101
	movzwl	(%rsi), %ecx
	sall	$16, %eax
	addq	$2, %rsi
	orl	%ecx, %eax
	movl	%eax, (%r10)
.L1136:
	addq	$4, %r10
	addq	$4, %r9
	cmpq	%r9, 24(%rsp)
	je	.L1365
.L1137:
	movq	16(%rsp), %rcx
	movl	28(%rdi), %r8d
	movq	(%rbx,%r9,2), %r11
	movl	(%rcx,%r9), %eax
	addl	%r12d, %eax
	movl	%eax, %ecx
	subl	%r14d, %ecx
	cmpl	%r14d, %eax
	cmovnb	%ecx, %eax
	testl	%r8d, %r8d
	jne	.L1366
	xorl	%ecx, %ecx
	jmp	.L1129
	.p2align 4,,10
	.p2align 3
.L1186:
	movq	64(%rsp), %rbp
	jmp	.L1122
	.p2align 4,,10
	.p2align 3
.L1365:
	addl	$1, %r12d
	movl	40(%rsp), %r15d
	addq	$1, %rdx
	cmpl	%r14d, %r12d
	jne	.L1367
.L1138:
	cmpq	%rdx, 136(%rsp)
	je	.L1190
	xorl	%r12d, %r12d
	jmp	.L1140
.L1355:
	testl	%ebp, %ebp
	je	.L1368
	cmpl	$4, %ebp
	jne	.L983
	movq	%r9, %rcx
	shrq	$3, %rcx
	cmpq	$31, %rax
	jbe	.L1101
	movl	(%rdx), %r9d
	movl	%r9d, 304(%rsp)
	cmpl	$65535, %r9d
	jbe	.L1101
	movq	%r8, 560(%rsp)
	movq	%r8, %rax
	movl	4(%rdx), %r8d
	movl	%r14d, %r10d
	movl	$0, 432(%rsp)
	movl	%r8d, 308(%rsp)
	cmpl	$65535, %r8d
	jbe	.L1101
	leaq	(%rax,%rcx), %rbx
	xorl	%edx, %edx
	movq	%rcx, %rax
	movl	8(%rsi), %r12d
	divq	%r10
	movq	%rbx, 224(%rsp)
	movq	%rbx, 568(%rsp)
	movl	%r12d, 312(%rsp)
	movl	%edx, 436(%rsp)
	cmpl	$65535, %r12d
	jbe	.L1101
	leaq	(%rcx,%rcx), %r11
	xorl	%edx, %edx
	movl	12(%rsi), %r13d
	addq	%rcx, %rbx
	movq	%r11, %rax
	movq	%rbx, 216(%rsp)
	divq	%r10
	movq	%rbx, 576(%rsp)
	movl	%r13d, 316(%rsp)
	movl	%edx, 440(%rsp)
	cmpl	$65535, %r13d
	jbe	.L1101
	leaq	(%r11,%rcx), %rax
	xorl	%edx, %edx
	addq	%rcx, %rbx
	divq	%r10
	movl	16(%rsi), %eax
	movq	%rbx, 208(%rsp)
	movq	%rbx, 584(%rsp)
	movl	%eax, 24(%rsp)
	movl	%eax, 320(%rsp)
	movl	%edx, 444(%rsp)
	cmpl	$65535, %eax
	jbe	.L1101
	leaq	(%rbx,%rcx), %r15
	leaq	0(,%rcx,4), %rbx
	xorl	%edx, %edx
	movq	%rbx, %rax
	movq	%r15, 200(%rsp)
	divq	%r10
	movl	20(%rsi), %eax
	movq	%r15, 592(%rsp)
	movl	%eax, 40(%rsp)
	movl	%eax, 324(%rsp)
	movl	%edx, 448(%rsp)
	cmpl	$65535, %eax
	jbe	.L1101
	leaq	(%rbx,%rcx), %rax
	xorl	%edx, %edx
	addq	%rcx, %r15
	divq	%r10
	movl	24(%rsi), %eax
	movq	%r15, 192(%rsp)
	movq	%r15, 600(%rsp)
	movl	%eax, 56(%rsp)
	movl	%eax, 328(%rsp)
	movl	%edx, 452(%rsp)
	cmpl	$65535, %eax
	jbe	.L1101
	leaq	(%r11,%rcx), %rax
	xorl	%edx, %edx
	leaq	(%r15,%rcx), %rbx
	addq	%rax, %rax
	movq	%rbx, 184(%rsp)
	leaq	32(%rsi), %r15
	divq	%r10
	movl	28(%rsi), %eax
	movq	%rbx, 608(%rsp)
	movl	%eax, 48(%rsp)
	movl	%eax, 332(%rsp)
	movl	%edx, 456(%rsp)
	cmpl	$65535, %eax
	jbe	.L1101
	leaq	0(,%rcx,8), %rax
	xorl	%edx, %edx
	addq	%rcx, %rbx
	movl	40(%rdi), %esi
	subq	%rcx, %rax
	movq	%rbx, 104(%rsp)
	movq	%rax, 264(%rsp)
	divq	%r10
	movq	%rbx, 616(%rsp)
	movl	32(%rdi), %eax
	movl	44(%rdi), %ebx
	cmpl	$1, 48(%rdi)
	movl	%edx, 460(%rsp)
	movq	%rdx, %r11
	movl	36(%rdi), %edx
	je	.L1369
	movl	56(%rdi), %r8d
	leaq	316(%rdi), %r9
	leaq	60(%rdi), %rbp
	movq	%r9, 40(%rsp)
	leaq	2358(%rdi), %r9
	leaq	2870(%rdi), %r13
	cmpq	%rcx, %r8
	movq	%r9, 48(%rsp)
	cmova	%rcx, %r8
	movq	%r8, 128(%rsp)
	testq	%rcx, %rcx
	je	.L986
	movl	%edx, %r8d
	movl	%ebx, %r9d
	negq	%rax
	negq	%rsi
	negq	%r8
	negq	%r9
	movl	$0, 24(%rsp)
	xorl	%edx, %edx
	movq	%rax, 56(%rsp)
	leaq	432(%rsp), %r12
	leaq	560(%rsp), %rbx
	movq	%r8, 64(%rsp)
	movq	%rsi, 72(%rsp)
	movq	%r9, 80(%rsp)
	movq	%rcx, 96(%rsp)
	movq	%r10, 112(%rsp)
	movq	%r12, 16(%rsp)
	leaq	304(%rsp), %r12
.L1031:
	movq	128(%rsp), %rax
	cmpq	%rax, %rdx
	jb	.L1013
	movq	32(%rsp), %rax
	subq	%r15, %rax
	cmpq	$15, %rax
	jbe	.L1013
	movq	%rdi, 120(%rsp)
	xorl	%r8d, %r8d
.L1018:
	movq	16(%rsp), %rsi
	movq	(%rbx,%r8,2), %rcx
	movl	24(%rsp), %eax
	movq	56(%rsp), %r9
	addl	(%rsi,%r8), %eax
	addq	%rdx, %rcx
	movq	48(%rsp), %rdi
	movl	%eax, %esi
	movzbl	(%rcx,%r9), %r9d
	subl	%r14d, %esi
	cmpl	%r14d, %eax
	cmovnb	%rsi, %rax
	movq	64(%rsp), %rsi
	movzbl	0(%rbp,%r9), %r9d
	movzbl	(%rcx,%rsi), %esi
	movzbl	0(%rbp,%rsi), %esi
	addl	%r9d, %esi
	movq	72(%rsp), %r9
	movzbl	(%rcx,%r9), %r9d
	movzbl	0(%rbp,%r9), %r10d
	movl	(%r8,%r12), %r9d
	addl	%esi, %r10d
	movq	80(%rsp), %rsi
	andl	$4095, %r9d
	movzbl	(%rcx,%rsi), %esi
	movzbl	0(%rbp,%rsi), %esi
	addl	%r10d, %esi
	movq	40(%rsp), %r10
	movzwl	(%r10,%rsi,2), %esi
	movzbl	-1(%rcx), %r10d
	movzwl	(%rdi,%r10,2), %r10d
	andw	0(%r13,%rax,2), %r10w
	addl	%eax, %esi
	movzwl	%r10w, %eax
	addl	%esi, %eax
	movq	8(%rsp), %rsi
	movl	%eax, %eax
	movq	(%rsi,%rax,8), %rsi
	movl	%r9d, %eax
	movzbl	(%rsi,%rax), %r10d
	movl	(%r8,%r12), %eax
	movb	%r10b, (%rcx)
	movzbl	%r10b, %ecx
	shrl	$12, %eax
	movzwl	4096(%rsi,%rcx,4), %r10d
	movzwl	4098(%rsi,%rcx,4), %ecx
	imull	%r10d, %eax
	subl	%ecx, %eax
	movzwl	(%r15), %ecx
	addl	%r9d, %eax
	cmpl	$65535, %eax
	ja	.L1015
	sall	$16, %eax
	addq	$4, %r8
	addq	$2, %r15
	orl	%ecx, %eax
	movl	%eax, -4(%r8,%r12)
	cmpq	$32, %r8
	jne	.L1018
.L1349:
	movq	120(%rsp), %rdi
.L1017:
	addl	$1, 24(%rsp)
	movl	24(%rsp), %eax
	addq	$1, %rdx
	cmpl	%r14d, %eax
	je	.L1029
	cmpq	%rdx, 96(%rsp)
	jne	.L1031
.L1341:
	movq	96(%rsp), %rcx
	movq	112(%rsp), %r10
.L986:
	movq	88(%rsp), %r8
	movq	264(%rsp), %rax
	movl	332(%rsp), %esi
	subq	%rax, %r8
	cmpq	%r8, %rcx
	jnb	.L1032
	movq	104(%rsp), %r9
	jmp	.L1042
.L1370:
	movl	32(%rdi), %ebp
	xorl	%eax, %eax
	cmpq	%rbp, %rcx
	jb	.L1035
	movq	%rcx, %rax
	subq	%rbp, %rax
	movzbl	(%r9,%rax), %eax
	movzbl	60(%rdi,%rax), %eax
.L1035:
	cmpl	$1, %ebx
	je	.L1034
	movl	36(%rdi), %ebp
	cmpq	%rbp, %rcx
	jb	.L1037
	movq	%rcx, %r12
	subq	%rbp, %r12
	movzbl	(%r9,%r12), %ebp
	movzbl	60(%rdi,%rbp), %ebp
	addl	%ebp, %eax
.L1037:
	cmpl	$2, %ebx
	je	.L1034
	movl	40(%rdi), %ebp
	cmpq	%rbp, %rcx
	jb	.L1038
	movq	%rcx, %r12
	subq	%rbp, %r12
	movzbl	(%r9,%r12), %ebp
	movzbl	60(%rdi,%rbp), %ebp
	addl	%ebp, %eax
.L1038:
	cmpl	$3, %ebx
	je	.L1034
	movl	44(%rdi), %ebx
	cmpq	%rbx, %rcx
	jb	.L1034
	movq	%rcx, %rbp
	subq	%rbx, %rbp
	movzbl	(%r9,%rbp), %ebx
	movzbl	60(%rdi,%rbx), %ebx
	addl	%ebx, %eax
.L1034:
	movzwl	316(%rdi,%rax,2), %ebx
	xorl	%eax, %eax
	testq	%rcx, %rcx
	je	.L1040
	movzbl	-1(%r9,%rcx), %eax
.L1040:
	movzwl	2358(%rdi,%rax,2), %eax
	andw	2870(%rdi,%rdx,2), %ax
	addl	%edx, %ebx
	movzwl	%ax, %eax
	addl	%ebx, %eax
	movq	8(%rsp), %rbx
	movl	%eax, %eax
	leaq	(%rbx,%rax,8), %rax
.L1033:
	movl	%esi, %ebp
	movq	(%rax), %rbx
	shrl	$12, %esi
	andl	$4095, %ebp
	movl	%ebp, %eax
	movzbl	(%rbx,%rax), %edx
	movb	%dl, (%r9,%rcx)
	movzwl	4096(%rbx,%rdx,4), %eax
	movzwl	4098(%rbx,%rdx,4), %edx
	imull	%esi, %eax
	subl	%edx, %eax
	leal	0(%rbp,%rax), %esi
	cmpl	$65535, %esi
	ja	.L1041
	movq	32(%rsp), %rax
	subq	%r15, %rax
	cmpq	$1, %rax
	jle	.L1101
	movzwl	(%r15), %eax
	sall	$16, %esi
	addq	$2, %r15
	orl	%eax, %esi
.L1041:
	addq	$1, %rcx
	cmpq	%rcx, %r8
	je	.L1032
.L1042:
	cmpl	$1, 48(%rdi)
	movq	8(%rsp), %rax
	je	.L1033
	movl	28(%rdi), %ebx
	leaq	(%r11,%rcx), %rax
	xorl	%edx, %edx
	divq	%r10
	testl	%ebx, %ebx
	jne	.L1370
	xorl	%eax, %eax
	jmp	.L1034
	.p2align 4,,10
	.p2align 3
.L1362:
	movq	8(%rsp), %rax
	movq	(%rax), %r8
	testq	%r11, %r11
	je	.L1185
	movq	32(%rsp), %rcx
	leaq	(%r10,%r10), %rdx
	movq	%rcx, %rax
	subq	%rsi, %rax
	cmpq	%rdx, %rax
	jb	.L1184
	movq	24(%rsp), %rax
	movl	%r15d, 48(%rsp)
	xorl	%ebp, %ebp
	movq	%r11, 40(%rsp)
	leaq	(%rax,%r12), %r14
	movq	%r14, 16(%rsp)
	movq	%rdi, %r14
	.p2align 4,,10
	.p2align 3
.L1111:
	movq	%rbx, %r9
	movq	%r12, %rdi
	jmp	.L1116
	.p2align 4,,10
	.p2align 3
.L1372:
	sall	$16, %eax
	addq	$2, %rsi
	orl	%r10d, %eax
	movl	%eax, (%rdi)
.L1352:
	movq	16(%rsp), %rax
	addq	$4, %rdi
	addq	$8, %r9
	cmpq	%rax, %rdi
	je	.L1371
.L1116:
	movl	(%rdi), %eax
	movq	(%r9), %r15
	movl	%eax, %r11d
	shrl	$12, %eax
	andl	$4095, %r11d
	movl	%r11d, %r10d
	movzbl	(%r8,%r10), %r10d
	movb	%r10b, (%r15,%rbp)
	movzwl	4096(%r8,%r10,4), %r15d
	movzwl	4098(%r8,%r10,4), %r10d
	imull	%r15d, %eax
	subl	%r10d, %eax
	movzwl	(%rsi), %r10d
	addl	%r11d, %eax
	cmpl	$65535, %eax
	jbe	.L1372
	movl	%eax, (%rdi)
	jmp	.L1352
	.p2align 4,,10
	.p2align 3
.L1371:
	addq	$1, %rbp
	cmpq	%rbp, 40(%rsp)
	je	.L1345
	movq	%rcx, %rax
	subq	%rsi, %rax
	cmpq	%rdx, %rax
	jnb	.L1111
	movq	40(%rsp), %r11
	movl	48(%rsp), %r15d
	movq	%r14, %rdi
	cmpq	%r11, %rbp
	jnb	.L1109
.L1110:
	movq	24(%rsp), %rax
	movq	%r11, 16(%rsp)
	movq	%rdi, %rdx
	movl	%r15d, 40(%rsp)
	movq	32(%rsp), %r15
	leaq	(%rax,%r12), %r14
.L1119:
	movq	%rbx, %rdi
	movq	%r12, %rcx
	.p2align 4,,10
	.p2align 3
.L1118:
	movl	(%rcx), %eax
	movq	(%rdi), %r11
	movl	%eax, %r10d
	shrl	$12, %eax
	andl	$4095, %r10d
	movl	%r10d, %r9d
	movzbl	(%r8,%r9), %r9d
	movb	%r9b, (%r11,%rbp)
	movzwl	4096(%r8,%r9,4), %r11d
	movzwl	4098(%r8,%r9,4), %r9d
	imull	%r11d, %eax
	subl	%r9d, %eax
	addl	%r10d, %eax
	movl	%eax, (%rcx)
	cmpl	$65535, %eax
	ja	.L1117
	movq	%r15, %r9
	subq	%rsi, %r9
	cmpq	$1, %r9
	jle	.L1101
	movzwl	(%rsi), %r9d
	sall	$16, %eax
	addq	$2, %rsi
	orl	%r9d, %eax
	movl	%eax, (%rcx)
.L1117:
	addq	$4, %rcx
	addq	$8, %rdi
	cmpq	%r14, %rcx
	jne	.L1118
	movq	16(%rsp), %rax
	addq	$1, %rbp
	cmpq	%rax, %rbp
	jb	.L1119
	movl	40(%rsp), %r15d
	movq	%rax, %r11
	movq	%rdx, %rdi
	jmp	.L1109
.L1368:
	movq	%r9, %r10
	shrq	$3, %r10
	cmpq	$31, %rax
	jbe	.L1101
	movl	(%rdx), %r8d
	movl	%r8d, 304(%rsp)
	cmpl	$65535, %r8d
	jbe	.L1101
	movq	16(%rsp), %rax
	movl	4(%rdx), %r12d
	movl	%r14d, %r9d
	movl	$0, 432(%rsp)
	movq	%rax, 560(%rsp)
	movl	%r12d, 308(%rsp)
	cmpl	$65535, %r12d
	jbe	.L1101
	leaq	(%rax,%r10), %rbx
	xorl	%edx, %edx
	movq	%r10, %rax
	movl	8(%rsi), %r13d
	divq	%r9
	movq	%rbx, 72(%rsp)
	movq	%rbx, 568(%rsp)
	movl	%r13d, 312(%rsp)
	movl	%edx, 436(%rsp)
	cmpl	$65535, %r13d
	jbe	.L1101
	leaq	(%r10,%r10), %r11
	xorl	%edx, %edx
	addq	%r10, %rbx
	movq	%r11, %rax
	movq	%rbx, 80(%rsp)
	divq	%r9
	movl	12(%rsi), %eax
	movq	%rbx, 576(%rsp)
	movl	%eax, 56(%rsp)
	movl	%eax, 316(%rsp)
	movl	%edx, 440(%rsp)
	cmpl	$65535, %eax
	jbe	.L1101
	leaq	(%r11,%r10), %rax
	xorl	%edx, %edx
	addq	%r10, %rbx
	divq	%r9
	movl	16(%rsi), %eax
	movq	%rbx, 96(%rsp)
	movq	%rbx, 584(%rsp)
	movl	%eax, 40(%rsp)
	movl	%eax, 320(%rsp)
	movl	%edx, 444(%rsp)
	cmpl	$65535, %eax
	jbe	.L1101
	leaq	0(,%r10,4), %rcx
	xorl	%edx, %edx
	addq	%r10, %rbx
	movq	%rcx, %rax
	movq	%rbx, 120(%rsp)
	divq	%r9
	movl	20(%rsi), %eax
	movq	%rbx, 592(%rsp)
	movl	%eax, 48(%rsp)
	movl	%eax, 324(%rsp)
	movl	%edx, 448(%rsp)
	cmpl	$65535, %eax
	jbe	.L1101
	leaq	(%rcx,%r10), %rax
	xorl	%edx, %edx
	addq	%r10, %rbx
	divq	%r9
	movl	24(%rsi), %eax
	movq	%rbx, 104(%rsp)
	movq	%rbx, 600(%rsp)
	movl	%eax, 24(%rsp)
	movl	%eax, 328(%rsp)
	movl	%edx, 452(%rsp)
	cmpl	$65535, %eax
	jbe	.L1101
	leaq	(%r11,%r10), %rax
	xorl	%edx, %edx
	movl	28(%rsi), %ecx
	addq	%r10, %rbx
	addq	%rax, %rax
	movq	%rbx, 112(%rsp)
	leaq	32(%rsi), %r15
	divq	%r9
	movq	%rbx, 608(%rsp)
	movl	%ecx, 332(%rsp)
	movl	%edx, 456(%rsp)
	cmpl	$65535, %ecx
	jbe	.L1101
	leaq	0(,%r10,8), %rax
	xorl	%edx, %edx
	addq	%r10, %rbx
	subq	%r10, %rax
	movq	%rbx, 64(%rsp)
	movq	%rax, 240(%rsp)
	divq	%r9
	movq	%rbx, 616(%rsp)
	cmpl	$1, 48(%rdi)
	movl	%edx, 460(%rsp)
	movq	%rdx, %r11
	je	.L1373
	movl	56(%rdi), %eax
	leaq	2358(%rdi), %r13
	leaq	2870(%rdi), %rbp
	cmpq	%r10, %rax
	cmova	%r10, %rax
	movq	%rax, %rsi
	testq	%r10, %r10
	je	.L924
	leaq	432(%rsp), %rax
	movq	%rsi, 48(%rsp)
	xorl	%ecx, %ecx
	xorl	%edx, %edx
	movq	%rax, 16(%rsp)
	leaq	304(%rsp), %r12
	leaq	560(%rsp), %rbx
	movq	$-1, 24(%rsp)
	movq	%r10, 40(%rsp)
	movq	%r9, 72(%rsp)
	movq	%r11, 80(%rsp)
	movq	%r12, 56(%rsp)
	movl	%ecx, %r12d
.L970:
	movq	48(%rsp), %rax
	cmpq	%rax, %rdx
	jb	.L951
	movq	32(%rsp), %rax
	subq	%r15, %rax
	cmpq	$15, %rax
	jbe	.L951
	movq	56(%rsp), %rsi
	xorl	%eax, %eax
	.p2align 4,,10
	.p2align 3
.L956:
	movq	16(%rsp), %r9
	movq	(%rbx,%rax,2), %r10
	movl	(%r9,%rax), %ecx
	addq	%rdx, %r10
	movl	(%rax,%rsi), %r9d
	addl	%r12d, %ecx
	movl	%ecx, %r8d
	subl	%r14d, %r8d
	cmpl	%r14d, %ecx
	cmovnb	%r8d, %ecx
	movzbl	-1(%r10), %r8d
	andl	$4095, %r9d
	movl	%ecx, %r11d
	movzwl	0(%r13,%r8,2), %r8d
	andw	0(%rbp,%r11,2), %r8w
	movzwl	316(%rdi), %r11d
	movzwl	%r8w, %r8d
	addl	%ecx, %r11d
	leal	(%r8,%r11), %ecx
	movq	8(%rsp), %r11
	movq	(%r11,%rcx,8), %r11
	movl	%r9d, %ecx
	movzbl	(%r11,%rcx), %r8d
	movb	%r8b, (%r10)
	movl	(%rax,%rsi), %r10d
	movzwl	4096(%r11,%r8,4), %ecx
	movzwl	4098(%r11,%r8,4), %r8d
	shrl	$12, %r10d
	imull	%r10d, %ecx
	subl	%r8d, %ecx
	movzwl	(%r15), %r8d
	addl	%r9d, %ecx
	cmpl	$65535, %ecx
	ja	.L953
	sall	$16, %ecx
	addq	$4, %rax
	addq	$2, %r15
	orl	%r8d, %ecx
	movl	%ecx, -4(%rax,%rsi)
	cmpq	$32, %rax
	jne	.L956
.L955:
	addl	$1, %r12d
	addq	$1, 24(%rsp)
	addq	$1, %rdx
	cmpl	%r14d, %r12d
	je	.L967
	cmpq	%rdx, 40(%rsp)
	jne	.L970
.L1338:
	movq	40(%rsp), %r10
	movq	72(%rsp), %r9
	movq	80(%rsp), %r11
	movl	332(%rsp), %ecx
.L924:
	movq	88(%rsp), %rsi
	movq	240(%rsp), %rax
	subq	%rax, %rsi
	cmpq	%rsi, %r10
	jnb	.L971
	movq	64(%rsp), %r8
	movq	8(%rsp), %rbx
	jmp	.L981
.L1374:
	movl	32(%rdi), %r12d
	xorl	%eax, %eax
	cmpq	%r12, %r10
	jb	.L974
	movq	%r10, %rax
	subq	%r12, %rax
	movzbl	(%r8,%rax), %eax
	movzbl	60(%rdi,%rax), %eax
.L974:
	cmpl	$1, %ebp
	je	.L973
	movl	36(%rdi), %r12d
	cmpq	%r12, %r10
	jb	.L976
	movq	%r10, %r13
	subq	%r12, %r13
	movzbl	(%r8,%r13), %r12d
	movzbl	60(%rdi,%r12), %r12d
	addl	%r12d, %eax
.L976:
	cmpl	$2, %ebp
	je	.L973
	movl	40(%rdi), %r12d
	cmpq	%r12, %r10
	jb	.L977
	movq	%r10, %r13
	subq	%r12, %r13
	movzbl	(%r8,%r13), %r12d
	movzbl	60(%rdi,%r12), %r12d
	addl	%r12d, %eax
.L977:
	cmpl	$3, %ebp
	je	.L973
	movl	44(%rdi), %ebp
	cmpq	%rbp, %r10
	jb	.L973
	movq	%r10, %r12
	subq	%rbp, %r12
	movzbl	(%r8,%r12), %ebp
	movzbl	60(%rdi,%rbp), %ebp
	addl	%ebp, %eax
.L973:
	movzwl	316(%rdi,%rax,2), %ebp
	xorl	%eax, %eax
	testq	%r10, %r10
	je	.L979
	movzbl	-1(%r8,%r10), %eax
.L979:
	movzwl	2358(%rdi,%rax,2), %eax
	andw	2870(%rdi,%rdx,2), %ax
	addl	%edx, %ebp
	movzwl	%ax, %eax
	addl	%ebp, %eax
	movl	%eax, %eax
	leaq	(%rbx,%rax,8), %rax
.L972:
	movl	%ecx, %ebp
	movq	(%rax), %r12
	shrl	$12, %ecx
	andl	$4095, %ebp
	movl	%ebp, %eax
	movzbl	(%r12,%rax), %edx
	movb	%dl, (%r8,%r10)
	movzwl	4096(%r12,%rdx,4), %eax
	movzwl	4098(%r12,%rdx,4), %edx
	imull	%ecx, %eax
	subl	%edx, %eax
	leal	0(%rbp,%rax), %ecx
	cmpl	$65535, %ecx
	ja	.L980
	movq	32(%rsp), %rax
	subq	%r15, %rax
	cmpq	$1, %rax
	jle	.L1101
	movzwl	(%r15), %eax
	sall	$16, %ecx
	addq	$2, %r15
	orl	%eax, %ecx
.L980:
	addq	$1, %r10
	cmpq	%r10, %rsi
	je	.L971
.L981:
	cmpl	$1, 48(%rdi)
	movq	%rbx, %rax
	je	.L972
	movl	28(%rdi), %ebp
	leaq	(%r11,%r10), %rax
	xorl	%edx, %edx
	divq	%r9
	testl	%ebp, %ebp
	jne	.L1374
	xorl	%eax, %eax
	jmp	.L973
.L1345:
	movl	48(%rsp), %r15d
	movq	40(%rsp), %r11
	movq	%r14, %rdi
	jmp	.L1109
.L1373:
	movq	8(%rsp), %rax
	movq	(%rax), %rax
	testq	%r10, %r10
	je	.L924
	movq	32(%rsp), %rdx
	xorl	%esi, %esi
	subq	%r15, %rdx
	cmpq	$15, %rdx
	jbe	.L1337
	movq	%r10, 232(%rsp)
	movl	40(%rsp), %r10d
	movq	%r9, 248(%rsp)
	movl	24(%rsp), %r9d
	movq	%rdi, 256(%rsp)
	movl	56(%rsp), %edi
	movq	%r11, 264(%rsp)
	movl	48(%rsp), %r11d
	jmp	.L925
.L936:
	movq	32(%rsp), %rdx
	subq	%r15, %rdx
	cmpq	$15, %rdx
	jbe	.L927
.L925:
	movl	%r8d, %ebx
	movq	16(%rsp), %r14
	shrl	$12, %r8d
	andl	$4095, %ebx
	movl	%ebx, %edx
	movzbl	(%rax,%rdx), %edx
	movb	%dl, (%r14,%rsi)
	movzwl	4096(%rax,%rdx,4), %ebp
	movzwl	4098(%rax,%rdx,4), %edx
	imull	%ebp, %r8d
	subl	%edx, %r8d
	movzbl	(%r15), %edx
	addl	%ebx, %r8d
	movzbl	1(%r15), %ebx
	cmpl	$65535, %r8d
	setbe	%bpl
	ja	.L928
	sall	$8, %ebx
	sall	$16, %r8d
	orl	%ebx, %edx
	orl	%edx, %r8d
.L928:
	leaq	(%rbp,%rbp), %rdx
	movl	%r12d, %ebx
	shrl	$12, %r12d
	andl	$4095, %ebx
	andl	$2, %edx
	addq	%r15, %rdx
	movl	%ebx, %ebp
	movq	72(%rsp), %r15
	movzbl	(%rax,%rbp), %r14d
	leaq	(%r15,%rsi), %rbp
	movq	%rbp, 136(%rsp)
	movzbl	%r14b, %ebp
	movb	%r14b, (%r15,%rsi)
	movb	%r14b, 128(%rsp)
	movzwl	4096(%rax,%rbp,4), %r14d
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r14d, %r12d
	subl	%ebp, %r12d
	movzbl	(%rdx), %ebp
	addl	%ebx, %r12d
	movzbl	1(%rdx), %ebx
	cmpl	$65535, %r12d
	setbe	%r14b
	ja	.L929
	sall	$16, %r12d
	sall	$8, %ebx
	orl	%r12d, %ebp
	orl	%ebx, %ebp
	movl	%ebp, %r12d
.L929:
	leaq	(%r14,%r14), %rbx
	movq	80(%rsp), %r15
	andl	$2, %ebx
	addq	%rbx, %rdx
	movl	%r13d, %ebx
	shrl	$12, %r13d
	andl	$4095, %ebx
	movl	%ebx, %ebp
	movzbl	(%rax,%rbp), %r14d
	leaq	(%r15,%rsi), %rbp
	movq	%rbp, 152(%rsp)
	movzbl	%r14b, %ebp
	movb	%r14b, (%r15,%rsi)
	movb	%r14b, 144(%rsp)
	movzwl	4096(%rax,%rbp,4), %r14d
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r14d, %r13d
	subl	%ebp, %r13d
	movzbl	(%rdx), %ebp
	addl	%ebx, %r13d
	movzbl	1(%rdx), %ebx
	cmpl	$65535, %r13d
	setbe	%r14b
	ja	.L930
	sall	$16, %r13d
	sall	$8, %ebx
	orl	%r13d, %ebp
	orl	%ebx, %ebp
	movl	%ebp, %r13d
.L930:
	leaq	(%r14,%r14), %rbx
	movq	96(%rsp), %r15
	andl	$2, %ebx
	addq	%rbx, %rdx
	movl	%edi, %ebx
	shrl	$12, %edi
	andl	$4095, %ebx
	movl	%ebx, %ebp
	movzbl	(%rax,%rbp), %r14d
	leaq	(%r15,%rsi), %rbp
	movq	%rbp, 168(%rsp)
	movzbl	%r14b, %ebp
	movb	%r14b, (%r15,%rsi)
	movb	%r14b, 164(%rsp)
	movzwl	4096(%rax,%rbp,4), %r14d
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r14d, %edi
	subl	%ebp, %edi
	movzbl	(%rdx), %ebp
	addl	%ebx, %edi
	movzbl	1(%rdx), %ebx
	cmpl	$65535, %edi
	setbe	%r14b
	ja	.L931
	sall	$16, %edi
	sall	$8, %ebx
	orl	%edi, %ebp
	movl	%ebp, %edi
	orl	%ebx, %edi
.L931:
	leaq	(%r14,%r14), %rbx
	movq	120(%rsp), %r15
	andl	$2, %ebx
	addq	%rbx, %rdx
	movl	%r10d, %ebx
	shrl	$12, %r10d
	andl	$4095, %ebx
	movl	%ebx, %ebp
	movzbl	(%rax,%rbp), %r14d
	leaq	(%r15,%rsi), %rbp
	movq	%rbp, 184(%rsp)
	movzbl	%r14b, %ebp
	movb	%r14b, (%r15,%rsi)
	movb	%r14b, 176(%rsp)
	movzwl	4096(%rax,%rbp,4), %r14d
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r14d, %r10d
	subl	%ebp, %r10d
	movzbl	(%rdx), %ebp
	addl	%ebx, %r10d
	movzbl	1(%rdx), %ebx
	cmpl	$65535, %r10d
	setbe	%r14b
	ja	.L932
	sall	$16, %r10d
	sall	$8, %ebx
	orl	%r10d, %ebp
	movl	%ebp, %r10d
	orl	%ebx, %r10d
.L932:
	leaq	(%r14,%r14), %rbx
	movq	104(%rsp), %r15
	andl	$2, %ebx
	addq	%rbx, %rdx
	movl	%r11d, %ebx
	shrl	$12, %r11d
	andl	$4095, %ebx
	movl	%ebx, %ebp
	movzbl	(%rax,%rbp), %r14d
	leaq	(%r15,%rsi), %rbp
	movq	%rbp, 200(%rsp)
	movzbl	%r14b, %ebp
	movb	%r14b, (%r15,%rsi)
	movb	%r14b, 192(%rsp)
	movzwl	4096(%rax,%rbp,4), %r14d
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r14d, %r11d
	subl	%ebp, %r11d
	movzbl	(%rdx), %ebp
	addl	%ebx, %r11d
	movzbl	1(%rdx), %ebx
	cmpl	$65535, %r11d
	setbe	%r14b
	ja	.L933
	sall	$16, %r11d
	sall	$8, %ebx
	orl	%r11d, %ebp
	movl	%ebp, %r11d
	orl	%ebx, %r11d
.L933:
	leaq	(%r14,%r14), %rbx
	movq	112(%rsp), %r15
	andl	$2, %ebx
	addq	%rbx, %rdx
	movl	%r9d, %ebx
	shrl	$12, %r9d
	andl	$4095, %ebx
	movl	%ebx, %ebp
	movzbl	(%rax,%rbp), %r14d
	leaq	(%r15,%rsi), %rbp
	movq	%rbp, 216(%rsp)
	movzbl	%r14b, %ebp
	movb	%r14b, (%r15,%rsi)
	movb	%r14b, 208(%rsp)
	movzwl	4096(%rax,%rbp,4), %r14d
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r14d, %r9d
	subl	%ebp, %r9d
	movzbl	(%rdx), %ebp
	addl	%ebx, %r9d
	movzbl	1(%rdx), %ebx
	cmpl	$65535, %r9d
	setbe	%r14b
	ja	.L934
	sall	$16, %r9d
	sall	$8, %ebx
	orl	%r9d, %ebp
	movl	%ebp, %r9d
	orl	%ebx, %r9d
.L934:
	leaq	(%r14,%r14), %rbx
	movq	64(%rsp), %r14
	andl	$2, %ebx
	addq	%rbx, %rdx
	movl	%ecx, %ebx
	leaq	(%r14,%rsi), %r15
	shrl	$12, %ecx
	andl	$4095, %ebx
	movq	%r15, 224(%rsp)
	movl	%ebx, %ebp
	movzbl	(%rax,%rbp), %ebp
	movb	%bpl, (%r14,%rsi)
	movzbl	%bpl, %r14d
	movzwl	4096(%rax,%r14,4), %r15d
	movzwl	4098(%rax,%r14,4), %r14d
	imull	%r15d, %ecx
	subl	%r14d, %ecx
	movzbl	(%rdx), %r14d
	addl	%ebx, %ecx
	movzbl	1(%rdx), %ebx
	cmpl	$65535, %ecx
	setbe	%r15b
	ja	.L935
	sall	$16, %ecx
	sall	$8, %ebx
	orl	%ecx, %r14d
	movl	%r14d, %ecx
	orl	%ebx, %ecx
.L935:
	addq	%r15, %r15
	addq	$1, %rsi
	andl	$2, %r15d
	addq	%rdx, %r15
	cmpq	%rsi, 232(%rsp)
	jne	.L936
	movq	136(%rsp), %rax
	movzbl	128(%rsp), %ebx
	movl	%edi, 56(%rsp)
	movl	%r11d, 48(%rsp)
	movq	256(%rsp), %rdi
	movb	%bl, (%rax)
	movq	152(%rsp), %rax
	movzbl	144(%rsp), %ebx
	movl	%r9d, 24(%rsp)
	movl	%r10d, 40(%rsp)
	movq	248(%rsp), %r9
	movb	%bl, (%rax)
	movq	168(%rsp), %rax
	movzbl	164(%rsp), %ebx
	movl	%r8d, 304(%rsp)
	movl	%r12d, 308(%rsp)
	movq	264(%rsp), %r11
	movb	%bl, (%rax)
	movl	56(%rsp), %eax
	movzbl	176(%rsp), %ebx
	movl	%r13d, 312(%rsp)
	movl	%eax, 316(%rsp)
	movq	184(%rsp), %rax
	movl	%r10d, 320(%rsp)
	movb	%bl, (%rax)
	movq	200(%rsp), %rax
	movzbl	192(%rsp), %ebx
	movb	%bl, (%rax)
	movl	48(%rsp), %eax
	movzbl	208(%rsp), %ebx
	movl	%eax, 324(%rsp)
	movq	216(%rsp), %rax
	movb	%bl, (%rax)
	movl	24(%rsp), %eax
	movl	%ecx, 332(%rsp)
	movl	%eax, 328(%rsp)
	movq	224(%rsp), %rax
	movb	%bpl, (%rax)
.L1348:
	movq	%rsi, %r10
	jmp	.L924
	.p2align 4,,10
	.p2align 3
.L953:
	movl	%ecx, (%rax,%rsi)
	addq	$4, %rax
	cmpq	$32, %rax
	jne	.L956
	jmp	.L955
.L951:
	movq	%r13, 96(%rsp)
	movq	56(%rsp), %r9
	xorl	%r8d, %r8d
	movq	%rbp, 104(%rsp)
	jmp	.L966
	.p2align 4,,10
	.p2align 3
.L1376:
	movl	32(%rdi), %r11d
	xorl	%ecx, %ecx
	cmpq	%r11, %rdx
	jb	.L959
	movq	%rdx, %rcx
	subq	%r11, %rcx
	movzbl	(%r10,%rcx), %ecx
	movzbl	60(%rdi,%rcx), %ecx
.L959:
	cmpl	$1, %esi
	je	.L958
	movl	36(%rdi), %r11d
	cmpq	%r11, %rdx
	jb	.L961
	movq	%rdx, %rbp
	subq	%r11, %rbp
	movzbl	(%r10,%rbp), %r11d
	movzbl	60(%rdi,%r11), %r11d
	addl	%r11d, %ecx
.L961:
	cmpl	$2, %esi
	je	.L958
	movl	40(%rdi), %r11d
	cmpq	%r11, %rdx
	jb	.L962
	movq	%rdx, %rbp
	subq	%r11, %rbp
	movzbl	(%r10,%rbp), %r11d
	movzbl	60(%rdi,%r11), %r11d
	addl	%r11d, %ecx
.L962:
	cmpl	$3, %esi
	je	.L958
	movl	44(%rdi), %esi
	cmpq	%rsi, %rdx
	jb	.L958
	movq	%rdx, %r11
	subq	%rsi, %r11
	movzbl	(%r10,%r11), %esi
	movzbl	60(%rdi,%rsi), %esi
	addl	%esi, %ecx
.L958:
	movzwl	316(%rdi,%rcx,2), %ebp
	xorl	%ecx, %ecx
	testq	%rdx, %rdx
	je	.L964
	movq	24(%rsp), %rsi
	movzbl	(%r10,%rsi), %ecx
.L964:
	movl	%eax, %r13d
	movzwl	2358(%rdi,%rcx,2), %ecx
	movl	(%r9), %esi
	addl	%ebp, %eax
	andw	2870(%rdi,%r13,2), %cx
	movzwl	%cx, %ecx
	movl	%esi, %r11d
	shrl	$12, %esi
	addl	%ecx, %eax
	movq	8(%rsp), %rcx
	andl	$4095, %r11d
	movl	%eax, %eax
	movq	(%rcx,%rax,8), %rcx
	movl	%r11d, %eax
	movzbl	(%rcx,%rax), %eax
	movb	%al, (%r10,%rdx)
	movzbl	%al, %r10d
	movzwl	4096(%rcx,%r10,4), %eax
	movzwl	4098(%rcx,%r10,4), %ecx
	imull	%esi, %eax
	subl	%ecx, %eax
	addl	%r11d, %eax
	movl	%eax, (%r9)
	cmpl	$65535, %eax
	ja	.L965
	movq	32(%rsp), %rcx
	subq	%r15, %rcx
	cmpq	$1, %rcx
	jle	.L1101
	movzwl	(%r15), %ecx
	sall	$16, %eax
	addq	$2, %r15
	orl	%ecx, %eax
	movl	%eax, (%r9)
.L965:
	addq	$4, %r8
	addq	$4, %r9
	cmpq	$32, %r8
	je	.L1375
.L966:
	movq	16(%rsp), %rsi
	movq	(%rbx,%r8,2), %r10
	movl	(%rsi,%r8), %eax
	movl	28(%rdi), %esi
	addl	%r12d, %eax
	movl	%eax, %ecx
	subl	%r14d, %ecx
	cmpl	%r14d, %eax
	cmovnb	%ecx, %eax
	testl	%esi, %esi
	jne	.L1376
	xorl	%ecx, %ecx
	jmp	.L958
.L1015:
	movl	%eax, (%r8,%r12)
	addq	$4, %r8
	cmpq	$32, %r8
	jne	.L1018
	jmp	.L1349
.L1064:
	leaq	432(%rsp), %rax
	movq	%rcx, 120(%rsp)
	leaq	304(%rsp), %r9
	xorl	%r8d, %r8d
	movq	%rax, 24(%rsp)
	leaq	560(%rsp), %r13
	movq	%rbx, 128(%rsp)
	movq	%r11, 136(%rsp)
	movq	%rbp, 144(%rsp)
	jmp	.L1083
	.p2align 4,,10
	.p2align 3
.L1378:
	movl	32(%rdi), %r11d
	xorl	%ecx, %ecx
	cmpq	%r11, %rdx
	jb	.L1076
	movq	%rdx, %rcx
	subq	%r11, %rcx
	movzbl	(%r10,%rcx), %ecx
	movzbl	60(%rdi,%rcx), %ecx
.L1076:
	cmpl	$1, %esi
	je	.L1075
	movl	36(%rdi), %r11d
	cmpq	%r11, %rdx
	jb	.L1078
	movq	%rdx, %rbx
	subq	%r11, %rbx
	movzbl	(%r10,%rbx), %r11d
	movzbl	60(%rdi,%r11), %r11d
	addl	%r11d, %ecx
.L1078:
	cmpl	$2, %esi
	je	.L1075
	movl	40(%rdi), %r11d
	cmpq	%r11, %rdx
	jb	.L1079
	movq	%rdx, %rbx
	subq	%r11, %rbx
	movzbl	(%r10,%rbx), %r11d
	movzbl	60(%rdi,%r11), %r11d
	addl	%r11d, %ecx
.L1079:
	cmpl	$3, %esi
	je	.L1075
	movl	44(%rdi), %esi
	cmpq	%rsi, %rdx
	jb	.L1075
	movq	%rdx, %r11
	subq	%rsi, %r11
	movzbl	(%r10,%r11), %esi
	movzbl	60(%rdi,%rsi), %esi
	addl	%esi, %ecx
.L1075:
	movzwl	316(%rdi,%rcx,2), %ebx
	xorl	%ecx, %ecx
	testq	%rdx, %rdx
	je	.L1081
	movzbl	-1(%r10,%rdx), %ecx
.L1081:
	movl	%eax, %ebp
	movzwl	2358(%rdi,%rcx,2), %ecx
	movl	(%r9), %esi
	addl	%ebx, %eax
	andw	2870(%rdi,%rbp,2), %cx
	movq	8(%rsp), %rbx
	movzwl	%cx, %ecx
	movl	%esi, %r11d
	shrl	$12, %esi
	addl	%ecx, %eax
	andl	$4095, %r11d
	movl	%eax, %eax
	movq	(%rbx,%rax,8), %rcx
	movl	%r11d, %eax
	movzbl	(%rcx,%rax), %eax
	movb	%al, (%r10,%rdx)
	movzbl	%al, %r10d
	movzwl	4096(%rcx,%r10,4), %eax
	movzwl	4098(%rcx,%r10,4), %ecx
	imull	%esi, %eax
	subl	%ecx, %eax
	addl	%r11d, %eax
	movl	%eax, (%r9)
	cmpl	$65535, %eax
	ja	.L1082
	movq	32(%rsp), %rcx
	subq	%r15, %rcx
	cmpq	$1, %rcx
	jle	.L1101
	movzwl	(%r15), %ecx
	sall	$16, %eax
	addq	$2, %r15
	orl	%ecx, %eax
	movl	%eax, (%r9)
.L1082:
	addq	$4, %r8
	addq	$4, %r9
	cmpq	$16, %r8
	je	.L1377
.L1083:
	movq	24(%rsp), %rbx
	movl	28(%rdi), %esi
	movq	0(%r13,%r8,2), %r10
	movl	(%rbx,%r8), %eax
	addl	%r12d, %eax
	movl	%eax, %ecx
	subl	%r14d, %ecx
	cmpl	%r14d, %eax
	cmovnb	%ecx, %eax
	testl	%esi, %esi
	jne	.L1378
	xorl	%ecx, %ecx
	jmp	.L1075
.L1375:
	movq	96(%rsp), %r13
	movq	104(%rsp), %rbp
	jmp	.L955
.L1377:
	movq	120(%rsp), %rcx
	movq	128(%rsp), %rbx
	movq	136(%rsp), %r11
	movq	144(%rsp), %rbp
	jmp	.L1073
.L1363:
	movl	%ecx, 304(%rsp,%rbx,4)
	movq	8(%rsp), %r12
.L1141:
	movq	24(%rsp), %rax
	addq	%r12, %rax
	jmp	.L1152
	.p2align 4,,10
	.p2align 3
.L1380:
	addq	$4, %r12
	cmpq	%rax, %r12
	je	.L1379
.L1152:
	cmpl	$65536, (%r12)
	je	.L1380
	jmp	.L1101
.L1013:
	movq	%r13, 136(%rsp)
	movq	%r12, %r9
	xorl	%esi, %esi
	movq	16(%rsp), %r13
	movq	%rbp, 120(%rsp)
	movq	%r11, 144(%rsp)
	movq	%r12, 152(%rsp)
	jmp	.L1028
	.p2align 4,,10
	.p2align 3
.L1382:
	movl	32(%rdi), %r11d
	xorl	%ecx, %ecx
	cmpq	%r11, %rdx
	jb	.L1021
	movq	%rdx, %rcx
	subq	%r11, %rcx
	movzbl	(%r10,%rcx), %ecx
	movzbl	60(%rdi,%rcx), %ecx
.L1021:
	cmpl	$1, %r8d
	je	.L1020
	movl	36(%rdi), %r11d
	cmpq	%r11, %rdx
	jb	.L1023
	movq	%rdx, %rbp
	subq	%r11, %rbp
	movzbl	(%r10,%rbp), %r11d
	movzbl	60(%rdi,%r11), %r11d
	addl	%r11d, %ecx
.L1023:
	cmpl	$2, %r8d
	je	.L1020
	movl	40(%rdi), %r11d
	cmpq	%r11, %rdx
	jb	.L1024
	movq	%rdx, %rbp
	subq	%r11, %rbp
	movzbl	(%r10,%rbp), %r11d
	movzbl	60(%rdi,%r11), %r11d
	addl	%r11d, %ecx
.L1024:
	cmpl	$3, %r8d
	je	.L1020
	movl	44(%rdi), %r8d
	cmpq	%r8, %rdx
	jb	.L1020
	movq	%rdx, %r11
	subq	%r8, %r11
	movzbl	(%r10,%r11), %r8d
	movzbl	60(%rdi,%r8), %r8d
	addl	%r8d, %ecx
.L1020:
	movzwl	316(%rdi,%rcx,2), %ebp
	xorl	%ecx, %ecx
	testq	%rdx, %rdx
	je	.L1026
	movzbl	-1(%r10,%rdx), %ecx
.L1026:
	movl	%eax, %r12d
	movzwl	2358(%rdi,%rcx,2), %ecx
	movl	(%r9), %r8d
	addl	%ebp, %eax
	andw	2870(%rdi,%r12,2), %cx
	movzwl	%cx, %ecx
	movl	%r8d, %r11d
	shrl	$12, %r8d
	addl	%ecx, %eax
	movq	8(%rsp), %rcx
	andl	$4095, %r11d
	movl	%eax, %eax
	movq	(%rcx,%rax,8), %rcx
	movl	%r11d, %eax
	movzbl	(%rcx,%rax), %eax
	movb	%al, (%r10,%rdx)
	movzbl	%al, %r10d
	movzwl	4096(%rcx,%r10,4), %eax
	movzwl	4098(%rcx,%r10,4), %ecx
	imull	%r8d, %eax
	subl	%ecx, %eax
	addl	%r11d, %eax
	movl	%eax, (%r9)
	cmpl	$65535, %eax
	ja	.L1027
	movq	32(%rsp), %rcx
	subq	%r15, %rcx
	cmpq	$1, %rcx
	jle	.L1101
	movzwl	(%r15), %ecx
	sall	$16, %eax
	addq	$2, %r15
	orl	%ecx, %eax
	movl	%eax, (%r9)
.L1027:
	addq	$4, %rsi
	addq	$4, %r9
	cmpq	$32, %rsi
	je	.L1381
.L1028:
	movl	24(%rsp), %eax
	addl	0(%r13,%rsi), %eax
	movl	%eax, %ecx
	movl	28(%rdi), %r8d
	movq	(%rbx,%rsi,2), %r10
	subl	%r14d, %ecx
	cmpl	%r14d, %eax
	cmovnb	%ecx, %eax
	testl	%r8d, %r8d
	jne	.L1382
	xorl	%ecx, %ecx
	jmp	.L1020
.L967:
	cmpq	%rdx, 40(%rsp)
	je	.L1338
	xorl	%r12d, %r12d
	jmp	.L970
.L1084:
	cmpq	%rdx, %rcx
	je	.L1086
	xorl	%r12d, %r12d
	jmp	.L1087
.L1182:
	xorl	%eax, %eax
	jmp	.L1106
.L1379:
	xorl	%eax, %eax
	cmpq	%rsi, 32(%rsp)
	setne	%al
	negl	%eax
	jmp	.L919
.L1381:
	movq	120(%rsp), %rbp
	movq	136(%rsp), %r13
	movq	144(%rsp), %r11
	movq	152(%rsp), %r12
	jmp	.L1017
.L1185:
	xorl	%ebp, %ebp
	jmp	.L1109
.L1029:
	cmpq	%rdx, 96(%rsp)
	je	.L1341
	movl	$0, 24(%rsp)
	jmp	.L1031
.L927:
	movq	136(%rsp), %rdx
	movl	%r10d, 40(%rsp)
	movl	%edi, %ebx
	movzbl	128(%rsp), %r14d
	movl	%r11d, 48(%rsp)
	movl	%edi, 56(%rsp)
	movq	232(%rsp), %r10
	movb	%r14b, (%rdx)
	movq	152(%rsp), %rdx
	movzbl	144(%rsp), %r14d
	movl	%r9d, 24(%rsp)
	movl	%ebx, 316(%rsp)
	movq	248(%rsp), %r9
	movb	%r14b, (%rdx)
	movq	168(%rsp), %rdx
	movl	%r8d, 304(%rsp)
	movq	256(%rsp), %rdi
	movl	%r12d, 308(%rsp)
	movq	264(%rsp), %r11
	movzbl	164(%rsp), %r14d
	movl	%r13d, 312(%rsp)
	movb	%r14b, (%rdx)
	movq	184(%rsp), %rdx
	movzbl	176(%rsp), %r14d
	movb	%r14b, (%rdx)
	movl	40(%rsp), %edx
	movzbl	192(%rsp), %r14d
	movl	%edx, 320(%rsp)
	movq	200(%rsp), %rdx
	movb	%r14b, (%rdx)
	movl	48(%rsp), %r14d
	movq	216(%rsp), %rdx
	movl	%r14d, 324(%rsp)
	movzbl	208(%rsp), %r14d
	movl	24(%rsp), %ebx
	movl	%ecx, 332(%rsp)
	movb	%r14b, (%rdx)
	movl	%ebx, 328(%rsp)
	movq	224(%rsp), %rbx
	movb	%bpl, (%rbx)
	cmpq	%r10, %rsi
	jnb	.L1348
.L1337:
	movq	%r10, 128(%rsp)
	movq	32(%rsp), %rdx
	movq	%r9, 136(%rsp)
	movl	24(%rsp), %r9d
	movq	%rdi, 144(%rsp)
	movl	56(%rsp), %edi
	movq	%r11, 152(%rsp)
	movl	40(%rsp), %r11d
.L949:
	movl	%r8d, %r14d
	shrl	$12, %r8d
	andl	$4095, %r14d
	movl	%r14d, %ebx
	movzbl	(%rax,%rbx), %ebp
	movq	16(%rsp), %rbx
	movb	%bpl, (%rbx,%rsi)
	movzwl	4096(%rax,%rbp,4), %ebx
	imull	%r8d, %ebx
	movzwl	4098(%rax,%rbp,4), %r8d
	subl	%r8d, %ebx
	leal	(%rbx,%r14), %r8d
	cmpl	$65535, %r8d
	ja	.L938
	movq	%rdx, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	1(%r15), %ebp
	movzbl	(%r15), %ebx
	sall	$16, %r8d
	addq	$2, %r15
	sall	$8, %ebp
	orl	%ebp, %ebx
	orl	%ebx, %r8d
.L938:
	movl	%r12d, %ebp
	movq	72(%rsp), %r14
	shrl	$12, %r12d
	andl	$4095, %ebp
	movl	%ebp, %ebx
	movzbl	(%rax,%rbx), %ebx
	movb	%bl, (%r14,%rsi)
	movzwl	4096(%rax,%rbx,4), %r14d
	movzwl	4098(%rax,%rbx,4), %ebx
	imull	%r14d, %r12d
	subl	%ebx, %r12d
	addl	%ebp, %r12d
	cmpl	$65535, %r12d
	ja	.L941
	movq	%rdx, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %r12d
	addq	$2, %r15
	orl	%ebx, %r12d
	movzbl	-1(%r15), %ebx
	sall	$8, %ebx
	orl	%ebx, %r12d
.L941:
	movl	%r13d, %ebp
	movq	80(%rsp), %r14
	shrl	$12, %r13d
	andl	$4095, %ebp
	movl	%ebp, %ebx
	movzbl	(%rax,%rbx), %ebx
	movb	%bl, (%r14,%rsi)
	movzwl	4096(%rax,%rbx,4), %r14d
	movzwl	4098(%rax,%rbx,4), %ebx
	imull	%r14d, %r13d
	subl	%ebx, %r13d
	addl	%ebp, %r13d
	cmpl	$65535, %r13d
	ja	.L943
	movq	%rdx, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %r13d
	addq	$2, %r15
	orl	%ebx, %r13d
	movzbl	-1(%r15), %ebx
	sall	$8, %ebx
	orl	%ebx, %r13d
.L943:
	movl	%edi, %r14d
	shrl	$12, %edi
	andl	$4095, %r14d
	movl	%r14d, %ebx
	movzbl	(%rax,%rbx), %ebp
	movq	96(%rsp), %rbx
	movb	%bpl, (%rbx,%rsi)
	movzwl	4096(%rax,%rbp,4), %ebx
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%edi, %ebx
	subl	%ebp, %ebx
	leal	(%rbx,%r14), %edi
	cmpl	$65535, %edi
	ja	.L944
	movq	%rdx, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %edi
	addq	$2, %r15
	movl	%edi, %ebp
	orl	%ebx, %ebp
	movzbl	-1(%r15), %ebx
	movl	%ebp, %edi
	sall	$8, %ebx
	orl	%ebx, %edi
.L944:
	movl	%r11d, %r14d
	shrl	$12, %r11d
	andl	$4095, %r14d
	movl	%r14d, %ebx
	movzbl	(%rax,%rbx), %ebp
	movq	120(%rsp), %rbx
	movb	%bpl, (%rbx,%rsi)
	movzwl	4096(%rax,%rbp,4), %ebx
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r11d, %ebx
	subl	%ebp, %ebx
	leal	(%rbx,%r14), %r11d
	cmpl	$65535, %r11d
	ja	.L945
	movq	%rdx, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %r11d
	addq	$2, %r15
	movl	%r11d, %ebp
	orl	%ebx, %ebp
	movzbl	-1(%r15), %ebx
	movl	%ebp, %r11d
	sall	$8, %ebx
	orl	%ebx, %r11d
.L945:
	movl	48(%rsp), %r10d
	movl	%r10d, %r14d
	shrl	$12, %r10d
	andl	$4095, %r14d
	movl	%r14d, %ebx
	movzbl	(%rax,%rbx), %ebp
	movq	104(%rsp), %rbx
	movb	%bpl, (%rbx,%rsi)
	movzwl	4096(%rax,%rbp,4), %ebx
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r10d, %ebx
	subl	%ebp, %ebx
	addl	%ebx, %r14d
	movl	%r14d, 48(%rsp)
	cmpl	$65535, %r14d
	ja	.L946
	movq	%rdx, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %r14d
	addq	$2, %r15
	movl	%r14d, %ebp
	orl	%ebx, %ebp
	movzbl	-1(%r15), %ebx
	sall	$8, %ebx
	orl	%ebx, %ebp
	movl	%ebp, 48(%rsp)
.L946:
	movl	%r9d, %r14d
	shrl	$12, %r9d
	andl	$4095, %r14d
	movl	%r14d, %ebx
	movzbl	(%rax,%rbx), %ebp
	movq	112(%rsp), %rbx
	movb	%bpl, (%rbx,%rsi)
	movzwl	4096(%rax,%rbp,4), %ebx
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r9d, %ebx
	subl	%ebp, %ebx
	leal	(%rbx,%r14), %r9d
	cmpl	$65535, %r9d
	ja	.L947
	movq	%rdx, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %r9d
	addq	$2, %r15
	movl	%r9d, %ebp
	orl	%ebx, %ebp
	movzbl	-1(%r15), %ebx
	movl	%ebp, %r9d
	sall	$8, %ebx
	orl	%ebx, %r9d
.L947:
	movl	%ecx, %ebp
	movq	64(%rsp), %r14
	shrl	$12, %ecx
	andl	$4095, %ebp
	movl	%ebp, %ebx
	movzbl	(%rax,%rbx), %ebx
	movb	%bl, (%r14,%rsi)
	movzwl	4096(%rax,%rbx,4), %r14d
	movzwl	4098(%rax,%rbx,4), %ebx
	imull	%r14d, %ecx
	subl	%ebx, %ecx
	addl	%ebp, %ecx
	cmpl	$65535, %ecx
	ja	.L948
	movq	%rdx, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %ecx
	addq	$2, %r15
	orl	%ebx, %ecx
	movzbl	-1(%r15), %ebx
	sall	$8, %ebx
	orl	%ebx, %ecx
.L948:
	movq	128(%rsp), %rbx
	addq	$1, %rsi
	cmpq	%rbx, %rsi
	jb	.L949
	movl	%r9d, 24(%rsp)
	movl	24(%rsp), %eax
	movl	%r11d, 40(%rsp)
	movq	136(%rsp), %r9
	movl	%eax, 328(%rsp)
	movl	48(%rsp), %eax
	movl	%edi, 56(%rsp)
	movq	152(%rsp), %r11
	movl	%eax, 324(%rsp)
	movl	40(%rsp), %eax
	movl	%r8d, 304(%rsp)
	movq	144(%rsp), %rdi
	movl	%eax, 320(%rsp)
	movl	56(%rsp), %eax
	movl	%ecx, 332(%rsp)
	movl	%eax, 316(%rsp)
	movl	%r13d, 312(%rsp)
	movl	%r12d, 308(%rsp)
	jmp	.L1348
.L1360:
	movq	8(%rsp), %rax
	movq	(%rax), %rax
	testq	%rcx, %rcx
	je	.L1045
	movq	32(%rsp), %rsi
	xorl	%edx, %edx
	subq	%r15, %rsi
	cmpq	$7, %rsi
	jbe	.L1343
	movq	%rbp, 104(%rsp)
	movq	56(%rsp), %rbp
	movq	%r11, 80(%rsp)
	movq	%rdi, 96(%rsp)
	jmp	.L1046
.L1053:
	movq	32(%rsp), %rsi
	subq	%r15, %rsi
	cmpq	$7, %rsi
	jbe	.L1048
.L1046:
	movl	%r12d, %edi
	movq	16(%rsp), %rbx
	shrl	$12, %r12d
	andl	$4095, %edi
	movl	%edi, %esi
	movzbl	(%rax,%rsi), %esi
	movb	%sil, (%rbx,%rdx)
	movzwl	4096(%rax,%rsi,4), %r10d
	movzwl	4098(%rax,%rsi,4), %esi
	imull	%r10d, %r12d
	subl	%esi, %r12d
	movzbl	(%r15), %esi
	addl	%edi, %r12d
	movzbl	1(%r15), %edi
	cmpl	$65535, %r12d
	setbe	%r10b
	ja	.L1049
	sall	$8, %edi
	sall	$16, %r12d
	orl	%edi, %esi
	orl	%esi, %r12d
.L1049:
	leaq	(%r10,%r10), %rsi
	movq	48(%rsp), %rbx
	andl	$2, %esi
	leaq	(%r15,%rsi), %r10
	movl	%r13d, %esi
	leaq	(%rbx,%rdx), %r14
	shrl	$12, %r13d
	andl	$4095, %esi
	movq	%r14, 24(%rsp)
	movl	%esi, %edi
	movzbl	(%rax,%rdi), %r11d
	movb	%r11b, (%rbx,%rdx)
	movq	%r11, %rdi
	movzwl	4096(%rax,%r11,4), %ebx
	movzwl	4098(%rax,%r11,4), %r11d
	imull	%ebx, %r13d
	subl	%r11d, %r13d
	movzbl	(%r10), %r11d
	addl	%esi, %r13d
	movzbl	1(%r10), %esi
	cmpl	$65535, %r13d
	setbe	%bl
	ja	.L1050
	sall	$16, %r13d
	sall	$8, %esi
	orl	%r13d, %r11d
	orl	%esi, %r11d
	movl	%r11d, %r13d
.L1050:
	leaq	(%rbx,%rbx), %rsi
	movq	40(%rsp), %rbx
	andl	$2, %esi
	addq	%rsi, %r10
	movl	%r9d, %esi
	leaq	(%rbx,%rdx), %r14
	shrl	$12, %r9d
	andl	$4095, %esi
	movq	%r14, 64(%rsp)
	movl	%esi, %r11d
	movzbl	(%rax,%r11), %r11d
	movb	%r11b, (%rbx,%rdx)
	movzbl	%r11b, %ebx
	movzwl	4096(%rax,%rbx,4), %r14d
	movzwl	4098(%rax,%rbx,4), %ebx
	imull	%r14d, %r9d
	subl	%ebx, %r9d
	movzbl	(%r10), %ebx
	addl	%esi, %r9d
	movzbl	1(%r10), %esi
	cmpl	$65535, %r9d
	setbe	%r14b
	ja	.L1051
	sall	$16, %r9d
	sall	$8, %esi
	orl	%r9d, %ebx
	orl	%esi, %ebx
	movl	%ebx, %r9d
.L1051:
	leaq	(%r14,%r14), %rsi
	movl	%r8d, %ebx
	leaq	0(%rbp,%rdx), %r14
	shrl	$12, %r8d
	andl	$4095, %ebx
	andl	$2, %esi
	movq	%r14, 72(%rsp)
	addq	%r10, %rsi
	movl	%ebx, %r10d
	movzbl	(%rax,%r10), %r10d
	movzbl	%r10b, %r14d
	movb	%r10b, 0(%rbp,%rdx)
	movzwl	4096(%rax,%r14,4), %r15d
	movzwl	4098(%rax,%r14,4), %r14d
	imull	%r15d, %r8d
	subl	%r14d, %r8d
	movzbl	(%rsi), %r14d
	addl	%ebx, %r8d
	movzbl	1(%rsi), %ebx
	cmpl	$65535, %r8d
	setbe	%r15b
	ja	.L1052
	sall	$16, %r8d
	sall	$8, %ebx
	orl	%r8d, %r14d
	orl	%ebx, %r14d
	movl	%r14d, %r8d
.L1052:
	addq	%r15, %r15
	addq	$1, %rdx
	andl	$2, %r15d
	addq	%rsi, %r15
	cmpq	%rdx, %rcx
	jne	.L1053
	movq	24(%rsp), %rax
	movl	%edi, %ebx
	movl	%r11d, %esi
	movd	%r9d, %xmm1
	movd	%r8d, %xmm7
	movd	%r12d, %xmm0
	movd	%r13d, %xmm3
	movq	80(%rsp), %r11
	movb	%bl, (%rax)
	movq	64(%rsp), %rax
	punpckldq	%xmm7, %xmm1
	punpckldq	%xmm3, %xmm0
	punpcklqdq	%xmm1, %xmm0
	movq	96(%rsp), %rdi
	movq	104(%rsp), %rbp
	movb	%sil, (%rax)
	movq	72(%rsp), %rax
	movaps	%xmm0, 304(%rsp)
	movb	%r10b, (%rax)
.L1351:
	movq	%rdx, %rcx
	jmp	.L1045
.L1048:
	movq	24(%rsp), %rbx
	movl	%edi, %esi
	movl	%r11d, %r14d
	movd	%r12d, %xmm0
	movd	%r13d, %xmm3
	movd	%r9d, %xmm1
	movd	%r8d, %xmm4
	movq	80(%rsp), %r11
	movb	%sil, (%rbx)
	movq	64(%rsp), %rbx
	punpckldq	%xmm3, %xmm0
	punpckldq	%xmm4, %xmm1
	punpcklqdq	%xmm1, %xmm0
	movq	96(%rsp), %rdi
	movq	104(%rsp), %rbp
	movb	%r14b, (%rbx)
	movq	72(%rsp), %rbx
	movaps	%xmm0, 304(%rsp)
	movb	%r10b, (%rbx)
	cmpq	%rcx, %rdx
	jnb	.L1351
.L1343:
	movq	32(%rsp), %r10
.L1062:
	movl	%r12d, %ebx
	shrl	$12, %r12d
	andl	$4095, %ebx
	movl	%ebx, %esi
	movzbl	(%rax,%rsi), %r14d
	movq	16(%rsp), %rsi
	movb	%r14b, (%rsi,%rdx)
	movzwl	4096(%rax,%r14,4), %esi
	imull	%r12d, %esi
	movzwl	4098(%rax,%r14,4), %r12d
	subl	%r12d, %esi
	leal	(%rsi,%rbx), %r12d
	cmpl	$65535, %r12d
	ja	.L1055
	movq	%r10, %rsi
	subq	%r15, %rsi
	cmpq	$1, %rsi
	jle	.L1101
	movzbl	1(%r15), %esi
	movzbl	(%r15), %ebx
	sall	$16, %r12d
	addq	$2, %r15
	sall	$8, %esi
	orl	%ebx, %esi
	orl	%esi, %r12d
.L1055:
	movl	%r13d, %ebx
	shrl	$12, %r13d
	andl	$4095, %ebx
	movl	%ebx, %esi
	movzbl	(%rax,%rsi), %r14d
	movq	48(%rsp), %rsi
	movb	%r14b, (%rsi,%rdx)
	movzwl	4096(%rax,%r14,4), %esi
	imull	%r13d, %esi
	movzwl	4098(%rax,%r14,4), %r13d
	subl	%r13d, %esi
	leal	(%rsi,%rbx), %r13d
	cmpl	$65535, %r13d
	ja	.L1058
	movq	%r10, %rsi
	subq	%r15, %rsi
	cmpq	$1, %rsi
	jle	.L1101
	movzbl	(%r15), %esi
	sall	$16, %r13d
	addq	$2, %r15
	orl	%esi, %r13d
	movzbl	-1(%r15), %esi
	sall	$8, %esi
	orl	%esi, %r13d
.L1058:
	movl	%r9d, %ebx
	shrl	$12, %r9d
	andl	$4095, %ebx
	movl	%ebx, %esi
	movzbl	(%rax,%rsi), %r14d
	movq	40(%rsp), %rsi
	movb	%r14b, (%rsi,%rdx)
	movzwl	4096(%rax,%r14,4), %esi
	imull	%r9d, %esi
	movzwl	4098(%rax,%r14,4), %r9d
	subl	%r9d, %esi
	leal	(%rsi,%rbx), %r9d
	cmpl	$65535, %r9d
	ja	.L1060
	movq	%r10, %rsi
	subq	%r15, %rsi
	cmpq	$1, %rsi
	jle	.L1101
	movzbl	(%r15), %esi
	sall	$16, %r9d
	addq	$2, %r15
	orl	%esi, %r9d
	movzbl	-1(%r15), %esi
	sall	$8, %esi
	orl	%esi, %r9d
.L1060:
	movl	%r8d, %ebx
	shrl	$12, %r8d
	andl	$4095, %ebx
	movl	%ebx, %esi
	movzbl	(%rax,%rsi), %r14d
	movq	56(%rsp), %rsi
	movb	%r14b, (%rsi,%rdx)
	movzwl	4096(%rax,%r14,4), %esi
	imull	%r8d, %esi
	movzwl	4098(%rax,%r14,4), %r8d
	subl	%r8d, %esi
	leal	(%rsi,%rbx), %r8d
	cmpl	$65535, %r8d
	ja	.L1061
	movq	%r10, %rsi
	subq	%r15, %rsi
	cmpq	$1, %rsi
	jle	.L1101
	movzbl	(%r15), %esi
	sall	$16, %r8d
	addq	$2, %r15
	orl	%esi, %r8d
	movzbl	-1(%r15), %esi
	sall	$8, %esi
	orl	%esi, %r8d
.L1061:
	addq	$1, %rdx
	cmpq	%rcx, %rdx
	jb	.L1062
	movd	%r9d, %xmm1
	movd	%r8d, %xmm5
	movd	%r12d, %xmm0
	movd	%r13d, %xmm6
	punpckldq	%xmm5, %xmm1
	punpckldq	%xmm6, %xmm0
	punpcklqdq	%xmm1, %xmm0
	movaps	%xmm0, 304(%rsp)
	jmp	.L1351
.L1184:
	xorl	%ebp, %ebp
	jmp	.L1110
.L971:
	cmpl	$65536, 304(%rsp)
	jne	.L1101
	cmpl	$65536, 308(%rsp)
	jne	.L1101
	cmpl	$65536, 312(%rsp)
	jne	.L1101
	cmpl	$65536, 316(%rsp)
	jne	.L1101
	cmpl	$65536, 320(%rsp)
	jne	.L1101
	cmpl	$65536, 324(%rsp)
	jne	.L1101
	cmpl	$65536, 328(%rsp)
	jne	.L1101
	cmpl	$65536, %ecx
	jne	.L1101
.L1354:
	xorl	%eax, %eax
	cmpq	%r15, 32(%rsp)
	setne	%al
	negl	%eax
	jmp	.L919
.L1369:
	movq	8(%rsp), %rax
	movq	(%rax), %rax
	testq	%rcx, %rcx
	je	.L986
	movq	32(%rsp), %rsi
	xorl	%edx, %edx
	subq	%r15, %rsi
	cmpq	$15, %rsi
	jbe	.L1340
	movq	%r10, 240(%rsp)
	movl	56(%rsp), %r10d
	movq	%rdi, 248(%rsp)
	movl	24(%rsp), %edi
	movq	%r11, 256(%rsp)
	movl	40(%rsp), %r11d
	movq	%rcx, 232(%rsp)
	jmp	.L987
.L998:
	movq	32(%rsp), %rsi
	subq	%r15, %rsi
	cmpq	$15, %rsi
	jbe	.L989
.L987:
	movl	%r9d, %ebx
	movq	16(%rsp), %rcx
	shrl	$12, %r9d
	andl	$4095, %ebx
	movl	%ebx, %esi
	movzbl	(%rax,%rsi), %esi
	movb	%sil, (%rcx,%rdx)
	movzwl	4096(%rax,%rsi,4), %ebp
	movzwl	4098(%rax,%rsi,4), %esi
	imull	%ebp, %r9d
	subl	%esi, %r9d
	movzbl	(%r15), %esi
	addl	%ebx, %r9d
	movzbl	1(%r15), %ebx
	cmpl	$65535, %r9d
	setbe	%bpl
	ja	.L990
	sall	$8, %ebx
	sall	$16, %r9d
	orl	%ebx, %esi
	orl	%esi, %r9d
.L990:
	leaq	(%rbp,%rbp), %rsi
	movq	224(%rsp), %r14
	andl	$2, %esi
	leaq	(%r15,%rsi), %rbx
	movl	%r8d, %esi
	leaq	(%r14,%rdx), %r15
	shrl	$12, %r8d
	andl	$4095, %esi
	movq	%r15, 72(%rsp)
	movl	%esi, %ebp
	movzbl	(%rax,%rbp), %ebp
	movb	%bpl, (%r14,%rdx)
	movzwl	4096(%rax,%rbp,4), %r14d
	movb	%bpl, 64(%rsp)
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r14d, %r8d
	subl	%ebp, %r8d
	movzbl	(%rbx), %ebp
	addl	%esi, %r8d
	movzbl	1(%rbx), %esi
	cmpl	$65535, %r8d
	setbe	%r14b
	ja	.L991
	sall	$16, %r8d
	sall	$8, %esi
	orl	%r8d, %ebp
	movl	%ebp, %r8d
	orl	%esi, %r8d
.L991:
	leaq	(%r14,%r14), %rsi
	movq	216(%rsp), %r14
	andl	$2, %esi
	addq	%rsi, %rbx
	movl	%r12d, %esi
	leaq	(%r14,%rdx), %r15
	shrl	$12, %r12d
	andl	$4095, %esi
	movq	%r15, 96(%rsp)
	movl	%esi, %ebp
	movzbl	(%rax,%rbp), %ebp
	movb	%bpl, (%r14,%rdx)
	movzwl	4096(%rax,%rbp,4), %r14d
	movb	%bpl, 80(%rsp)
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r14d, %r12d
	subl	%ebp, %r12d
	movzbl	(%rbx), %ebp
	addl	%esi, %r12d
	movzbl	1(%rbx), %esi
	cmpl	$65535, %r12d
	setbe	%r14b
	ja	.L992
	sall	$16, %r12d
	sall	$8, %esi
	orl	%r12d, %ebp
	orl	%esi, %ebp
	movl	%ebp, %r12d
.L992:
	leaq	(%r14,%r14), %rsi
	movq	208(%rsp), %r14
	andl	$2, %esi
	addq	%rsi, %rbx
	movl	%r13d, %esi
	leaq	(%r14,%rdx), %r15
	shrl	$12, %r13d
	andl	$4095, %esi
	movq	%r15, 120(%rsp)
	movl	%esi, %ebp
	movzbl	(%rax,%rbp), %ebp
	movb	%bpl, (%r14,%rdx)
	movzwl	4096(%rax,%rbp,4), %r14d
	movb	%bpl, 112(%rsp)
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r14d, %r13d
	subl	%ebp, %r13d
	movzbl	(%rbx), %ebp
	addl	%esi, %r13d
	movzbl	1(%rbx), %esi
	cmpl	$65535, %r13d
	setbe	%r14b
	ja	.L993
	sall	$16, %r13d
	sall	$8, %esi
	orl	%r13d, %ebp
	orl	%esi, %ebp
	movl	%ebp, %r13d
.L993:
	leaq	(%r14,%r14), %rsi
	movl	%edi, %ebp
	movq	200(%rsp), %rcx
	andl	$4095, %ebp
	andl	$2, %esi
	addq	%rsi, %rbx
	movl	%ebp, %esi
	leaq	(%rcx,%rdx), %r14
	movzbl	(%rax,%rsi), %esi
	movq	%r14, 136(%rsp)
	movzbl	%sil, %r14d
	movb	%sil, (%rcx,%rdx)
	movb	%sil, 128(%rsp)
	movl	%edi, %esi
	movzwl	4096(%rax,%r14,4), %r15d
	movzwl	4098(%rax,%r14,4), %r14d
	shrl	$12, %esi
	imull	%r15d, %esi
	subl	%r14d, %esi
	leal	(%rsi,%rbp), %edi
	movzbl	(%rbx), %ebp
	movzbl	1(%rbx), %esi
	cmpl	$65535, %edi
	setbe	%r14b
	ja	.L994
	sall	$16, %edi
	sall	$8, %esi
	orl	%edi, %ebp
	movl	%ebp, %edi
	orl	%esi, %edi
.L994:
	leaq	(%r14,%r14), %rsi
	movl	%r11d, %ebp
	movq	192(%rsp), %rcx
	andl	$4095, %ebp
	andl	$2, %esi
	addq	%rsi, %rbx
	movl	%ebp, %esi
	leaq	(%rcx,%rdx), %r14
	movzbl	(%rax,%rsi), %esi
	movq	%r14, 152(%rsp)
	movzbl	%sil, %r14d
	movb	%sil, (%rcx,%rdx)
	movb	%sil, 144(%rsp)
	movl	%r11d, %esi
	movzwl	4096(%rax,%r14,4), %r15d
	movzwl	4098(%rax,%r14,4), %r14d
	shrl	$12, %esi
	imull	%r15d, %esi
	subl	%r14d, %esi
	leal	(%rsi,%rbp), %r11d
	movzbl	(%rbx), %ebp
	movzbl	1(%rbx), %esi
	cmpl	$65535, %r11d
	setbe	%r14b
	ja	.L995
	movl	%r11d, %r15d
	sall	$8, %esi
	sall	$16, %r15d
	orl	%r15d, %ebp
	movl	%ebp, %r11d
	orl	%esi, %r11d
.L995:
	leaq	(%r14,%r14), %rsi
	movl	%r10d, %ebp
	movq	184(%rsp), %rcx
	andl	$4095, %ebp
	andl	$2, %esi
	addq	%rsi, %rbx
	movl	%ebp, %esi
	leaq	(%rcx,%rdx), %r14
	movzbl	(%rax,%rsi), %esi
	movq	%r14, 168(%rsp)
	movzbl	%sil, %r14d
	movb	%sil, (%rcx,%rdx)
	movb	%sil, 164(%rsp)
	movl	%r10d, %esi
	movzwl	4096(%rax,%r14,4), %r15d
	movzwl	4098(%rax,%r14,4), %r14d
	shrl	$12, %esi
	imull	%r15d, %esi
	subl	%r14d, %esi
	leal	(%rsi,%rbp), %r10d
	movzbl	(%rbx), %ebp
	movzbl	1(%rbx), %esi
	cmpl	$65535, %r10d
	setbe	%r14b
	ja	.L996
	movl	%r10d, %r15d
	sall	$8, %esi
	sall	$16, %r15d
	orl	%r15d, %ebp
	movl	%ebp, %r10d
	orl	%esi, %r10d
.L996:
	movl	48(%rsp), %ecx
	leaq	(%r14,%r14), %rsi
	andl	$2, %esi
	movl	%ecx, %r15d
	addq	%rbx, %rsi
	shrl	$12, %ecx
	andl	$4095, %r15d
	movl	%r15d, %ebx
	movzbl	(%rax,%rbx), %ebp
	movq	104(%rsp), %rbx
	leaq	(%rbx,%rdx), %r14
	movb	%bpl, (%rbx,%rdx)
	movq	%r14, 176(%rsp)
	movzbl	%bpl, %r14d
	movzwl	4096(%rax,%r14,4), %ebx
	movzwl	4098(%rax,%r14,4), %r14d
	imull	%ecx, %ebx
	subl	%r14d, %ebx
	movzbl	(%rsi), %r14d
	leal	(%rbx,%r15), %ecx
	movzbl	1(%rsi), %ebx
	cmpl	$65535, %ecx
	movl	%ecx, 48(%rsp)
	setbe	%r15b
	ja	.L997
	sall	$16, %ecx
	sall	$8, %ebx
	orl	%ecx, %r14d
	orl	%ebx, %r14d
	movl	%r14d, 48(%rsp)
.L997:
	addq	%r15, %r15
	addq	$1, %rdx
	andl	$2, %r15d
	addq	%rsi, %r15
	cmpq	%rdx, 232(%rsp)
	jne	.L998
	movq	72(%rsp), %rax
	movzbl	64(%rsp), %ebx
	movl	%edi, 24(%rsp)
	movl	%r11d, 40(%rsp)
	movq	248(%rsp), %rdi
	movb	%bl, (%rax)
	movq	96(%rsp), %rax
	movzbl	80(%rsp), %ebx
	movl	%r10d, 56(%rsp)
	movl	%r9d, 304(%rsp)
	movq	240(%rsp), %r10
	movb	%bl, (%rax)
	movq	120(%rsp), %rax
	movzbl	112(%rsp), %ebx
	movl	%r8d, 308(%rsp)
	movl	%r12d, 312(%rsp)
	movq	256(%rsp), %r11
	movb	%bl, (%rax)
	movq	136(%rsp), %rax
	movzbl	128(%rsp), %ebx
	movl	%r13d, 316(%rsp)
	movb	%bl, (%rax)
	movl	24(%rsp), %eax
	movzbl	144(%rsp), %ebx
	movl	%eax, 320(%rsp)
	movq	152(%rsp), %rax
	movb	%bl, (%rax)
	movl	40(%rsp), %eax
	movzbl	164(%rsp), %ebx
	movl	%eax, 324(%rsp)
	movq	168(%rsp), %rax
	movb	%bl, (%rax)
	movl	56(%rsp), %eax
	movl	%eax, 328(%rsp)
	movq	176(%rsp), %rax
	movb	%bpl, (%rax)
	movl	48(%rsp), %eax
	movl	%eax, 332(%rsp)
.L1350:
	movq	%rdx, %rcx
	jmp	.L986
.L989:
	movq	72(%rsp), %rsi
	movzbl	64(%rsp), %ebx
	movl	%edi, 24(%rsp)
	movl	%r10d, %r14d
	movl	%r11d, 40(%rsp)
	movq	232(%rsp), %rcx
	movb	%bl, (%rsi)
	movq	96(%rsp), %rbx
	movzbl	80(%rsp), %esi
	movl	%r10d, 56(%rsp)
	movl	%r9d, 304(%rsp)
	movq	240(%rsp), %r10
	movb	%sil, (%rbx)
	movq	120(%rsp), %rbx
	movzbl	112(%rsp), %esi
	movl	%r8d, 308(%rsp)
	movl	%r12d, 312(%rsp)
	movq	248(%rsp), %rdi
	movb	%sil, (%rbx)
	movq	136(%rsp), %rbx
	movzbl	128(%rsp), %esi
	movl	%r13d, 316(%rsp)
	movq	256(%rsp), %r11
	movb	%sil, (%rbx)
	movl	24(%rsp), %ebx
	movzbl	144(%rsp), %esi
	movl	%ebx, 320(%rsp)
	movq	152(%rsp), %rbx
	movb	%sil, (%rbx)
	movl	40(%rsp), %esi
	movq	168(%rsp), %rbx
	movl	%esi, 324(%rsp)
	movzbl	164(%rsp), %esi
	movb	%sil, (%rbx)
	movq	176(%rsp), %rbx
	movl	%r14d, 328(%rsp)
	movb	%bpl, (%rbx)
	movl	48(%rsp), %ebx
	movl	%ebx, 332(%rsp)
	cmpq	%rcx, %rdx
	jnb	.L1350
.L1340:
	movq	%rcx, 64(%rsp)
	movq	32(%rsp), %rsi
	movq	%r10, 72(%rsp)
	movl	56(%rsp), %r10d
	movq	%rdi, 80(%rsp)
	movl	24(%rsp), %edi
	movq	%r11, 96(%rsp)
	movl	40(%rsp), %r11d
.L1011:
	movl	%r9d, %ebp
	movq	16(%rsp), %rcx
	shrl	$12, %r9d
	andl	$4095, %ebp
	movl	%ebp, %ebx
	movzbl	(%rax,%rbx), %ebx
	movb	%bl, (%rcx,%rdx)
	movzwl	4096(%rax,%rbx,4), %r14d
	movzwl	4098(%rax,%rbx,4), %ebx
	imull	%r14d, %r9d
	subl	%ebx, %r9d
	addl	%ebp, %r9d
	cmpl	$65535, %r9d
	ja	.L1000
	movq	%rsi, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	1(%r15), %ebp
	movzbl	(%r15), %ebx
	sall	$16, %r9d
	addq	$2, %r15
	sall	$8, %ebp
	orl	%ebp, %ebx
	orl	%ebx, %r9d
.L1000:
	movl	%r8d, %ebp
	movq	224(%rsp), %rcx
	shrl	$12, %r8d
	andl	$4095, %ebp
	movl	%ebp, %ebx
	movzbl	(%rax,%rbx), %ebx
	movb	%bl, (%rcx,%rdx)
	movzwl	4096(%rax,%rbx,4), %r14d
	movzwl	4098(%rax,%rbx,4), %ebx
	imull	%r14d, %r8d
	subl	%ebx, %r8d
	addl	%ebp, %r8d
	cmpl	$65535, %r8d
	ja	.L1003
	movq	%rsi, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %r8d
	addq	$2, %r15
	orl	%ebx, %r8d
	movzbl	-1(%r15), %ebx
	sall	$8, %ebx
	orl	%ebx, %r8d
.L1003:
	movl	%r12d, %ebp
	movq	216(%rsp), %rcx
	shrl	$12, %r12d
	andl	$4095, %ebp
	movl	%ebp, %ebx
	movzbl	(%rax,%rbx), %ebx
	movb	%bl, (%rcx,%rdx)
	movzwl	4096(%rax,%rbx,4), %r14d
	movzwl	4098(%rax,%rbx,4), %ebx
	imull	%r14d, %r12d
	subl	%ebx, %r12d
	addl	%ebp, %r12d
	cmpl	$65535, %r12d
	ja	.L1005
	movq	%rsi, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %r12d
	addq	$2, %r15
	orl	%ebx, %r12d
	movzbl	-1(%r15), %ebx
	sall	$8, %ebx
	orl	%ebx, %r12d
.L1005:
	movl	%r13d, %ebp
	movq	208(%rsp), %rcx
	shrl	$12, %r13d
	andl	$4095, %ebp
	movl	%ebp, %ebx
	movzbl	(%rax,%rbx), %ebx
	movb	%bl, (%rcx,%rdx)
	movzwl	4096(%rax,%rbx,4), %r14d
	movzwl	4098(%rax,%rbx,4), %ebx
	imull	%r14d, %r13d
	subl	%ebx, %r13d
	addl	%ebp, %r13d
	cmpl	$65535, %r13d
	ja	.L1006
	movq	%rsi, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %r13d
	addq	$2, %r15
	orl	%ebx, %r13d
	movzbl	-1(%r15), %ebx
	sall	$8, %ebx
	orl	%ebx, %r13d
.L1006:
	movl	%edi, %r14d
	shrl	$12, %edi
	andl	$4095, %r14d
	movl	%r14d, %ebx
	movzbl	(%rax,%rbx), %ebp
	movq	200(%rsp), %rbx
	movb	%bpl, (%rbx,%rdx)
	movzwl	4096(%rax,%rbp,4), %ebx
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%edi, %ebx
	subl	%ebp, %ebx
	leal	(%rbx,%r14), %edi
	cmpl	$65535, %edi
	ja	.L1007
	movq	%rsi, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %edi
	addq	$2, %r15
	movl	%edi, %ebp
	orl	%ebx, %ebp
	movzbl	-1(%r15), %ebx
	movl	%ebp, %edi
	sall	$8, %ebx
	orl	%ebx, %edi
.L1007:
	movl	%r11d, %r14d
	shrl	$12, %r11d
	andl	$4095, %r14d
	movl	%r14d, %ebx
	movzbl	(%rax,%rbx), %ebp
	movq	192(%rsp), %rbx
	movb	%bpl, (%rbx,%rdx)
	movzwl	4096(%rax,%rbp,4), %ebx
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r11d, %ebx
	subl	%ebp, %ebx
	leal	(%rbx,%r14), %r11d
	cmpl	$65535, %r11d
	ja	.L1008
	movq	%rsi, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %r11d
	addq	$2, %r15
	movl	%r11d, %ebp
	orl	%ebx, %ebp
	movzbl	-1(%r15), %ebx
	movl	%ebp, %r11d
	sall	$8, %ebx
	orl	%ebx, %r11d
.L1008:
	movl	%r10d, %r14d
	shrl	$12, %r10d
	andl	$4095, %r14d
	movl	%r14d, %ebx
	movzbl	(%rax,%rbx), %ebp
	movq	184(%rsp), %rbx
	movb	%bpl, (%rbx,%rdx)
	movzwl	4096(%rax,%rbp,4), %ebx
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%r10d, %ebx
	subl	%ebp, %ebx
	leal	(%rbx,%r14), %r10d
	cmpl	$65535, %r10d
	ja	.L1009
	movq	%rsi, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %r10d
	addq	$2, %r15
	movl	%r10d, %ebp
	orl	%ebx, %ebp
	movzbl	-1(%r15), %ebx
	movl	%ebp, %r10d
	sall	$8, %ebx
	orl	%ebx, %r10d
.L1009:
	movl	48(%rsp), %ecx
	movl	%ecx, %r14d
	shrl	$12, %ecx
	andl	$4095, %r14d
	movl	%r14d, %ebx
	movzbl	(%rax,%rbx), %ebp
	movq	104(%rsp), %rbx
	movb	%bpl, (%rbx,%rdx)
	movzwl	4096(%rax,%rbp,4), %ebx
	movzwl	4098(%rax,%rbp,4), %ebp
	imull	%ecx, %ebx
	subl	%ebp, %ebx
	leal	(%rbx,%r14), %ecx
	movl	%ecx, 48(%rsp)
	cmpl	$65535, %ecx
	ja	.L1010
	movq	%rsi, %rbx
	subq	%r15, %rbx
	cmpq	$1, %rbx
	jle	.L1101
	movzbl	(%r15), %ebx
	sall	$16, %ecx
	addq	$2, %r15
	movl	%ecx, %ebp
	orl	%ebx, %ebp
	movzbl	-1(%r15), %ebx
	sall	$8, %ebx
	orl	%ebx, %ebp
	movl	%ebp, 48(%rsp)
.L1010:
	movq	64(%rsp), %rbx
	addq	$1, %rdx
	cmpq	%rbx, %rdx
	jb	.L1011
	movl	48(%rsp), %eax
	movl	%r10d, 56(%rsp)
	movl	%r11d, 40(%rsp)
	movq	72(%rsp), %r10
	movl	%eax, 332(%rsp)
	movl	56(%rsp), %eax
	movl	%edi, 24(%rsp)
	movq	96(%rsp), %r11
	movl	%eax, 328(%rsp)
	movl	40(%rsp), %eax
	movl	%r9d, 304(%rsp)
	movq	80(%rsp), %rdi
	movl	%eax, 324(%rsp)
	movl	24(%rsp), %eax
	movl	%r13d, 316(%rsp)
	movl	%eax, 320(%rsp)
	movl	%r12d, 312(%rsp)
	movl	%r8d, 308(%rsp)
	jmp	.L1350
.L1088:
	cmpl	$65536, 304(%rsp)
	jne	.L1101
	cmpl	$65536, 308(%rsp)
	jne	.L1101
	cmpl	$65536, 312(%rsp)
	jne	.L1101
	cmpl	$65536, %r8d
	je	.L1354
	jmp	.L1101
	.p2align 4,,10
	.p2align 3
.L1032:
	cmpl	$65536, 304(%rsp)
	jne	.L1101
	cmpl	$65536, 308(%rsp)
	jne	.L1101
	cmpl	$65536, 312(%rsp)
	jne	.L1101
	cmpl	$65536, 316(%rsp)
	jne	.L1101
	cmpl	$65536, 320(%rsp)
	jne	.L1101
	cmpl	$65536, 324(%rsp)
	jne	.L1101
	cmpl	$65536, 328(%rsp)
	jne	.L1101
	cmpl	$65536, %esi
	je	.L1354
	jmp	.L1101
.L1359:
	call	*__stack_chk_fail@GOTPCREL(%rip)
	.cfi_endproc
.LFE42:
	.size	kr_dec_syms, .-kr_dec_syms
	.p2align 4
	.globl	kolmo_krans_decompress
	.type	kolmo_krans_decompress, @function
kolmo_krans_decompress:
.LFB43:
	.cfi_startproc
	endbr64
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	movq	%rcx, %r13
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	movq	%rdi, %rbp
	movl	$2888, %edi
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	movq	%rsi, %rbx
	subq	$136, %rsp
	.cfi_def_cfa_offset 192
	movq	%rdx, 24(%rsp)
	movq	%rcx, (%rsp)
	movq	%fs:40, %rax
	movq	%rax, 120(%rsp)
	xorl	%eax, %eax
	call	*malloc@GOTPCREL(%rip)
	movl	$32768, %edi
	movq	%rax, %r15
	call	*malloc@GOTPCREL(%rip)
	movl	$4096, %edi
	movq	%rax, %r14
	movq	%rax, 32(%rsp)
	call	*malloc@GOTPCREL(%rip)
	testq	%r15, %r15
	movq	%rax, 8(%rsp)
	movq	%rax, %rsi
	sete	%al
	testq	%r14, %r14
	sete	%dl
	orb	%dl, %al
	jne	.L1427
	testq	%rsi, %rsi
	je	.L1427
	testq	%r13, %r13
	je	.L1428
	testq	%rbx, %rbx
	je	.L1430
	movzbl	0(%rbp), %edx
	cmpq	$1, %rbx
	je	.L1430
	movl	$0, 52(%rsp)
	xorl	%r13d, %r13d
	xorl	%r10d, %r10d
	movl	$1, %eax
	movq	$0, 16(%rsp)
.L1387:
	movzbl	0(%rbp,%rax), %eax
	leaq	2(%r13), %r14
	movl	%eax, %ecx
	movq	%rax, %r12
	andl	$127, %ecx
	andl	$127, %r12d
	testb	%al, %al
	jns	.L1388
	cmpq	%rbx, %r14
	jnb	.L1386
	movzbl	0(%rbp,%r14), %ecx
	leaq	3(%r13), %rsi
	movq	%rcx, %rax
	andl	$127, %eax
	salq	$7, %rax
	orq	%rax, %r12
	testb	%cl, %cl
	jns	.L1390
	cmpq	%rbx, %rsi
	jnb	.L1386
	movzbl	0(%rbp,%rsi), %ecx
	leaq	4(%r13), %r14
	movq	%rcx, %rax
	andl	$127, %eax
	salq	$14, %rax
	orq	%rax, %r12
	testb	%cl, %cl
	jns	.L1391
	cmpq	%rbx, %r14
	jnb	.L1386
	movzbl	0(%rbp,%r14), %ecx
	leaq	5(%r13), %rsi
	movq	%rcx, %rax
	andl	$127, %eax
	salq	$21, %rax
	orq	%rax, %r12
	testb	%cl, %cl
	jns	.L1431
	cmpq	%rbx, %rsi
	jnb	.L1386
	movzbl	0(%rbp,%rsi), %eax
	testb	%al, %al
	jns	.L1527
.L1386:
	movl	$-1, %ebx
.L1384:
	movq	16(%rsp), %rdi
	call	*free@GOTPCREL(%rip)
	movq	%r15, %rdi
	call	*free@GOTPCREL(%rip)
	movq	32(%rsp), %rdi
	call	*free@GOTPCREL(%rip)
	movq	8(%rsp), %rdi
	call	*free@GOTPCREL(%rip)
	movq	120(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L1528
	addq	$136, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	movl	%ebx, %eax
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L1388:
	.cfi_restore_state
	testb	%cl, %cl
	je	.L1386
	.p2align 4,,10
	.p2align 3
.L1393:
	movq	(%rsp), %rax
	subq	%r10, %rax
	cmpq	%r12, %rax
	jb	.L1386
	testb	%dl, %dl
	jne	.L1394
	movq	%rbx, %rax
	subq	%r14, %rax
	cmpq	%r12, %rax
	jb	.L1386
	movq	24(%rsp), %rax
	movq	%r10, 40(%rsp)
	leaq	0(%rbp,%r14), %rsi
	movq	%r12, %rdx
	leaq	(%r14,%r12), %r13
	leaq	(%rax,%r10), %rdi
	call	*memcpy@GOTPCREL(%rip)
	movq	40(%rsp), %r10
	addq	%r12, %r10
.L1424:
	movq	(%rsp), %rax
	cmpq	%rax, %r10
	jnb	.L1385
	cmpq	%rbx, %r13
	jnb	.L1386
	leaq	1(%r13), %rax
	movzbl	0(%rbp,%r13), %edx
	cmpq	%rbx, %rax
	jb	.L1387
	jmp	.L1386
	.p2align 4,,10
	.p2align 3
.L1394:
	cmpb	$1, %dl
	je	.L1529
	cmpb	$2, %dl
	jne	.L1386
	movq	%rbx, %rax
	subq	%r14, %rax
	cmpq	$2, %rax
	jbe	.L1386
	movzbl	0(%rbp,%r14), %eax
	pxor	%xmm0, %xmm0
	movzbl	1(%rbp,%r14), %edx
	leaq	2(%r14), %r13
	movups	%xmm0, 28(%r15)
	movl	%eax, %esi
	movl	%edx, %edi
	movl	%edx, %ecx
	movl	$0, 44(%r15)
	andl	$7, %esi
	andl	$3, %edi
	andl	$3, %ecx
	movd	%esi, %xmm0
	movl	%eax, %esi
	shrb	$6, %al
	movd	%edi, %xmm7
	movzbl	%al, %eax
	shrb	$3, %sil
	movd	%eax, %xmm1
	movl	%edx, %eax
	andl	$7, %esi
	shrb	$2, %al
	addl	$1, %esi
	punpckldq	%xmm7, %xmm1
	andl	$3, %eax
	movd	%esi, %xmm6
	movl	%eax, 16(%r15)
	movl	%edx, %eax
	punpckldq	%xmm6, %xmm0
	shrb	$4, %al
	punpcklqdq	%xmm1, %xmm0
	andl	$3, %eax
	movups	%xmm0, (%r15)
	movl	%eax, 20(%r15)
	movl	%edx, %eax
	shrb	$6, %al
	andl	$1, %eax
	movl	%eax, 24(%r15)
	testb	%dl, %dl
	js	.L1386
	cmpb	$3, %cl
	je	.L1386
	testb	%cl, %cl
	jne	.L1530
.L1398:
	movq	%r15, %rdi
	movq	%r10, 40(%rsp)
	call	kr_ctx_init
	cmpq	%rbx, %r13
	jnb	.L1386
	testl	%eax, %eax
	jne	.L1386
	movzbl	0(%rbp,%r13), %edx
	leaq	1(%r13), %rcx
	movb	%dl, 56(%rsp)
	leal	1(%rdx), %r14d
	cmpl	$255, %edx
	je	.L1386
	movl	48(%r15), %r13d
	cmpl	%r14d, %r13d
	jb	.L1386
	movq	%r12, %rax
	movl	%r14d, %esi
	shrq	$4, %rax
	addq	$1, %rax
	cmpq	%rsi, %rax
	jb	.L1386
	cmpl	%r14d, 52(%rsp)
	movq	40(%rsp), %r10
	jb	.L1531
.L1405:
	addq	%rbp, %rcx
	leaq	0(%rbp,%rbx), %r8
	movq	$0, 96(%rsp)
	movq	%rcx, 80(%rsp)
	movq	%r8, 88(%rsp)
	movq	$0, 104(%rsp)
	testl	%edx, %edx
	je	.L1532
	bsrl	%edx, %edx
	movl	$32, %r11d
	xorl	%edi, %edi
	xorl	%esi, %esi
	xorl	$31, %edx
	movl	$-1, %r9d
	movq	%r10, 64(%rsp)
	xorl	%eax, %eax
	subl	%edx, %r11d
	movq	8(%rsp), %r10
	movl	%r11d, %ecx
	sall	%cl, %r9d
	leal	-32(%rdx), %ecx
	movl	%ecx, 40(%rsp)
	notl	%r9d
	jmp	.L1408
	.p2align 4,,10
	.p2align 3
.L1534:
	leaq	1(%rdx), %rcx
	movq	%rcx, 80(%rsp)
	movzbl	(%rdx), %edx
	movl	%eax, %ecx
	salq	%cl, %rdx
	orq	%rdx, %rsi
.L1412:
	addl	$8, %eax
.L1410:
	movl	%r11d, %ecx
	movl	%r9d, %edx
	andl	%esi, %edx
	shrq	%cl, %rsi
	movl	40(%rsp), %ecx
	movq	%rsi, 96(%rsp)
	addl	%ecx, %eax
	movl	%eax, 104(%rsp)
	cmpl	%r14d, %edx
	jnb	.L1386
	movb	%dl, (%r10,%rdi)
	addq	$1, %rdi
	cmpl	%r13d, %edi
	jnb	.L1533
.L1408:
	cmpl	%r11d, %eax
	jnb	.L1410
	movq	80(%rsp), %rdx
	cmpq	%r8, %rdx
	jb	.L1534
	movl	$1, 108(%rsp)
	jmp	.L1412
	.p2align 4,,10
	.p2align 3
.L1529:
	cmpq	%rbx, %r14
	jnb	.L1386
	movq	24(%rsp), %rax
	movq	%r10, 40(%rsp)
	movq	%r12, %rdx
	leaq	1(%r14), %r13
	movzbl	0(%rbp,%r14), %esi
	leaq	(%rax,%r10), %rdi
	call	*memset@GOTPCREL(%rip)
	movq	40(%rsp), %r10
	addq	%r12, %r10
	jmp	.L1424
	.p2align 4,,10
	.p2align 3
.L1530:
	cmpq	%rbx, %r13
	jnb	.L1386
	movzbl	0(%rbp,%r13), %esi
	addq	$3, %r14
	leal	-1(%rsi), %eax
	movl	%esi, 28(%r15)
	cmpl	$3, %eax
	ja	.L1386
	cmpq	%rbx, %r14
	jnb	.L1386
	movl	$1, %ecx
	.p2align 4,,10
	.p2align 3
.L1399:
	movzbl	0(%rbp,%r14), %edx
	leaq	1(%r14), %rdi
	movl	%edx, %r8d
	movq	%rdx, %rax
	andl	$127, %r8d
	andl	$127, %eax
	testb	%dl, %dl
	jns	.L1535
	cmpq	%rbx, %rdi
	jnb	.L1386
	movzbl	0(%rbp,%rdi), %edi
	leaq	2(%r14), %r8
	movq	%rdi, %rdx
	andl	$127, %edx
	salq	$7, %rdx
	orq	%rdx, %rax
	testb	%dil, %dil
	jns	.L1536
	cmpq	%rbx, %r8
	jnb	.L1386
	movzbl	0(%rbp,%r8), %edi
	leaq	3(%r14), %r9
	movq	%rdi, %rdx
	andl	$127, %edx
	salq	$14, %rdx
	orq	%rdx, %rax
	testb	%dil, %dil
	jns	.L1537
	cmpq	%rbx, %r9
	jnb	.L1386
	movzbl	0(%rbp,%r9), %edi
	leaq	4(%r14), %r8
	movq	%rdi, %rdx
	andl	$127, %edx
	salq	$21, %rdx
	orq	%rdx, %rax
	testb	%dil, %dil
	jns	.L1433
	cmpq	%rbx, %r8
	jnb	.L1386
	movzbl	0(%rbp,%r8), %edx
	testb	%dl, %dl
	js	.L1386
	salq	$28, %rdx
	addq	$5, %r14
	orq	%rdx, %rax
.L1404:
	leaq	-1(%rax), %rdx
	cmpq	$16777215, %rdx
	ja	.L1386
.L1401:
	movl	%eax, 28(%r15,%rcx,4)
	cmpl	%esi, %ecx
	jnb	.L1432
	addq	$1, %rcx
	cmpq	%rbx, %r14
	jb	.L1399
	jmp	.L1386
	.p2align 4,,10
	.p2align 3
.L1532:
	movq	8(%rsp), %rsi
	leaq	0(%r13,%rsi), %rdx
	andl	$1, %r13d
	movq	%rsi, %rax
	je	.L1407
	addq	$1, %rax
	movb	$0, (%rsi)
	cmpq	%rax, %rdx
	je	.L1409
	.p2align 4,,10
	.p2align 3
.L1407:
	movb	$0, (%rax)
	addq	$2, %rax
	movb	$0, -1(%rax)
	cmpq	%rax, %rdx
	jne	.L1407
.L1409:
	movzbl	56(%rsp), %eax
	movq	%r10, 40(%rsp)
	leaq	80(%rsp), %r14
	movq	%rbp, 56(%rsp)
	movq	%r15, %rbp
	movq	%rbx, %r15
	leaq	5(%rax,%rax,4), %r13
	movq	16(%rsp), %rax
	salq	$10, %r13
	leaq	0(%r13,%rax), %rsi
	movq	%rax, %rbx
	movq	%rsi, %r13
	jmp	.L1414
	.p2align 4,,10
	.p2align 3
.L1539:
	addq	$5120, %rbx
	cmpq	%r13, %rbx
	je	.L1538
.L1414:
	movl	52(%rbp), %esi
	movq	%rbx, %rdx
	movq	%r14, %rdi
	call	kr_dec_table
	testl	%eax, %eax
	je	.L1539
	movq	%rbp, %r15
	jmp	.L1386
	.p2align 4,,10
	.p2align 3
.L1531:
	leaq	(%rsi,%rsi,4), %rsi
	movq	%r10, 72(%rsp)
	movq	16(%rsp), %rdi
	salq	$10, %rsi
	movl	%edx, 64(%rsp)
	movq	%rcx, 40(%rsp)
	call	*realloc@GOTPCREL(%rip)
	testq	%rax, %rax
	je	.L1386
	movq	72(%rsp), %r10
	movl	64(%rsp), %edx
	movl	%r14d, 52(%rsp)
	movq	%rax, 16(%rsp)
	movq	40(%rsp), %rcx
	jmp	.L1405
.L1538:
	movl	108(%rsp), %edx
	movq	%r15, %rbx
	movq	40(%rsp), %r10
	movq	%rbp, %r15
	movq	56(%rsp), %rbp
	testl	%edx, %edx
	jne	.L1386
	cmpq	$0, 96(%rsp)
	jne	.L1386
	movq	80(%rsp), %rax
	movq	%rax, %rdx
	subq	%rbp, %rdx
	cmpq	%rbx, %rdx
	jnb	.L1386
	movzbl	(%rax), %esi
	leaq	1(%rdx), %rcx
	movq	%rsi, %r13
	andl	$127, %r13d
	testb	%sil, %sil
	jns	.L1415
	cmpq	%rbx, %rcx
	jnb	.L1386
	movzbl	1(%rax), %edi
	leaq	2(%rdx), %rcx
	movq	%rdi, %rsi
	andl	$127, %esi
	salq	$7, %rsi
	orq	%rsi, %r13
	testb	%dil, %dil
	jns	.L1415
	cmpq	%rbx, %rcx
	jnb	.L1386
	movzbl	2(%rax), %edi
	leaq	3(%rdx), %rcx
	movq	%rdi, %rsi
	andl	$127, %esi
	salq	$14, %rsi
	orq	%rsi, %r13
	testb	%dil, %dil
	jns	.L1415
	cmpq	%rbx, %rcx
	jnb	.L1386
	movzbl	3(%rax), %edi
	leaq	4(%rdx), %rcx
	movq	%rdi, %rsi
	andl	$127, %esi
	salq	$21, %rsi
	orq	%rsi, %r13
	testb	%dil, %dil
	jns	.L1415
	cmpq	%rbx, %rcx
	jnb	.L1386
	movzbl	4(%rax), %eax
	testb	%al, %al
	js	.L1386
	salq	$28, %rax
	leaq	5(%rdx), %rcx
	orq	%rax, %r13
.L1415:
	movq	%rbx, %rax
	subq	%rcx, %rax
	cmpq	%r13, %rax
	jb	.L1386
	testb	$1, %r13b
	jne	.L1386
	movl	48(%r15), %esi
	testl	%esi, %esi
	je	.L1422
	leal	-1(%rsi), %eax
	cmpl	$14, %eax
	jbe	.L1436
	movl	%esi, %edi
	movq	32(%rsp), %r11
	movq	8(%rsp), %rdx
	pxor	%xmm7, %xmm7
	movq	16(%rsp), %xmm4
	shrl	$4, %edi
	movdqa	.LC76(%rip), %xmm6
	pxor	%xmm3, %xmm3
	salq	$7, %rdi
	movq	%r11, %rax
	punpcklqdq	%xmm4, %xmm4
	addq	%r11, %rdi
	.p2align 4,,10
	.p2align 3
.L1420:
	movdqu	(%rdx), %xmm2
	movdqa	%xmm6, %xmm1
	subq	$-128, %rax
	addq	$16, %rdx
	movdqa	%xmm2, %xmm0
	punpckhbw	%xmm7, %xmm2
	punpcklbw	%xmm7, %xmm0
	pmullw	%xmm0, %xmm1
	pmulhuw	%xmm6, %xmm0
	movdqa	%xmm1, %xmm8
	punpckhwd	%xmm0, %xmm1
	punpcklwd	%xmm0, %xmm8
	movdqa	%xmm6, %xmm0
	pmullw	%xmm2, %xmm0
	pmulhuw	%xmm6, %xmm2
	movdqa	%xmm0, %xmm5
	punpckhwd	%xmm2, %xmm0
	punpcklwd	%xmm2, %xmm5
	movdqa	%xmm8, %xmm2
	punpckhdq	%xmm3, %xmm8
	punpckldq	%xmm3, %xmm2
	paddq	%xmm4, %xmm8
	paddq	%xmm4, %xmm2
	movups	%xmm8, -112(%rax)
	movups	%xmm2, -128(%rax)
	movdqa	%xmm1, %xmm2
	punpckldq	%xmm3, %xmm2
	paddq	%xmm4, %xmm2
	movups	%xmm2, -96(%rax)
	movdqa	%xmm1, %xmm2
	movdqa	%xmm5, %xmm1
	punpckldq	%xmm3, %xmm1
	punpckhdq	%xmm3, %xmm2
	paddq	%xmm4, %xmm1
	paddq	%xmm4, %xmm2
	movups	%xmm1, -64(%rax)
	movdqa	%xmm5, %xmm1
	punpckhdq	%xmm3, %xmm1
	movups	%xmm2, -80(%rax)
	paddq	%xmm4, %xmm1
	movups	%xmm1, -48(%rax)
	movdqa	%xmm0, %xmm1
	punpckhdq	%xmm3, %xmm0
	punpckldq	%xmm3, %xmm1
	paddq	%xmm4, %xmm0
	paddq	%xmm4, %xmm1
	movups	%xmm0, -16(%rax)
	movups	%xmm1, -32(%rax)
	cmpq	%rax, %rdi
	jne	.L1420
	movl	%esi, %eax
	andl	$-16, %eax
	testb	$15, %sil
	je	.L1422
.L1419:
	movq	8(%rsp), %r11
	movl	%eax, %edi
	movq	16(%rsp), %r9
	movq	32(%rsp), %r8
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	1(%rax), %edx
	cmpl	%esi, %edx
	jnb	.L1422
	movl	%edx, %edi
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	2(%rax), %edx
	cmpl	%esi, %edx
	jnb	.L1422
	movl	%edx, %edi
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	3(%rax), %edi
	cmpl	%esi, %edi
	jnb	.L1422
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	4(%rax), %edx
	cmpl	%esi, %edx
	jnb	.L1422
	movl	%edx, %edi
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	5(%rax), %edi
	cmpl	%esi, %edi
	jnb	.L1422
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	6(%rax), %edx
	cmpl	%esi, %edx
	jnb	.L1422
	movl	%edx, %edi
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	7(%rax), %edi
	cmpl	%esi, %edi
	jnb	.L1422
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	8(%rax), %edx
	cmpl	%esi, %edx
	jnb	.L1422
	movl	%edx, %edi
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	9(%rax), %edi
	cmpl	%esi, %edi
	jnb	.L1422
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	10(%rax), %edi
	cmpl	%esi, %edi
	jnb	.L1422
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	11(%rax), %edi
	cmpl	%esi, %edi
	jnb	.L1422
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	12(%rax), %edx
	cmpl	%esi, %edx
	jnb	.L1422
	movl	%edx, %edi
	movzbl	(%r11,%rdi), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	leal	13(%rax), %edi
	cmpl	%esi, %edi
	jnb	.L1422
	movzbl	(%r11,%rdi), %edx
	addl	$14, %eax
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rdi,8)
	cmpl	%esi, %eax
	jnb	.L1422
	movzbl	(%r11,%rax), %edx
	leaq	(%rdx,%rdx,4), %rdx
	salq	$10, %rdx
	addq	%r9, %rdx
	movq	%rdx, (%r8,%rax,8)
.L1422:
	movq	24(%rsp), %rsi
	addq	%rcx, %r13
	leaq	0(%rbp,%rcx), %rdx
	movq	%r12, %r9
	leaq	0(%rbp,%r13), %rax
	movq	%r15, %rdi
	movq	%r10, 40(%rsp)
	leaq	(%rsi,%r10), %r8
	movq	32(%rsp), %rsi
	movq	%rax, %rcx
	call	kr_dec_syms
	movq	40(%rsp), %r10
	testl	%eax, %eax
	jne	.L1386
	addq	%r12, %r10
	jmp	.L1424
.L1535:
	testb	%r8b, %r8b
	je	.L1386
	movq	%rdi, %r14
	jmp	.L1401
.L1536:
	testq	%rax, %rax
	je	.L1386
	movq	%r8, %r14
	jmp	.L1401
.L1537:
	testq	%rax, %rax
	je	.L1386
	movq	%r9, %r14
	jmp	.L1401
.L1390:
	testq	%r12, %r12
	je	.L1386
	movq	%rsi, %r14
	jmp	.L1393
.L1391:
	testq	%r12, %r12
	jne	.L1393
	jmp	.L1386
	.p2align 4,,10
	.p2align 3
.L1433:
	movq	%r8, %r14
	jmp	.L1404
.L1431:
	movq	%rsi, %r14
.L1392:
	leaq	-1(%r12), %rax
	cmpq	$16777215, %rax
	jbe	.L1393
	jmp	.L1386
.L1527:
	salq	$28, %rax
	leaq	6(%r13), %r14
	orq	%rax, %r12
	jmp	.L1392
.L1432:
	movq	%r14, %r13
	jmp	.L1398
.L1533:
	movq	64(%rsp), %r10
	jmp	.L1409
.L1430:
	movq	$0, 16(%rsp)
	jmp	.L1386
.L1428:
	movq	$0, 16(%rsp)
	xorl	%r13d, %r13d
.L1385:
	cmpq	%r13, %rbx
	setne	%bl
	movzbl	%bl, %ebx
	negl	%ebx
	jmp	.L1384
.L1436:
	xorl	%eax, %eax
	jmp	.L1419
.L1427:
	xorl	%eax, %eax
	orl	$-1, %ebx
	movq	%rax, 16(%rsp)
	jmp	.L1384
.L1528:
	call	*__stack_chk_fail@GOTPCREL(%rip)
	.cfi_endproc
.LFE43:
	.size	kolmo_krans_decompress, .-kolmo_krans_decompress
	.p2align 4
	.globl	kolmo_krans_bound
	.type	kolmo_krans_bound, @function
kolmo_krans_bound:
.LFB63:
	.cfi_startproc
	endbr64
	movq	%rdi, %rax
	shrq	$12, %rax
	leaq	3(%rax,%rax,2), %rax
	leaq	16(%rdi,%rax,2), %rax
	ret
	.cfi_endproc
.LFE63:
	.size	kolmo_krans_bound, .-kolmo_krans_bound
	.p2align 4
	.globl	kolmo_krans_compress
	.type	kolmo_krans_compress, @function
kolmo_krans_compress:
.LFB64:
	.cfi_startproc
	endbr64
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	subq	$4096, %rsp
	.cfi_def_cfa_offset 4152
	orq	$0, (%rsp)
	subq	$4096, %rsp
	.cfi_def_cfa_offset 8248
	orq	$0, (%rsp)
	subq	$1320, %rsp
	.cfi_def_cfa_offset 9568
	movq	%rdi, 136(%rsp)
	movq	%rsi, 80(%rsp)
	movq	%rdx, 112(%rsp)
	movq	%rcx, 144(%rsp)
	movq	%r8, 120(%rsp)
	movq	%fs:40, %rax
	movq	%rax, 9496(%rsp)
	xorl	%eax, %eax
	testq	%r8, %r8
	je	.L1542
	movzbl	4(%r8), %eax
	movl	$1, %edx
	movl	(%r8), %ecx
	testb	%al, %al
	cmove	%edx, %eax
	movzbl	%al, %edi
	movl	8(%r8), %eax
	testl	%eax, %eax
	je	.L1543
	movl	%eax, %r8d
	cmpl	$16777216, %eax
	jbe	.L1544
	movl	$16777216, %esi
	testl	%ecx, %ecx
	jne	.L1808
.L1545:
	cmpq	$0, 80(%rsp)
	je	.L1547
.L1813:
	movq	120(%rsp), %rax
	movzbl	40(%rax), %ebp
	testb	%bpl, %bpl
	je	.L1667
	imulq	$700, %rbp, %rax
	movq	%rbp, %r13
	movzbl	%bpl, %ebx
	salq	$9, %r13
	leaq	4400(%rax), %r12
.L1548:
	movq	80(%rsp), %rax
	leaq	384(%rsp), %r15
	movl	$9088, %edx
	movq	%r15, 48(%rsp)
	movq	%r15, %rdi
	cmpq	%rsi, %rax
	cmovbe	%rax, %rsi
	movq	%rsi, %r14
	movq	%rsi, 152(%rsp)
	xorl	%esi, %esi
	call	*memset@GOTPCREL(%rip)
	leaq	2(%r14,%r14), %rax
	movq	%rbp, 32(%rsp)
	movq	%rax, 8(%rsp)
	leaq	6288(%rsp), %rax
	movq	%r14, 384(%rsp)
	movq	%r12, 392(%rsp)
	movq	%rax, 16(%rsp)
	movq	%r12, 40(%rsp)
	movl	%ebx, 400(%rsp)
	movq	%r15, %rbx
.L1552:
	movl	$4096, %edi
	call	*malloc@GOTPCREL(%rip)
	movq	%r13, %rdi
	movq	%rax, %rbp
	movq	%rax, 2936(%rbx)
	call	*malloc@GOTPCREL(%rip)
	movq	32(%rsp), %rdi
	movq	%rax, %r12
	movq	%rax, 2944(%rbx)
	call	*malloc@GOTPCREL(%rip)
	movq	40(%rsp), %rdi
	movq	%rax, %r14
	movq	%rax, 2952(%rbx)
	call	*malloc@GOTPCREL(%rip)
	movq	8(%rsp), %rdi
	movq	%rax, %r15
	movq	%rax, 2960(%rbx)
	call	*malloc@GOTPCREL(%rip)
	testq	%r12, %r12
	movq	%rax, 2976(%rbx)
	movq	%rax, %rdx
	sete	%al
	testq	%rbp, %rbp
	sete	%sil
	orl	%esi, %eax
	testq	%r14, %r14
	sete	%cl
	orl	%ecx, %eax
	testq	%r15, %r15
	sete	%cl
	orb	%cl, %al
	jne	.L1553
	testq	%rdx, %rdx
	je	.L1553
	movq	16(%rsp), %rax
	addq	$2952, %rbx
	cmpq	%rax, %rbx
	jne	.L1552
	movq	32(%rsp), %rbp
	movq	%rbp, %rbx
	salq	$10, %rbx
	movq	%rbx, %rdi
	call	*malloc@GOTPCREL(%rip)
	movq	%rbp, %rdi
	salq	$11, %rdi
	movq	%rax, 6336(%rsp)
	movq	%rax, 16(%rsp)
	call	*malloc@GOTPCREL(%rip)
	movq	%rbx, %rdi
	movq	%rax, 8384(%rsp)
	movq	%rax, 32(%rsp)
	call	*malloc@GOTPCREL(%rip)
	movl	$32768, %edi
	movq	%rax, %r15
	movq	%rax, 8408(%rsp)
	call	*malloc@GOTPCREL(%rip)
	movl	$4096, %edi
	movq	%rax, %r13
	movq	%rax, 8416(%rsp)
	call	*malloc@GOTPCREL(%rip)
	movq	8(%rsp), %rdi
	movq	%rax, %r12
	movq	%rax, 8424(%rsp)
	addq	$126, %rdi
	call	*malloc@GOTPCREL(%rip)
	movq	%rbp, %rdi
	movq	%rax, %rbx
	salq	$12, %rdi
	movq	%rax, 8432(%rsp)
	call	*malloc@GOTPCREL(%rip)
	movq	32(%rsp), %rcx
	movq	16(%rsp), %rdx
	movq	%rax, 8440(%rsp)
	movq	%rax, %rsi
	testq	%rcx, %rcx
	setne	%al
	testq	%rdx, %rdx
	setne	%dl
	andl	%edx, %eax
	testq	%r15, %r15
	setne	%dl
	andl	%edx, %eax
	testq	%r13, %r13
	setne	%dl
	andl	%edx, %eax
	testq	%r12, %r12
	setne	%dl
	andl	%edx, %eax
	testq	%rbx, %rbx
	setne	%dl
	testb	%dl, %al
	je	.L1553
	testq	%rsi, %rsi
	je	.L1553
	leaq	8448(%rsp), %rax
	leaq	9479(%rsp), %r13
	movq	$0, 40(%rsp)
	movq	%rax, 8(%rsp)
	movq	48(%rsp), %rax
	movq	$0, 64(%rsp)
	leaq	9088(%rax), %r14
	.p2align 4,,10
	.p2align 3
.L1658:
	movq	40(%rsp), %rax
	movq	80(%rsp), %rbp
	movq	152(%rsp), %rsi
	movq	144(%rsp), %rbx
	subq	%rax, %rbp
	cmpq	%rsi, %rbp
	cmova	%rsi, %rbp
	movq	64(%rsp), %rsi
	subq	%rsi, %rbx
	movl	%ebp, %edx
	movq	%rbx, 88(%rsp)
	movq	112(%rsp), %rbx
	addq	%rsi, %rbx
	movq	%rbx, 128(%rsp)
	movq	136(%rsp), %rbx
	leaq	(%rbx,%rax), %r12
	cmpq	$127, %rbp
	ja	.L1668
	movq	8(%rsp), %rdi
	movl	$128, %ecx
	xorl	%eax, %eax
	movb	%bpl, 9480(%rsp)
	rep stosq
	testq	%rbp, %rbp
	jne	.L1669
	movq	$1, 32(%rsp)
	xorl	%ebx, %ebx
.L1556:
	pxor	%xmm1, %xmm1
	movq	8(%rsp), %rax
	movdqa	%xmm1, %xmm2
	.p2align 4,,10
	.p2align 3
.L1558:
	movdqa	%xmm2, %xmm0
	addq	$16, %rax
	pcmpeqd	-16(%rax), %xmm0
	pcmpeqd	%xmm2, %xmm0
	psubd	%xmm0, %xmm1
	cmpq	%rax, %r14
	jne	.L1558
	movdqa	%xmm1, %xmm0
	psrldq	$8, %xmm0
	paddd	%xmm0, %xmm1
	movdqa	%xmm1, %xmm0
	psrldq	$4, %xmm0
	paddd	%xmm0, %xmm1
	movd	%xmm1, %eax
	cmpl	$1, %eax
	je	.L1809
	movq	%rbp, %rdx
	movl	400(%rsp), %ecx
	movl	$3, 56(%rsp)
	shrq	$4, %rdx
	leaq	1(%rdx), %rsi
	movq	%rcx, %rax
	addl	$1, %edx
	cmpq	%rcx, %rsi
	cmovb	%edx, %eax
	movl	%eax, 404(%rsp)
	cmpq	$32767, %rbp
	ja	.L1564
	movl	$2, 56(%rsp)
	cmpq	$4095, %rbp
	ja	.L1564
	xorl	%eax, %eax
	cmpq	$511, %rbp
	seta	%al
	movl	%eax, 56(%rsp)
.L1564:
	movq	120(%rsp), %rsi
	cmpb	$2, 5(%rsi)
	movzbl	4(%rsi), %r10d
	sbbl	%r9d, %r9d
	addl	$2, %r9d
	cmpb	$0, 12(%rsi)
	je	.L1566
	movzbl	13(%rsi), %eax
	movdqu	24(%rsi), %xmm7
	movl	%r9d, %r8d
	movq	%rbp, %rcx
	leaq	416(%rsp), %r15
	movq	48(%rsp), %rdi
	movq	%r12, %rdx
	movl	%eax, 416(%rsp)
	movzbl	14(%rsi), %eax
	movaps	%xmm7, 16(%rsp)
	movl	%eax, 420(%rsp)
	movzbl	15(%rsi), %eax
	movaps	%xmm7, 448(%rsp)
	movl	%eax, 424(%rsp)
	movzbl	16(%rsi), %eax
	movl	%eax, 428(%rsp)
	movzbl	17(%rsi), %eax
	movl	%eax, 432(%rsp)
	movzbl	18(%rsi), %eax
	movl	%eax, 436(%rsp)
	movzbl	19(%rsi), %eax
	movl	%eax, 440(%rsp)
	movzbl	20(%rsi), %eax
	movq	%r15, %rsi
	movl	%eax, 444(%rsp)
	call	kr_plan_build
	testl	%eax, %eax
	jne	.L1567
	movsd	3312(%rsp), %xmm0
	movq	%r15, 16(%rsp)
.L1568:
	movq	32(%rsp), %rax
	mulsd	.LC79(%rip), %xmm0
	addsd	.LC63(%rip), %xmm0
	leaq	1(%rbp,%rax), %r9
	testq	%rax, %rax
	js	.L1597
	pxor	%xmm1, %xmm1
	cvtsi2sdq	%rax, %xmm1
.L1598:
	addsd	%xmm1, %xmm0
	pxor	%xmm1, %xmm1
	cvtsi2sdq	%rbp, %xmm1
	comisd	%xmm0, %xmm1
	jbe	.L1595
	leaq	2(%rbx), %rax
	movq	%rax, 192(%rsp)
	cmpq	%rax, 88(%rsp)
	jnb	.L1810
.L1595:
	cmpq	%r9, 88(%rsp)
	movq	%r9, 16(%rsp)
	jnb	.L1811
.L1561:
	xorl	%edx, %edx
	movq	%rdx, 64(%rsp)
.L1659:
	movq	48(%rsp), %rdi
	call	kr_enc_free
.L1541:
	movq	9496(%rsp), %rax
	subq	%fs:40, %rax
	jne	.L1812
	movq	64(%rsp), %rax
	addq	$9512, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L1544:
	.cfi_restore_state
	movl	$4096, %esi
	cmpq	%rsi, %r8
	cmovnb	%r8, %rsi
	testl	%ecx, %ecx
	je	.L1545
	movl	%ecx, %eax
	xorl	%edx, %edx
	divl	%edi
	testl	%edx, %edx
	jne	.L1545
	cmpq	$32768, %r8
	movl	%ecx, %edx
	sbbl	%ecx, %ecx
	addl	$3, %ecx
	jmp	.L1546
.L1543:
	testl	%ecx, %ecx
	je	.L1664
	movl	%ecx, %eax
	xorl	%edx, %edx
	movl	$1048576, %esi
	divl	%edi
	testl	%edx, %edx
	jne	.L1545
.L1803:
	movl	%ecx, %edx
	movl	$3, %ecx
.L1546:
	salq	%cl, %rdx
	leaq	(%rdx,%rdx), %rax
	movq	%rdx, %rcx
	cmpq	%rax, %rsi
	jb	.L1545
	movq	%rsi, %rax
	xorl	%edx, %edx
	divq	%rcx
	movq	%rax, %rsi
	imulq	%rcx, %rsi
	cmpq	$0, 80(%rsp)
	jne	.L1813
.L1547:
	movq	$0, 64(%rsp)
	jmp	.L1541
	.p2align 4,,10
	.p2align 3
.L1668:
	movq	%rbp, %rax
	xorl	%ebx, %ebx
	.p2align 4,,10
	.p2align 3
.L1554:
	orl	$-128, %edx
	movq	%rbx, %rcx
	shrq	$7, %rax
	addq	$1, %rbx
	movb	%dl, 0(%r13,%rbx)
	movl	%eax, %edx
	cmpq	$127, %rax
	ja	.L1554
	movq	8(%rsp), %rdi
	movb	%al, 9480(%rsp,%rbx)
	leaq	2(%rcx), %rsi
	xorl	%eax, %eax
	movl	$128, %ecx
	movq	%rsi, 32(%rsp)
	rep stosq
.L1555:
	movq	%r12, %rax
	leaq	0(%rbp,%r12), %rcx
.L1557:
	movzbl	(%rax), %edx
	addq	$1, %rax
	addl	$1, 8448(%rsp,%rdx,4)
	cmpq	%rax, %rcx
	jne	.L1557
	jmp	.L1556
.L1809:
	leaq	3(%rbx), %r9
	cmpq	%r9, 88(%rsp)
	jb	.L1561
	movq	128(%rsp), %r15
	movq	%r9, 16(%rsp)
	leaq	9480(%rsp), %rsi
	movq	32(%rsp), %rdx
	movb	$1, (%r15)
	leaq	1(%r15), %rdi
	call	*memcpy@GOTPCREL(%rip)
	movzbl	(%r12), %eax
	movq	16(%rsp), %r9
	movb	%al, 2(%r15,%rbx)
.L1562:
	testq	%r9, %r9
	je	.L1561
.L1657:
	addq	%rbp, 40(%rsp)
	movq	80(%rsp), %rbx
	movq	40(%rsp), %rax
	addq	%r9, 64(%rsp)
	cmpq	%rbx, %rax
	jb	.L1658
	jmp	.L1659
.L1566:
	movl	56(%rsp), %esi
	xorl	%eax, %eax
	movq	48(%rsp), %rdi
	pxor	%xmm0, %xmm0
	cmpq	$65535, %rbp
	movq	%rbp, %rcx
	movq	%r12, %rdx
	movl	%r9d, 96(%rsp)
	leaq	416(%rsp), %r15
	seta	%al
	xorl	%r8d, %r8d
	movl	%esi, 416(%rsp)
	movq	%r15, %rsi
	movb	%r10b, 16(%rsp)
	movl	%eax, 72(%rsp)
	movl	$0, 460(%rsp)
	movl	$1, 420(%rsp)
	movl	%eax, 424(%rsp)
	movq	%r15, 160(%rsp)
	movups	%xmm0, 428(%rsp)
	movups	%xmm0, 444(%rsp)
	call	kr_plan_build
	movzbl	16(%rsp), %r10d
	movl	96(%rsp), %r9d
	testl	%eax, %eax
	je	.L1673
	movl	408(%rsp), %r11d
	testl	%r11d, %r11d
	jne	.L1561
	movq	$0, 16(%rsp)
	movl	$9056, %edi
.L1571:
	movq	120(%rsp), %r8
	movl	(%r8), %esi
	cmpb	$8, %r10b
	ja	.L1572
	testl	%esi, %esi
	jne	.L1814
.L1572:
	cmpq	$2047, %rbp
	ja	.L1815
.L1578:
	cmpq	$0, 16(%rsp)
	jne	.L1594
.L1789:
	movq	32(%rsp), %rax
	leaq	1(%rbp,%rax), %r9
	cmpq	%r9, 88(%rsp)
	movq	%r9, 16(%rsp)
	jb	.L1561
.L1811:
	movq	128(%rsp), %r15
	movq	32(%rsp), %rdx
	leaq	9480(%rsp), %rsi
	leaq	1(%r15), %rdi
	movb	$0, (%r15)
	call	*memcpy@GOTPCREL(%rip)
	leaq	2(%r15,%rbx), %rdi
	movq	%rbp, %rdx
	movq	%r12, %rsi
	call	*memcpy@GOTPCREL(%rip)
	movq	16(%rsp), %r9
	jmp	.L1562
.L1673:
	movq	160(%rsp), %rax
	movl	$6104, %edi
	leaq	3368(%rsp), %r15
	movq	%rax, 16(%rsp)
	jmp	.L1571
.L1567:
	movl	408(%rsp), %r15d
	testl	%r15d, %r15d
	je	.L1789
	jmp	.L1561
.L1597:
	movq	%rax, %rsi
	shrq	%rax
	pxor	%xmm1, %xmm1
	andl	$1, %esi
	orq	%rsi, %rax
	cvtsi2sdq	%rax, %xmm1
	addsd	%xmm1, %xmm1
	jmp	.L1598
.L1814:
	testb	%r10b, %r10b
	movl	$1, %eax
	cmove	%eax, %r10d
	movl	%esi, %eax
	xorl	%edx, %edx
	divl	%r10d
	movl	%edx, %r11d
	testl	%edx, %edx
	jne	.L1572
	leal	(%r10,%r10), %eax
	cmpl	%eax, %esi
	jb	.L1572
	movzbl	56(%rsp), %ecx
	movq	%rbp, %rdx
	movl	%esi, %eax
	addq	%rax, %rax
	shrq	%cl, %rdx
	cmpq	%rax, %rdx
	jb	.L1572
	cmpq	$2047, %rbp
	jbe	.L1578
	xorl	%eax, %eax
	cmpl	$1, %r10d
	movq	%rdi, 192(%rsp)
	seta	%al
	movl	%r9d, 104(%rsp)
	movl	%eax, 96(%rsp)
	movq	%rbx, 168(%rsp)
	movl	%r11d, %ebx
	movq	%r14, 176(%rsp)
	movl	%r10d, %r14d
	movq	%r13, 184(%rsp)
	movq	%r8, %r13
	.p2align 4,,10
	.p2align 3
.L1579:
	movl	56(%rsp), %eax
	movl	%esi, 36(%r15)
	movq	%rbp, %rcx
	movq	%r12, %rdx
	movl	%r14d, 4(%r15)
	movl	104(%rsp), %r8d
	movl	%eax, (%r15)
	movl	72(%rsp), %eax
	movl	%ebx, 20(%r15)
	movq	48(%rsp), %rdi
	movl	%eax, 8(%r15)
	movq	.LC77(%rip), %rax
	movl	$4, 28(%r15)
	movq	%rax, 12(%r15)
	movl	96(%rsp), %eax
	movl	%r14d, 32(%r15)
	movl	%eax, 24(%r15)
	leal	(%r14,%rsi), %eax
	subl	%r14d, %esi
	movl	%esi, 44(%r15)
	movq	%r15, %rsi
	movl	%eax, 40(%r15)
	call	kr_plan_build
	testl	%eax, %eax
	jne	.L1574
	movq	16(%rsp), %rax
	testq	%rax, %rax
	je	.L1575
	movsd	2896(%rax), %xmm0
	comisd	2896(%r15), %xmm0
	jbe	.L1576
.L1575:
	movl	$2952, %esi
	movl	$9056, %edx
	movq	%r15, 16(%rsp)
	movq	160(%rsp), %rax
	cmpq	%rax, %r15
	movl	$0, %eax
	cmove	%rsi, %rax
	movq	48(%rsp), %rsi
	subq	%rax, %rdx
	leaq	32(%rsi,%rax), %r15
	movq	%rdx, 192(%rsp)
.L1576:
	cmpb	$1, 5(%r13)
	jbe	.L1797
.L1824:
	addl	$1, %ebx
	cmpl	$1, %ebx
	jne	.L1790
	movl	0(%r13), %esi
	jmp	.L1579
.L1542:
	cmpq	$0, 80(%rsp)
	je	.L1547
	movl	$131072, %esi
	movl	$64, %ebp
	movl	$64, %ebx
	movl	$32768, %r13d
	leaq	def.0(%rip), %rax
	movl	$49200, %r12d
	movq	%rax, 120(%rsp)
	jmp	.L1548
.L1815:
	movl	56(%rsp), %eax
	pxor	%xmm0, %xmm0
	movq	48(%rsp), %rdi
	movl	%r9d, %r8d
	movups	%xmm0, 12(%r15)
	leaq	12(%r15), %r10
	movq	%rbp, %rcx
	movq	%r12, %rdx
	movl	%eax, (%r15)
	movl	72(%rsp), %eax
	movq	%r15, %rsi
	movl	$0, 44(%r15)
	movl	$1, 4(%r15)
	movl	%eax, 8(%r15)
	movl	$1, 16(%r15)
	movups	%xmm0, 28(%r15)
	movq	%r10, 104(%rsp)
	movl	%r9d, 96(%rsp)
	call	kr_plan_build
	movl	96(%rsp), %r9d
	movq	104(%rsp), %r10
	pxor	%xmm0, %xmm0
	testl	%eax, %eax
	jne	.L1585
	movq	120(%rsp), %rax
	movq	16(%rsp), %rsi
	movzbl	5(%rax), %eax
	testq	%rsi, %rsi
	je	.L1586
	movsd	2896(%rsi), %xmm0
	comisd	2896(%r15), %xmm0
	jbe	.L1792
.L1586:
	cmpb	$1, %al
	jbe	.L1816
	movl	$0, %edx
	movq	48(%rsp), %rsi
	movq	%r15, 16(%rsp)
	movq	160(%rsp), %rax
	cmpq	%rax, %r15
	movl	$2952, %eax
	cmovne	%rdx, %rax
	leaq	32(%rsi,%rax), %rax
	leaq	12(%rax), %r10
	movq	%rax, %r15
.L1591:
	movl	56(%rsp), %eax
	pxor	%xmm0, %xmm0
	movq	48(%rsp), %rdi
	movl	%r9d, %r8d
	movups	%xmm0, (%r10)
	movq	%rbp, %rcx
	movq	%r12, %rdx
	movq	%r15, %rsi
	movl	$0, 32(%r10)
	movups	%xmm0, 16(%r10)
	movl	%eax, (%r15)
	movl	72(%rsp), %eax
	movl	$1, 4(%r15)
	movl	%eax, 8(%r15)
	movl	$3, 16(%r15)
	call	kr_plan_build
	testl	%eax, %eax
	je	.L1689
	movl	408(%rsp), %eax
	testl	%eax, %eax
	jne	.L1561
.L1594:
	movq	16(%rsp), %rax
	movsd	2896(%rax), %xmm0
	jmp	.L1568
.L1810:
	movq	128(%rsp), %rax
	movq	32(%rsp), %r15
	movq	%r9, 56(%rsp)
	leaq	9480(%rsp), %rsi
	leaq	1(%rax), %rdi
	movq	%r15, %rdx
	movb	$2, (%rax)
	call	*memcpy@GOTPCREL(%rip)
	movq	16(%rsp), %rdi
	movq	88(%rsp), %rax
	movq	8432(%rsp), %rdx
	movq	56(%rsp), %r9
	movl	(%rdi), %esi
	subq	$1, %rax
	subq	%r15, %rax
	movl	%esi, %ecx
	movq	%rax, 200(%rsp)
	movl	$1, %eax
	sall	%cl, %eax
	movl	%esi, 212(%rsp)
	movl	%eax, %r8d
	movq	%rbp, %rax
	shrq	%cl, %rax
	movq	384(%rsp), %rcx
	movq	%rax, 72(%rsp)
	movl	2888(%rdi), %eax
	leaq	128(%rdx,%rcx,2), %rsi
	testl	%eax, %eax
	movq	%rsi, 96(%rsp)
	je	.L1609
	salq	$9, %rax
	movq	%r12, 104(%rsp)
	movq	8440(%rsp), %rsi
	movl	$4096, %r11d
	movq	2912(%rdi), %rdi
	movq	%rax, 56(%rsp)
	movq	$-1, %r10
	movq	%rbp, 160(%rsp)
	leaq	4096(%rsi), %r15
	xorl	%esi, %esi
	movq	%r9, 168(%rsp)
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movl	%r8d, 176(%rsp)
.L1608:
	leaq	(%rsi,%rdx), %r9
	leaq	-4096(%r15), %rcx
	xorl	%ebp, %ebp
	movq	%rdx, %r12
	.p2align 4,,10
	.p2align 3
.L1607:
	movzwl	(%r9), %eax
	testw	%ax, %ax
	je	.L1604
	movl	%r11d, %edx
	subl	%eax, %edx
	sall	$16, %eax
	movzwl	%dx, %edx
	orl	%edx, %eax
	leal	4095(%rbp), %edx
	movl	%eax, 12(%rcx)
	movzwl	(%r9), %edi
	movq	$-1, %rax
	cmpw	$1, %di
	je	.L1606
	movzwl	%di, %r8d
	movq	%r10, %rax
	xorl	%edx, %edx
	divq	%r8
	movl	%ebp, %edx
	addq	$1, %rax
.L1606:
	movl	%edx, 8(%rcx)
	addl	%edi, %ebp
	movq	%rax, (%rcx)
.L1604:
	addq	$16, %rcx
	addq	$2, %r9
	cmpq	%r15, %rcx
	jne	.L1607
	movq	%r12, %rdx
	addq	$4096, %r15
	addq	$512, %rdx
	cmpq	%rdx, 56(%rsp)
	jne	.L1608
	movq	104(%rsp), %r12
	movq	160(%rsp), %rbp
	movq	168(%rsp), %r9
	movl	176(%rsp), %r8d
.L1609:
	leal	-1(%r8), %eax
	movl	%eax, 104(%rsp)
	cmpl	$2, %eax
	jbe	.L1676
	movdqa	.LC80(%rip), %xmm0
	movl	%r8d, %eax
	shrl	$2, %eax
	movaps	%xmm0, 256(%rsp)
	cmpl	$1, %eax
	je	.L1602
	movaps	%xmm0, 272(%rsp)
	cmpl	$2, %eax
	je	.L1602
	movaps	%xmm0, 288(%rsp)
	cmpl	$3, %eax
	je	.L1602
	movaps	%xmm0, 304(%rsp)
	cmpl	$4, %eax
	je	.L1602
	movaps	%xmm0, 320(%rsp)
	cmpl	$5, %eax
	je	.L1602
	movaps	%xmm0, 336(%rsp)
	cmpl	$6, %eax
	je	.L1602
	movaps	%xmm0, 352(%rsp)
	cmpl	$7, %eax
	je	.L1602
	movaps	%xmm0, 368(%rsp)
.L1602:
	movl	%r8d, %eax
	andl	$-4, %eax
	testb	$3, %r8b
	je	.L1610
.L1601:
	movl	%eax, %edx
	movl	$65536, 256(%rsp,%rdx,4)
	leal	1(%rax), %edx
	cmpl	%r8d, %edx
	jnb	.L1610
	movl	$65536, 256(%rsp,%rdx,4)
	addl	$2, %eax
	cmpl	%r8d, %eax
	jnb	.L1610
	movl	$65536, 256(%rsp,%rax,4)
.L1610:
	movq	16(%rsp), %rax
	leaq	-1(%rbp), %rdi
	movq	2944(%rax), %rsi
	movq	%rsi, 168(%rsp)
	movq	2904(%rax), %rsi
	movl	48(%rax), %eax
	movq	%rsi, 176(%rsp)
	movl	104(%rsp), %esi
	movq	%rsi, 56(%rsp)
	movl	256(%rsp,%rsi,4), %r11d
	movl	%r8d, %esi
	movq	%rsi, %rcx
	movq	%rsi, 160(%rsp)
	movq	72(%rsp), %rsi
	imulq	%rcx, %rsi
	movq	%rsi, 184(%rsp)
	cmpq	%rbp, %rsi
	jnb	.L1611
	movq	8440(%rsp), %r10
	movq	96(%rsp), %r15
	cmpl	$1, %eax
	jne	.L1612
	jmp	.L1817
.L1819:
	movw	%r11w, -2(%r15)
	shrl	$16, %r11d
	leaq	-2(%r15), %rsi
	movl	8(%rcx), %r15d
	movq	%r11, %rax
	mulq	(%rcx)
	movzwl	12(%rcx), %eax
	movq	184(%rsp), %rcx
	addl	%r11d, %r15d
	imull	%edx, %eax
	leal	(%rax,%r15), %r11d
	leaq	-1(%rdi), %rax
	cmpq	%rcx, %rdi
	je	.L1818
	movq	%rsi, %r15
.L1805:
	movq	%rax, %rdi
.L1612:
	movq	168(%rsp), %rax
	movq	176(%rsp), %rsi
	movl	%r11d, %edx
	shrl	$20, %edx
	movzwl	(%rax,%rdi,2), %eax
	movzbl	(%rsi,%rax), %ecx
	movzbl	(%r12,%rdi), %eax
	salq	$12, %rcx
	salq	$4, %rax
	addq	%rax, %rcx
	addq	%r10, %rcx
	movzwl	14(%rcx), %eax
	cmpl	%eax, %edx
	jnb	.L1819
	movl	%r11d, %eax
	mulq	(%rcx)
	movzwl	12(%rcx), %eax
	movl	8(%rcx), %esi
	imull	%edx, %eax
	addl	%r11d, %esi
	leal	(%rsi,%rax), %r11d
	movq	184(%rsp), %rsi
	leaq	-1(%rdi), %rax
	cmpq	%rsi, %rdi
	jne	.L1805
	movq	56(%rsp), %rax
	movl	%r11d, 256(%rsp,%rax,4)
	movq	72(%rsp), %rax
	leaq	-1(%rax), %rcx
	testq	%rax, %rax
	je	.L1622
.L1806:
	movq	%rax, %rdi
.L1623:
	movq	160(%rsp), %rax
	movq	%rdi, %rdx
	movq	168(%rsp), %rsi
	movq	%r12, 216(%rsp)
	negq	%rdx
	movq	%r9, 232(%rsp)
	imulq	%rdi, %rax
	addq	%rdx, %rdx
	movl	%r8d, 240(%rsp)
	movq	%rdx, 72(%rsp)
	movq	%r12, %rdx
	movq	176(%rsp), %r12
	subq	%rdi, %rdx
	movq	%rbx, 176(%rsp)
	leaq	-2(%rsi,%rax,2), %rsi
	addq	%rdx, %rax
	movq	%r14, 248(%rsp)
	movq	72(%rsp), %r14
	movq	%rax, 184(%rsp)
	movq	56(%rsp), %rax
	movq	%rcx, %rdx
	movq	%rbp, 224(%rsp)
	movq	%rdi, %rbp
	leaq	256(%rsp,%rax,4), %rax
	movq	%rax, 168(%rsp)
.L1625:
	movq	184(%rsp), %rax
	movl	104(%rsp), %edi
	movq	%rdx, 72(%rsp)
	movq	%rsi, %r8
	movq	168(%rsp), %r9
	leaq	(%rdx,%rax), %rbx
	jmp	.L1634
	.p2align 4,,10
	.p2align 3
.L1632:
	movl	%edi, %eax
	movl	256(%rsp,%rax,4), %r11d
.L1634:
	movzwl	(%r8), %eax
	movl	%r11d, %edx
	shrl	$20, %edx
	movzbl	(%r12,%rax), %ecx
	movzbl	(%rbx), %eax
	salq	$12, %rcx
	salq	$4, %rax
	addq	%rax, %rcx
	addq	%r10, %rcx
	movzwl	14(%rcx), %eax
	cmpl	%eax, %edx
	jb	.L1631
	movw	%r11w, -2(%r15)
	shrl	$16, %r11d
	subq	$2, %r15
.L1631:
	movl	%r11d, %eax
	mulq	(%rcx)
	movzwl	12(%rcx), %eax
	subl	$1, %edi
	addl	8(%rcx), %r11d
	addq	%r14, %r8
	subq	%rbp, %rbx
	subq	$4, %r9
	imull	%edx, %eax
	addl	%r11d, %eax
	movl	%eax, 4(%r9)
	cmpl	$-1, %edi
	jne	.L1632
	movq	72(%rsp), %rdx
	subq	$2, %rsi
	subq	$1, %rdx
	cmpq	$-1, %rdx
	je	.L1800
	movq	56(%rsp), %rax
	movl	256(%rsp,%rax,4), %r11d
	jmp	.L1625
.L1808:
	movl	%ecx, %eax
	xorl	%edx, %edx
	divl	%edi
	testl	%edx, %edx
	jne	.L1545
	jmp	.L1803
.L1585:
	movl	408(%rsp), %esi
	testl	%esi, %esi
	jne	.L1561
	movq	120(%rsp), %rax
	cmpb	$1, 5(%rax)
	jbe	.L1578
	movl	56(%rsp), %eax
	movups	%xmm0, (%r10)
	movq	48(%rsp), %rdi
	movl	%r9d, %r8d
	movups	%xmm0, 16(%r10)
	movq	%rbp, %rcx
	movq	%r12, %rdx
	movq	%r15, %rsi
	movl	$0, 32(%r10)
	movl	%eax, (%r15)
	movl	72(%rsp), %eax
	movl	$1, 4(%r15)
	movl	%eax, 8(%r15)
	movl	$3, 16(%r15)
	call	kr_plan_build
	testl	%eax, %eax
	jne	.L1592
	movsd	2896(%r15), %xmm0
	cmpq	$0, 16(%rsp)
	jne	.L1689
	movq	%r15, 16(%rsp)
	jmp	.L1568
	.p2align 4,,10
	.p2align 3
.L1800:
	movq	216(%rsp), %r12
	movq	224(%rsp), %rbp
	movq	232(%rsp), %r9
	movl	240(%rsp), %r8d
	movq	176(%rsp), %rbx
	movq	248(%rsp), %r14
.L1622:
	movq	96(%rsp), %rax
	subq	%r15, %rax
	movq	%rax, 168(%rsp)
	sarq	%rax
	movq	%rax, %r10
	movq	160(%rsp), %rax
	addq	%rax, %rax
	movq	%rax, 176(%rsp)
	addq	%r10, %rax
	leaq	(%rax,%rax), %rsi
	movl	%esi, %edx
	cmpq	$127, %rsi
	ja	.L1684
	leaq	9488(%rsp), %rax
	movl	$1, %ecx
	movq	%rax, 72(%rsp)
.L1638:
	movb	%dl, (%rax)
	movq	16(%rsp), %rax
	movq	2936(%rax), %rdx
	leaq	(%rdx,%rcx), %r11
	leaq	(%rsi,%r11), %rax
	movq	%rax, 56(%rsp)
	cmpq	%rax, 200(%rsp)
	jb	.L1595
	movq	192(%rsp), %rsi
	movq	128(%rsp), %rax
	movq	%r10, 240(%rsp)
	movq	%r11, 224(%rsp)
	addq	%rsi, %rax
	movq	16(%rsp), %rsi
	movl	%r8d, 216(%rsp)
	movq	%r9, 200(%rsp)
	movq	%rax, %rdi
	movq	%rcx, 232(%rsp)
	movq	2928(%rsi), %rsi
	movq	%rdx, 184(%rsp)
	movq	%rax, 16(%rsp)
	call	*memcpy@GOTPCREL(%rip)
	movq	16(%rsp), %rax
	movq	72(%rsp), %rsi
	movq	184(%rsp), %rdx
	leaq	(%rax,%rdx), %rdi
	movq	232(%rsp), %rdx
	call	*memcpy@GOTPCREL(%rip)
	movq	16(%rsp), %rax
	movq	224(%rsp), %r11
	movq	200(%rsp), %r9
	movl	216(%rsp), %r8d
	addq	%r11, %rax
	cmpl	$14, 104(%rsp)
	movq	240(%rsp), %r10
	movq	%rax, 16(%rsp)
	jbe	.L1685
	movdqa	256(%rsp), %xmm1
	movq	64(%rsp), %rax
	movl	%r8d, %edx
	movdqa	272(%rsp), %xmm8
	movq	112(%rsp), %rsi
	shrl	$4, %edx
	movdqa	%xmm1, %xmm4
	movdqa	%xmm1, %xmm0
	movdqa	288(%rsp), %xmm3
	movdqa	304(%rsp), %xmm7
	punpcklwd	%xmm8, %xmm4
	punpckhwd	%xmm8, %xmm0
	movdqa	%xmm8, %xmm9
	movdqa	%xmm4, %xmm2
	punpcklwd	%xmm0, %xmm4
	movdqa	%xmm7, %xmm10
	punpckhwd	%xmm0, %xmm2
	movdqa	%xmm3, %xmm0
	leaq	2(%rbx,%rax), %rax
	punpcklwd	%xmm2, %xmm4
	movdqa	%xmm3, %xmm2
	punpckhwd	%xmm7, %xmm0
	addq	%r11, %rax
	punpcklwd	%xmm7, %xmm2
	psrld	$16, %xmm9
	addq	%rsi, %rax
	psrld	$16, %xmm10
	psrld	$24, %xmm8
	movdqa	%xmm2, %xmm5
	punpckhwd	%xmm0, %xmm5
	psrld	$24, %xmm7
	punpcklwd	%xmm0, %xmm2
	movdqa	.LC54(%rip), %xmm0
	punpcklwd	%xmm5, %xmm2
	movdqa	%xmm0, %xmm6
	movdqa	%xmm0, %xmm5
	pand	%xmm2, %xmm5
	pand	%xmm4, %xmm6
	packuswb	%xmm5, %xmm6
	movdqa	%xmm4, %xmm5
	movdqa	%xmm2, %xmm4
	psrlw	$8, %xmm5
	psrlw	$8, %xmm4
	pand	%xmm0, %xmm5
	pand	%xmm0, %xmm4
	movdqa	%xmm5, %xmm2
	movdqa	%xmm1, %xmm5
	psrld	$16, %xmm5
	psrld	$24, %xmm1
	packuswb	%xmm4, %xmm2
	movdqa	%xmm5, %xmm4
	punpcklwd	%xmm9, %xmm5
	punpckhwd	%xmm9, %xmm4
	movdqa	%xmm5, %xmm9
	punpckhwd	%xmm4, %xmm9
	punpcklwd	%xmm4, %xmm5
	movdqa	%xmm3, %xmm4
	psrld	$16, %xmm4
	psrld	$24, %xmm3
	punpcklwd	%xmm9, %xmm5
	movdqa	%xmm4, %xmm9
	punpcklwd	%xmm10, %xmm4
	pand	%xmm0, %xmm5
	punpckhwd	%xmm10, %xmm9
	movdqa	%xmm4, %xmm10
	punpckhwd	%xmm9, %xmm10
	punpcklwd	%xmm9, %xmm4
	punpcklwd	%xmm10, %xmm4
	pand	%xmm0, %xmm4
	packuswb	%xmm4, %xmm5
	movdqa	%xmm1, %xmm4
	punpcklwd	%xmm8, %xmm1
	punpckhwd	%xmm8, %xmm4
	movdqa	%xmm1, %xmm8
	punpckhwd	%xmm4, %xmm8
	punpcklwd	%xmm4, %xmm1
	movdqa	%xmm3, %xmm4
	punpcklwd	%xmm7, %xmm3
	punpckhwd	%xmm7, %xmm4
	punpcklwd	%xmm8, %xmm1
	movdqa	%xmm3, %xmm7
	punpcklwd	%xmm4, %xmm3
	pand	%xmm0, %xmm1
	punpckhwd	%xmm4, %xmm7
	movdqa	%xmm2, %xmm4
	punpcklwd	%xmm7, %xmm3
	pand	%xmm0, %xmm3
	packuswb	%xmm3, %xmm1
	movdqa	%xmm6, %xmm3
	punpckhbw	%xmm5, %xmm6
	punpcklbw	%xmm5, %xmm3
	punpcklbw	%xmm1, %xmm4
	punpckhbw	%xmm1, %xmm2
	movdqa	%xmm3, %xmm1
	punpckhbw	%xmm4, %xmm3
	punpcklbw	%xmm4, %xmm1
	movups	%xmm3, 16(%rax)
	movups	%xmm1, (%rax)
	movdqa	%xmm6, %xmm1
	punpckhbw	%xmm2, %xmm6
	punpcklbw	%xmm2, %xmm1
	movups	%xmm6, 48(%rax)
	movups	%xmm1, 32(%rax)
	cmpl	$2, %edx
	je	.L1820
	movq	16(%rsp), %rax
	cmpl	$4, 212(%rsp)
	leaq	64(%rax), %rdi
	je	.L1641
	movl	$16, %eax
.L1639:
	movl	%r8d, %esi
	subl	%eax, %esi
	leal	-1(%rsi), %edx
	movl	%esi, 72(%rsp)
	cmpl	$6, %edx
	jbe	.L1642
	movq	.LC81(%rip), %xmm6
	movl	%eax, %esi
	movq	64(%rsp), %rdx
	salq	$2, %rsi
	leaq	256(%rsp,%rsi), %rcx
	movdqa	%xmm6, %xmm5
	leaq	2(%rbx,%rdx), %rdx
	movq	(%rcx), %xmm0
	movq	8(%rcx), %xmm8
	addq	%r11, %rdx
	movq	16(%rcx), %xmm2
	movq	24(%rcx), %xmm7
	addq	%rsi, %rdx
	movdqa	%xmm0, %xmm3
	movdqa	%xmm0, %xmm1
	movq	112(%rsp), %rsi
	punpcklwd	%xmm8, %xmm3
	punpcklwd	%xmm8, %xmm1
	movdqa	%xmm2, %xmm4
	pshufd	$78, %xmm3, %xmm3
	punpcklwd	%xmm7, %xmm4
	addq	%rsi, %rdx
	movl	72(%rsp), %esi
	punpcklwd	%xmm3, %xmm1
	movdqa	%xmm2, %xmm3
	pshufd	$78, %xmm4, %xmm4
	punpcklwd	%xmm7, %xmm3
	pand	%xmm1, %xmm5
	punpcklwd	%xmm4, %xmm3
	psrlw	$8, %xmm1
	movdqa	%xmm6, %xmm4
	pand	%xmm3, %xmm4
	psrlw	$8, %xmm3
	pand	%xmm6, %xmm1
	packuswb	%xmm4, %xmm5
	pand	%xmm6, %xmm3
	movdqa	%xmm0, %xmm4
	packuswb	%xmm3, %xmm1
	psrld	$16, %xmm4
	movdqa	%xmm8, %xmm3
	psrld	$16, %xmm3
	movdqa	%xmm4, %xmm9
	pshufd	$8, %xmm5, %xmm5
	punpcklwd	%xmm3, %xmm9
	punpcklwd	%xmm3, %xmm4
	movdqa	%xmm2, %xmm3
	pshufd	$78, %xmm9, %xmm9
	psrld	$16, %xmm3
	pshufd	$8, %xmm1, %xmm1
	punpcklwd	%xmm9, %xmm4
	movdqa	%xmm7, %xmm9
	movdqa	%xmm3, %xmm10
	psrld	$16, %xmm9
	psrld	$24, %xmm0
	pand	%xmm6, %xmm4
	punpcklwd	%xmm9, %xmm10
	psrld	$24, %xmm8
	punpcklwd	%xmm9, %xmm3
	psrld	$24, %xmm2
	psrld	$24, %xmm7
	pshufd	$78, %xmm10, %xmm10
	punpcklwd	%xmm10, %xmm3
	pand	%xmm6, %xmm3
	packuswb	%xmm3, %xmm4
	movdqa	%xmm0, %xmm3
	punpcklwd	%xmm8, %xmm0
	punpcklwd	%xmm8, %xmm3
	pshufd	$8, %xmm4, %xmm4
	pshufd	$78, %xmm3, %xmm3
	punpcklwd	%xmm3, %xmm0
	movdqa	%xmm2, %xmm3
	punpcklwd	%xmm7, %xmm2
	punpcklwd	%xmm7, %xmm3
	pand	%xmm6, %xmm0
	pshufd	$78, %xmm3, %xmm3
	punpcklwd	%xmm3, %xmm2
	movdqa	%xmm1, %xmm3
	pand	%xmm6, %xmm2
	packuswb	%xmm2, %xmm0
	movdqa	%xmm5, %xmm2
	punpcklbw	%xmm4, %xmm5
	pshufd	$8, %xmm0, %xmm0
	punpcklbw	%xmm4, %xmm2
	pshufd	$78, %xmm5, %xmm5
	punpcklbw	%xmm0, %xmm3
	punpcklbw	%xmm0, %xmm1
	movdqa	%xmm2, %xmm0
	pshufd	$78, %xmm1, %xmm1
	punpcklbw	%xmm3, %xmm0
	punpcklbw	%xmm3, %xmm2
	movq	%xmm0, (%rdx)
	movdqa	%xmm5, %xmm0
	punpcklbw	%xmm1, %xmm5
	pshufd	$78, %xmm2, %xmm2
	punpcklbw	%xmm1, %xmm0
	pshufd	$78, %xmm5, %xmm5
	movq	%xmm2, 8(%rdx)
	movq	%xmm0, 16(%rdx)
	movq	%xmm5, 24(%rdx)
	movl	%esi, %edx
	andl	$-8, %edx
	addl	%edx, %eax
	andl	$7, %esi
	leaq	(%rdi,%rdx,4), %rdi
	je	.L1641
.L1642:
	movl	%eax, %edx
	movl	256(%rsp,%rdx,4), %edx
	movl	%edx, (%rdi)
	leal	1(%rax), %edx
	cmpl	%r8d, %edx
	jnb	.L1641
	movl	256(%rsp,%rdx,4), %edx
	movl	%edx, 4(%rdi)
	leal	2(%rax), %edx
	cmpl	%r8d, %edx
	jnb	.L1641
	movl	256(%rsp,%rdx,4), %edx
	movl	%edx, 8(%rdi)
	leal	3(%rax), %edx
	cmpl	%r8d, %edx
	jnb	.L1641
	movl	256(%rsp,%rdx,4), %edx
	movl	%edx, 12(%rdi)
	leal	4(%rax), %edx
	cmpl	%r8d, %edx
	jnb	.L1641
	movl	256(%rsp,%rdx,4), %edx
	movl	%edx, 16(%rdi)
	leal	5(%rax), %edx
	cmpl	%r8d, %edx
	jnb	.L1641
	movl	256(%rsp,%rdx,4), %edx
	addl	$6, %eax
	movl	%edx, 20(%rdi)
	cmpl	%r8d, %eax
	jnb	.L1641
	movl	256(%rsp,%rax,4), %eax
	movl	%eax, 24(%rdi)
.L1641:
	movq	160(%rsp), %rdx
	movq	16(%rsp), %rax
	movq	96(%rsp), %rsi
	salq	$2, %rdx
	addq	%rdx, %rax
	cmpq	%rsi, %r15
	je	.L1651
	cmpq	$14, 168(%rsp)
	jbe	.L1686
	movq	64(%rsp), %rsi
	movq	112(%rsp), %rdi
	leaq	(%rbx,%rsi), %rcx
	leaq	(%rcx,%r11), %r8
	addq	%r8, %rdx
	leaq	2(%rdi,%rdx), %rsi
	leaq	34(%rdi,%rdx), %rdx
	cmpq	%rdx, %r15
	jnb	.L1690
	leaq	32(%r15), %rcx
	xorl	%edx, %edx
	cmpq	%rcx, %rsi
	jnb	.L1690
.L1655:
	movzwl	(%r15,%rdx,2), %ecx
	movb	%cl, (%rax,%rdx,2)
	movzbl	1(%r15,%rdx,2), %ecx
	movb	%cl, 1(%rax,%rdx,2)
	addq	$1, %rdx
	cmpq	%r10, %rdx
	jb	.L1655
.L1651:
	cmpq	$0, 56(%rsp)
	je	.L1595
	movq	192(%rsp), %rax
	movq	56(%rsp), %rsi
	addq	%rsi, %rax
	je	.L1595
	cmpq	%r9, %rax
	jnb	.L1595
	movq	%rax, %r9
	jmp	.L1657
.L1553:
	movq	48(%rsp), %rdi
	call	kr_enc_free
	movq	$0, 64(%rsp)
	jmp	.L1541
.L1667:
	movl	$32768, %r13d
	movl	$49200, %r12d
	movl	$64, %ebp
	movl	$64, %ebx
	jmp	.L1548
.L1684:
	leaq	9488(%rsp), %rdi
	movq	%rsi, %rcx
	xorl	%eax, %eax
	movq	%rdi, 72(%rsp)
	leaq	9487(%rsp), %rdi
.L1635:
	orl	$-128, %edx
	movq	%rax, %r11
	shrq	$7, %rcx
	addq	$1, %rax
	movb	%dl, (%rdi,%rax)
	movl	%ecx, %edx
	cmpq	$127, %rcx
	ja	.L1635
	movq	72(%rsp), %rdi
	leaq	2(%r11), %rcx
	addq	%rdi, %rax
	jmp	.L1638
.L1818:
	movq	56(%rsp), %rax
	movq	%rsi, %r15
	movl	%r11d, 256(%rsp,%rax,4)
	movq	72(%rsp), %rax
	leaq	-1(%rax), %rcx
	testq	%rax, %rax
	jne	.L1806
	jmp	.L1622
.L1817:
	leaq	-1(%r12,%rsi), %rax
	addq	%r12, %rdi
	movq	%rax, 168(%rsp)
	jmp	.L1804
.L1822:
	movw	%r11w, -2(%rsi)
	shrl	$16, %r11d
	leaq	-2(%rsi), %r15
	movl	8(%rcx), %esi
	movq	%r11, %rax
	mulq	(%rcx)
	movzwl	12(%rcx), %eax
	subq	$1, %rdi
	addl	%r11d, %esi
	imull	%edx, %eax
	leal	(%rsi,%rax), %r11d
	movq	168(%rsp), %rax
	cmpq	%rax, %rdi
	je	.L1821
.L1804:
	movq	%r15, %rsi
.L1617:
	movzbl	(%rdi), %ecx
	movl	%r11d, %edx
	shrl	$20, %edx
	salq	$4, %rcx
	addq	%r10, %rcx
	movzwl	14(%rcx), %eax
	cmpl	%eax, %edx
	jnb	.L1822
	movl	%r11d, %eax
	mulq	(%rcx)
	movzwl	12(%rcx), %eax
	subq	$1, %rdi
	movl	8(%rcx), %r15d
	imull	%edx, %eax
	addl	%r11d, %r15d
	leal	(%rax,%r15), %r11d
	cmpq	%rdi, 168(%rsp)
	jne	.L1617
	movq	56(%rsp), %rax
	movq	%rsi, %r15
	movl	%r11d, 256(%rsp,%rax,4)
	movq	72(%rsp), %rax
	leaq	-1(%rax), %rcx
	testq	%rax, %rax
	je	.L1622
.L1807:
	movq	%rax, %rsi
.L1624:
	movq	160(%rsp), %rdx
	movq	%r12, %rax
	movq	%rbp, 184(%rsp)
	subq	%rsi, %rax
	movq	%r9, 216(%rsp)
	imulq	%rsi, %rdx
	movq	%r12, 176(%rsp)
	movq	%rsi, %r12
	addq	%rdx, %rax
	movq	%rax, 72(%rsp)
	movq	56(%rsp), %rax
	leaq	256(%rsp,%rax,4), %rax
	movq	%rax, 168(%rsp)
.L1630:
	movq	72(%rsp), %rax
	movq	168(%rsp), %r9
	movl	104(%rsp), %edi
	leaq	(%rax,%rcx), %rbp
	jmp	.L1628
.L1823:
	movl	%edi, %eax
	movl	256(%rsp,%rax,4), %r11d
.L1628:
	movzbl	0(%rbp), %esi
	movl	%r11d, %edx
	shrl	$20, %edx
	salq	$4, %rsi
	addq	%r10, %rsi
	movzwl	14(%rsi), %eax
	cmpl	%eax, %edx
	jb	.L1626
	movw	%r11w, -2(%r15)
	shrl	$16, %r11d
	subq	$2, %r15
.L1626:
	movl	%r11d, %eax
	mulq	(%rsi)
	movzwl	12(%rsi), %eax
	subl	$1, %edi
	addl	8(%rsi), %r11d
	subq	%r12, %rbp
	subq	$4, %r9
	imull	%edx, %eax
	addl	%r11d, %eax
	movl	%eax, 4(%r9)
	cmpl	$-1, %edi
	jne	.L1823
	subq	$1, %rcx
	jb	.L1799
	movq	56(%rsp), %rax
	movl	256(%rsp,%rax,4), %r11d
	jmp	.L1630
.L1792:
	cmpb	$1, %al
	jbe	.L1568
	jmp	.L1591
.L1820:
	movdqa	320(%rsp), %xmm1
	movdqa	336(%rsp), %xmm8
	movdqa	352(%rsp), %xmm3
	movdqa	368(%rsp), %xmm7
	movdqa	%xmm1, %xmm4
	movdqa	%xmm1, %xmm2
	movdqa	%xmm8, %xmm9
	punpcklwd	%xmm8, %xmm4
	punpckhwd	%xmm8, %xmm2
	movdqa	%xmm7, %xmm10
	psrld	$16, %xmm9
	psrld	$16, %xmm10
	movdqa	%xmm4, %xmm5
	punpckhwd	%xmm2, %xmm5
	psrld	$24, %xmm8
	punpcklwd	%xmm2, %xmm4
	movdqa	%xmm3, %xmm2
	punpcklwd	%xmm5, %xmm4
	movdqa	%xmm3, %xmm5
	punpcklwd	%xmm7, %xmm2
	punpckhwd	%xmm7, %xmm5
	movdqa	%xmm2, %xmm6
	psrld	$24, %xmm7
	punpcklwd	%xmm5, %xmm2
	punpckhwd	%xmm5, %xmm6
	movdqa	%xmm0, %xmm5
	punpcklwd	%xmm6, %xmm2
	movdqa	%xmm0, %xmm6
	pand	%xmm2, %xmm5
	pand	%xmm4, %xmm6
	packuswb	%xmm5, %xmm6
	movdqa	%xmm4, %xmm5
	movdqa	%xmm2, %xmm4
	psrlw	$8, %xmm5
	psrlw	$8, %xmm4
	pand	%xmm0, %xmm5
	pand	%xmm0, %xmm4
	movdqa	%xmm5, %xmm2
	movdqa	%xmm1, %xmm5
	psrld	$16, %xmm5
	psrld	$24, %xmm1
	packuswb	%xmm4, %xmm2
	movdqa	%xmm5, %xmm4
	punpcklwd	%xmm9, %xmm5
	punpckhwd	%xmm9, %xmm4
	movdqa	%xmm5, %xmm9
	punpckhwd	%xmm4, %xmm9
	punpcklwd	%xmm4, %xmm5
	movdqa	%xmm3, %xmm4
	psrld	$16, %xmm4
	psrld	$24, %xmm3
	punpcklwd	%xmm9, %xmm5
	movdqa	%xmm4, %xmm9
	punpcklwd	%xmm10, %xmm4
	pand	%xmm0, %xmm5
	punpckhwd	%xmm10, %xmm9
	movdqa	%xmm4, %xmm10
	punpckhwd	%xmm9, %xmm10
	punpcklwd	%xmm9, %xmm4
	punpcklwd	%xmm10, %xmm4
	pand	%xmm0, %xmm4
	packuswb	%xmm4, %xmm5
	movdqa	%xmm1, %xmm4
	punpcklwd	%xmm8, %xmm1
	punpckhwd	%xmm8, %xmm4
	movdqa	%xmm1, %xmm8
	punpckhwd	%xmm4, %xmm8
	punpcklwd	%xmm4, %xmm1
	movdqa	%xmm3, %xmm4
	punpcklwd	%xmm7, %xmm3
	punpckhwd	%xmm7, %xmm4
	punpcklwd	%xmm8, %xmm1
	movdqa	%xmm3, %xmm7
	punpcklwd	%xmm4, %xmm3
	pand	%xmm0, %xmm1
	punpckhwd	%xmm4, %xmm7
	punpcklwd	%xmm7, %xmm3
	pand	%xmm0, %xmm3
	movdqa	%xmm1, %xmm0
	movdqa	%xmm6, %xmm1
	packuswb	%xmm3, %xmm0
	punpcklbw	%xmm5, %xmm1
	movdqa	%xmm2, %xmm3
	punpcklbw	%xmm0, %xmm3
	punpckhbw	%xmm0, %xmm2
	movdqa	%xmm1, %xmm0
	punpckhbw	%xmm5, %xmm6
	punpcklbw	%xmm3, %xmm0
	punpckhbw	%xmm3, %xmm1
	movups	%xmm0, 64(%rax)
	movdqa	%xmm6, %xmm0
	punpckhbw	%xmm2, %xmm6
	punpcklbw	%xmm2, %xmm0
	movups	%xmm1, 80(%rax)
	movups	%xmm0, 96(%rax)
	movups	%xmm6, 112(%rax)
	jmp	.L1641
.L1664:
	movl	$131072, %esi
	jmp	.L1545
.L1816:
	movsd	2896(%r15), %xmm0
	movq	%r15, 16(%rsp)
	jmp	.L1568
	.p2align 4,,10
	.p2align 3
.L1574:
	movl	408(%rsp), %r9d
	testl	%r9d, %r9d
	jne	.L1561
	cmpb	$1, 5(%r13)
	ja	.L1824
.L1797:
	movq	168(%rsp), %rbx
	movq	176(%rsp), %r14
	movq	184(%rsp), %r13
	jmp	.L1578
.L1790:
	movq	192(%rsp), %rdi
	movl	$9088, %eax
	movl	%r14d, 192(%rsp)
	movq	168(%rsp), %rbx
	movq	176(%rsp), %r14
	cmpq	%rax, %rdi
	movq	184(%rsp), %r13
	cmovnb	%rdi, %rax
	movq	%rax, %rdx
	subq	%rdi, %rdx
	cmpq	%rax, %rdx
	cmovb	%rax, %rdx
	subq	%rax, %rdi
	xorl	%esi, %esi
	leaq	(%rdx,%rdi), %r11
	movl	$28, %edx
	movq	%r15, %rdi
	movq	%r11, %rcx
	movq	%r11, 176(%rsp)
	call	*__memset_chk@GOTPCREL(%rip)
	movl	56(%rsp), %eax
	movq	%rbp, %rcx
	movq	%r15, %rsi
	movl	192(%rsp), %r10d
	movl	$1, 12(%r15)
	movl	104(%rsp), %r8d
	movq	48(%rsp), %rdi
	movl	%eax, (%r15)
	movl	72(%rsp), %eax
	movl	%r10d, 4(%r15)
	movl	%r10d, 32(%r15)
	movl	%eax, 8(%r15)
	movq	120(%rsp), %rax
	movl	$4, 28(%r15)
	movl	(%rax), %eax
	movl	%r10d, 168(%rsp)
	leal	(%r10,%rax), %edx
	movl	%eax, 36(%r15)
	subl	%r10d, %eax
	movl	%edx, 40(%r15)
	movq	%r12, %rdx
	movl	%eax, 44(%r15)
	call	kr_plan_build
	movl	104(%rsp), %r9d
	movl	168(%rsp), %r10d
	testl	%eax, %eax
	movq	176(%rsp), %r11
	jne	.L1580
	movq	16(%rsp), %rax
	testq	%rax, %rax
	je	.L1581
	movsd	2896(%rax), %xmm0
	comisd	2896(%r15), %xmm0
	jbe	.L1791
.L1581:
	movq	160(%rsp), %rax
	movl	$0, %edx
	movq	48(%rsp), %rsi
	cmpq	%rax, %r15
	movl	$2952, %eax
	cmovne	%rdx, %rax
	movl	$9056, %edx
	subq	%rax, %rdx
	leaq	32(%rsi,%rax), %rax
	movq	%rax, 16(%rsp)
	movq	%rdx, %r11
	movq	%rax, %rdi
.L1582:
	xorl	%esi, %esi
	movl	%r10d, 104(%rsp)
	movq	%r11, %rcx
	movl	$48, %edx
	movl	%r9d, 160(%rsp)
	call	*__memset_chk@GOTPCREL(%rip)
	movq	16(%rsp), %rsi
	movq	%rbp, %rcx
	movq	%r12, %rdx
	movl	56(%rsp), %eax
	movl	104(%rsp), %r10d
	movl	$2, 28(%rsi)
	movl	160(%rsp), %r8d
	movl	%eax, (%rsi)
	movl	72(%rsp), %eax
	movl	%r10d, 4(%rsi)
	movq	48(%rsp), %rdi
	movl	%eax, 8(%rsi)
	movq	.LC78(%rip), %rax
	movl	%r10d, 32(%rsi)
	movq	%rax, 12(%rsi)
	movl	96(%rsp), %eax
	movl	%eax, 24(%rsi)
	movq	120(%rsp), %rax
	movl	(%rax), %eax
	movl	%eax, 36(%rsi)
	call	kr_plan_build
	testl	%eax, %eax
	jne	.L1584
	movq	16(%rsp), %rax
	movsd	2896(%rax), %xmm0
	testq	%r15, %r15
	je	.L1568
	movsd	2896(%r15), %xmm1
	comisd	%xmm0, %xmm1
	minsd	%xmm1, %xmm0
	cmova	%rax, %r15
	movq	%r15, 16(%rsp)
	jmp	.L1568
.L1580:
	movl	408(%rsp), %r8d
	testl	%r8d, %r8d
	jne	.L1561
.L1791:
	movq	%r15, %rax
	movq	16(%rsp), %r15
	movq	%rax, %rdi
	movq	%rax, 16(%rsp)
	jmp	.L1582
.L1584:
	movl	408(%rsp), %edi
	testl	%edi, %edi
	jne	.L1561
	movq	%r15, 16(%rsp)
	jmp	.L1578
.L1689:
	movsd	2896(%r15), %xmm0
	movq	16(%rsp), %rax
	movsd	2896(%rax), %xmm1
	comisd	%xmm0, %xmm1
	minsd	%xmm1, %xmm0
	cmova	%r15, %rax
	movq	%rax, 16(%rsp)
	jmp	.L1568
.L1821:
	movq	56(%rsp), %rax
	movl	%r11d, 256(%rsp,%rax,4)
	movq	72(%rsp), %rax
	leaq	-1(%rax), %rcx
	testq	%rax, %rax
	jne	.L1807
	jmp	.L1622
	.p2align 4,,10
	.p2align 3
.L1690:
	movq	168(%rsp), %rcx
	movl	$1, %edi
	testq	%rcx, %rcx
	cmovne	%r10, %rdi
	cmpq	$30, %rcx
	jbe	.L1688
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movdqa	.LC54(%rip), %xmm0
	movq	%r15, %rcx
	shrq	$4, %rsi
	salq	$5, %rsi
	addq	%rdx, %rsi
.L1649:
	movdqu	(%rcx), %xmm2
	movdqu	16(%rcx), %xmm1
	addq	$32, %rdx
	addq	$32, %rcx
	movdqu	-16(%rcx), %xmm3
	pand	%xmm0, %xmm1
	pand	%xmm0, %xmm2
	packuswb	%xmm1, %xmm2
	movdqu	-32(%rcx), %xmm1
	psrlw	$8, %xmm3
	pand	%xmm0, %xmm3
	psrlw	$8, %xmm1
	pand	%xmm0, %xmm1
	packuswb	%xmm3, %xmm1
	movdqa	%xmm2, %xmm3
	punpcklbw	%xmm1, %xmm3
	punpckhbw	%xmm1, %xmm2
	movups	%xmm3, -32(%rdx)
	movups	%xmm2, -16(%rdx)
	cmpq	%rsi, %rdx
	jne	.L1649
	movq	%rdi, %rdx
	andq	$-16, %rdx
	leaq	(%rdx,%rdx), %r11
	addq	%r11, %rax
	testb	$15, %dil
	je	.L1651
.L1648:
	subq	%rdx, %rdi
	leaq	-1(%rdi), %rsi
	cmpq	$6, %rsi
	jbe	.L1653
	movq	.LC81(%rip), %xmm2
	leaq	(%r11,%r15), %rcx
	movq	176(%rsp), %rsi
	movq	(%rcx), %xmm0
	movq	8(%rcx), %xmm3
	movq	%rdi, %rcx
	movdqa	%xmm2, %xmm1
	movdqa	%xmm2, %xmm4
	leaq	1(%rsi,%rdx), %rsi
	andq	$-8, %rcx
	pand	%xmm0, %xmm1
	pand	%xmm3, %xmm4
	leaq	(%r8,%rsi,2), %rsi
	addq	%rcx, %rdx
	psrlw	$8, %xmm0
	psrlw	$8, %xmm3
	packuswb	%xmm4, %xmm1
	movq	112(%rsp), %r8
	pand	%xmm2, %xmm0
	pand	%xmm3, %xmm2
	pshufd	$8, %xmm1, %xmm1
	packuswb	%xmm2, %xmm0
	movdqa	%xmm1, %xmm2
	leaq	(%rax,%rcx,2), %rax
	addq	%r8, %rsi
	pshufd	$8, %xmm0, %xmm0
	andl	$7, %edi
	punpcklbw	%xmm0, %xmm1
	punpcklbw	%xmm0, %xmm2
	pshufd	$78, %xmm1, %xmm1
	movq	%xmm2, (%rsi)
	movq	%xmm1, 8(%rsi)
	je	.L1651
	leaq	(%rdx,%rdx), %r11
.L1653:
	leaq	(%r15,%r11), %rsi
	movzwl	(%rsi), %edi
	movb	%dil, (%rax)
	movzwl	(%rsi), %ecx
	leaq	1(%rdx), %rsi
	movb	%ch, 1(%rax)
	cmpq	%r10, %rsi
	jnb	.L1651
	leaq	2(%r15,%r11), %rsi
	movzwl	(%rsi), %edi
	movb	%dil, 2(%rax)
	movzwl	(%rsi), %ecx
	leaq	2(%rdx), %rsi
	movb	%ch, 3(%rax)
	cmpq	%r10, %rsi
	jnb	.L1651
	leaq	4(%r15,%r11), %rsi
	movzwl	(%rsi), %edi
	movb	%dil, 4(%rax)
	movzwl	(%rsi), %ecx
	leaq	3(%rdx), %rsi
	movb	%ch, 5(%rax)
	cmpq	%r10, %rsi
	jnb	.L1651
	leaq	6(%r15,%r11), %rsi
	movzwl	(%rsi), %edi
	movb	%dil, 6(%rax)
	movzwl	(%rsi), %ecx
	leaq	4(%rdx), %rsi
	movb	%ch, 7(%rax)
	cmpq	%r10, %rsi
	jnb	.L1651
	leaq	8(%r15,%r11), %rsi
	movzwl	(%rsi), %edi
	movb	%dil, 8(%rax)
	movzwl	(%rsi), %ecx
	leaq	5(%rdx), %rsi
	movb	%ch, 9(%rax)
	cmpq	%r10, %rsi
	jnb	.L1651
	leaq	10(%r15,%r11), %rsi
	addq	$6, %rdx
	movzwl	(%rsi), %edi
	movb	%dil, 10(%rax)
	movzwl	(%rsi), %ecx
	movb	%ch, 11(%rax)
	cmpq	%r10, %rdx
	jnb	.L1651
	leaq	12(%r15,%r11), %rdx
	movzwl	(%rdx), %ecx
	movb	%cl, 12(%rax)
	movzwl	(%rdx), %edx
	movb	%dh, 13(%rax)
	jmp	.L1651
.L1686:
	xorl	%edx, %edx
	jmp	.L1655
.L1799:
	movq	176(%rsp), %r12
	movq	184(%rsp), %rbp
	movq	216(%rsp), %r9
	jmp	.L1622
.L1688:
	xorl	%edx, %edx
	xorl	%r11d, %r11d
	jmp	.L1648
.L1685:
	movq	%rax, %rdi
	xorl	%eax, %eax
	jmp	.L1639
.L1592:
	movl	408(%rsp), %ecx
	testl	%ecx, %ecx
	je	.L1578
	jmp	.L1561
.L1611:
	movq	72(%rsp), %rsi
	leaq	-1(%rsi), %rcx
	testq	%rsi, %rsi
	je	.L1682
	movq	8440(%rsp), %r10
	movq	96(%rsp), %r15
	cmpl	$1, %eax
	je	.L1624
	movq	%rsi, %rdi
	jmp	.L1623
.L1812:
	call	*__stack_chk_fail@GOTPCREL(%rip)
.L1682:
	movq	96(%rsp), %r15
	jmp	.L1622
.L1676:
	xorl	%eax, %eax
	jmp	.L1601
.L1669:
	movq	$1, 32(%rsp)
	xorl	%ebx, %ebx
	jmp	.L1555
	.cfi_endproc
.LFE64:
	.size	kolmo_krans_compress, .-kolmo_krans_compress
	.section	.rodata
	.align 8
	.type	CSWTCH.114, @object
	.size	CSWTCH.114, 12
CSWTCH.114:
	.long	256
	.long	6
	.long	16
	.align 32
	.type	def.0, @object
	.size	def.0, 44
def.0:
	.zero	44
	.type	kr_mbits, @object
	.size	kr_mbits, 4
kr_mbits:
	.ascii	"\003\004\006\f"
	.section	.rodata.cst16,"aM",@progbits,16
	.align 16
.LC0:
	.long	0
	.long	1
	.long	2
	.long	3
	.align 16
.LC1:
	.byte	0
	.byte	1
	.byte	2
	.byte	3
	.byte	4
	.byte	5
	.byte	6
	.byte	7
	.byte	8
	.byte	9
	.byte	10
	.byte	11
	.byte	12
	.byte	13
	.byte	14
	.byte	15
	.align 16
.LC2:
	.value	0
	.value	1
	.value	2
	.value	3
	.value	4
	.value	5
	.value	6
	.value	7
	.align 16
.LC3:
	.value	8
	.value	9
	.value	10
	.value	11
	.value	12
	.value	13
	.value	14
	.value	15
	.align 16
.LC4:
	.byte	16
	.byte	17
	.byte	18
	.byte	19
	.byte	20
	.byte	21
	.byte	22
	.byte	23
	.byte	24
	.byte	25
	.byte	26
	.byte	27
	.byte	28
	.byte	29
	.byte	30
	.byte	31
	.align 16
.LC5:
	.value	16
	.value	17
	.value	18
	.value	19
	.value	20
	.value	21
	.value	22
	.value	23
	.align 16
.LC6:
	.value	24
	.value	25
	.value	26
	.value	27
	.value	28
	.value	29
	.value	30
	.value	31
	.align 16
.LC7:
	.byte	32
	.byte	33
	.byte	34
	.byte	35
	.byte	36
	.byte	37
	.byte	38
	.byte	39
	.byte	40
	.byte	41
	.byte	42
	.byte	43
	.byte	44
	.byte	45
	.byte	46
	.byte	47
	.align 16
.LC8:
	.value	32
	.value	33
	.value	34
	.value	35
	.value	36
	.value	37
	.value	38
	.value	39
	.align 16
.LC9:
	.value	40
	.value	41
	.value	42
	.value	43
	.value	44
	.value	45
	.value	46
	.value	47
	.align 16
.LC10:
	.byte	48
	.byte	49
	.byte	50
	.byte	51
	.byte	52
	.byte	53
	.byte	54
	.byte	55
	.byte	56
	.byte	57
	.byte	58
	.byte	59
	.byte	60
	.byte	61
	.byte	62
	.byte	63
	.align 16
.LC11:
	.value	48
	.value	49
	.value	50
	.value	51
	.value	52
	.value	53
	.value	54
	.value	55
	.align 16
.LC12:
	.value	56
	.value	57
	.value	58
	.value	59
	.value	60
	.value	61
	.value	62
	.value	63
	.align 16
.LC13:
	.byte	64
	.byte	65
	.byte	66
	.byte	67
	.byte	68
	.byte	69
	.byte	70
	.byte	71
	.byte	72
	.byte	73
	.byte	74
	.byte	75
	.byte	76
	.byte	77
	.byte	78
	.byte	79
	.align 16
.LC14:
	.value	64
	.value	65
	.value	66
	.value	67
	.value	68
	.value	69
	.value	70
	.value	71
	.align 16
.LC15:
	.value	72
	.value	73
	.value	74
	.value	75
	.value	76
	.value	77
	.value	78
	.value	79
	.align 16
.LC16:
	.byte	80
	.byte	81
	.byte	82
	.byte	83
	.byte	84
	.byte	85
	.byte	86
	.byte	87
	.byte	88
	.byte	89
	.byte	90
	.byte	91
	.byte	92
	.byte	93
	.byte	94
	.byte	95
	.align 16
.LC17:
	.value	80
	.value	81
	.value	82
	.value	83
	.value	84
	.value	85
	.value	86
	.value	87
	.align 16
.LC18:
	.value	88
	.value	89
	.value	90
	.value	91
	.value	92
	.value	93
	.value	94
	.value	95
	.align 16
.LC19:
	.byte	96
	.byte	97
	.byte	98
	.byte	99
	.byte	100
	.byte	101
	.byte	102
	.byte	103
	.byte	104
	.byte	105
	.byte	106
	.byte	107
	.byte	108
	.byte	109
	.byte	110
	.byte	111
	.align 16
.LC20:
	.value	96
	.value	97
	.value	98
	.value	99
	.value	100
	.value	101
	.value	102
	.value	103
	.align 16
.LC21:
	.value	104
	.value	105
	.value	106
	.value	107
	.value	108
	.value	109
	.value	110
	.value	111
	.align 16
.LC22:
	.byte	112
	.byte	113
	.byte	114
	.byte	115
	.byte	116
	.byte	117
	.byte	118
	.byte	119
	.byte	120
	.byte	121
	.byte	122
	.byte	123
	.byte	124
	.byte	125
	.byte	126
	.byte	127
	.align 16
.LC23:
	.value	112
	.value	113
	.value	114
	.value	115
	.value	116
	.value	117
	.value	118
	.value	119
	.align 16
.LC24:
	.value	120
	.value	121
	.value	122
	.value	123
	.value	124
	.value	125
	.value	126
	.value	127
	.align 16
.LC25:
	.byte	-128
	.byte	-127
	.byte	-126
	.byte	-125
	.byte	-124
	.byte	-123
	.byte	-122
	.byte	-121
	.byte	-120
	.byte	-119
	.byte	-118
	.byte	-117
	.byte	-116
	.byte	-115
	.byte	-114
	.byte	-113
	.align 16
.LC26:
	.value	128
	.value	129
	.value	130
	.value	131
	.value	132
	.value	133
	.value	134
	.value	135
	.align 16
.LC27:
	.value	136
	.value	137
	.value	138
	.value	139
	.value	140
	.value	141
	.value	142
	.value	143
	.align 16
.LC28:
	.byte	-112
	.byte	-111
	.byte	-110
	.byte	-109
	.byte	-108
	.byte	-107
	.byte	-106
	.byte	-105
	.byte	-104
	.byte	-103
	.byte	-102
	.byte	-101
	.byte	-100
	.byte	-99
	.byte	-98
	.byte	-97
	.align 16
.LC29:
	.value	144
	.value	145
	.value	146
	.value	147
	.value	148
	.value	149
	.value	150
	.value	151
	.align 16
.LC30:
	.value	152
	.value	153
	.value	154
	.value	155
	.value	156
	.value	157
	.value	158
	.value	159
	.align 16
.LC31:
	.byte	-96
	.byte	-95
	.byte	-94
	.byte	-93
	.byte	-92
	.byte	-91
	.byte	-90
	.byte	-89
	.byte	-88
	.byte	-87
	.byte	-86
	.byte	-85
	.byte	-84
	.byte	-83
	.byte	-82
	.byte	-81
	.align 16
.LC32:
	.value	160
	.value	161
	.value	162
	.value	163
	.value	164
	.value	165
	.value	166
	.value	167
	.align 16
.LC33:
	.value	168
	.value	169
	.value	170
	.value	171
	.value	172
	.value	173
	.value	174
	.value	175
	.align 16
.LC34:
	.byte	-80
	.byte	-79
	.byte	-78
	.byte	-77
	.byte	-76
	.byte	-75
	.byte	-74
	.byte	-73
	.byte	-72
	.byte	-71
	.byte	-70
	.byte	-69
	.byte	-68
	.byte	-67
	.byte	-66
	.byte	-65
	.align 16
.LC35:
	.value	176
	.value	177
	.value	178
	.value	179
	.value	180
	.value	181
	.value	182
	.value	183
	.align 16
.LC36:
	.value	184
	.value	185
	.value	186
	.value	187
	.value	188
	.value	189
	.value	190
	.value	191
	.align 16
.LC37:
	.byte	-64
	.byte	-63
	.byte	-62
	.byte	-61
	.byte	-60
	.byte	-59
	.byte	-58
	.byte	-57
	.byte	-56
	.byte	-55
	.byte	-54
	.byte	-53
	.byte	-52
	.byte	-51
	.byte	-50
	.byte	-49
	.align 16
.LC38:
	.value	192
	.value	193
	.value	194
	.value	195
	.value	196
	.value	197
	.value	198
	.value	199
	.align 16
.LC39:
	.value	200
	.value	201
	.value	202
	.value	203
	.value	204
	.value	205
	.value	206
	.value	207
	.align 16
.LC40:
	.byte	-48
	.byte	-47
	.byte	-46
	.byte	-45
	.byte	-44
	.byte	-43
	.byte	-42
	.byte	-41
	.byte	-40
	.byte	-39
	.byte	-38
	.byte	-37
	.byte	-36
	.byte	-35
	.byte	-34
	.byte	-33
	.align 16
.LC41:
	.value	208
	.value	209
	.value	210
	.value	211
	.value	212
	.value	213
	.value	214
	.value	215
	.align 16
.LC42:
	.value	216
	.value	217
	.value	218
	.value	219
	.value	220
	.value	221
	.value	222
	.value	223
	.align 16
.LC43:
	.byte	-32
	.byte	-31
	.byte	-30
	.byte	-29
	.byte	-28
	.byte	-27
	.byte	-26
	.byte	-25
	.byte	-24
	.byte	-23
	.byte	-22
	.byte	-21
	.byte	-20
	.byte	-19
	.byte	-18
	.byte	-17
	.align 16
.LC44:
	.value	224
	.value	225
	.value	226
	.value	227
	.value	228
	.value	229
	.value	230
	.value	231
	.align 16
.LC45:
	.value	232
	.value	233
	.value	234
	.value	235
	.value	236
	.value	237
	.value	238
	.value	239
	.align 16
.LC46:
	.byte	-16
	.byte	-15
	.byte	-14
	.byte	-13
	.byte	-12
	.byte	-11
	.byte	-10
	.byte	-9
	.byte	-8
	.byte	-7
	.byte	-6
	.byte	-5
	.byte	-4
	.byte	-3
	.byte	-2
	.byte	-1
	.align 16
.LC47:
	.value	240
	.value	241
	.value	242
	.value	243
	.value	244
	.value	245
	.value	246
	.value	247
	.align 16
.LC48:
	.value	248
	.value	249
	.value	250
	.value	251
	.value	252
	.value	253
	.value	254
	.value	255
	.align 16
.LC49:
	.long	16
	.long	16
	.long	16
	.long	16
	.align 16
.LC50:
	.long	1
	.long	1
	.long	1
	.long	1
	.align 16
.LC51:
	.long	5
	.long	5
	.long	5
	.long	5
	.align 16
.LC52:
	.long	9
	.long	9
	.long	9
	.long	9
	.align 16
.LC53:
	.long	13
	.long	13
	.long	13
	.long	13
	.align 16
.LC54:
	.value	255
	.value	255
	.value	255
	.value	255
	.value	255
	.value	255
	.value	255
	.value	255
	.align 16
.LC55:
	.long	4
	.long	4
	.long	4
	.long	4
	.align 16
.LC56:
	.long	8
	.long	8
	.long	8
	.long	8
	.align 16
.LC57:
	.long	12
	.long	12
	.long	12
	.long	12
	.align 16
.LC58:
	.long	-1
	.long	0
	.long	0
	.long	0
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC60:
	.long	-2013235812
	.long	2117592124
	.align 8
.LC62:
	.long	0
	.long	1077477376
	.align 8
.LC63:
	.long	0
	.long	1072693248
	.section	.rodata.cst4,"aM",@progbits,4
	.align 4
.LC64:
	.long	1900671690
	.section	.rodata.cst8
	.align 8
.LC65:
	.long	0
	.long	1085276160
	.align 8
.LC66:
	.long	0
	.long	1085275648
	.align 8
.LC67:
	.long	1889785610
	.long	1072630333
	.align 8
.LC68:
	.long	0
	.long	1076363264
	.align 8
.LC69:
	.long	0
	.long	1077936128
	.align 8
.LC71:
	.long	0
	.long	1075314688
	.align 8
.LC72:
	.long	0
	.long	1071644672
	.align 8
.LC73:
	.long	0
	.long	1074266112
	.section	.rodata.cst16
	.align 16
.LC75:
	.quad	1
	.quad	1
	.align 16
.LC76:
	.value	5120
	.value	5120
	.value	5120
	.value	5120
	.value	5120
	.value	5120
	.value	5120
	.value	5120
	.section	.rodata.cst8
	.align 8
.LC77:
	.long	1
	.long	2
	.align 8
.LC78:
	.long	2
	.long	2
	.align 8
.LC79:
	.long	0
	.long	1069547520
	.section	.rodata.cst16
	.align 16
.LC80:
	.long	65536
	.long	65536
	.long	65536
	.long	65536
	.set	.LC81,.LC54
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
