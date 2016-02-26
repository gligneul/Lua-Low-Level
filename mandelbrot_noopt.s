	.text
	.file	"mandelbrot.bc"
	.globl	lll
	.align	16, 0x90
	.type	lll,@function
lll:                                    # @lll
	.cfi_startproc
# BB#0:                                 # %block.8.FORPREP
	pushq	%r15
.Ltmp0:
	.cfi_def_cfa_offset 16
	pushq	%r14
.Ltmp1:
	.cfi_def_cfa_offset 24
	pushq	%rbx
.Ltmp2:
	.cfi_def_cfa_offset 32
	subq	$16, %rsp
.Ltmp3:
	.cfi_def_cfa_offset 48
.Ltmp4:
	.cfi_offset %rbx, -32
.Ltmp5:
	.cfi_offset %r14, -24
.Ltmp6:
	.cfi_offset %r15, -16
	movq	%rdi, %r14
	movq	32(%r14), %rbx
	movq	32(%rbx), %rax
	movq	44609744, %rcx
	movq	44609752, %rdx
	movq	%rdx, 8(%rax)
	movq	%rcx, (%rax)
	movq	32(%rbx), %rax
	movq	(%rax), %rcx
	movq	8(%rax), %rdx
	movq	%rdx, 24(%rax)
	movq	%rcx, 16(%rax)
	movq	32(%rbx), %rcx
	leaq	32(%rcx), %rsi
	movl	$44609760, %edx         # imm = 0x2A8B0E0
                                        # kill: RDI<def> R14<kill>
	callq	lll_divrr
	movq	32(%rbx), %rax
	movq	44609776, %rcx
	movq	44609784, %rdx
	movq	%rdx, 56(%rax)
	movq	%rcx, 48(%rax)
	movq	32(%rbx), %rax
	movq	44609792, %rcx
	movq	44609800, %rdx
	movq	%rdx, 72(%rax)
	movq	%rcx, 64(%rax)
	movq	32(%rbx), %rax
	movq	44609808, %rcx
	movq	44609816, %rdx
	movq	%rdx, 88(%rax)
	movq	%rcx, 80(%rax)
	movq	32(%rbx), %rdx
	leaq	96(%rdx), %rsi
	addq	$16, %rdx
	movl	$44609824, %ecx         # imm = 0x2A8B120
	movq	%r14, %rdi
	callq	lll_subrr
	movq	32(%rbx), %rax
	movq	44609824, %rcx
	movq	44609832, %rdx
	movq	%rdx, 120(%rax)
	movq	%rcx, 112(%rax)
	movq	32(%rbx), %rsi
	addq	$80, %rsi
	movq	%r14, %rdi
	callq	lll_forprep
	jmp	.LBB0_17
	.align	16, 0x90
.LBB0_1:                                # %block.15.FORPREP
                                        #   in Loop: Header=BB0_17 Depth=1
	movq	32(%rbx), %rcx
	leaq	144(%rcx), %rsi
	subq	$-128, %rcx
	movl	$44609760, %edx         # imm = 0x2A8B0E0
	movq	%r14, %rdi
	callq	lll_mulrr
	movq	32(%rbx), %rcx
	leaq	144(%rcx), %rsi
	addq	$16, %rcx
	movq	%r14, %rdi
	movq	%rsi, %rdx
	callq	lll_divrr
	movq	32(%rbx), %rsi
	addq	$144, %rsi
	movl	$44609824, %ecx         # imm = 0x2A8B120
	movq	%r14, %rdi
	movq	%rsi, %rdx
	callq	lll_subrr
	movq	32(%rbx), %rax
	movq	44609808, %rcx
	movq	44609816, %rdx
	movq	%rdx, 168(%rax)
	movq	%rcx, 160(%rax)
	movq	32(%rbx), %rdx
	leaq	176(%rdx), %rsi
	movl	$44609824, %ecx         # imm = 0x2A8B120
	movq	%r14, %rdi
	callq	lll_subrr
	movq	32(%rbx), %rax
	movq	44609840, %rcx
	movq	44609848, %rdx
	movq	%rdx, 200(%rax)
	movq	%rcx, 192(%rax)
	movq	32(%rbx), %rsi
	addq	$160, %rsi
	movq	%r14, %rdi
	callq	lll_forprep
	jmp	.LBB0_16
	.align	16, 0x90
