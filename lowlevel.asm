; ASY (8250/16550A "com" port) interrupt hooks
; Copyright 1991 Phil Karn, KA9Q

; Linux PPPD DOS port by ALM, tonilop@redestb.es

include asmgloba.h

        LOCALS

        extrn   asyint:near

        .DATA

        extrn   Isat:word
        public  StktopC
        public  jifflag
        public  timercount
        public  jiffies

intickint       db      0
jifflag         db      0
timercount      dw      0
StkSem          dw      0
jiffies         dd      0
StktopC         dw      ?       ; Interrupt working stack
Spsave          dw      ?       ; Save location for SP during interrupts
Sssave          dw      ?       ; Save location for SS during interrupts
call08vector    dd      ?

        .CODE

dbase           dw      @Data
orig08vector    dd      ?
vector          dd      ?       ; place to stash chained vector
vectlo          equ     word ptr vector
vecthi          equ     word ptr vector+2

; This routine handles the "timer tick" interrupt, 08. It decrements a
; counter and jumps to the C funtion with the address stored in the
; gloval variable 'call08vector'.

                PUBLIC  int08handler

int08handler    PROC    FAR

                pushf                   ; Invocar rutina original
                call    dword ptr cs:[orig08vector]

                cld
                push    ds              ; save on user stack
                mov     ds,cs:dbase     ; establish interrupt data segment

                cmp     word ptr [timercount], 0
                je      @@timer1        ; Si el contador es 0, no decrementar
                                        ; No es 0, decrementarlo
                dec     word ptr [timercount]
                                        ; actualiza contador de jiffies
@@timer1:       add     word ptr jiffies, 1
                adc     word ptr jiffies+2, 0
                mov     byte ptr jifflag, 0
                                        ; Mira si llamada anidada
                cmp     byte ptr [intickint], 0
                jne     @@timer2        ; Si, salir con IRET sin ir al C
                                        ; No, marca llamada en curso y
                mov     byte ptr [intickint], 1

                cmp     StkSem,0
                jg      @@timer3

                mov     Sssave,ss       ; stash user stack context
                mov     Spsave,sp

                mov     ss,cs:dbase
                mov     sp,StktopC

@@timer3:       inc     word ptr StkSem

                PUSHALL
                push    es

                sti                     ; Llamar a la rutina en C
                call    dword ptr [call08vector]
                cli

                pop     es
                POPALL

                dec     word ptr StkSem
                jnz     @@timer4

                mov     ss,Sssave
                mov     sp,Spsave       ; restore original stack context
                                        ; Borra flag llamada en curso y
@@timer4:       mov     byte ptr [intickint], 0

@@timer2:       pop     ds             ; Acabar con IRET

                iret

int08handler    ENDP


                PUBLIC  set08handler

set08handler    PROC
                ARG     oldhandler:dword
                ARG     callback:dword

   ;        orig08vector = oldhandler;
   ;
                mov     ax,word ptr oldhandler+2
                mov     dx,word ptr oldhandler
                mov     word ptr cs:orig08vector+2,ax
                mov     word ptr cs:orig08vector,dx
   ;
   ;        call08vector = callback;
   ;
                mov     ax,word ptr callback+2
                mov     dx,word ptr callback
                mov     word ptr call08vector+2,ax
                mov     word ptr call08vector,dx

                ret

set08handler    ENDP

;------------------------------------------------------------------

; Re-arm 8259 interrupt controller(s)
; Should be called just after taking an interrupt, instead of just
; before returning. This is because the 8259 inputs are edge triggered, and
; new interrupts arriving during an interrupt service routine might be missed.

eoi     proc

        cmp     Isat,1
        jnz     @@1             ; Only one 8259, so skip this stuff
        mov     al,0bh          ; read in-service register from
        out     0a0h,al         ; secondary 8259
        nop                     ; settling delay
        nop
        nop
        in      al,0a0h         ; get it
        or      al,al           ; Any bits set?
        jz      @@1             ; nope, not a secondary interrupt
        mov     al,20h          ; Get EOI instruction
        out     0a0h,al         ; Secondary 8259 (PC/AT only)
@@1:    mov     al,20h          ; 8259 end-of-interrupt command
        out     20h,al          ; Primary 8259
        ret

