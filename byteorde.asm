; Linux PPPD DOS port by ALM, tonilop@redestb.es

_TEXT   segment byte public 'CODE'
_TEXT   ends
DGROUP  group   _DATA,_BSS
        assume  cs:_TEXT,ds:DGROUP
_DATA   segment word public 'DATA'
d@      label   byte
d@w     label   word
_DATA   ends
_BSS    segment word public 'BSS'
b@      label   byte
b@w     label   word
_BSS    ends
_TEXT   segment byte public 'CODE'
   ;    
   ;    unsigned long _fastcall htonl(unsigned long lv)
   ;    
        assume  cs:_TEXT
@htonl  proc    near
        xchg     ah,al
        xchg     ax,dx
        xchg     ah,al
        ret     
@htonl  endp
   ;    
   ;    unsigned short _fastcall htons(unsigned short sv)
   ;    
        assume  cs:_TEXT
@htons  proc    near
        xchg     ah,al
        ret     
@htons  endp
_TEXT   ends
_DATA   segment word public 'DATA'
s@      label   byte
_DATA   ends
_TEXT   segment byte public 'CODE'
_TEXT   ends
        public  @htons
        public  @htonl
_s@     equ     s@
        end
