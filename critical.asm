; Linux PPPD DOS port by ALM, tonilop@redestb.es

_TEXT       segment byte public 'CODE'
_TEXT       ends

DGROUP      group   _DATA,_BSS

    assume  cs:_TEXT,ds:DGROUP

_DATA       segment word public 'DATA'

d@  label   byte
d@w label   word

_DATA       ends

_BSS        segment word public 'BSS'

b@  label   byte
b@w label   word

_BSS        ends

_TEXT       segment byte public 'CODE'

    assume  cs:_TEXT

_crithandler proc   far

            mov     ah, 54h             ; Clean up DOS
            int     21h

            pop     ax                  ; Discard DOS return adress and flags
            pop     ax
            pop     ax

            pop     ax                  ; Get user's AX (AH = DOS function)

            cmp     ah, 56              ; Old-style DOS function?
            jae     short @3@338

;       If it's a function that can return an error condition,
;       set AL to 0xFF and return to program after int 21h call.

            cmp     ah, 15
            jb      short @3@170
            cmp     ah, 19
            jbe     short @3@282
@3@170:
            cmp     ah, 22
            je      short @3@282
            cmp     ah, 23
            je      short @3@282
            cmp     ah, 35
            je      short @3@282
            cmp     ah, 41
            jne     short @3@394
@3@282:
            mov     al, 255
            jmp     short @3@394
@3@338:

;       It's a new-style function. Set the carry
;       flag and put the return code in AL.

            mov     bp, sp
            or      byte ptr 20[bp], 1  ; Set carry flag in saved flags reg.

            mov     ax, di              ; Transform critical error code to
            and     ax, 0FFh            ;  the DOS function return code
            add     ax, 19
@3@394:
            pop     bx                  ; Restore user registers and
            pop     cx                  ;  return to the point after
            pop     dx                  ;  the failed DOS call
            pop     si
            pop     di
            pop     bp
            pop     ds
            pop     es
            iret

_crithandler endp

_TEXT       ends

_BSS        segment word public 'BSS'
_BSS        ends

_DATA       segment word public 'DATA'

s@  label   byte

_DATA       ends

_TEXT       segment byte public 'CODE'
_TEXT       ends

    public  _crithandler

_s@ equ s@

            end

