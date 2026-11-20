@ Linux2D (prototype): RISC OS on Linux's desktop rectangle copies and
@ fills, done by the native rol-2d helper. See linux2d.c.
@
@ Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE).

	.syntax	unified
	.arm
	.section .text.header, "ax"
	.global	module_start
module_start:
	.word	0				@ start
	.word	cm_init - module_start
	.word	cm_final - module_start
	.word	0				@ service call handler
	.word	title - module_start
	.word	help - module_start
	.word	commands - module_start
	.word	0				@ no SWIs
	.word	0
	.word	0
	.word	0
	.word	0				@ messages file
	.word	flags - module_start

title:	.asciz	"Linux2D"
help:	.asciz	"Linux2D\t0.01 (20 Nov 2026) Desktop copies and fills by Linux (prototype)"
	.align	2
commands:
	.asciz	"Linux2D"
	.align	2
	.word	cm_command_0 - module_start
	.word	0x00030000			@ 0 to 3 parameters
	.word	0
	.word	cmd_help - module_start
	.word	0
cmd_help:
	.asciz	"*Linux2D shows how many of the desktop's rectangle copies and fills Linux has done; on or off turns it on or off; limits sets the smallest copy and fill (bytes) sent to Linux.\rSyntax: *Linux2D [on | off | limits <copy bytes> <fill bytes>]"
	.align	2
flags:	.word	1				@ 32-bit compatible

@ GraphicsV: r0-r9 the call's registers, r12 = the module's private word.
@ gfxv_c (r) returns 1 if it did the call (r[4] = 0): then the vector is
@ claimed (return to the address on the stack); else pass it on.
	.global	gfxv_entry
gfxv_entry:
	stmfd	sp!, {r0-r10, lr}
	mov	r10, sp
	bic	sp, sp, #7			@ (an 8-byte aligned stack for C)
	mov	r0, r10
	bl	gfxv_c
	mov	sp, r10
	cmp	r0, #0
	ldmfd	sp!, {r0-r10, lr}
	moveq	pc, lr				@ not done: on to the next claimant
	ldmfd	sp!, {pc}			@ done: claimed

	.include "cmodule.inc"
	CM_COMMAND 0