eoi     endp

; istate - return current interrupt state

        public  istate

istate  proc

        pushf
        pop     ax
        and     ax,200h
        jnz     @@1
        ret
@@1:    mov     ax,1
        ret

istate  endp

; dirps - disable interrupts and return previous state: 0 = disabled,
;       1 = enabled

        public dirps

dirps   proc

        pushf                   ; save flags on stack
        pop     ax              ; flags -> ax
        and     ax,200h         ; 1<<9 is IF bit
        jz      @@1             ; ints are already off; return 0
        mov     ax,1
        cli                     ; interrupts now off
@@1:    ret

dirps   endp

; This routine returns a far pointer to the INDOS flag. The byte prior the
; address returned by DOS func 34h is meant to be the crtical error flag, so
; if we decrement the addres by 1 the resulting pointer addresses a word
; tha must be 0 when DOS access is safe :-)

        public  getindosflag

getindosflag    proc

        push    bp
        push    si              ; Guarda registros de posible
        push    di              ;  utilizaci�n por el C
        push    ds

        mov     ah, 34h         ; Llama a funci�n DOS para obtener
        int     21h             ;  el famoso puntero

        mov     ax, bx          ; Transfiere resultado a los registros
        mov     dx, es          ;  que el C espera como resultado
        dec     ax

        pop     ds              ; Restaura registros y acaba
        pop     di
        pop     si
        pop     bp
        ret

getindosflag    endp

; restore - restore interrupt state: 0 = off, nonzero = on

        public  restore

restore proc
        arg is:word

        test    word ptr is,0ffffh
        jz      @@1
        sti

        ret

@@1:    cli     ; should already be off, but just in case...

        ret

restore endp

; this routine gets called whenever the packet driver interface needs a
; buffer for storing an incoming packet.

                PUBLIC  get_buffer

get_buffer      PROC
                ARG     handle:word
                ARG     count:word
                ARG     receiver:dword

                push    ds
                push    es
                push    si
                push    di
                push    bp

                mov     bx,handle
                mov     cx,count
                mov     ax,0
                stc
                push    ax
                pushf
                call    receiver
                pop     bx
                cmp     bx,0
                je      @@1

                add     sp,2
@@1:
                mov     dx,es
                mov     ax,di

                pop     bp
                pop     di
                pop     si
                pop     es
                pop     ds

                ret

get_buffer      ENDP

; this routine gets called whenever the packet driver interface has
; completed a packet copy operation and the application can handle it.

                PUBLIC  copy_done

copy_done       PROC
                ARG     handle:word
                ARG     count:word
                ARG     dptr:dword
                ARG     receiver:dword

                push    ds
                push    es
                push    si
                push    di
                push    bp

                mov     bx,handle
                mov     cx,count
                lds     si,dptr
                mov     ax,1
                clc
                push    ax
                pushf
                call    receiver
                pop     bx
                cmp     bx,1
                je      @@1

                add     sp,2
@@1:
                pop     bp
                pop     di
                pop     si
                pop     es
                pop     ds

                ret

copy_done       ENDP

; this routine calculates internet style checksums.

                PUBLIC  inchksum

inchksum        PROC
                ARG     dptr:dword
                ARG     len:word

                push    si
                push    ds
                lds     si,dword ptr dptr
                mov     cx,word ptr len

                mov     BL,CL

                shr     CX,1            ; group into words
                xor     DX,DX           ; set checksum to 0
                cld
                clc

deloop:         lodsw
                adc     DX,AX
                loop    deloop

                adc     DX,0            ; only two necessary
                adc     DX,0

remain:         and     BL,1
                jz      done

                xor     AH,AH
                lodsb
                add     DX,AX
                adc     DX,0
                adc     DX,0

done:           mov     AX,DX           ; result into ax

                pop     ds
                pop     si

                ret

inchksum        ENDP


; this routine is the entry point for the packet driver interrupt service.

                PUBLIC  pktinthandler
                EXTRN   pktdriver:near

pktinthandler   PROC    FAR

                jmp     short @@1
                nop
                db      'PKT DRVR',0

