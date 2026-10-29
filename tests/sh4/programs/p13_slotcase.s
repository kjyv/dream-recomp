! A jump table whose second case is the delay slot of a bt/s. r4 = case (0..1), r5 = the flag the
! first case tests; returns r0, or -1 for out of range. Entering at the slot runs it once and falls
! through (case 1: 11). The bt/s runs it once whether taken (case 0, r5 = 0: 1) or not (case 0,
! r5 != 0: 11). The slot is both a switch target and a delay slot, which the emitter once labelled
! twice. Rez met it at 0x0c0e0dd0 through a table read past its end.
	.text
	.global _slotcase
_slotcase:
	mov	#1,r1
	cmp/hi	r1,r4		! r4 > 1 ?
	bt	.Lbad
	mov.l	.Ltabp,r1
	mov	r4,r0
	shll2	r0
	mov.l	@(r0,r1),r3
	jmp	@r3
	mov	#0,r0
.Lc0:	tst	r5,r5
	bt/s	.Lout
.Lc1:	add	#1,r0
	add	#10,r0
.Lout:	rts
	nop
.Lbad:	rts
	mov	#-1,r0
	.align 2
.Ltabp:	.long	.Ltab
.Ltab:
	.long	.Lc0
	.long	.Lc1
