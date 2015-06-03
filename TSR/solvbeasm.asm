.286

_TEXT SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  cs:_TEXT
;        EXTRN _oldvideo:proc
        PUBLIC bankswitchproc_
        PUBLIC _gMouseTrap

_gMouseTrap:
    dw 0,0,0,0

; Bankswitch procedure is a "fast bank switching" hack
; for DOS apps. "high performance" applications are meant
; to switch banks using direct call to the video BIOS via
; calling this procedure. This is supposedly faster than
; using the interrupt. Since we don't care about such
; tiny speed increases, we'll just do the interrupt here.
bankswitchproc_:
        mov ax, 4f05h
        int 10h
        retf


_TEXT ENDS
END