@@1:            PUSHALL
                push    ds
                push    es

                mov     ds,cs:dbase

                mov     dx,ss
                mov     ax,sp

                cmp     StkSem,0
                jg      @@2

                mov     Sssave,ss       ; stash user stack context
                mov     Spsave,sp

                mov     ss,cs:dbase
                mov     sp,StktopC

@@2:            inc     word ptr StkSem

                push    dx
                push    ax

                sti
                call    pktdriver
                cli

                add     sp,4

                dec     word ptr StkSem
                jnz     @@3

                mov     ss,Sssave
                mov     sp,Spsave       ; restore original stack context

@@3:            pop     es
                pop     ds
                POPALL

                iret

pktinthandler   ENDP

; asy0vec - asynch channel 0 interrupt handler

        public  asy0vec
        label   asy0vec far

        cld
        push    ds              ; save on user stack
        mov     ds,cs:dbase     ; establish interrupt data segment

        cmp     StkSem,0
        jg      @@1

        mov     Sssave,ss       ; stash user stack context
        mov     Spsave,sp

        mov     ss,cs:dbase
        mov     sp,StktopC

@@1:    inc     word ptr StkSem

        PUSHALL
        push    es

        call    eoi

        mov     ax,0            ; arg for service routine
        push    ax
        call    asyint
        inc     sp
        inc     sp

        jmp     doret

; asy1vec - asynch channel 1 interrupt handler

        public  asy1vec
        label   asy1vec far

        cld
        push    ds              ; save on user stack
        mov     ds,cs:dbase     ; establish interrupt data segment

        cmp     StkSem,0
        jg      @@2

        mov     Sssave,ss       ; stash user stack context
        mov     Spsave,sp

        mov     ss,cs:dbase
        mov     sp,StktopC

@@2:    inc     word ptr StkSem

        PUSHALL
        push    es

        call    eoi

        mov     ax,1            ; arg for service routine
        push    ax
        call    asyint
        inc     sp
        inc     sp

        jmp     doret

; asy2vec - asynch channel 2 interrupt handler

        public  asy2vec
        label   asy2vec far

        cld
        push    ds              ; save on user stack
        mov     ds,cs:dbase     ; establish interrupt data segment

        cmp     StkSem,0
        jg      @@3

        mov     Sssave,ss       ; stash user stack context
        mov     Spsave,sp

        mov     ss,cs:dbase
        mov     sp,StktopC

@@3:    inc     word ptr StkSem

        PUSHALL
        push    es

        call    eoi

        mov     ax,2            ; arg for service routine
        push    ax
        call    asyint
        inc     sp
        inc     sp

        jmp     doret

; asy3vec - asynch channel 3 interrupt handler

        public  asy3vec
        label   asy3vec far

        cld
        push    ds              ; save on user stack
        mov     ds,cs:dbase     ; establish interrupt data segment

        cmp     StkSem,0
        jg      @@4

        mov     Sssave,ss       ; stash user stack context
        mov     Spsave,sp

        mov     ss,cs:dbase
        mov     sp,StktopC

@@4:    inc     word ptr StkSem

        PUSHALL
        push    es

        call    eoi

        mov     ax,3            ; arg for service routine
        push    ax
        call    asyint
        inc     sp
        inc     sp

; common routine for interrupt return
; Note that all hardware interrupt handlers are expected to return
; the original vector found when the device first attached. We branch
; to it just after we've cleaned up here -- this implements shared
; interrupts through vector chaining. If the original vector isn't
; available, the interrupt handler must return NULL to avoid a crash!

doret:  cmp     ax,0            ; is a chained vector present?
        jne     @@5             ; yes
        cmp     dx,ax
        jne     @@5             ; yes

        pop     es              ; nope, return directly from interrupt
        POPALL

        dec     word ptr StkSem
        jnz     @@6

        mov     ss,Sssave
        mov     sp,Spsave       ; restore original stack context

@@6:    pop     ds

        iret

; Code to handle vector chaining

@@5:    mov     cs:vectlo,ax    ; stash vector for later branch
        mov     cs:vecthi,dx

        pop     es
        POPALL

        dec     word ptr StkSem
        jnz     @@7

        mov     ss,Sssave
        mov     sp,Spsave       ; restore original stack context

@@7:    pop     ds

        jmp     cs:[vector]     ; jump to the original interrupt handler

        end