.LBB0_2:                                # %block.19.LT
                                        #   in Loop: Header=BB0_16 Depth=2
	movq	32(%rbx), %rax
	movq	44609808, %rcx
	movq	44609816, %rdx
	movq	%rdx, 232(%rax)
	movq	%rcx, 224(%rax)
	movq	32(%rbx), %rdx
	leaq	240(%rdx), %rsi
	addq	$208, %rdx
	movl	$44609856, %ecx         # imm = 0x2A8B140
	movq	%r14, %rdi
	callq	lll_addrr
	movq	32(%rbx), %rax
	movq	208(%rax), %rcx
	movq	216(%rax), %rdx
	movq	%rdx, 264(%rax)
	movq	%rcx, 256(%rax)
	movq	32(%rbx), %rdx
	leaq	240(%rdx), %rsi
	movq	%r14, %rdi
	callq	luaV_lessthan
	testl	%eax, %eax
	je	.LBB0_5
# BB#3:                                 # %block.21.TESTSET
                                        #   in Loop: Header=BB0_16 Depth=2
	movq	32(%rbx), %r15
	leaq	240(%r15), %rsi
	movl	$1, %edi
	callq	lll_test
	testl	%eax, %eax
	je	.LBB0_4
.LBB0_5:                                # %block.23.SUB
                                        #   in Loop: Header=BB0_16 Depth=2
	movq	32(%rbx), %rdx
	leaq	272(%rdx), %rsi
	movl	$44609824, %ecx         # imm = 0x2A8B120
	movq	%r14, %rdi
	callq	lll_subrr
	jmp	.LBB0_6
	.align	16, 0x90
.LBB0_4:                                # %block.22.JMP
                                        #   in Loop: Header=BB0_16 Depth=2
	movq	240(%r15), %rax
	movq	248(%r15), %rcx
	movq	%rcx, 280(%r15)
	movq	%rax, 272(%r15)
.LBB0_6:                                # %block.25.FORPREP
                                        #   in Loop: Header=BB0_16 Depth=2
	movq	32(%rbx), %rax
	movq	44609824, %rcx
	movq	44609832, %rdx
	movq	%rdx, 296(%rax)
	movq	%rcx, 288(%rax)
	movq	32(%rbx), %rsi
	addq	$256, %rsi              # imm = 0x100
	movq	%r14, %rdi
	callq	lll_forprep
	jmp	.LBB0_11
	.align	16, 0x90
.LBB0_8:                                # %block.48.JMP
                                        #   in Loop: Header=BB0_11 Depth=3
	movq	32(%rbx), %rsi
	addq	$224, %rsi
	movl	$44609824, %ecx         # imm = 0x2A8B120
	movq	%r14, %rdi
	movq	%rsi, %rdx
	callq	lll_addrr
	jmp	.LBB0_11
	.align	16, 0x90
.LBB0_10:                               # %block.49.FORLOOP
                                        #   Parent Loop BB0_17 Depth=1
                                        #     Parent Loop BB0_16 Depth=2
                                        #       Parent Loop BB0_11 Depth=3
                                        # =>      This Inner Loop Header: Depth=4
	movq	32(%rbx), %rdi
	addq	$400, %rdi              # imm = 0x190
	callq	lll_forloop
	testl	%eax, %eax
	je	.LBB0_11
