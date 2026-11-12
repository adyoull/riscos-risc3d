@ RISC3D: RISC OS's 3D driver interface. Mesa's front end (EGL, GLES) on
@ one side, 3D drivers on the other; this module only joins them.
@ See risc3d.c.
@
@ PROTOTYPE: the SWI chunk (&C3D00) and error numbers are provisional.
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
	.word	service - module_start
	.word	title - module_start
	.word	help - module_start
	.word	commands - module_start
	.word	0xC3D00				@ SWI chunk (provisional)
	.word	cm_swi - module_start
	.word	swi_names - module_start
	.word	0				@ SWI decoding code
	.word	0				@ messages file
	.word	flags - module_start

title:	.asciz	"RISC3D"
help:	.asciz	"RISC3D\t0.01 (12 Nov 2026) 3D driver interface (prototype)"
swi_names:
	.asciz	"RISC3D\0Info\0Register\0Open\0Close\0Sync\0"
	.align	2
commands:
	.asciz	"RISC3D"
	.align	2
	.word	cm_command_0 - module_start
	.word	0				@ no parameters
	.word	0
	.word	cmd_help - module_start
	.word	0
cmd_help:
	.asciz	"*RISC3D lists the 3D drivers registered with RISC3D and the contexts programs have open.\rSyntax: *RISC3D"
	.align	2
flags:	.word	1				@ 32-bit compatible

	.word	0				@ (no fast table)
@ Service_WimpCloseDown (&53): a task's contexts go with it
service:
	teq	r1, #0x53
	movne	pc, lr
	b	cm_service

@ oserror *call_driver (unsigned entry, unsigned r12, regs *r): calls a
@ driver's entry with r0-r9 from r and its own r12; r0-r9 back into r.
@ Returns the driver's error (V set on its return), or 0.
	.global	call_driver
call_driver:
	stmfd	sp!, {r2, r4-r11, lr}
	mov	lr, r0
	mov	r12, r1
	ldmia	r2, {r0-r9}
	blx	lr
	ldr	r12, [sp]
	stmia	r12, {r0-r9}
	movvc	r0, #0
	ldmfd	sp!, {r2, r4-r11, pc}

	.include "cmodule.inc"
	CM_COMMAND 0
