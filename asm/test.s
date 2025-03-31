

YARRB = $BFFE		; YARRB control register
WRCVEC	= $0208		; OSWRCH vector address
WRCVEC_DEFAULT = $FE52 ; Normal WRCVEC value
PORTC = $B002		; PIA port C - bit 2 is connected to the ATOM's loudspeaker

SCRA	= $9C00		; memory addresses for the screen 
SCRB	= $9D00
SCRC	= $9E00 
SCRD	= $9F00 
CRTA	= $0AFE		; CRT controller 
CRTB	= $0AFF

; SCAP 	= $90		; column counter?
; LINE	= SCAP+1
; WORK	= LINE+2


LINE = $DE			; DE,DF address of current line 
SCAP = $E0			; horizontal cursor position 0-39
WORK = $90

SAVEX	= $E4
SAVEY	= $E5

YARRB_MASK = $FD

CH_SPACE= $20
CH_DELETE = $7F
CH_RETURN = $0D
CH_LINE_FEED = $0A
CH_FORM_FEED = $0C

INIT_CURSOR_POS = SCRA + 960


LINE_LEN = 40

CURSOR_H = $0E
CURSOR_L = $0F

START_ADD_H = $0C
START_ADD_L = $0D

	.org	$A00

; Initialisation 
	lda YARRB		; Disable YARRB ram at A00-AFF
	and #YARRB_MASK
	sta YARRB
	lda #<CHATS		; update WRCVEC subroutine pointer
	sta WRCVEC
	lda #>CHATS
	sta WRCVEC+1
	jsr CLEARS
	rts

; CHATS_TST:
; 	PHA
; 	LDA PORTC
; 	EOR #$04
; 	STA PORTC
; 	PLA
; 	JSR WRCVEC_DEFAULT
; 	nop
; 	RTS

CHATS:
	;JSR WRCVEC_DEFAULT
	PHP				; Save flags
	PHA				; Save accumulator
	CLD				; Clera decimal mode
	STY SAVEY		; Save Y register
	STX SAVEX		; Save X register
	JSR CHATS2		; Send character in accumulator to screen
	PLA				; Restore accumulator
	LDX SAVEX		; Restore X register
	LDY SAVEY		; Restore Y register
	PLP				; Restore flags
	RTS

;  Send ASCII Character to Screen subroutine
;  -----------------------------------------

CHATS2:
	LDY SCAP		; character to screen 
 	CMP #CH_SPACE 
 	BCC CTL			; all control characters 
 	CMP #CH_DELETE
 	BEQ DELETE
 TOSCRN:
 	JSR WRCH 
 	INY
 	CPY #LINE_LEN
 	BCC VDUB	; automatic scroll when line filled 
FILLED:
 	JSR SCROLL
VDUA:
 	LDY #$00 
VDUB:
 	JSR CALCN 
 	STY SCAP 
 	LDY #CURSOR_L	; rewrite cursor position 
 	STY CRTA	; select cursor L
 	LDY WORK
 	STY CRTB 	; store in cursor L
 	LDY #CURSOR_H 
 	STY CRTA 	; select cursor H
 	LDY WORK+02 
 	STY CRTB	; store in cursor H
VDUC:
 	RTS 
DELETE:
 	DEY 
 	BMI VDUC	; refuse to delete before line start 
 	LDA #CH_SPACE	; write in a blank 
 	JSR WRCH 
 	LDA #CH_DELETE 
 	BNE VDUB
CTL:
 	CMP #CH_RETURN		; carriage return? 
 	BEQ VDUA 
 	CMP #CH_LINE_FEED	; line feed? 
 	BEQ SCROL 
 	CMP #CH_FORM_FEED	; form feed? 
 	BEQ CLEARS 
 	BNE TOSCRN
SCROL:
	JSR SCROLL	; vscroll screen and rewrite cursor
	LDY SCAP
	BCS VDUB
CLEARS:
	PHA		; clear entire buffer
	LDY #$00
	LDA #' '
CLEAR:
	STA SCRA,Y
	STA SCRB,Y
	STA SCRC,Y
	STA SCRD,Y
	INY
	BNE CLEAR
	STY SCAP	; set column to 0
; 	LDY #$0F
; SETCRT:
; 	STY CRTA	; set up all the CRT parameters
; 	LDA CRTTAB,Y
; 	STA CRTB
; 	DEY
; 	BPL SETCRT
	LDA #<INIT_CURSOR_POS
	STA LINE
	LDA #>INIT_CURSOR_POS
	STA LINE+01 
	PLA
	RTS 
SCROLL:
	PHP		; scroll subroutine 
	PHA
	CLD 
	LDY #LINE_LEN 
	JSR CALCN 
	LDA WORK 
	STA LINE 
	LDA WORK+02 
	STA LINE+01 
	LDY #START_ADD_L
	STY CRTA	; select start address L
	LDA LINE
 	SEC
	SBC #$C0
	STA CRTB	; store in start address L
	DEY
	STY CRTA	; select start address H
	LDA WORK+02
	SBC #$03
	STA CRTB	; store in start address H
	LDY #$27
	LDA #' '
CLEARL:
	JSR WRCH
	DEY
	BPL CLEARL
	PLA
	PLP
	RTS
CALCN:
	PHP		; do calculation to make sure that the
	PHA		; processor and CRT controller agree on
	CLD		; position of screen r :
	CLC
	TYA				; A <- column
 	ADC LINE		; A <- column + low byte of line start address
 	STA WORK		; WORK <- column + low byte of line start address
 	LDA LINE+01		; A <- high byte of line start address
 	ADC #$00		; A <- column + high byte of line start address
 	STA WORK+02		; WORK+2 <- column + high byte of line start address
 	AND #$07		
 	ORA #$9C
 	STA WORK+01
	PLA
	PLP
	RTS 
WRCH:
	JSR CALCN
	STY WORK+02
	LDY #$00
	STA (WORK),Y
	LDY WORK+02
	RTS 
; CRTTAB:
;  	.byte $3F ; total number of characters per line,
;  	.byte $28 ; 40 characters displayed
;  	.byte $33 ; position of horizontal sync
;  	.byte $05 ; width in m S of horizontal sync pulse
;  	.byte $1E ; total number of character row
;  	.byte $02 ; additional no. of lines for 312 total
;  	.byte $19 ; 25 displayed character rows
;  	.byte $1B ; position of vertical sync pulse
;  	.byte $00 ; set non-interlace mode
;  	.byte $09 ; set 10 lines per character row
;  	.byte $68 ; slow blink cursor from line 9
;  	.byte $09 ; to line 10
;  	.byte $04 ; high address of VDU ram
;  	.byte $00 ; low address of VDU ram
;  	.byte $07 ; high address of initial cursor position
;  	.byte $C0 ; low address of initial cursor position 