# BB#7:                                 # %block.45.LT
                                        #   in Loop: Header=BB0_10 Depth=4
	movq	32(%rbx), %rax
	leaq	464(%rax), %rsi
	leaq	320(%rax), %rdx
	leaq	336(%rax), %rcx
	movq	%r14, %rdi
	callq	lll_mulrr
	movq	32(%rbx), %rax
	leaq	480(%rax), %rsi
	leaq	352(%rax), %rdx
	leaq	368(%rax), %rcx
	movq	%r14, %rdi
	callq	lll_subrr
	movq	32(%rbx), %rax
	leaq	320(%rax), %rsi
	leaq	480(%rax), %rdx
	leaq	384(%rax), %rcx
	movq	%r14, %rdi
	callq	lll_addrr
	movq	32(%rbx), %rdx
	leaq	480(%rdx), %rsi
	addq	$464, %rdx              # imm = 0x1D0
	movq	%r14, %rdi
	movq	%rdx, %rcx
	callq	lll_addrr
	movq	32(%rbx), %rax
	leaq	336(%rax), %rsi
	leaq	480(%rax), %rdx
	leaq	144(%rax), %rcx
	movq	%r14, %rdi
	callq	lll_addrr
	movq	32(%rbx), %rdx
	leaq	352(%rdx), %rsi
	addq	$320, %rdx              # imm = 0x140
	movq	%r14, %rdi
	movq	%rdx, %rcx
	callq	lll_mulrr
	movq	32(%rbx), %rdx
	leaq	368(%rdx), %rsi
	addq	$336, %rdx              # imm = 0x150
	movq	%r14, %rdi
	movq	%rdx, %rcx
	callq	lll_mulrr
	movq	32(%rbx), %rax
	leaq	480(%rax), %rsi
	leaq	352(%rax), %rdx
	leaq	368(%rax), %rcx
	movq	%r14, %rdi
	callq	lll_addrr
	movq	32(%rbx), %rdx
	leaq	64(%rdx), %rsi
	addq	$480, %rdx              # imm = 0x1E0
	movq	%r14, %rdi
	callq	luaV_lessthan
	testl	%eax, %eax
	je	.LBB0_10
	jmp	.LBB0_8
	.align	16, 0x90
.LBB0_11:                               # %block.50.FORLOOP
                                        #   Parent Loop BB0_17 Depth=1
                                        #     Parent Loop BB0_16 Depth=2
                                        # =>    This Loop Header: Depth=3
                                        #         Child Loop BB0_10 Depth 4
	movq	32(%rbx), %rdi
	addq	$256, %rdi              # imm = 0x100
	callq	lll_forloop
	testl	%eax, %eax
	je	.LBB0_12
# BB#9:                                 # %block.36.FORPREP
                                        #   in Loop: Header=BB0_11 Depth=3
	movq	32(%rbx), %rsi
	addq	$224, %rsi
	movq	%r14, %rdi
	movq	%rsi, %rdx
	movq	%rsi, %rcx
	callq	lll_addrr
	movq	32(%rbx), %rax
	movq	44609872, %rcx
	movq	44609880, %rdx
	movq	%rdx, 328(%rax)
	movq	%rcx, 320(%rax)
	movq	32(%rbx), %rax
	movq	44609872, %rcx
	movq	44609880, %rdx
	movq	%rdx, 344(%rax)
	movq	%rcx, 336(%rax)
	movq	32(%rbx), %rax
	movq	44609872, %rcx
	movq	44609880, %rdx
	movq	%rdx, 360(%rax)
	movq	%rcx, 352(%rax)
	movq	32(%rbx), %rax
	movq	44609872, %rcx
	movq	44609880, %rdx
	movq	%rdx, 376(%rax)
	movq	%rcx, 368(%rax)
	movq	32(%rbx), %rax
	leaq	384(%rax), %rsi
	leaq	304(%rax), %rdx
	leaq	32(%rax), %rcx
	movq	%r14, %rdi
	callq	lll_mulrr
	movq	32(%rbx), %rsi
	addq	$384, %rsi              # imm = 0x180
	movl	$44609888, %ecx         # imm = 0x2A8B160
	movq	%r14, %rdi
	movq	%rsi, %rdx
	callq	lll_subrr
	movq	32(%rbx), %rax
	movq	44609824, %rcx
	movq	44609832, %rdx
	movq	%rdx, 408(%rax)
	movq	%rcx, 400(%rax)
	movq	32(%rbx), %rax
	movq	48(%rax), %rcx
	movq	56(%rax), %rdx
	movq	%rdx, 424(%rax)
	movq	%rcx, 416(%rax)
	movq	32(%rbx), %rax
	movq	44609824, %rcx
	movq	44609832, %rdx
	movq	%rdx, 440(%rax)
	movq	%rcx, 432(%rax)
	movq	32(%rbx), %rsi
	addq	$400, %rsi              # imm = 0x190
	movq	%r14, %rdi
	callq	lll_forprep
	jmp	.LBB0_10
	.align	16, 0x90
