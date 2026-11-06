**
** Audio Interrupt routine
**
** setup in snd_paula.c
** used for M68k as well as for PowerPC
** written by Frank Wille <frank@phoenix.owl.de>
** modified by Matt Hey <matthey7@gmail.com>
**
** 2026-11-06 (RadeonGL, the A4000D): NOT used by the gcc build any more,
** which assembles snd_int68k_gas.s -- an integer-only version. This one
** uses the FPU in an interrupt: on a 68060 with MMULib's 68060.library an
** audio interrupt in the middle of the library's software emulation of
** FSIN/FCOS (the server's AngleVectors) ended in the fatal alert BF00 0002,
** and FPSR, never saved here, came back changed (a branch in the task after
** fcmp could go the wrong way: GLQuake's water chain closed into a ring).
** Kept as it was for the PowerPC builds, whose snd_paula.c still reads a
** double; a 68k vbcc build needs the integer version (snd_paula.c's
** struct AudTime) as well.
**

	include	"devices/timer.i"
	include "hardware/custom.i"
	include	"hardware/intbits.i"

GetSysTime	equ	-66

; See exec.library SetIntVector()
; d0,d1,a0,a1,a5,a6 may be trashed
; d1 = INTENAR & INTREQR (enabled & requested interrupts)
; a0 = -> Amiga custom chip base
; a1 = ->IntData (is_Data of the interrupt structure)
; a6 = ->SysBase (exec.library base)
; IntData offset 0:	*TimerBase
;         offset 4:	*FirstTime2
;         offset 8:	*aud_start_time

	code
	cnop	0,4

	xdef	_AudioIntCode
	xdef	AudioIntCode
_AudioIntCode:
AudioIntCode:
	fsave    -(sp)
	fmove.x  fp0,-(sp)
	move.w   (intreqr,a0),d0
	move.l	 a1,a5
	and.w    #INTF_AUD0|INTF_AUD1|INTF_AUD2|INTF_AUD3,d0
	move.l	 (a5)+,a6		; TimerBase
	move.w   d0,(intreq,a0)		; clear audio requests
	lea      (TVstruct,pc),a0
	jsr      (GetSysTime,a6)	; a0 is preserved!
	move.l   (a5)+,a1		; ->FirstTime2
	move.l   (a0)+,d0		; TV_SECS
	fmove.l  (a0),fp0		; TV_MICRO
	move.l   (a5),a5		; ->aud_start_time
	fmul.d   #0.000001,fp0		; TV_MICRO/1000000
	sub.l    (a1),d0		; TV_SECS - FirstTime2
	fadd.l   d0,fp0
	fmove.d  fp0,(a5)
	fmove.x  (sp)+,fp0
	frestore (sp)+
	rts


	cnop	0,8
TVstruct:
	dcb.b	TV_SIZE,0
