	c := $B002
	yarrb = $BFFE

	.org $0A00
		lda yarrb		; clear bit 1 of the yarrb byte
		and #$fd
		sta yarrb
	loop:
		lda mem
		sta c
		and #$42
		adc #$3e
		asl a
		asl a
		rol mem+2
		rol mem+1
		rol mem
		jmp loop
	mem:
		.byte 1,2,3



