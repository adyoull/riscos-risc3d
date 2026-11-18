@ Linux3D: the 3D driver for RISC OS on Linux. It registers with RISC3D
@ and joins each context to Linux's graphics driver through the rol-3d
@ helper. See linux3d.c.
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

title:	.asciz	"Linux3D"
help:	.asciz	"Linux3D\t0.05 (18 Nov 2026) RISC3D driver: Linux's graphics driver"
	.align	2
commands:
	.asciz	"Linux3D"
	.align	2
	.word	cm_command_0 - module_start
	.word	0				@ no parameters
	.word	0
	.word	cmd_help - module_start
	.word	0
cmd_help:
	.asciz	"*Linux3D lists the 3D contexts Linux is drawing for RISC OS programs.\rSyntax: *Linux3D"
	.align	2
flags:	.word	1				@ 32-bit compatible

@ RISC3D's driver entry: r0 = reason, r1-r9 as it says; V set and r0 ->
@ error to fail.
	.global	driver_entry
driver_entry:
	stmfd	sp!, {r0-r9, lr}
	mov	r0, sp
	bl	driver_call
	cmp	r0, #0
	bne	1f
	ldmfd	sp!, {r0-r9, lr}
	msr	APSR_nzcvq, #0
	mov	pc, lr
1:	add	sp, sp, #4			@ r0 = the error
	ldmfd	sp!, {r1-r9, lr}
	msr	APSR_nzcvq, #0x10000000
	mov	pc, lr

	.include "cmodule.inc"
	CM_COMMAND 0