.LBB0_12:                               # %block.51.LE
                                        #   in Loop: Header=BB0_16 Depth=2
	movq	32(%rbx), %rsi
	leaq	240(%rsi), %rdx
	movq	%r14, %rdi
	callq	luaV_lessequal
	testl	%eax, %eax
	je	.LBB0_16
# BB#13:                                # %block.56.FORPREP
                                        #   in Loop: Header=BB0_16 Depth=2
	movq	32(%rbx), %rax
	movq	(%rax), %rcx
	movq	8(%rax), %rdx
	movq	%rdx, 264(%rax)
	movq	%rcx, 256(%rax)
	movq	32(%rbx), %rax
	movq	240(%rax), %rcx
	movq	248(%rax), %rdx
	movq	%rdx, 280(%rax)
	movq	%rcx, 272(%rax)
	movq	32(%rbx), %rax
	movq	44609824, %rcx
	movq	44609832, %rdx
	movq	%rdx, 296(%rax)
	movq	%rcx, 288(%rax)
	movq	32(%rbx), %rsi
	addq	$256, %rsi              # imm = 0x100
	movq	%r14, %rdi
	callq	lll_forprep
	jmp	.LBB0_15
	.align	16, 0x90
.LBB0_14:                               # %block.58.ADD
                                        #   in Loop: Header=BB0_15 Depth=3
	movq	32(%rbx), %rdx
	leaq	320(%rdx), %rsi
	addq	$224, %rdx
	movq	%r14, %rdi
	movq	%rdx, %rcx
	callq	lll_addrr
	movq	32(%rbx), %rdx
	leaq	224(%rdx), %rsi
	addq	$320, %rdx              # imm = 0x140
	movl	$44609824, %ecx         # imm = 0x2A8B120
	movq	%r14, %rdi
	callq	lll_addrr
.LBB0_15:                               # %block.59.FORLOOP
                                        #   Parent Loop BB0_17 Depth=1
                                        #     Parent Loop BB0_16 Depth=2
                                        # =>    This Inner Loop Header: Depth=3
	movq	32(%rbx), %rdi
	addq	$256, %rdi              # imm = 0x100
	callq	lll_forloop
	testl	%eax, %eax
	jne	.LBB0_14
.LBB0_16:                               # %block.60.FORLOOP
                                        #   Parent Loop BB0_17 Depth=1
                                        # =>  This Loop Header: Depth=2
                                        #       Child Loop BB0_11 Depth 3
                                        #         Child Loop BB0_10 Depth 4
                                        #       Child Loop BB0_15 Depth 3
	movq	32(%rbx), %rdi
	addq	$160, %rdi
	callq	lll_forloop
	testl	%eax, %eax
	jne	.LBB0_2
.LBB0_17:                               # %block.61.FORLOOP
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_16 Depth 2
                                        #       Child Loop BB0_11 Depth 3
                                        #         Child Loop BB0_10 Depth 4
                                        #       Child Loop BB0_15 Depth 3
	movq	32(%rbx), %rdi
	addq	$80, %rdi
	callq	lll_forloop
	testl	%eax, %eax
	jne	.LBB0_1
# BB#18:                                # %block.62.RETURN
	xorl	%eax, %eax
	addq	$16, %rsp
	popq	%rbx
	popq	%r14
	popq	%r15
	retq
.Ltmp7:
	.size	lll, .Ltmp7-lll
	.cfi_endproc


	.section	".note.GNU-stack","",@progbits
