@ DAClamp for !RISC3D: lifts RISC OS 5's clamp on the maximum size of
@ dynamic areas made with a maximum given (OS_DynamicArea 8, R2 = -1;
@ R1 and R3 = 0, left as they are), as riscos-mesa's !MesaLibs does. Run
@ by !RISC3D's !Boot before RISC3D loads the GPU driver, so V3DRender's
@ buffer area gets the size it asks for (V3DRender$Size, 256 MB) rather
@ than being stopped at 128 MB, whatever order Resources boot in. A
@ Utility (filetype FFC): position-independent, returns with V clear.
@ Errors from older systems without reason 8 are ignored.
@
@ Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE).
	.arm
	.text
	mov	r0, #8
	mov	r1, #0
	mvn	r2, #0
	mov	r3, #0
	swi	0x20066			@ XOS_DynamicArea
	msr	cpsr_f, #0		@ (V clear: no error)
	mov	pc, lr
