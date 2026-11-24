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
