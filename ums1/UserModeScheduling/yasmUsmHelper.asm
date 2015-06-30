USE64
;.code

;FADD/FADDP/FIADD—Add
masm_jump_context: ; proc
;49 B9 BC 9A 78 56 34 12 00 00 mov         r9,123456789ABCh
	mov  r9, qword 0ababababababababh
	mov rax, qword 0123456789ABCDEFAh
;	jmp QWORD [QWORD ptr 00ababababababh]
	jmp rax
;	ret
;masm_jump_context endp

;end