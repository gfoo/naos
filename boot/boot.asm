; naos — B0 : boot sector minimal
; -----------------------------------------------------------------------------
; But : prouver que la chaîne de build fonctionne (NASM -> binaire plat -> image
; -> QEMU). On affiche un message via le BIOS, puis on arrête le CPU.
;
; Le BIOS charge ce secteur (512 octets) à l'adresse 0x7C00 et y saute, alors
; que le CPU est en real mode 16 bits. Le mode protégé, la GDT, etc. : c'est B1.

bits 16                 ; le CPU démarre en real mode (16 bits)
org  0x7C00             ; le BIOS charge ce secteur à 0x7C00 -> on s'aligne dessus

start:
    cli                 ; pas d'interruptions pendant qu'on installe les segments
    xor ax, ax          ; AX = 0
    mov ds, ax          ; DS = ES = SS = 0 : adressage simple, segments à zéro
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00      ; pile juste sous notre code (croît vers le bas)
    sti                 ; on réautorise les interruptions (le BIOS en a besoin)

    mov si, msg         ; SI pointe sur la chaîne à afficher
.print:
    lodsb               ; AL = [DS:SI], puis SI++
    test al, al         ; octet nul ? -> fin de chaîne
    jz .hang
    mov ah, 0x0E        ; int 0x10, fonction 0x0E = téléscripteur (affiche AL)
    mov bh, 0x00        ; page vidéo 0
    int 0x10            ; service BIOS d'affichage
    jmp .print

.hang:
    hlt                 ; arrête le CPU jusqu'à la prochaine interruption
    jmp .hang           ; si réveillé, on se rendort : boucle d'arrêt propre

msg db "naos B0: it boots!", 13, 10, 0   ; 13,10 = CR LF ; 0 = fin de chaine

; -----------------------------------------------------------------------------
; Remplissage jusqu'à 510 octets, puis signature de boot obligatoire (0xAA55).
; Sans cette signature aux offsets 510-511, le BIOS déclare le disque non-bootable.
times 510-($-$$) db 0   ; padding : zéros jusqu'à l'octet 510
dw 0xAA55               ; signature little-endian -> octets 0x55 0xAA
