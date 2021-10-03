; BIOS source for 8086tiny IBM PC emulator (revision 1.25 and above). Compiles with NASM.
; Copyright 2013-14, Adrian Cable (adrian.cable@gmail.com) - http://www.megalith.co.uk/8086tiny
;
; Based on 8086tiny plus 2.34
;
; Modified by Julian Olds to work with CGA and provide more complete BIOS functions.
;
;
;
; Modified by Fabrizio Di Vittorio for fabgl:
;   - removed instruction decoding tables (moved into host code)
;   - removed ZS and XF initialization
;   - added PIC 8259 initialization
;   - added PIC 8259 EOI at exit code of INT8, INT9 interrupt handlers
;   - default to blinking mode for video text modes 0..3
;   - replaced cga_glyphs (8x8 CGA glyphs) with original style fonts
;   - replaced vga_glyphs (8x16 VGA glyphs) with original style fonts
;   - "int 10, 1a" returns 0x02 (CGA) instead of 0x0C (MCGA)
;   - implemented "int 10, 08" (read char/attr) on graphics mode (320x200 and 640x200) in int10_charatcur_graph
;   - bugfix: missing CLD in put_cga320_char_vidoffset and put_cga640_char (may draw chars in reverse and wrong pos in graphics mode, try with gwbasic!)
;   - bugfix: "int 10, 09" in graphics mode, when bit 7 of BL is set then destination pixels are XORed, not copied
;   - communications to the emulator performed with "int 0xf1", "int 0xf2"... instead of custom CPU opcodes
;   - INT9 managed partially by the emulator
;   - INT16 managed partially by the emulator
;   - INT16 implemented missing functions
;   - added MEMSIZE equ for memory size
;   - fixed int12, get mem size from memsize field
;   - implemeted int15, 0xc0 function
;   - copy of rom_config into BIOS e6f5, configuration table
;   - INT16 implemented function 0x92
;   - INT16 implemented function 0x09
;   - INT15 implemented function 0xc1
;   - added Extended BIOS Data Area (EBDA)
;   - INT9, support for INT15,4F keyboard intercept
;   - removed internal variables from BIOS data area
;   - INT9, support for CTRL+ALT+DEL, PRINTSCREEN, CTRLBREAK, SYSREQ, PAUSE
;   - INT15 implemented function 0xC2
;   - implemented INT74 (pointing device)
;   - INT8, implemented 24H rollover
;   - rewritten INT1A from scratch, now fully implemented
;   - INT15, implemented 0x80, 0x81, 0x82, 0x83, 0x85, 0x86 functions
;   - INT13, reiplemented using emulated helpers
;   - int10_write_char_tty, checked page num for old MSDOS versions
;   - int10, implemented service 0c, write pixel
;    
;
;
;
;
; This work is licensed under the MIT License. See included LICENSE.TXT.


cpu 8086


; PIC 8259A/B interrupt controllers stuff
PIC8259A_A00 equ 0x20 ; 8259A PORT
PIC8259A_A01 equ 0x21 ; 8259A PORT
PIC8259B_A00 equ 0xa0 ; 8259B PORT
PIC8259B_A01 equ 0xa1 ; 8259B PORT
PIC8259_EOI equ 0x20 ; EOI (End Of Interrupt)

; MC156818 stuff
RTC_SEL equ 0x70
RTC_REG equ 0x71

; memory size in K
MEMSIZE equ 639

; Extended BIOS Data Area (EBDA) stuff
EBDA_SEG           equ 0x9fc0   ; segment of Extended BIOS Data Area (EBDA) - must match with same value in bios.h
EBDA_DRIVER_OFFSET equ 0x22     ; Pointing device device driver far call offset
EBDA_DRIVER_SEG    equ 0x24     ; Pointing device device driver far call segment
EBDA_FLAGS1        equ 0x26     ; Flags 1
EBDA_FLAGS2        equ 0x27     ; Flags 2
EBDA_PACKET        equ 0x28     ; Start of packet


; model and submodel
; fc/01 = generic AT
MODEL    equ 0xfc
SUBMODEL equ 0x01
BIOSREV  equ 0x00


; emulator macros
; These macros allows BIOS to call helper functions implemented in the emulator side.
; The INTs aren't managed as normal INTs in the 8086 emulator. For example
; The helper code is executed immediately without using stack to preserve flags/CS/IP.

%macro emu_read_disk 0
  int 0xf1
%endmacro

%macro emu_write_disk 0
  int 0xf2
%endmacro

%macro emu_putchar_al 0
  int 0xf4
%endmacro

%macro emu_helper 0
  int 0xf5
%endmacro

%macro emu_iret_replace_CF 0
  int 0xf6
%endmacro

%macro emu_iret_replace_ZF 0
  int 0xf7
%endmacro

%macro emu_iret_replace_IF 0
  int 0xf8
%endmacro

%macro emu_testP0 0
  int 0xf9
%endmacro

%macro emu_testP1 0
  int 0xfa
%endmacro

%macro emu_disk_handler 0
  int 0xfb
%endmacro

%macro emu_video_handler 0
  int 0xfc
%endmacro






org  100h        ; BIOS loads at offset 0x0100



main:
  jmp  bios_entry


; These values (BIOS ID string, BIOS date and so forth) go at the very top of memory

biosstr  db  'Based on 8086tiny plus 2.34!', 0, 0
mem_top  db  0xea, 0, 0x01, 0, 0xf0, '01/01/91', 0, MODEL, SUBMODEL


bios_entry:

  ; Set up initial stack to F000:F000

  mov   sp, 0xf000
  mov   ss, sp

  push  cs
  pop   es

  push  ax

  ; DL starts off being the boot disk.

  mov   [cs:boot_device], dl

  ; set original location of int 1e (BIOS floppy disk parameters)

  mov ah, 0x08
  mov bx, int1e
  emu_helper

  ; Set up CGA graphics support. We start with the adapter in text mode

  push  dx

  mov   dx, 0x3d8
  mov   al, 0x29
  out   dx, al    ; Set CGA to hires text, video enabled

  mov   dx, 0x3d9
  mov   al, 0x07
  out   dx, al

  pop   dx

  pop   ax

  ; Check cold boot/warm boot. We initialise disk parameters on cold boot only

  cmp   byte [cs:boot_state], 0  ; Cold boot?
  jne   boot

  mov   byte [cs:boot_state], 1  ; Set flag so next boot will be warm boot


; Main BIOS entry point. Zero the flags, and set up registers.

boot:
  mov  ax, 0
  push  ax
  popf

  push  cs
  push  cs
  pop   ds
  pop   ss
  mov   sp, 0xf000

; Set up the IVT. First we zero out the table

  cld

  mov  ax, 0
  mov  es, ax
  mov  di, 0
  mov  cx, 512
  rep  stosw

; Then we load in the pointers to our interrupt handlers

  mov  di, 0
  mov  si, int_table
  mov  cx, [itbl_size]
  rep  movsb

; Set int 43 to the 8x8 double dot char table
  mov  cx, cga_glyphs
  mov  word [es:4*0x43], cx
  mov  cx, 0xf000
  mov  word [es:4*0x43 + 2], cx

; Set up last 16 bytes of memory, including boot jump, BIOS date, machine ID byte

  mov  ax, 0xffff
  mov  es, ax
  mov  di, 0x0
  mov  si, mem_top
  mov  cx, 16
  rep  movsb

; Set up the BIOS data area

  mov  ax, 0x40
  mov  es, ax
  mov  di, 0
  mov  si, bios_data
  mov  cx, 0x100
  rep  movsb

; Clear video memory

  mov  ax, 0xb800
  mov  es, ax
  mov  di, 0
  mov  cx, 80*25
  mov  ax, 0x0700
  rep  stosw

; copy cga font to the correct bios location
  mov  ax, cs
  mov  es, ax
  mov  di, glyphs8x8
  mov  ds, ax
  mov  si, cga_glyphs
  mov  cx, 1024
  rep  movsb

; copy rom_config into BIOS configuration table
  mov  ax, cs
  mov  es, ax
  mov  di, confDataTable
  mov  ds, ax
  mov  si, rom_config
  mov  cx, 16
  rep  movsb

; copy Extended BIOS Data Area (EBDA)
  mov ax, EBDA_SEG
  mov  es, ax
  mov  di, 0x0000
  mov  ax, cs
  mov  ds, ax
  mov  si, eda
  mov  cx, 1024
  rep  movsb

; Add BIOS redirects for 100% compatible vectors
  mov  ax, cs
  mov  es, ax
  mov  di, int09_bios_redirect
  mov  ds, ax
  mov  si, int09_redirect_start
  mov  cx, (int09_redirect_end - int09_redirect_start)
  rep  movsb

; Set initial CRTC register values and CGA registers for 80x25 colour mode
  mov  si, 0
init_crtc_loop:
  mov  dx, 0x3d4
  mov  ax, si
  out  dx, al
  mov  dx, 0x3d5
  mov  al, [cs:si+video_init_table+16]
  out  dx, al

  inc  si
  cmp  si, 16
  jl  init_crtc_loop

  mov  dx, 0x3DA  ; CGA status port
  mov  al, 0
  out  dx, al

  mov  dx, 0x3d8  ; CGA Mode Control port
  mov  al, 0x29
  out  dx, al

  mov  dx, 0x3d9  ; CGA Color Control port
  mov  al, 0x07
  out  dx, al

  mov  dx, 0x3BC  ; LPT1
  mov  al, 0
  out  dx, al

  mov  dx, 0x40  ; PIT channel 0
  mov  al, 0
  out  dx, al    ; Write LSB
  out  dx, al    ; Write MSB

  mov  dx, 0x61  ; Keyboard ctrl / speaker ctrl
  mov  al, 0x50  ; kbd clock enable, RAM parity enable
  out  dx, al

  mov  dx, 0x62  ; PPI - needed for memory parity checks
  mov  al, 0
  out  dx, al

; Initialize the 8259A interrupt controller

  mov  al, 0x13          ; ICW1 - EDGE, SNGL, ICW4
  out  PIC8259A_A00, al
  mov  al, 8             ; setup ICW2 - (08h-0Fh)
  out  PIC8259A_A01, al
  mov  al, 9             ; setup ICW4 - BUFFRD, 8086 MODE
  out  PIC8259A_A01, al
  mov  al, 0x00          ; enable all interrupts
  out  PIC8259A_A01, al

; Initialize the 8259B interrupt controller

  mov  al, 0x13          ; ICW1 - EDGE, SNGL, ICW4
  out  PIC8259B_A00, al
  mov  al, 0x70          ; setup ICW2 - (70h-77h)
  out  PIC8259B_A01, al
  mov  al, 9             ; setup ICW4 - BUFFRD, 8086 MODE
  out  PIC8259B_A01, al
  mov  al, 0x00          ; enable all interrupts
  out  PIC8259B_A01, al

; Initialize RTC

  mov  al, 0x0b
  out  RTC_SEL, al
  mov  al, 0b00000010 ; just set h24 mode
  out  RTC_REG, al
  ; synchronize system tick with RTC
  mov  ah, 0x07
  emu_helper

; Enable interrupts
  sti

; Read boot sector from boot device, and load it into 0:7C00

  mov  ax, 0
  mov  es, ax

  mov  ax, 0x0201
  mov  dh, 0
  mov  dl, [cs:boot_device]
  mov  cx, 1
  mov  bx, 0x7c00
  int  13h

; Jump to boot sector

  jmp  0:0x7c00



reportEOIA:
; signal EOI (End Of Interrupt) to 8259A
  mov   al, PIC8259_EOI
  out   PIC8259A_A00, al
  ret


reportEOIB:
; signal EOI (End Of Interrupt) to 8259B
  mov   al, PIC8259_EOI
  out   PIC8259B_A00, al
  ret



; ************************* INT 07h handler

int7:
  iret



; ************************* INT 08h handler - timer

int8:
  ; timer interupt every 1/18.2 seconds
  push  ax
  push  ds

  mov   ax, 0x40
  mov   ds, ax

  add   word [clk_dtimer_lo - bios_data], 1
  adc   word [clk_dtimer_hi - bios_data], 0

  ; does time count exceed 24 hours?
  ; 60*60*24*18.2 = 0x17fe80
  ; IBM-AT BIOS compares to 0x0018:0x00b0
  cmp   word [clk_dtimer_hi - bios_data], 0x18
  jnz   i8_end
  cmp   word [clk_dtimer_lo - bios_data], 0xb0
  jnz   i8_end

  ; reset counter and set 24h rollover flag
  xor   ax, ax
  mov   [clk_dtimer_hi - bios_data], ax
  mov   [clk_dtimer_lo - bios_data], ax
  mov   byte [clk_rollover - bios_data], 1

i8_end:

  ; transfer control to user routine
  int   0x1c

  call  reportEOIA

  pop   ds
  pop   ax

  iret



; ************************* INT 09h handler - keyboard data ready

int9:

  sti

  push ax
  push ds

  ; Get the bios data segment into ds
  mov  ax, 0x40
  mov  ds, ax

int9_getcode:
  ; read keyboard output buffer
  in   al, 0x60

  ; call INT 15, 4F (keyboard intercept)
  stc
  mov  ah, 0x4f
  int  0x15
  jnc  int9_exit  ; CF=0, keystroke absorbed by INT 15

  ; emulator does the actual job (convert to ASCII and insert into the keyboard buffer)
  mov  ah, 0x00
  emu_helper

  ; check result from emu_helper (in AH)
  cmp  ah, 0x02
  jz   int9_reboot
  cmp  ah, 0x03
  jz   int9_printscreen
  cmp  ah, 0x04
  jz   int9_ctrlbreak
  cmp  ah, 0x05
  jz   int9_sysreq

  ; PAUSE state?
  test byte [keyflags2 - bios_data], 0x08
  jnz int9_pause

int9_exit:
  call reportEOIA
  pop  ds
  pop  ax
  iret

; manage CTRL + ALT + DEL
int9_reboot:
  call reportEOIA
  ; reset using 8042 keyboard controller
  mov al, 0xfe
  out 0x64, al

; manage PAUSE state
int9_pause:
  ; wait for a code from keyboard
  in   al, 0x64
  test al, 0x01
  jz   int9_pause
  jmp  int9_getcode

; manage PRINTSCREEN
int9_printscreen:
  int 0x05
  mov byte [0x100], 0   ; reset PRINTSCREEN flag
  jmp int9_exit

; manage CTRL + BREAK
int9_ctrlbreak:
  int 0x1b
  jmp int9_exit

; manage SYSREQ (ALT + PRINTSCREEN)
int9_sysreq:
  mov ah, 0x85  ; AL already assigned by emulator
  int 0x15
  jmp int9_exit



; ************************* INT 10h handler - video services

int10:
  cmp  ah, 0x00 ; Set video mode
  je   int10_set_vm
  cmp  ah, 0x01 ; Set cursor shape
  je   int10_set_cshape
  cmp  ah, 0x02 ; Set cursor position
  je   int10_set_cursor
  cmp  ah, 0x03 ; Get cursor position
  je   int10_get_cursor
  cmp  ah, 0x05 ; Set active display page
  je   int10_set_disp_page
  cmp  ah, 0x06 ; Scroll up window
  je   int10_scrollup
  cmp  ah, 0x07 ; Scroll down window
  je   int10_scrolldown
  cmp  ah, 0x08 ; Get character at cursor
  je   int10_charatcur
  cmp  ah, 0x09 ; Write char and attribute
  je   int10_write_char_attrib
  cmp  ah, 0x0a ; Write char only
  je   int10_write_char
  cmp  ah, 0x0b ; set colour
  je   int10_set_colour
  cmp  ah, 0x0e ; Write character at cursor position, tty mode
  je   int10_write_char_tty
  cmp  ah, 0x0f ; Get video mode
  je   int10_get_vm
  cmp  ah, 0x10 ; Get/Set Palette registers
  je   int10_palette
  cmp  ax, 0x1123 ; Set int43 to 8x8 double dot font
  je   int10_set_int43_8x8dd
  cmp  ax, 0x1130 ; get font information
  je   int10_get_font_info
  cmp  ah, 0x12 ; Video sub-system configure
  je   int10_video_subsystem_cfg
  cmp  ah, 0x1a ; Video combination
  je   int10_vid_combination
  cmp  ah, 0x1b
  je   int10_get_state_info

  emu_video_handler
  iret


int10_set_vm:

  ; AL contains the requested mode.
  ; CGA supports modes 0 to 6 only
  ; MCGA adds modes 11h and 13h

  push  ds
  push  dx
  push  cx
  push  bx
  push  ax

  push ax
  ; Get the bios data segment into ds
  mov   ax, 0x40
  mov   ds, ax

  ; Disable video out when changing modes
  mov   dx, 0x3d8
  mov   al, 0
  out   dx, al
  pop   ax

;debug - display requested video mode
  push  ax
  mov   al, 'V'
  emu_putchar_al
  mov   al, 'I'
  emu_putchar_al
  mov   al, 'D'
  emu_putchar_al
  pop   ax
  call  puts_hex_al
  push  ax
  mov   al, 0x0a
  emu_putchar_al
  mov   al, 0x0d
  emu_putchar_al
  pop   ax

  mov   ah, al
  and   ax, 0x807f
  push  ax

  ; Store the video mode. This if fixed later for invalid modes.
  mov   byte [vid_mode-bios_data], al

  ; Check MCGA only video modes
  cmp   al, 0x11
  je    int10_set_vm_11
  cmp   al, 0x13
  je    int10_set_vm_13

  ; Get back into CGA emulation mode

  push  ax

  ; Set sequencer registers
  mov  ax, 0x03c4
  mov  dx, ax
  mov  al, 0x01
  out  dx, al
  mov  ax, 0x03c5
  mov  dx, ax
  mov  al, 0x01
  out  dx, al

  mov  ax, 0x03c4
  mov  dx, ax
  mov  al, 0x03
  out  dx, al
  mov  ax, 0x03c5
  mov  dx, ax
  mov  al, 0x00
  out  dx, al

  mov  ax, 0x03c4
  mov  dx, ax
  mov  al, 0x04
  out  dx, al
  mov  ax, 0x03c5
  mov  dx, ax
  mov  al, 0x04
  out  dx, al

  ; Set the Graphics mode register 3 to 0x00.
  mov  ax, 0x03ce
  mov  dx, ax
  mov  al, 0x03
  out  dx, al
  mov  ax, 0x03cf
  mov  dx, ax
  mov  al, 0x00
  out  dx, al

  ; Set the Graphics mode register 5 to 0x10.
  mov  ax, 0x03ce
  mov  dx, ax
  mov  al, 0x05
  out  dx, al
  mov  ax, 0x03cf
  mov  dx, ax
  mov  al, 0x10
  out  dx, al

  ; Set the Graphics mode register 8 to 0xff.
  mov  ax, 0x03ce
  mov  dx, ax
  mov  al, 0x08
  out  dx, al
  mov  ax, 0x03cf
  mov  dx, ax
  mov  al, 0xff
  out  dx, al

  pop  ax

  ; Check CGA video modes
  cmp  al, 0
  je   int10_set_vm_0
  cmp  al, 1
  je   int10_set_vm_1
  cmp  al, 2
  je   int10_set_vm_2
  cmp  al, 3
  je   int10_set_vm_3
  cmp  al, 4
  je   int10_set_vm_4
  cmp  al, 5
  je   int10_set_vm_5
  cmp  al, 6
  je   int10_set_vm_6

  ; All other video modes set mode 3
  mov  al, 3
  mov  byte [vid_mode-bios_data], al
  jmp  int10_set_vm_3

int10_set_vm_0:
  ; BW40
  mov  word [vid_page_size-bios_data], 0x0800
  mov  ax, 0x002c
  mov  bx, 40    ; BX = video columns
  mov  cx, 0x0818  ; CL = video rows, CH = scan lines per character
  jmp  int10_set_vm_write

int10_set_vm_1:
  ; CO40
  mov  word [vid_page_size-bios_data], 0x0800
  mov  ax, 0x0028
  mov  bx, 40    ; BX = video columns
  mov  cx, 0x0818  ; CL = video rows, CH = scan lines per character
  jmp  int10_set_vm_write

int10_set_vm_2:
  ; BW80
  mov  word [vid_page_size-bios_data], 0x1000
  mov  ax, 0x002d
  mov  bx, 80    ; BX = video columns
  mov  cx, 0x0818  ; CL = video rows, CH = scan lines per character
  jmp  int10_set_vm_write

int10_set_vm_3:
  ; CO80
  mov  word [vid_page_size-bios_data], 0x1000
  mov  ax, 0x0029
  mov  bx, 80    ; BX = video columns
  mov  cx, 0x0818  ; CL = video rows, CH = scan lines per character
  jmp  int10_set_vm_write

int10_set_vm_4:
  ; 320x200, 4 colour
  mov  word [vid_page_size-bios_data], 0x4000
  mov  ax, 0x000a
  mov  bx, 40    ; BX = video columns
  mov  cx, 0x0818  ; CL = video rows, CH = scan lines per character
  jmp  int10_set_vm_write

int10_set_vm_5:
  ; 320x200 grey
  mov  word [vid_page_size-bios_data], 0x4000
  mov  ax, 0x000e
  mov  bx, 40    ; BX = video columns
  mov  cx, 0x0818  ; CL = video rows, CH = scan lines per character
  jmp  int10_set_vm_write

int10_set_vm_6:
  ; 640x200 mono

  ; Set int 43 to the 8x8 single dot char table
  push  es
  mov   ax, 0
  mov   es, ax
  mov   cx, cga_glyphs
  mov   word [es:4*0x43], cx
  mov   cx, 0xf000
  mov   word [es:4*0x43 + 2], cx
  pop   es

  mov   word [vid_page_size-bios_data], 0x4000
  mov   ax, 0x071a
  mov   bx, 80      ; BX = video columns
  mov   cx, 0x0818  ; CL = video rows, CH = scan lines per character
  jmp   int10_set_vm_write

int10_set_vm_11:
  ; 640x480, 2 colour
  ; Reset Attribute Controller to index mode
  mov   ax, 0x03da
  mov   dx, ax
  in    al, dx

  ; Update Attribute controller registers
  mov   ax, 0x03c0
  mov   dx, ax

  mov   al, 0x10
  out   dx, al
  mov   al, 0x01
  out   dx, al

  mov   al, 0x11
  out   dx, al
  mov   al, 0x00
  out   dx, al

  mov   al, 0x12
  out   dx, al
  mov   al, 0x0f
  out   dx, al

  mov   al, 0x13
  out   dx, al
  mov   al, 0x00
  out   dx, al

  mov   al, 0x14
  out   dx, al
  mov   al, 0x00
  out   dx, al

  ; set Misc Output Register
  mov   ax, 0x03c2
  mov   dx, ax
  mov   al, 0x63
  out   dx, al

  ; Set Sequence Registers
  mov   ax, 0x03c4
  mov   dx, ax
  mov   al, 0x01
  out   dx, al
  mov   ax, 0x03c5
  mov   dx, ax
  mov   al, 0x01
  out   dx, al

  mov   ax, 0x03c4
  mov   dx, ax
  mov   al, 0x03
  out   dx, al
  mov   ax, 0x03c5
  mov   dx, ax
  mov   al, 0x00
  out   dx, al

  mov   ax, 0x03c4
  mov   dx, ax
  mov   al, 0x04
  out   dx, al
  mov   ax, 0x03c5
  mov   dx, ax
  mov   al, 0x02
  out   dx, al

  ; Set Graphics controller registers
  mov   ax, 0x03ce
  mov   dx, ax
  mov   al, 0x05
  out   dx, al
  mov   ax, 0x03cf
  mov   dx, ax
  mov   al, 0x00
  out   dx, al

  mov   ax, 0x03ce
  mov   dx, ax
  mov   al, 0x06
  out   dx, al
  mov   ax, 0x03cf
  mov   dx, ax
  mov   al, 0x05
  out   dx, al

  ; Set int 43 to the 8x16 single dot char table
  push  es
  mov   ax, 0
  mov   es, ax
  mov   cx, vga_glyphs
  mov   word [es:4*0x43], cx
  mov   cx, 0xf000
  mov   word [es:4*0x43 + 2], cx
  pop   es


  mov   bx, 80      ; BX = video columns
  mov   cx, 0x101d  ; CL = video rows, CH = scan lines per character


  jmp   int10_set_vm_upd

int10_set_vm_13:
  ; 320x200, 256 colour
  ; Reset Attribute Controller to index mode
  mov   ax, 0x03da
  mov   dx, ax
  in    al, dx

  ; Update Attribute controller registers
  mov   ax, 0x03c0
  mov   dx, ax

  mov   al, 0x10
  out   dx, al
  mov   al, 0x41
  out   dx, al

  mov   al, 0x11
  out   dx, al
  mov   al, 0x00
  out   dx, al

  mov   al, 0x12
  out   dx, al
  mov   al, 0x0f
  out   dx, al

  mov   al, 0x13
  out   dx, al
  mov   al, 0x00
  out   dx, al

  mov   al, 0x14
  out   dx, al
  mov   al, 0x00
  out   dx, al

  ; set Misc Output Register
  mov   ax, 0x03c2
  mov   dx, ax
  mov   al, 0x63
  out   dx, al

  ; Set Sequence Registers
  mov   ax, 0x03c4
  mov   dx, ax
  mov   al, 0x01
  out   dx, al
  mov   ax, 0x03c5
  mov   dx, ax
  mov   al, 0x01
  out   dx, al

  mov   ax, 0x03c4
  mov   dx, ax
  mov   al, 0x03
  out   dx, al
  mov   ax, 0x03c5
  mov   dx, ax
  mov   al, 0x00
  out   dx, al

  mov   ax, 0x03c4
  mov   dx, ax
  mov   al, 0x04
  out   dx, al
  mov   ax, 0x03c5
  mov   dx, ax
  mov   al, 0x02
  out   dx, al

  ; Set Graphics controller registers
  mov   ax, 0x03ce
  mov   dx, ax
  mov   al, 0x05
  out   dx, al
  mov   ax, 0x03cf
  mov   dx, ax
  mov   al, 0x40
  out   dx, al

  mov   ax, 0x03ce
  mov   dx, ax
  mov   al, 0x06
  out   dx, al
  mov   ax, 0x03cf
  mov   dx, ax
  mov   al, 0x05
  out   dx, al

  mov   bx, 40    ; BX = video columns
  mov   cx, 0x0818  ; CL = video rows, CH = scan lines per character

  jmp   int10_set_vm_upd

int10_set_vm_write:
  ; CGA video modes
  ; write the video data to the video mode registers and bios data area
  ; al = port 0x3d8 value, ah = port 0x3d9 value
  ; bx = video text columns
  xchg  ah, al
  mov   dx, 0x3d9
  out   dx, al
  mov   byte [vid_3x9-bios_data], al

  xchg  ah, al
  mov   dx, 0x3d8
  out   dx, al
  mov   byte [vid_3x8-bios_data], al


int10_set_vm_upd:
  mov   word [vid_cols-bios_data], bx
  mov   word [vid_rows-bios_data], cx
  mov   byte [disp_page-bios_data], 0x00
  mov   word [vid_page_offset-bios_data], 0x0000

  ; Set all page cursors back to 0, 0
  mov   word [curpos_x-bios_data], 0
  mov   word [curpos_x+2-bios_data], 0
  mov   word [curpos_x+4-bios_data], 0
  mov   word [curpos_x+6-bios_data], 0
  mov   word [curpos_x+8-bios_data], 0
  mov   word [curpos_x+10-bios_data], 0
  mov   word [curpos_x+12-bios_data], 0
  mov   word [curpos_x+14-bios_data], 0

int10_set_vm_cls:
  pop   ax
  cmp   ah, 0x80
  je    int10_set_vm_nocls

  mov   bh, 7            ; Black background, white foreground
  call  clear_screen

int10_set_vm_nocls:
  pop   ax
  pop   bx
  pop   cx
  pop   dx
  pop   ds
  iret

int10_set_cshape:
  ; CH = cursor start line (bits 0-4) and options (bits 5-7).
  ; CL = bottom cursor line (bits 0-4).

  push  ds
  push  ax
  push  cx
  push  dx

  mov   ax, 0x40
  mov   ds, ax

  mov   ax, cx
  and   ch, 01100000b
  cmp   ch, 00100000b
  jne   cur_visible

cur_not_visible:
  mov   cx, ax
  jmp   cur_vischk_done

cur_visible:
  ; Only store cursor shape if visible.
  and   ax, 0x1f1f
  mov   [cur_v_end-bios_data], ax
  or    ax, 0x6000
  mov   cx, ax

cur_vischk_done:
  ; Set CRTC registers for cursor shape
  mov   al, 0x0a
  mov   dx, 0x3d4
  out   dx, al

  mov   al, ch
  mov   dx, 0x3d5
  out   dx, al

  mov   al, 0x0b
  mov   dx, 0x3d4
  out   dx, al

  mov   al, cl
  mov   dx, 0x3d5
  out   dx, al

set_cshape_done:
  pop   dx
  pop   cx
  pop   ax
  pop   ds
  iret

int10_set_cursor:
  ; DH = row.
  ; DL = column.
  ; BH = page number (0..7).

  push  ds
  push  si
  push  ax
  push  dx

  mov   ax, 0x40
  mov   ds, ax

  cmp   bh, 0x07
  jg    int10_set_cursor_done

  push  bx
  mov   bl, bh
  mov   bh, 0
  shl   bx, 1
  mov   si, bx
  pop   bx

  mov   [si+curpos_y-bios_data], dh
  mov   [si+curpos_x-bios_data], dl

  cmp   bh, [disp_page-bios_data]
  jne   int10_set_cursor_done

  mov   [cs:crt_curpos_y], dh
  mov   [cs:crt_curpos_x], dl
  call  set_crtc_cursor_pos

int10_set_cursor_done:
  pop   dx
  pop   ax
  pop   si
  pop   ds
  iret

int10_get_cursor:
  ; On entry:
  ;   BH = page number (0..7).
  ; At exit:
  ;   CH, CL = beginning line of cursor, ending line of cursor
  ;   DH, DL = row, column

  push  ds
  push  si
  push  ax

  mov   ax, 0x40
  mov   ds, ax

  mov   cx, [cur_v_end-bios_data]

  cmp   bh, 0x07
  jg    int10_get_cursor_done

  push  bx
  mov   bl, bh
  mov   bh, 0
  shl   bx, 1
  mov   si, bx
  pop   bx

  mov   dh, [si+curpos_y-bios_data]
  mov   dl, [si+curpos_x-bios_data]

int10_get_cursor_done:
  pop   ax
  pop   si
  pop   ds

  iret

int10_set_disp_page:
  ; On entry:
  ;   AL = page number

  push  ds
  push  si
  push  dx
  push  bx
  push  ax

  push  ax
  mov   al, 'P'
  emu_putchar_al
  mov   al, 'G'
  emu_putchar_al
  mov   al, ' '
  emu_putchar_al
  pop   ax
  call  puts_hex_al
  push  ax
  mov   al, 0x0a
  emu_putchar_al
  mov   al, 0x0d
  emu_putchar_al
  pop   ax

  ; SI = display page cursor pos offset
  mov    bl, al
  mov    bh, 0
  shl    bx, 1
  mov    si, bx

  mov    bx, 0x40
  mov    ds, bx

  cmp    byte [vid_mode-bios_data], 0x00
  je     int10_set_disp_page_t40

  cmp    byte [vid_mode-bios_data], 0x01
  je     int10_set_disp_page_t40

  cmp    byte [vid_mode-bios_data], 0x02
  je     int10_set_disp_page_t80

  cmp    byte [vid_mode-bios_data], 0x03
  je     int10_set_disp_page_t80

  cmp    byte [vid_mode-bios_data], 0x04
  je     int10_set_disp_page_done

  cmp    byte [vid_mode-bios_data], 0x05
  je     int10_set_disp_page_done

  cmp    byte [vid_mode-bios_data], 0x06
  je     int10_set_disp_page_done

  jmp    int10_set_disp_page_done

int10_set_disp_page_t40:
  cmp    al, 8
  jge    int10_set_disp_page_done

  mov    [disp_page-bios_data], al
  mov    bl, 0x08
  mul    byte bl
  mov    bh, al
  mov    bl, 0

  jmp    int10_set_disp_page_upd

int10_set_disp_page_t80:
  cmp    al, 8
  jge    int10_set_disp_page_done

  mov    [disp_page-bios_data], al
  mov    bl, 0x10
  mul    byte bl
  mov    bh, al
  mov    bl, 0

int10_set_disp_page_upd:
  ; bx contains page offset, so store it in the BIOS data area
  mov    [vid_page_offset-bios_data], bx

  ; update CRTC page offset
  mov    dx, 0x03d4
  mov    al, 0x0c
  out    dx, al

  mov    dx, 0x03d5
  mov    al, bh
  out    dx, al

  mov    dx, 0x03d4
  mov    al, 0x0d
  out    dx, al

  mov    dx, 0x03d5
  mov    al, bl
  out    dx, al

  ; update CRTC cursor pos
  mov    dl, [si+curpos_x-bios_data]
  mov    [cs:crt_curpos_x], dl
  mov    dl, [si+curpos_y-bios_data]
  mov    [cs:crt_curpos_y], dl
  call   set_crtc_cursor_pos

int10_set_disp_page_done:
  pop    ax
  pop    bx
  pop    dx
  pop    si
  pop    ds
  iret

int10_scrollup:
  ; AL = number of lines by which to scroll up (00h = clear entire window)
  ; BH = attribute used to write blank lines at bottom of window
  ; CH,CL = row,column of window's upper left corner
  ; DH,DL = row,column of window's lower right corner
  push  bp

  push   ax
  push   ds
  mov    ax, 0x40
  mov    ds, ax
  mov    bp, [vid_page_offset-bios_data]
  pop    ds
  pop    ax

  cmp    al, 0
  jne    int10_scroll_up_window

  call   clear_window
  jmp    int10_scrollup_done

int10_scroll_up_window:

  call  scroll_up_window

int10_scrollup_done:
  pop    bp
  iret

int10_scrolldown:
  ; AL = number of lines by which to scroll down (00h = clear entire window)
  ; BH = attribute used to write blank lines at bottom of window
  ; CH,CL = row,column of window's upper left corner
  ; DH,DL = row,column of window's lower right corner

  push   bp

  push   ax
  push   ds
  mov    ax, 0x40
  mov    ds, ax
  mov    bp, [vid_page_offset-bios_data]
  pop    ds
  pop    ax

  cmp    al, 0
  jne    int10_scroll_down_window

  call   clear_window
  jmp    int10_scrolldown_done

int10_scroll_down_window:
  call   scroll_down_window

int10_scrolldown_done:
  pop    bp
  iret


int10_charatcur:
  ; This returns the character at the cursor.
  ;   BH = display page
  ; On exit:
  ;   AH, AL = attribute code, ASCII character code

  push   ds
  push   es
  push   si
  push   di
  push   bx

  mov    ax, 0x40
  mov    ds, ax

  cmp    byte [vid_mode-bios_data], 0x00
  je     int10_charatcur_t40
  cmp    byte [vid_mode-bios_data], 0x01
  je     int10_charatcur_t40
  cmp    byte [vid_mode-bios_data], 0x02
  je     int10_charatcur_t80
  cmp    byte [vid_mode-bios_data], 0x03
  je     int10_charatcur_t80

  ; graphics mode read char
  cmp    byte [vid_mode-bios_data], 0x04
  jge    int10_charatcur_graph

int10_charatcur_t40:
  cmp    bh, 0x07
  jg     int10_charatcur_done
  jmp    int10_charatcur_page_ok

int10_charatcur_t80:
  cmp    bh, 0x07
  jg     int10_charatcur_done

int10_charatcur_page_ok:
  ; Get bios data cursor position offset for this page into SI
  push   bx
  mov    bl, bh
  mov    bh, 0
  shl    bx, 1
  mov    si, bx
  pop    bx

  ; Get video page offset into DI
  mov    al, [vid_page_size+1-bios_data]
  mul    byte bh
  mov    ah, al
  mov    al, 0
  mov    di, ax

  ; Get video RAM offset for the cursor position into BX
  mov    al, [si+curpos_y-bios_data]
  mul    byte [vid_cols-bios_data]
  add    al, [si+curpos_x-bios_data]
  adc    ah, 0
  shl    ax, 1
  mov    bx, ax

  ; Get video RAM segment into ES
  mov    ax, 0xb800
  mov    es, ax

  mov    ax, [es:bx+di]

int10_charatcur_done:
  pop    bx
  pop    di
  pop    si
  pop    es
  pop    ds

  iret


int10_charatcur_graph:

  ; Get video RAM segment into ES
  mov    ax, 0xb800
  mov    es, ax

  call   curpos_to_offset
  mov    si, ax
  sub    sp, 8
  mov    bp, sp
  cmp    byte [vid_mode - bios_data], 0x06
  push   es
  pop    ds
  jc     int10_charatcur_graph_320x200

; 640x200 read
  mov    dh, 4
int10_charatcur_graph_640x200:
  mov    al, [si]
  mov    [bp], al
  inc    bp
  mov    al, [si + 0x2000]
  mov    [bp], al
  inc    bp
  add    si, 80
  dec    dh
  jnz    int10_charatcur_graph_640x200
  jmp    int10_charatcur_graph_find

; 320x200 read
int10_charatcur_graph_320x200:
  sal    si, 1
  mov    dh, 4
int10_charatcur_graph_320x200_rep1:
  call   int10_charatcur_graph_320x200_read
  add    si, 0x2000
  call   int10_charatcur_graph_320x200_read
  sub    si, 0x2000 - 80
  dec    dh
  jnz    int10_charatcur_graph_320x200_rep1

int10_charatcur_graph_find:
  mov    di, cga_glyphs
  push   cs
  pop    es
  sub    bp, 8
  mov    si, bp
  cld
  mov    al, 0
int10_charatcur_graph_find_1:
  push   ss
  pop    ds
  mov    dx, 128
int10_charatcur_graph_find_2:
  push   si
  push   di
  mov    cx, 8
  repe   cmpsb
  pop    di
  pop    si
  jz     int10_charatcur_graph_endsearch
  inc    al
  add    di, 8
  dec    dx
  jnz    int10_charatcur_graph_find_2
  cmp    al,0
  je     int10_charatcur_graph_endsearch
  sub    ax, ax
  mov    ds, ax
  les    di, [0x1f * 4]
  mov    ax, es
  or     ax, di
  jz     int10_charatcur_graph_endsearch
  mov    al, 128
  jmp    int10_charatcur_graph_find_1

int10_charatcur_graph_endsearch:
  add    sp, 8
  jmp    int10_charatcur_done

int10_charatcur_graph_320x200_read:
  mov    ah, [si]
  mov    al, [si + 1]
  mov    cx, 0xc000
  mov    dl, 0
int10_charatcur_graph_320x200_read_1:
  test   ax, cx
  clc
  jz     int10_charatcur_graph_320x200_read_2
  stc
int10_charatcur_graph_320x200_read_2:
  rcl    dl, 1
  shr    cx, 1
  shr    cx, 1
  jnc    int10_charatcur_graph_320x200_read_1
  mov    [bp], dl
  inc    bp
  ret

curpos_to_offset:
  mov    ax, [cursor_pos - bios_data]
graph_posn:
  push   bx
  mov    bx, ax
  mov    al, ah
  mul    byte [vid_cols - bios_data]
  shl    ax, 1
  shl    ax, 1
  sub    bh, bh
  add    ax, bx
  pop    bx
  ret



int10_write_char_tty:
  ; AL = character to display
  ; BH = page number
  ; BL = foreground pixel colour (gfx mode only)

  push   ds
  push   es
  push   ax
  push   bx
  push   cx
  push   si
  push   di
  push   bp

  push   ax
  mov    ax, 0x40
  mov    ds, ax
  pop    ax

  mov    bh, [disp_page-bios_data]

int10_write_char_tty_1:
  cmp    byte [vid_mode-bios_data], 0x00
  je     int10_write_char_tty_t40
  cmp    byte [vid_mode-bios_data], 0x01
  je     int10_write_char_tty_t40
  cmp    byte [vid_mode-bios_data], 0x02
  je     int10_write_char_tty_t80
  cmp    byte [vid_mode-bios_data], 0x03
  je     int10_write_char_tty_t80

  ; Not a text mode, so only page 1 is valid.
  cmp    bh, 0
  jne    int10_write_char_tty_done
  jmp    int10_write_char_tty_page_ok

int10_write_char_tty_t40:
  cmp    bh, 0x07
  jg     int10_write_char_tty_done
  jmp    int10_write_char_tty_page_ok

int10_write_char_tty_t80:
  cmp    bh, 0x07
  jg     int10_write_char_tty_done

int10_write_char_tty_page_ok:
  ; Get bios data cursor position offset for this page into SI
  push   bx
  mov    bl, bh
  mov    bh, 0
  shl    bx, 1
  mov    si, bx
  pop    bx

  push   ax

  ; Get video page offset into BP
  mov    al, [vid_page_size + 1 - bios_data]
  mul    byte bh
  mov    ah, al
  mov    al, 0
  mov    bp, ax

  ; Get offset for the cursor position within the page into DI
  mov    al, [si+curpos_y-bios_data]
  mul    byte [vid_cols-bios_data]
  add    al, [si+curpos_x-bios_data]
  adc    ah, 0
  shl    ax, 1
  mov    di, ax

  ; Get video RAM segment into ES
  mov    ax, 0xb800
  mov    es, ax

  pop    ax

  cmp    al, 0x08
  je     int10_write_char_tty_bs

  cmp    al, 0x0A
  je     int10_write_char_tty_nl

  cmp    al, 0x0D
  je     int10_write_char_tty_cr

  jmp    int10_write_char_tty_printable

int10_write_char_tty_bs:
  mov    byte [es:bp+di], 0x20
  dec    byte [si+curpos_x-bios_data]
  cmp    byte [si+curpos_x-bios_data], 0
  jg     int10_write_char_tty_set_crtc_cursor

  mov    byte [si+curpos_x-bios_data], 0
  jmp    int10_write_char_tty_set_crtc_cursor

int10_write_char_tty_nl:
  mov    byte [si+curpos_x-bios_data], 0
  inc    byte [si+curpos_y-bios_data]

  mov    cl, [vid_rows-bios_data]
  cmp    byte [si+curpos_y-bios_data], cl
  jbe    int10_write_char_tty_done
  mov    byte [si+curpos_y-bios_data], cl

  push   ax
  push   cx
  push   dx

  mov    al, 1
  mov    bx, 0x0700
  mov    cx, 0
  mov    dh, [vid_rows-bios_data]
  mov    dl, [vid_cols-bios_data]
  dec    dl

  call scroll_up_window

  pop    dx
  pop    cx
  pop    ax

  jmp    int10_write_char_tty_set_crtc_cursor

int10_write_char_tty_cr:
  mov    byte [si+curpos_x-bios_data],0
  jmp    int10_write_char_tty_set_crtc_cursor

int10_write_char_tty_printable:
  cmp    byte [vid_mode-bios_data], 4
  je     int10_write_char_tty_cga320
  cmp    byte [vid_mode-bios_data], 5
  je     int10_write_char_tty_cga320
  cmp    byte [vid_mode-bios_data], 6
  je     int10_write_char_tty_cga640
  cmp    byte [vid_mode-bios_data], 0x11
  je     int10_write_char_tty_mode11
  cmp    byte [vid_mode-bios_data], 0x13
  je     int10_write_char_tty_mode13

  jmp    int10_write_char_tty_text

int10_write_char_tty_cga320:
  call   put_cga320_char
  jmp    int10_write_char_tty_upd_cursor

int10_write_char_tty_cga640:
  call   put_cga640_char
  jmp    int10_write_char_tty_upd_cursor

int10_write_char_tty_mode11:
  call   put_mode11_char
  jmp    int10_write_char_tty_upd_cursor

int10_write_char_tty_mode13:
  call   put_mode13_char
  jmp    int10_write_char_tty_upd_cursor

int10_write_char_tty_text:
  mov    [es:bp+di], al
int10_write_char_tty_upd_cursor:
  inc    byte [si+curpos_x-bios_data]
  mov    ah, [vid_cols-bios_data]
  cmp    byte [si+curpos_x-bios_data], ah
  jge    int10_write_char_tty_nl

int10_write_char_tty_set_crtc_cursor:
  cmp    bh, [disp_page-bios_data]
  jne    int10_write_char_tty_done

  mov    ax, [si+curpos_x-bios_data]
  mov    [cs:crt_curpos_x], ax
  call   set_crtc_cursor_pos

int10_write_char_tty_done:
  pop    bp
  pop    di
  pop    si
  pop    cx
  pop    bx
  pop    ax
  pop    es
  pop    ds

  iret

int10_write_char_attrib:
  ; AL = character to display
  ; BH = page number
  ; BL = attribute (text mode) or foreground colour (gfx mode)
  ; CX = number of times to write character

  push   si
  push   di
  push   ds
  push   es
  push   ax
  push   bx
  push   cx
  push   bp

  push   ax
  mov    ax, 0x40
  mov    ds, ax
  pop    ax

  cmp    byte [vid_mode-bios_data], 0x00
  je     int10_write_char_attrib_t40
  cmp    byte [vid_mode-bios_data], 0x01
  je     int10_write_char_attrib_t40
  cmp    byte [vid_mode-bios_data], 0x02
  je     int10_write_char_attrib_t80
  cmp    byte [vid_mode-bios_data], 0x03
  je     int10_write_char_attrib_t80

  ; Not a text mode, so only page 1 is valid.
  cmp    bh, 0
  jne    int10_write_char_attrib_done
  jmp    int10_write_char_attrib_page_ok

int10_write_char_attrib_t40:
  cmp    bh, 0x07
  jg     int10_write_char_attrib_done
  jmp    int10_write_char_attrib_page_ok

int10_write_char_attrib_t80:
  cmp    bh, 0x07
  jg     int10_write_char_attrib_done

int10_write_char_attrib_page_ok:

  ; Get bios data cursor position offset for this page into SI
  push   bx
  mov    bl, bh
  mov    bh, 0
  shl    bx, 1
  mov    si, bx
  pop    bx

  push   ax

  ; Get video page offset into BP
  mov    al, [vid_page_size+1-bios_data]
  mul    byte bh
  mov    ah, al
  mov    al, 0
  mov    bp, ax

  ; Get offset for the cursor position within the page into DI
  mov    al, [si+curpos_y-bios_data]
  mul    byte [vid_cols-bios_data]
  add    al, [si+curpos_x-bios_data]
  adc    ah, 0
  shl    ax, 1
  mov    di, ax

  ; Get video RAM segment into ES
  mov    ax, 0xb800
  mov    es, ax

  pop    ax

  mov    ah, bl  ; ax = char + attr to write

  mov    bx, 0xb800
  mov    es, bx

int10_write_char_attrib_repeat:
  cmp    cx, 0
  je     int10_write_char_attrib_done

  cmp    byte [vid_mode-bios_data], 4
  je     int10_write_char_attrib_cga320
  cmp    byte [vid_mode-bios_data], 5
  je     int10_write_char_attrib_cga320
  cmp    byte [vid_mode-bios_data], 6
  je     int10_write_char_attrib_cga640
  cmp    byte [vid_mode-bios_data], 0x11
  je     int10_write_char_attrib_mode11
  cmp    byte [vid_mode-bios_data], 0x13
  je     int10_write_char_attrib_mode13

  jmp    int10_write_char_attrib_text

int10_write_char_attrib_cga320:
  call   put_cga320_char
  jmp    int10_write_char_attrib_next

int10_write_char_attrib_cga640:
  call   put_cga640_char
  jmp    int10_write_char_attrib_next

int10_write_char_attrib_mode11:
  call   put_mode11_char
  jmp    int10_write_char_attrib_next

int10_write_char_attrib_mode13:
  call   put_mode13_char
  jmp    int10_write_char_attrib_next

int10_write_char_attrib_text:
  mov    [es:bp+di], ax

int10_write_char_attrib_next:
  dec    cx
  add    di, 2
  jmp    int10_write_char_attrib_repeat

int10_write_char_attrib_done:
  pop    bp
  pop    cx
  pop    bx
  pop    ax
  pop    es
  pop    ds
  pop    di
  pop    si

  iret

int10_write_char:
  ; AL = character to display
  ; BH = page number
  ; BL = foreground colour (gfx mode only)
  ; CX = number of times to write character
  push   si
  push   di
  push   ds
  push   es
  push   ax
  push   bx
  push   cx
  push   bp

  push   ax
  mov    ax, 0x40
  mov    ds, ax
  pop    ax

  cmp    byte [vid_mode-bios_data], 0x00
  je     int10_write_char_t40
  cmp    byte [vid_mode-bios_data], 0x01
  je     int10_write_char_t40
  cmp    byte [vid_mode-bios_data], 0x02
  je     int10_write_char_t80
  cmp    byte [vid_mode-bios_data], 0x03
  je     int10_write_char_t80

  ; Not a text mode, so only page 1 is valid.
  cmp    bh, 0
  jne    int10_write_char_done
  jmp    int10_write_char_page_ok

int10_write_char_t40:
  cmp    bh, 0x07
  jg     int10_write_char_done
  jmp    int10_write_char_page_ok

int10_write_char_t80:
  cmp    bh, 0x07
  jg     int10_write_char_done

int10_write_char_page_ok:

  ; Get bios data cursor position offset for this page into SI
  push   bx
  mov    bl, bh
  mov    bh, 0
  shl    bx, 1
  mov    si, bx
  pop    bx

  push   ax

  ; Get video page offset into BP
  mov    al, [vid_page_size+1-bios_data]
  mul    byte bh
  mov    ah, al
  mov    al, 0
  mov    bp, ax

  ; Get offset for the cursor position within the page into DI
  mov    al, [si+curpos_y-bios_data]
  mul    byte [vid_cols-bios_data]
  add    al, [si+curpos_x-bios_data]
  adc    ah, 0
  shl    ax, 1
  mov    di, ax

  ; Get video RAM segment into ES
  mov    ax, 0xb800
  mov    es, ax

  pop    ax

  mov    bx, 0xb800
  mov    es, bx

int10_write_char_repeat:
  cmp    cx, 0
  je     int10_write_char_done

  cmp    byte [vid_mode-bios_data], 4
  je     int10_write_char_cga320
  cmp    byte [vid_mode-bios_data], 5
  je     int10_write_char_cga320
  cmp    byte [vid_mode-bios_data], 6
  je     int10_write_char_cga640
  cmp    byte [vid_mode-bios_data], 0x11
  je     int10_write_char_mode11
  cmp    byte [vid_mode-bios_data], 0x13
  je     int10_write_char_mode13

  jmp    int10_write_char_text

int10_write_char_cga320:
  call  put_cga320_char
  jmp   int10_write_char_next

int10_write_char_cga640:
  call  put_cga640_char
  jmp   int10_write_char_next

int10_write_char_mode11:
  call  put_mode11_char
  jmp   int10_write_char_next

int10_write_char_mode13:
  call  put_mode13_char
  jmp   int10_write_char_next

int10_write_char_text:
  mov   [es:bp+di], al

int10_write_char_next:
  dec   cx
  add   di, 2
  jmp   int10_write_char_repeat

int10_write_char_done:
  pop   bp
  pop   cx
  pop   bx
  pop   ax
  pop   es
  pop   ds
  pop   di
  pop   si

  iret


int10_set_colour:
  ; BH = 0 => set background colour
  ;           BL = background/border colour (5 bits)
  ; BH = 1 => set palette
  ;           BL = palette id
  ;              0 = red/green/brown
  ;              1 = cyan/magenta/white

  push  dx
  push  bx
  push  ax

  cmp  bh, 1
  je   int10_set_palette

  ; Set BG colour

  and  bl, 0x1f
  mov  dx, 0x3d9
  in   al, dx
  and  al, 0xf0
  or   al, bl
  out  dx, al

  jmp  int10_set_colour_done

int10_set_palette:

  cmp  bl, 0x00
  jne  int10_set_palette1

  jmp  int10_set_palette_update

int10_set_palette1:
  mov  bl, 0x20

int10_set_palette_update:
  mov  dx, 0x3d9
  in   al, dx
  and  al, 0xdF
  or   al, bl
  out  dx, al

int10_set_colour_done:
  pop  ax
  pop  bx
  pop  dx

  iret


int10_get_vm:
  ; int 10, 0f
  ; On exit:
  ;  AH = number of screen columns
  ;  AL = mode currently active
  ;  BH = current display page
  push  ds
  mov   ax, 0x40
  mov   ds, ax

  mov   ah, [vid_cols-bios_data]
  mov   al, [vid_mode-bios_data]
  mov   bh, [disp_page-bios_data]

  pop   ds
  iret

int10_palette:
  ; int 10, 10
  ; Supported functions:
  ;   AL = 10 : set DAC color register
  ;   AL = 12 : set block of DAC color registers
  ;   AL = 15 : get individual colour register
  ;   Al = 17 : get block of DAC colour registers

  cmp  al, 0x10
  je   int10_palette_set1
  cmp  al, 0x12
  je   int10_palette_setblock
  cmp  al, 0x15
  je   int10_palette_get1
  cmp  al, 0x17
  je   int10_palette_getblock
  jmp  int10_palette_done

int10_palette_set1:
  ; BX = register to set
  ; CH = green value
  ; CL = blue value
  ; DH = red value

  push  ax
  push  bx
  push  dx

  push  dx
  mov   ax, 0x03c8
  mov   dx, ax
  mov   ax, bx
  out   dx, al
  pop   bx

  mov   ax, 0x03c9
  mov   dx, ax
  mov   al, bh
  out   dx, al
  mov   al, ch
  out   dx, al
  mov   al, cl
  out   dx, al

  pop   dx
  pop   bx
  pop   ax
  jmp   int10_palette_done

int10_palette_setblock:
  ; BX = first colour register to set
  ; CX = number of colour registers to set
  ; ES:DX = pointer to table of colour values to set
  push   ax
  push   cx
  push   dx
  push   di

  mov   di, dx

  mov   ax, 0x03c8
  mov   dx, ax
  mov   ax, bx
  out   dx, al

  mov   ax, 0x03c9
  mov   dx, ax

int10_palette_setblock_loop:
  cmp   cx, 0
  je    int10_palette_setblock_done

  mov   al, [es:di]
  out   dx, al
  mov   al, [es:di+1]
  out   dx, al
  mov   al, [es:di+2]
  out   dx, al

  add   di, 3
  dec   cx
  jmp   int10_palette_setblock_loop

int10_palette_setblock_done:

  pop   di
  pop   dx
  pop   cx
  pop   ax

  jmp   int10_palette_done

int10_palette_get1:
  ; On entry:
  ;   BL = register to get
  ; On exit:
  ;   CH = green value
  ;   CL = blue value
  ;   DH = red value

  push  ax
  push  dx

  mov   ax, 0x03c7
  mov   dx, ax
  mov   al, bl
  out   dx, al

  mov   ax, 0x03c9
  mov   dx, ax
  in    al, dx
  mov   ah, dl
  in    al, dx
  mov   ch, al
  in    al, dx
  mov   cl, al

  pop   dx
  pop   ax
  jmp   int10_palette_done

int10_palette_getblock:
  ; BX = first colour register to set
  ; CX = number of colour registers to set
  ; ES:DX = pointer to table of colour values to get
  push  ax
  push  cx
  push  dx
  push  di

  mov   di, dx

  mov   ax, 0x03c7
  mov   dx, ax
  mov   ax, bx
  out   dx, al

  mov   ax, 0x03c9
  mov   dx, ax

int10_palette_getblock_loop:
  cmp   cx, 0
  je    int10_palette_getblock_done

  in    al, dx
  mov   [es:di], al
  in    al, dx
  mov   [es:di+1], al
  in    al, dx
  mov   [es:di+2],al

  add   di, 3
  dec   cx
  jmp   int10_palette_getblock_loop

int10_palette_getblock_done:

  pop   di
  pop   dx
  pop   cx
  pop   ax

int10_palette_done:
  iret

int10_set_int43_8x8dd:

  push  ax
  mov   al, '8'
  emu_putchar_al
  mov   al, 'x'
  emu_putchar_al
  mov   al, '8'
  emu_putchar_al
  mov   al, 0x0a
  emu_putchar_al
  mov   al, 0x0d
  emu_putchar_al
  pop   ax

  ; Set int 43 to the 8x8 double dot char table
  push  es
  push  ax
  mov   ax, 0
  mov   es, ax
  mov   ax, cga_glyphs
  mov   word [es:4*0x43], ax
  mov   ax, 0xf000
  mov   word [es:4*0x43 + 2], ax
  pop   ax
  pop   es
  iret

int10_get_font_info:
  ; int 10, 1130
  ; BH = information desired:
  ;      = 0  INT 1F pointer
  ;      = 1  INT 44h pointer
  ;      = 2  ROM 8x14 pointer
  ;      = 3  ROM 8x8 double dot pointer (base)
  ;      = 4  ROM 8x8 double dot pointer (top)
  ;      = 5  ROM 9x14 alpha alternate pointer
  ;      = 6  ROM 8x16 character table pointer
  ;      = 7  ROM 9x16 alternate character table pointer
  ; NOTE: Currently only BH = 0 is supported

  push  ax
  push  ds

  cmp   bh, 0x00
  je    int10_get_font_info_int1f_vector

  jmp   int10_get_font_info_done

int10_get_font_info_int1f_vector:
  mov   ax, 0x0000
  mov   ds, ax
  mov   ax, [ds:0x007e]
  mov   es, ax
  mov   bp, [ds:0x007c]

  mov   cx, 0x0008
  mov   dl, 24

int10_get_font_info_done:
  pop   ds
  pop   ax
  iret

int10_video_subsystem_cfg:
  ; int 10, 12
  ; Not supported on CGA or MCGA
  ; On entry
  ;   BL = 10 => return video configuration information
  ; on return:
  ;   BH = 0 if color mode in effect
  ;      = 1 if mono mode in effect
  ;   BL = 0 if 64k EGA memory
  ;      = 1 if 128k EGA memory
  ;      = 2 if 192k EGA memory
  ;      = 3 if 256k EGA memory
  ;   CH = feature bits
  ;   CL = switch settings
  ;
  ;   BL = other = unsupported
  iret

int10_vid_combination:
  ; int 10, 1a
  ; On entry:
  ;   AL = 0 => Get vidio display
  ;   AL = 1 => set video display (unsupported)
  ; On exit:
  ;   AL = 1A, if a valid function was requested
  ;   BL = active video display
  ;   BH = inactive video display
  ;
  ; Valid display codes:
  ;   FF  Unrecognized video system
  ;   00  No display
  ;   01  MDA with monochrome display
  ;   02  CGA with color display
  ;   03  Reserved
  ;   04  EGA with color display
  ;   05  EGA with monochrome display
  ;   06  Professional graphics controller
  ;   07  VGA with analog monochrome display
  ;   08  VGA with analog color display
  ;   09  Reserved
  ;   0A  MCGA with digital color display
  ;   0B  MCGA with analog monochrome display
  ;   0C  MCGA with analog color display
  cmp   al, 0
  jne   int10_vid_combination_done

  ;mov  bl, 0x0C  ; MCGA with colour display
  mov   bl, 0x02 ; CGA with color display

  mov   bh, 0  ; no inactive display
  mov   al, 0x1a

  int10_vid_combination_done:
  iret

int10_get_state_info:
  ; int 10, 1b
  ; On entry:
  ;   BX = implementation type (must be zero)
  ;   ES:DI = pointer to 64 byte buffer
  ; On exit:
  ;   AL = 1B
  ;   ES:DI = pointer to updated buffer
  push ax
  push ds

  mov  ax, 0x40
  mov  ds, ax

  mov  word [es:di], vid_static_table
  mov  word [es:di+2], 0xf000

  ; Copy video state from bios data area (30 bytes)
  mov  ax, [vid_mode-bios_data]
  mov  [es:di+0x04], ax
  mov  ax, [vid_mode-bios_data+2]
  mov  [es:di+0x06], ax
  mov  ax, [vid_mode-bios_data+4]
  mov  [es:di+0x08], ax
  mov  ax, [vid_mode-bios_data+6]
  mov  [es:di+0x0a], ax
  mov  ax, [vid_mode-bios_data+8]
  mov  [es:di+0x0c], ax
  mov  ax, [vid_mode-bios_data+10]
  mov  [es:di+0x0e], ax
  mov  ax, [vid_mode-bios_data+12]
  mov  [es:di+0x10], ax
  mov  ax, [vid_mode-bios_data+14]
  mov  [es:di+0x12],ax
  mov  ax, [vid_mode-bios_data+16]
  mov  [es:di+0x14], ax
  mov  ax, [vid_mode-bios_data+18]
  mov  [es:di+0x16], ax
  mov  ax, [vid_mode-bios_data+20]
  mov  [es:di+0x18], ax
  mov  ax, [vid_mode-bios_data+22]
  mov  [es:di+0x1a], ax
  mov  ax, [vid_mode-bios_data+24]
  mov  [es:di+0x1c], ax
  mov  ax, [vid_mode-bios_data+26]
  mov  [es:di+0x1d], ax
  mov  ax, [vid_mode-bios_data+28]
  mov  [es:di+0x20], ax

  mov  al, [vid_rows-bios_data]
  mov  byte [es:di+0x22], al
  mov  word [es:di+0x23], 0x0008
  mov  byte [es:di+0x25], 0x0c    ; active display combination code (0x0c=MCGA)
  mov  byte [es:di+0x26], 0x00    ; inactive display combination code
  mov  word [es:di+0x27], 0x0010  ; Number of dislayed colours

  cmp  byte [vid_mode-bios_data], 0x01
  jle  int10_get_state_info_pages_8
  cmp  byte [vid_mode-bios_data], 0x03
  jle  int10_get_state_info_pages_4
  mov  byte [es:di+0x29], 0x0001  ; number of supported video pages in gfx modes
  jmp  int10_get_state_info_pages_done
int10_get_state_info_pages_8:
  mov  byte [es:di+0x29], 0x0008  ; number of supported video pages in modes 0, 1
  jmp  int10_get_state_info_pages_done
int10_get_state_info_pages_4:
  mov  byte [es:di+0x29], 0x0004  ; number of supported video pages in modes 2, 3
int10_get_state_info_pages_done:
  mov  byte [es:di+0x2a], 0x00    ; Number of raster scan lines, 0 = 200, 1 = 350, 2 = 400, 3 = 480
  mov  byte [es:di+0x2b], 0x00    ; text character table used
  mov  byte [es:di+0x2c], 0x00    ; text character table used
  mov  byte [es:di+0x2d], 0x00    ; other state information

  pop  ds
  pop  ax
  mov  al, 0x1b
  iret



; ************************* INT 11h - get equipment list

int11:
  mov  ax, [cs:equip]
  iret



; ************************* INT 12h - return memory size

int12:
  mov   ax, [cs:memsize]
  iret



; ************************* INT 13h handler - disk services

int13:

  emu_disk_handler
  emu_iret_replace_CF
  sti                   ; MSDOS requires interrupts enabled on exit
  emu_iret_replace_IF
  iret



; ************************* INT 14h - serial port functions

; baud rate divisors
baud_rate_div:
  dw  0x0417  ; 110
  dw  0x0300  ; 150
  dw  0x0180  ; 300
  dw  0x00c0  ; 600
  dw  0x0060  ; 1200
  dw  0x0030  ; 2400
  dw  0x0018  ; 4800
  dw  0x000c  ; 9600

int14:
  cmp  ah, 0
  je   int14_init
  cmp  ah, 1
  je   int14_write_char
  cmp  ah, 2
  je   int14_read_char
  cmp  ah, 3
  je   int14_get_port_status

  iret

int14_init:
  ; On entry:
  ;   AL = port parameters
  ;        7-5    data rate (110,150,300,600,1200,2400,4800,9600 bps)
  ;        4-3    parity (00 or 10 = none, 01 = odd, 11 = even)
  ;        2      stop bits (set = 2, clear = 1)
  ;        1-0    data bits (00 = 5, 01 = 6, 10 = 7, 11 = 8)
  ;   DX = Port number (0x00 - 0x03)
  ; On return:
  ;   AH = line status
  ;        7 timeout
  ;        6 transmit shift register empty
  ;        5 transmit holding register empty
  ;        4 break detected
  ;        3 framing error
  ;        2 parity error
  ;        1 overrun error
  ;        0 receive data ready
  ;   AL = modem status
  ;        7      carrier detect
  ;        6      ring indicator
  ;        5      data set ready
  ;        4      clear to send
  ;        3      delta carrier detect
  ;        2      trailing edge of ring indicator
  ;        1      delta data set ready
  ;        0      delta clear to send

  push  bx
  push  cx
  push  dx
  push  ds

  ; Get the bios data area into ds
  mov  bx, 0x0040
  mov  ds, bx

  ; Get the serial port base address for this port
  and  dx, 0x0003
  shl  dx, 1
  mov  bx, dx
  mov  dx, [com1addr-bios_data+bx]

  ; set baud rate divisor
  mov  bx, 0
  mov  bl, al
  and  bl, 0xe0
  shr  bl, 1
  shr  bl, 1
  shr  bl, 1
  shr  bl, 1
  shr  bl, 1
  shl  bl, 1
  mov  cx, [cs:baud_rate_div+bx]

  push  ax
  add  dx, 3    ; dx = line control register
  mov  al, 0x80  ; turn on bit 7 (divisor latch)
  out  dx, al
  sub  dx, 2    ; MSB of divisor latch
  mov  al, ch
  out  dx, al
  dec  dx    ; LSB of divisor latch
  mov  al, cl
  out  dx, al
  pop  ax

  ; Initialise the Line Control Register
  mov  bl, al    ; Set stop bits and data bits into bl
  and  bl, 0x07
  mov  cl, al    ; Set parity bits and into bl
  and  cl, 0x18
  shl  cl, 1
  or   bl, cl
  add  dx, 3    ; write Line Control register
  mov  bl, al
  out  dx, al

  ; Assert RTS and DTR in the modme control register
  inc  dx
  mov  al, 0x03
  out  dx, al

  ; Disable all interrupts
  sub  dx, 3    ; Set Interrupt Control Register
  mov  al, 0
  out  dx, al

  ; Get status
  add  dx, 2    ; Get Line status Register
  in   al, dx
  mov  ah, al
  inc  dx    ; get Modem Status Register
  in   al, dx

  pop  ds
  pop  dx
  pop  cx
  pop  bx
  iret

int14_write_char:
  ; On entry:
  ;   AL = character to write
  ;   DX = Port number
  ; On return
  ;   AH bit 7 clear on success, set on error
  ;          as per 6..0 line status
  push  bx
  push  cx
  push  dx
  push  ds

  ; re-enable interrupts as we need the timer.
  sti

  ; Get the bios data area into ds
  mov  bx, 0x0040
  mov  ds, bx

  ; Get the serial port timeout and base address for this port
  ;   cl = timeout ticks
  ;   ch = character to send
  ;   dx = com port address
  ;   bh = initial timer tick LSB
  and  dx, 0x0003
  mov  bx, dx
  mov  cl, [com1_timeout-bios_data+bx]
  shl  dx, 1
  mov  bx, dx
  mov  dx, [com1addr-bios_data+bx]

  mov  ch, al

  add  dx, 5  ; dx = line status register
  mov  bh, [clk_dtimer-bios_data]

  ; Loop until either:
  ;   transmit holding empty
  ;   timeout
int14_write_loop:
  in   al, dx
  test al, 0x20
  jnz  int14_write_send

  mov  bl, [clk_dtimer-bios_data]
  sub  bl, bh
  cmp  bl, cl
  jle  int14_write_loop

  mov  ah, al
  or   ah, 0x80
  jmp  int14_write_done

int14_write_send:
  sub  dx, 5
  mov  al, ch
  out  dx, al

  ; get line status
  add  dx, 5
  in   al, dx
  mov  al, ah
  or   al, 0x7f
  mov  al, ch

int14_write_done:
  pop  ds
  pop  dx
  pop  cx
  pop  bx
  iret

int14_read_char:
  ; On entry:
  ;   DX = port number
  ; On return:
  ;   AH = line status
  ;   AL = received character if AH bit 7 is clear
  push  bx
  push  cx
  push  dx
  push  ds

  ; re-enable interrupts as we need the timer.
  sti

  ; Get the bios data area into ds
  mov  bx, 0x0040
  mov  ds, bx

  ; Get the serial port timeout and base address for this port
  ;   cl = timeout ticks
  ;   dx = com port address
  ;   bh = initial timer tick LSB
  and  dx, 0x0003
  mov  bx, dx
  mov  cl, [com1_timeout-bios_data+bx]
  shl  dx, 1
  mov  bx, dx
  mov  dx, [com1addr-bios_data+bx]

  add  dx, 5  ; dx = line status register
  mov  bh, [clk_dtimer-bios_data]

  ; Loop until either:
  ;   receive holding is not empty
  ;   timeout
int14_read_loop:
  in   al, dx
  test al, 0x01
  jnz  int14_read_get

  mov  bl, [clk_dtimer-bios_data]
  sub  bl, bh
  cmp  bl, cl
  jle  int14_read_loop

  mov  ah, al
  or   ah, 0x80
  jmp  int14_read_done

int14_read_get:
  sub  dx, 5
  in   al, dx
  mov  ch, al

  ; get line status
  add  dx, 5
  in   al, dx
  mov  ah, al
  or   ah, 0x7f
  mov  al, ch

int14_read_done:
  pop  ds
  pop  dx
  pop  cx
  pop  bx
  iret

int14_get_port_status:
  ; On entry:
  ;   DX = port number
  ; On return:
  ;   AH = line status
  ;   AL = modem status
  push  bx
  push  cx
  push  dx
  push  ds

  ; Get the bios data area into ds
  mov  bx, 0x0040
  mov  ds, bx

  ; Get the serial base address for this port
  ;   dx = com port address
  and  dx, 0x0003
  shl  dx, 1
  mov  bx, dx
  mov  dx, [com1addr-bios_data+bx]

  ; read line status and modem status registers
  add  dx, 5  ; dx = line status register
  in   al, dx
  mov  ah, al
  inc  dx  ; dx = modem status register
  in   al, dx

  pop  ds
  pop  dx
  pop  cx
  pop  bx
  iret



; ************************* INT 15h - System Services

int15:

  cmp  ah, 0x80
  je   int15_80   ; Device Open
  cmp  ah, 0x81
  je   int15_81   ; Device Close
  cmp  ah, 0x82
  je   int15_82   ; Program Termination
  cmp  ah, 0x83
  je   int15_83   ; Set Event Wait Interval
  cmp  ah, 0x85
  je   int15_85   ; System Request Key
  cmp  ah, 0x86
  je   int15_86   ; Wait
  cmp  ah, 0xc0
  je   int15_c0
  cmp  ah, 0xc1
  je   int15_c1
  cmp  ah, 0xc2
  je   int15_c2

; function not supported
  mov  ah, 0x86
  jmp  reach_stack_stc


; Device Open
int15_80:
; Device Close
int15_81:
; Program Termination
int15_82:
; System Request Key
int15_85:
  mov  ah, 0x00
  jmp  reach_stack_clc


; Set Event Wait Interval
int15_83:

  push ds

  ; load DS with bios data
  push ax
  mov  ax, 0x0040
  mov  ds, ax
  pop  ax

  ; cancel set interval?
  cmp  al, 0x01
  je   int15_83_cancel   ; yes, jump to service routine

  ; check if wait already in progress
  test byte [wait_active_flag - bios_data], 0x01
  jnz  int15_83_err

  ; setup count
  mov  [wait_count_lo - bios_data], dx           ; microseconds low byte
  mov  [wait_count_hi - bios_data], cx           ; microseconds high byte
  mov  [usr_waitflag_off - bios_data], bx        ; offset of user flag byte
  mov  [usr_waitfalg_seg - bios_data], es        ; segment of user flag byte
  mov  byte [wait_active_flag - bios_data], 0x01 ; wait active flag

  ; setup RTC periodic interrupt
  mov  al, 0x0a     ; select register A
  out  RTC_SEL, al
  in   al, RTC_REG
  and  al, 0xf0
  or   al, 0b0110   ; rate = 1024Khz (976.562us)
  out  RTC_REG, al
  mov  al, 0x0b     ; select register B
  out  RTC_SEL, al
  in   al, RTC_REG
  or   al, 0x40     ; set PIE
  out  RTC_REG, al

  ; success
  pop  ds
  mov  ah, 0x00
  jmp  reach_stack_clc

; int 15, 83, subfunction 01 (cancel set event)
int15_83_cancel:
  ; clear PIE
  mov  al, 0x0b      ; select register B
  out  RTC_SEL, al
  in   al, RTC_REG
  and  al, 0xbf
  out  RTC_REG, al
  ; clear wait_active_flag
  mov  byte [wait_active_flag - bios_data], 0x00
  ; success
  pop  ds
  mov  ah, 0x00
  jmp  reach_stack_clc

; wait already in progress
int15_83_err:
  pop  ds
  mov  ah, 0x00
  mov  al, 0x00
  jmp  reach_stack_stc


; Wait
int15_86:
  push ds
  push ax

  ; load DS with bios data
  mov  ax, 0x0040
  mov  ds, ax

  ; other wait already active?
  test byte [wait_active_flag - bios_data], 0x01
  jnz  int15_86_err     ; yes, error

  ; setup fields
  mov  word [usr_waitflag_off - bios_data], wait_active_flag - bios_data
  mov  word [usr_waitfalg_seg - bios_data], ds
  mov  word [wait_count_lo - bios_data], dx
  mov  word [wait_count_hi - bios_data], cx
  mov  byte [wait_active_flag - bios_data], 0x01

  ; setup RTC periodic interrupt
  mov  al, 0x0a     ; select register A
  out  RTC_SEL, al
  in   al, RTC_REG
  and  al, 0xf0
  or   al, 0b0110   ; rate = 1024Khz (976.562us)
  out  RTC_REG, al
  mov  al, 0x0b     ; select register B
  out  RTC_SEL, al
  in   al, RTC_REG
  or   al, 0x40     ; set PIE
  out  RTC_REG, al

  ; and wait...
  sti
int15_86_wait:
  test byte [wait_active_flag - bios_data], 0x80
  jz   int15_86_wait
  mov  byte [wait_active_flag - bios_data], 0x00
  pop  ax
  pop  ds
  jmp  reach_stack_clc

int15_86_err:
  pop  ax
  pop  ds
  jmp  reach_stack_stc


; Return address of system configuration table in ROM
int15_c0:
  mov  bx, cs
  mov  es, bx
  mov  bx, confDataTable
  mov  ah, 0
  jmp  reach_stack_clc


; RETURN EXTENDED-BIOS DATA-AREA SEGMENT ADDRESS
int15_c1:
  push ax
  mov  ax, EBDA_SEG
  mov  es, ax
  pop  ax
  jmp  reach_stack_clc


; Pointing device interface
int15_c2:
  mov ah, 0x06  ; helper for Pointing device interface
  emu_helper
  emu_iret_replace_CF
  iret


; return from INT setting AH and CF for "unsupported BIOS function"
iret_unsupported_bios_func:
  mov ah, 0x86
  stc
  emu_iret_replace_CF
  iret



; ************************* INT 16h handler - keyboard

int16:
  cmp  ah, 0x00 ; Get keystroke (remove from buffer)
  je   int16_00
  cmp  ah, 0x01 ; Check for keystroke (do not remove from buffer)
  je   int16_01
  cmp  ah, 0x02 ; Check shift flags
  je   int16_02
  cmp  ah, 0x03 ; Set typematic rate and delay
  je   int16_03
  cmp  ah, 0x05 ; Store Key Data
  je   int16_05
  cmp  ah, 0x09 ; Get keyboard functionality
  je   int16_09
  cmp  ah, 0x10 ; Read Extended Keyboard input
  je   int16_10
  cmp  ah, 0x11 ; Read Extended Keyboard Status
  je   int16_11
  cmp  ah, 0x12 ; Check shift flags
  je   int16_12
  cmp  ah, 0x92 ; Check for undocumented cap checks
  je   int16_92

  ; unsupported function
  jmp  iret_unsupported_bios_func


; INT 16, function 0x00 - Read Keyboard Input
; no register is changed other than return values
int16_00:

  sti

int16_00_wait:

  ; ask emulator for non extended key from buffer
  mov ah, 0x01
  mov al, 0x03  ; ask to remove from buffer, filter extended keys
  emu_helper

  ; ZF = 1 : key not ready
  jz int16_00_wait

  iret


; INT 16, function 0x01 - Read Keyboard Status
; no register is changed other than return values
int16_01:

  sti

  ; ask emulator for non extended key from buffer
  mov ah, 0x01
  mov al, 0x02  ; do not remove from buffer, filter extended keys
  emu_helper

  ; pass ZF to interrupt caller
  emu_iret_replace_ZF
  ; pass IF to interrupt caller (this is done by almost all BIOS I've seen)
  emu_iret_replace_IF

  iret


; INT 16, function 0x02 - Return Shift Flags Status
; no register is changed other than return values
int16_02:

  ; ask emulator for keyboard flags
  mov ah, 0x02
  mov al, 0x00
  emu_helper

  iret


; INT 16, function 0x03 - Set typematic rate and delay
; no register is changed
int16_03:

  ; ask emulator for service
  mov ah, 0x03
  emu_helper

  iret


; INT 16, function 0x05 - Store Key Data
; no register is changed other than return values
int16_05:

  ; ask emulator for service
  mov ah, 0x05
  emu_helper

  ; pass CF to interrupt caller
  emu_iret_replace_CF

  iret


; INT 16, function 0x09 - Get Keyboard functionality
int16_09:

  mov al, 0b00001111
  iret


; INT 16, function 0x10 - Read Extended Keyboard input
; no register is changed other than return values
int16_10:

  sti

int16_10_wait:

  ; ask emulator for extended key from buffer
  mov ah, 0x01
  mov al, 0x01  ; ask to remove from buffer, do not filter
  emu_helper

  ; ZF = 1 : key not ready
  jz int16_10_wait

  iret


; INT 16, function 0x11 - Read Keyboard Extended Status
; no register is changed other than return values
int16_11:

  ; ask emulator for non extended key from buffer
  mov ah, 0x01
  mov al, 0x00  ; do not remove from buffer, do not filter
  emu_helper

  ; pass ZF to interrupt caller
  emu_iret_replace_ZF

  iret


; INT 16, function 0x12 - Return Extended Shift Flags
; no register is changed other than return values
int16_12:

  ; ask emulator for keyboard extended flags
  mov ah, 0x02
  mov al, 0x01
  emu_helper

  iret


; INT 16, function 0x92 - Undocumented extended keyboard check
; see http://www.ctyme.com/intr/rb-1867.htm
int16_92:

  mov ah, 0x7f
  stc
  emu_iret_replace_CF
  iret




; ************************* INT 17h handler - printer

int17:
  cmp  ah, 0x01
  je   int17_initprint ; Initialise printer

  iret

int17_initprint:

  mov  ah, 0
  iret



; ************************* INT 19h = reboot

int19:
  jmp  boot



; ************************* INT 1Ah - System-Timer and Real-Time Clock Services

int1a:
  cmp   ah, 0x00
  je    int1a_00  ; Read System Time Counter
  cmp   ah, 0x01
  je    int1a_01  ; Set System Time Counter
  cmp   ah, 0x02
  je    int1a_02  ; Read Real-Time Clock Time
  cmp   ah, 0x03
  je    int1a_03  ; Set Real-Time Clock Time
  cmp   ah, 0x04
  je    int1a_04  ; Read Real-Time Clock Date
  cmp   ah, 0x05
  je    int1a_05  ; Set Real-Time Clock Date
  cmp   ah, 0x06
  je    int1a_06  ; Set Real-Time Clock Alarm
  cmp   ah, 0x07
  je    int1a_07  ; Reset Real-Time Clock Alarm

  ; unsupported function
  stc
  emu_iret_replace_CF
  iret

  ; success (AH = 0x00, CF = 0)
int1a_success:
  mov  ah, 0x00
  clc
  emu_iret_replace_CF
  iret

  ; fail (AH = 0x00, AL = 0x00, CF = 1)
int1a_fail:
  xor ax, ax
  stc
  emu_iret_replace_CF
  iret

; Read System Time Counter
int1a_00:
  push ds
  mov  dx, 0x0040
  mov  ds, dx
  mov  ah, 0x00                         ; AH = 0x00 (success)
  mov  al, [clk_rollover - bios_data]   ; AL = rollover flag
  mov  [clk_rollover - bios_data], ah   ; reset rollover flag
  mov  cx, [clk_dtimer_hi - bios_data]  ; CX = ticks high word
  mov  dx, [clk_dtimer_lo - bios_data]  ; DX = ticks low word
  pop  ds
  iret


; Set System Time Counter
int1a_01:
  push ds
  push dx
  mov  dx, 0x0040
  mov  ds, dx
  pop  dx
  mov  ah, 0x00                         ; AH = 0x00 (success)
  mov  [clk_rollover - bios_data],  ah  ; reset rollover flag
  mov  [clk_dtimer_hi - bios_data], cx  ; ticks high word = CX
  mov  [clk_dtimer_lo - bios_data], dx  ; ticks low word  = DX
  pop  ds
  iret


; Read Real-Time Clock Time
; note: because emulation we never check that RTC is actually updating
int1a_02:
  ; DL = daylight
  mov  al, 0x0b
  out  RTC_SEL, al
  in   al, RTC_REG
  and  al, 1
  mov  dl, al
  ; DH = seconds in BCD
  mov  al, 0x00
  out  RTC_SEL, al
  in   al, RTC_REG
  mov  dh, al
  ; CL = minutes in BCD
  mov  al, 0x02
  out  RTC_SEL, al
  in   al, RTC_REG
  mov  cl, al
  ; CH = AL = hours in BCD
  mov  al, 0x04
  out  RTC_SEL, al
  in   al, RTC_REG
  mov  ch, al
  ; exit
  jmp int1a_success


; Set Real-Time Clock Time
; note: because emulation we never check that RTC is actually updating
int1a_03:
  ; set seconds in BCD (from DH)
  mov  al, 0x00
  out  RTC_SEL, al
  mov  al, dh
  out  RTC_REG, al
  ; set minutes in BCD (from CL)
  mov  al, 0x02
  out  RTC_SEL, al
  mov  al, cl
  out  RTC_REG, al
  ; set hours in BCD (from CH)
  mov  al, 0x04
  out  RTC_SEL, al
  mov  al, ch
  out  RTC_REG, al
  ; set daylight (from DL)
  mov  al, 0x0b
  out  RTC_SEL, al
  in   al, RTC_REG
  and  al, 0xfe
  or   al, dl
  or   al, 0x02     ; set 24h mode
  out  RTC_REG, al  ; AL = Value written to CMOS RAM register OBh
  ; exit
  jmp int1a_success


; Read Real-Time Clock Date
; note: because emulation we never check that RTC is actually updating
int1a_04:
  ; CL = year in BCD
  mov  al, 0x09
  out  RTC_SEL, al
  in   al, RTC_REG
  mov  cl, al
  ; DH = month in BCD
  mov  al, 0x08
  out  RTC_SEL, al
  in   al, RTC_REG
  mov  dh, al
  ; DL = day in BCD
  mov  al, 0x07
  out  RTC_SEL, al
  in   al, RTC_REG
  mov  dl, al
  ; CH = AL = century
  mov  al, 0x32
  out  RTC_SEL, al
  in   al, RTC_REG
  mov  ch, al
  ; exit
  jmp int1a_success


; Set Real-Time Clock Date
; note: because emulation we never check that RTC is actually updating
int1a_05:
  ; set century in BCD (from CH)
  mov  al, 0x32
  out  RTC_SEL, al
  mov  al, ch
  out  RTC_REG, al
  ; set year in BCD (from CL)
  mov  al, 0x09
  out  RTC_SEL, al
  mov  al, cl
  out  RTC_REG, al
  ; set month in BCD (from DH)
  mov  al, 0x08
  out  RTC_SEL, al
  mov  al, dh
  out  RTC_REG, al
  ; set month in BCD (from DL)
  mov  al, 0x07
  out  RTC_SEL, al
  mov  al, dl
  out  RTC_REG, al
  ; set 24h mode
  mov  al, 0x0b
  out  RTC_SEL, al
  in   al, RTC_REG
  or   al, 0x02     ; set 24h mode
  out  RTC_REG, al  ; AL = Value written to CMOS RAM register OBh
  ; exit
  jmp int1a_success


; Set Real-Time Clock Alarm
; note: because emulation we never check that RTC is actually updating
int1a_06:
  ; alarm already in progress?
  mov  al, 0x0b
  out  RTC_SEL, al
  in   al, RTC_REG
  test al, 0x20     ; AIE = 1?
  jnz  int1a_fail   ; fail, alarm is already in progress
  ; set seconds in BCD (from DH)
  mov  al, 0x01
  out  RTC_SEL, al
  mov  al, dh
  out  RTC_REG, al
  ; set minutes in BCD (from CL)
  mov  al, 0x03
  out  RTC_SEL, al
  mov  al, cl
  out  RTC_REG, al
  ; set hours in BCD (from CH)
  mov  al, 0x05
  out  RTC_SEL, al
  mov  al, ch
  out  RTC_REG, al
  ; enable alarm interrupt (AIE)
  mov  al, 0x0b
  out  RTC_SEL, al
  in   al, RTC_REG
  or   al, 0x20
  out  RTC_REG, al
  ; exit
  mov  al, 0x00
  jmp int1a_success


; Reset Real-Time Clock Alarm
; note: because emulation we never check that RTC is actually updating
int1a_07:
  ; disable alarm interrupt (AIE)
  mov  al, 0x0b
  out  RTC_SEL, al
  in   al, RTC_REG
  and  al, 0xdf     ; reset AIE
  out  RTC_REG, al
  ; exit
  jmp int1a_success



; ************************* INT 1Ch - the other timer interrupt

int1c:

  iret



; ************************* INT 1Eh - diskette parameter table

int1e:

    db 10101111B  ; 0x00
    db 2          ; 0x01
    db 37         ; 0x02
    db 2          ; 0x03, bytes per sector (00h = 128, 01h = 256, 02h = 512, 03h = 1024)
    db 18         ; 0x04, sectors per track
    db 0x1B       ; 0x05
    db 0xFF       ; 0x06
    db 0x6C       ; 0x07
    db 0xF6       ; 0x08, Format filler byte
    db 15         ; 0x09
    db 8          ; 0x0a

    db 79         ; 0x0b, max track number
    db 0          ; 0x0c


; ************************* INT 41h - hard disk 0 parameter table

int41:

int41_max_cyls   dw 0
int41_max_heads  db 0
    dw 0
    dw 0
    db 0
    db 11000000b
    db 0
    db 0
    db 0
    dw 0
int41_max_sect  db 0
    db 0


; ************************* INT 46h - hard disk 1 parameter table

int46:

int46_max_cyls   dw 0
int46_max_heads  db 0
    dw 0
    dw 0
    db 0
    db 11000000b
    db 0
    db 0
    db 0
    dw 0
int46_max_sect  db 0
    db 0



; ************************* ROM configuration table

rom_config  dw 16          ; 16 bytes following
            db MODEL       ; Model - Generic AT system
            db SUBMODEL    ; Submodel
            db BIOSREV     ; BIOS revision
            ; feature 1:
            ;   bit 2 : extended BIOS area allocated
            ;   bit 4 : Keyboard intercept sequence (INT 15H) called in keyboard interrupt (INT 09H)
            ;   bit 5 : real time clock installed
            ;   bit 6 : second 8259 installed
            db 0b01110100
            ; feature 2:
            ;   bit 6 : INT 16/AH=09h (keyboard functionality) supported
            db 0b01000000
            db 0b00000000   ; Feature 3
            db 0b00000000   ; Feature 4
            db 0b00000000   ; Feature 5
            db 0, 0, 0, 0, 0, 0, 0



; ************************* Extended BIOS Data Area (EBDA)

eda              db 1        ; size in K
eda_drive_offset dw 0000
eda_drive_seg    dw 0000
                 db 1019 dup(0)

; Internal state variables

boot_state           db 0


; Default interrupt handlers

int0:
int1:
int2:
int3:
int4:
int5:
int6:
inta:
intb:
intc:
intd:
inte:
intf:
int18:
int1b:
int1d:

iret



; ************ Function call library ************

; Hex to BCD routine. Input is AX in hex (can be 0), and adds CX in hex to it, forming a BCD output in AX.

hex_to_bcd:

  push  bx

  jcxz  h2bfin

h2bloop:

  inc  ax

  ; First process the low nibble of AL
  mov  bh, al
  and  bh, 0x0f
  cmp  bh, 0x0a
  jne  c1
  add  ax, 0x0006

  ; Then the high nibble of AL
  c1:
  mov  bh, al
  and  bh, 0xf0
  cmp  bh, 0xa0
  jne  c2
  add  ax, 0x0060

  ; Then the low nibble of AH
  c2:
  mov  bh, ah
  and  bh, 0x0f
  cmp  bh, 0x0a
  jne  c3
  add  ax, 0x0600

  c3:
  loop h2bloop
  h2bfin:
  pop  bx
  ret

puts_hex_al:
  push ax

  mov  ah, al
  shr  al, 1
  shr  al, 1
  shr  al, 1
  shr  al, 1
  and  al, 0x0f
  cmp  al, 10
  jge  puts_hex_d1_letter
  add  al, '0'
  jmp  puts_hex_d1
  puts_hex_d1_letter:
  sub  al, 10
  add  al, 'A'
puts_hex_d1:
  emu_putchar_al  ; Print first digit

  mov  al, ah
  and  al, 0x0f
  cmp  al, 10
  jge  puts_hex_d2_letter
  add  al, '0'
  jmp  puts_hex_d2
puts_hex_d2_letter:
  sub  al, 10
  add  al, 'A'
  puts_hex_d2:
  emu_putchar_al  ; Print second digit

  pop  ax
  ret

; Takes a number in AL (from 0 to 99), and outputs the value in decimal using emu_putchar_al.

puts_decimal_al:

  push  ax

  aam
  add   ax, 0x3030  ; '00'

  xchg  ah, al    ; First digit is now in AL
  cmp   al, 0x30
  je    pda_2nd    ; First digit is zero, so print only 2nd digit

  emu_putchar_al  ; Print first digit

  pda_2nd:
  xchg  ah, al    ; Second digit is now in AL

  emu_putchar_al  ; Print second digit

  pop  ax
  ret

; Keyboard adjust buffer head and tail. If either head or the tail are at the end of the buffer, reset them
; back to the start, since it is a circular buffer.

kb_adjust_buf:

  push  ax
  push  bx

  ; Check to see if the head is at the end of the buffer (or beyond). If so, bring it back
  ; to the start

  mov   ax, [es:kbbuf_end_ptr-bios_data]
  cmp   [es:kbbuf_head-bios_data], ax
  jnge  kb_adjust_tail

  mov   bx, [es:kbbuf_start_ptr-bios_data]
  mov   [es:kbbuf_head-bios_data], bx

kb_adjust_tail:

  ; Check to see if the tail is at the end of the buffer (or beyond). If so, bring it back
  ; to the start

  mov   ax, [es:kbbuf_end_ptr-bios_data]
  cmp   [es:kbbuf_tail-bios_data], ax
  jnge  kb_adjust_done

  mov   bx, [es:kbbuf_start_ptr-bios_data]
  mov   [es:kbbuf_tail-bios_data], bx

kb_adjust_done:

  pop  bx
  pop  ax
  ret


; CRTC cursor position helper
set_crtc_cursor_pos:
  push  es
  push  ax
  push  bx
  push  dx

  mov  ax, 0x40
  mov  es, ax

  mov  bh, [cs:crt_curpos_y]
  mov  bl, [cs:crt_curpos_x]

  ; Set CRTC cursor address
  mov  al, [es:vid_cols-bios_data]
  mul  bh
  add  al, bl
  adc  ah, 0
  mov  bx, ax

  mov  al, 0x0e
  mov  dx, 0x3d4
  out  dx, al

  mov  al, bh
  mov  dx, 0x3d5
  out  dx, al

  mov  al, 0x0f
  mov  dx, 0x3d4
  out  dx, al

  mov  al, bl
  mov  dx, 0x3d5
  out  dx, al

  pop  dx
  pop  bx
  pop  ax
  pop  es

  ret

clear_window:
  ; BH = attribute used to write blank lines at bottom of window
  ; CH,CL = row,column of window's upper left corner
  ; DH,DL = row,column of window's lower right corner
  ; BP = video page offset

  push  ax
  push  bx
  push  cx
  push  dx

  push  ds
  push  es
  push  di

  mov  ax, 0x40
  mov  ds, ax

  cmp  byte [vid_mode-bios_data], 0x04
  je   clear_window_gfx320
  cmp  byte [vid_mode-bios_data], 0x05
  je   clear_window_gfx320
  cmp  byte [vid_mode-bios_data], 0x06
  je   clear_window_gfx640
  cmp  byte [vid_mode-bios_data], 0x11
  je   clear_window_mode11
  cmp  byte [vid_mode-bios_data], 0x13
  je   clear_window_mode13

  push  bx

  mov  bx, 0xb800
  mov  es, bx

  pop  bx

  ; Get the byte offset of the top left corner of the window into DI

  mov  ah, 0
  mov  al, [vid_cols-bios_data]  ; ax = characters per row
  mul  ch  ; ax = top row screen offset
  add  al, cl
  adc  ah, 0  ; ax = top left screen offset
  shl  ax, 1  ; convert to byte address
  add  ax, bp  ; add display page offset

  mov  di, ax

  ; bl = number of rows in the window to clear
  mov  bl, dh
  sub  bl, ch
  inc  bl

  ; cx = number of words to write
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax

  ; ax = clear screen value
  mov  al, 0
  mov  ah, bh

  cld

clear_window_1_row:
  cmp   bl, 0
  je    clear_window_done

  push  cx
  push  di

  rep   stosw

  pop   di
  add   di, [vid_cols-bios_data]
  add   di, [vid_cols-bios_data]

  pop   cx

  dec   bl
  jmp   clear_window_1_row

clear_window_gfx320:
  ; Get the address offset in video ram for the top left character into DI
  mov  al, 80 ; bytes per row
  mul  byte ch
  shl  ax, 1
  shl  ax, 1
  add  al, cl
  adc  ah, 0
  add  al, cl
  adc  ah, 0
  mov  di, ax

  ; bl = number of rows in the window to clear
  mov  bl, dh
  sub  bl, ch
  shl  bl, 1
  shl  bl, 1

  ; cx = number of words to clear
  push ax
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax
  pop  ax

  cld

clear_window_gfx320_loop:
  push cx
  push di
  mov  ax, 0xb800
  mov  es, ax
  mov  ax, 0
  rep  stosw
  pop  di
  pop  cx

  push cx
  push di
  mov  ax, 0xba00
  mov  es, ax
  mov  ax, 0
  rep  stosw
  pop  di
  pop  cx

  add  di, 80

  dec  bl
  jnz  clear_window_gfx320_loop

  jmp  clear_window_done

clear_window_gfx640:
  ; Get the address offset in video ram for the top left character into DI
  mov  al, 80 ; bytes per row
  mul  byte ch
  shl  ax, 1
  shl  ax, 1
  add  al, cl
  adc  ah, 0
  mov  di, ax

  ; bl = number of rows in the window to clear
  mov  bl, dh
  sub  bl, ch
  shl  bl, 1
  shl  bl, 1

  ; cx = number of bytes to clear
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax

  cld

clear_window_gfx640_loop:
  push cx
  push di
  mov  ax, 0xb800
  mov  es, ax
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  push cx
  push di
  mov  ax, 0xba00
  mov  es, ax
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  add  di, 80

  dec  bl
  jnz  clear_window_gfx640_loop

  jmp  clear_window_done

clear_window_mode11:
  ; Get the address offset in video ram for the top left character into DI
  mov  al, 80 ; bytes per row
  mul  byte ch
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  al, cl
  adc  ah, 0
  mov  di, ax

  ; bx = number of rows in the window to clear
  mov  bl, dh
  sub  bl, ch
  mov  bh, 0
  shl  bx, 1
  shl  bx, 1
  shl  bx, 1
  shl  bx, 1

  ; cx = number of bytes to clear
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax

  mov  ax, 0xa000
  mov  ds, ax
  mov  es, ax

  cld

clear_window_mode11_loop:
  push cx
  push di
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  add  di, 80

  dec  bx
  jnz  clear_window_mode11_loop

  jmp  clear_window_done

clear_window_mode13:
  ; Get the address offset in video ram for the top left character into DI
  mov  al, 40 ; characters per row
  mul  byte ch
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  al, cl
  adc  ah, 0
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  mov  di, ax

  add  ax, 2560
  mov  si, ax

  ; bx = number of rows in the window to clear
  mov  bl, dh
  sub  bl, ch
  mov  bh, 0
  shl  bx, 1
  shl  bx, 1
  shl  bx, 1

  ; cx = number of bytes to clear
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  mov  cx, ax

  mov  ax, 0xa000
  mov  ds, ax
  mov  es, ax

  cld

clear_window_mode13_loop:
  push cx
  push di
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  add  di, 320

  dec  bl
  jnz  clear_window_mode13_loop

  jmp  clear_window_done

clear_window_done:
  pop  di
  pop  es
  pop  ds

  pop  dx
  pop  cx
  pop  bx
  pop  ax
  ret

scroll_up_window:
  ; AL = number of lines by which to scroll up
  ; BH = attribute used to write blank lines at bottom of window
  ; CH,CL = row,column of window's upper left corner
  ; DH,DL = row,column of window's lower right corner
  ; BP = video page offset

  push  ax
  push  bx
  push  cx
  push  dx

  push  ds
  push  es
  push  di
  push  si

  push  ax
  mov   ax, 0x40
  mov   ds, ax
  pop   ax

  cmp  byte [vid_mode-bios_data], 0x04
  je   scroll_up_gfx320
  cmp  byte [vid_mode-bios_data], 0x05
  je   scroll_up_gfx320
  cmp  byte [vid_mode-bios_data], 0x06
  je   scroll_up_gfx640
  cmp  byte [vid_mode-bios_data], 0x11
  je   scroll_up_mode11
  cmp  byte [vid_mode-bios_data], 0x13
  je   scroll_up_mode13

  ; Get screen RAM address into es and ds
  push  ax

  mov  ax, 0xb800
  mov  es, ax
  mov  ds, ax

  pop  ax

  ; Get the first and second window row offsets into DI and SI
  push  ax
  push  ds

  mov  ax, 0x40
  mov  ds, ax

  mov  ah, 0
  mov  al, [vid_cols-bios_data]  ; ax = characters per row
  mul  ch  ; ax = window top row screen word offset
  add  al, cl
  adc  ah, 0  ; ax = window top left screen word offset
  shl  ax, 1  ; convert to byte address
  add  ax, bp  ; Add video page offset

  mov  di, ax
  add  al, [vid_cols-bios_data]
  adc  ah, 0
  add  al, [vid_cols-bios_data]
  adc  ah, 0
  mov  si, ax

  pop  ds
  pop  ax

  ; bl = number of rows in the window to scroll
  mov bl, dh
  sub bl, ch

  ; cx = number of words to move
  push ax
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax
  pop  ax

  cld

scroll_up_1_row:
  cmp   al, 0
  je    scroll_up_done

  push  di
  push  si
  push  bx

scroll_up_loop:
  cmp   bl, 0
  je    scroll_up_1_row_done

  push  cx
  push  di
  push  si

  rep   movsw

  pop   si
  pop   di
  pop   cx

  push  ax
  push  ds

  mov   ax, 0x40
  mov   ds, ax

  mov   ax, si
  add   al, [vid_cols-bios_data]
  adc   ah, 0
  add   al, [vid_cols-bios_data]
  adc   ah, 0
  mov   si, ax

  mov   ax, di
  add   al, [vid_cols-bios_data]
  adc   ah, 0
  add   al, [vid_cols-bios_data]
  adc   ah, 0
  mov   di, ax

  pop   ds
  pop   ax

  dec   bl
  jmp   scroll_up_loop

scroll_up_1_row_done:

  ; fill the last row with the bh attribute, null character
  push ax
  push cx
  mov  al, 0
  mov  ah, bh
  rep  stosw
  pop  cx
  pop  ax

  pop  bx
  pop  si
  pop  di

  dec  al
  jmp  scroll_up_1_row


scroll_up_gfx320:

  ; Get the first and second window row offsets into DI and SI
  push ax

  ; Get the address offset in video ram for the top left character into DI
  mov  al, 80 ; bytes per row
  mul  byte ch
  shl  ax, 1
  shl  ax, 1
  add  al, cl
  adc  ah, 0
  add  al, cl
  adc  ah, 0
  mov  di, ax

  add  ax, 320
  mov  si, ax

  pop  ax

  ; bl = number of rows in the window to scroll
  mov  bl, dh
  sub  bl, ch
  shl  bl, 1
  shl  bl, 1

  ; cx = number of words to move
  push ax
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax
  pop  ax

  cld

scroll_up_gfx320_1_row:
  cmp   al, 0
  je    scroll_up_done

  push  di
  push  si
  push  bx

scroll_up_gfx320_loop:
  cmp   bl, 0
  je    scroll_up_gfx320_1_row_done

  push  ax

  push  cx
  push  di
  push  si

  mov   ax, 0xb800
  mov   ds, ax
  mov   es, ax
  rep   movsw

  pop   si
  pop   di
  pop   cx

  push  cx
  push  di
  push  si

  mov   ax, 0xba00
  mov   ds, ax
  mov   es, ax
  rep   movsw

  pop   si
  pop   di
  pop   cx

  add   si, 80
  add   di, 80

  pop   ax

  dec   bl
  jmp   scroll_up_gfx320_loop

scroll_up_gfx320_1_row_done:

  ; fill the last row with zeros
  push  ax
  push  bx
  mov   bl, 4

scroll_up_gfx320_last_row_loop:
  push cx
  push di
  mov  ax, 0xb800
  mov  es, ax
  mov  ax, 0
  rep  stosw
  pop  di
  pop  cx

  push cx
  push di
  mov  ax, 0xba00
  mov  es, ax
  mov  ax, 0
  rep  stosw
  pop  di
  pop  cx

  add  di, 80

  dec  bl
  jnz  scroll_up_gfx320_last_row_loop

  pop  bx
  pop  ax

  pop  bx
  pop  si
  pop  di

  dec  al
  jmp  scroll_up_gfx320_1_row


scroll_up_gfx640:

  ; Get the first and second window row offsets into DI and SI
  push  ax

  ; Get the address offset in video ram for the top left character into DI
  mov  al, 80 ; bytes per row
  mul  byte ch
  shl  ax, 1
  shl  ax, 1
  add  al, cl
  adc  ah, 0
  mov  di, ax

  add  ax, 320
  mov  si, ax

  pop  ax

  ; bl = number of rows in the window to scroll
  mov  bl, dh
  sub  bl, ch
  shl  bl, 1
  shl  bl, 1

  ; cx = number of bytes to move
  push ax
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax
  pop  ax

  cld

scroll_up_gfx640_1_row:
  cmp  al, 0
  je   scroll_up_done

  push di
  push si
  push bx

scroll_up_gfx640_loop:
  cmp  bl, 0
  je   scroll_up_gfx640_1_row_done

  push ax

  push cx
  push di
  push si

  mov  ax, 0xb800
  mov  ds, ax
  mov  es, ax
  rep  movsb

  pop  si
  pop  di
  pop  cx

  push cx
  push di
  push si

  mov  ax, 0xba00
  mov  ds, ax
  mov  es, ax
  rep  movsb

  pop  si
  pop  di
  pop  cx

  add  si, 80
  add  di, 80

  pop  ax

  dec  bl
  jmp  scroll_up_gfx640_loop

scroll_up_gfx640_1_row_done:

  ; fill the last row with zeros
  push ax
  push bx
  mov  bl, 4

scroll_up_gfx640_last_row_loop:
  push cx
  push di
  mov  ax, 0xb800
  mov  es, ax
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  push cx
  push di
  mov  ax, 0xba00
  mov  es, ax
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  add  di, 80

  dec  bl
  jnz  scroll_up_gfx640_last_row_loop

  pop  bx
  pop  ax

  pop  bx
  pop  si
  pop  di

  dec  al
  jmp  scroll_up_gfx640_1_row

scroll_up_mode11:

  ; Get the first and second window row offsets into DI and SI
  push ax

  ; Get the address offset in video ram for the top left character into DI
  mov  al, 80 ; bytes per row
  mul  byte ch
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  al, cl
  adc  ah, 0
  mov  di, ax

  add  ax, 1280
  mov  si, ax

  pop  ax

  ; bx = number of rows in the window to scroll
  mov  bl, dh
  sub  bl, ch
  mov  bh, 0
  shl  bx, 1
  shl  bx, 1
  shl  bx, 1
  shl  bx, 1

  ; cx = number of bytes to move
  push ax

  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax

  mov  ax, 0xa000
  mov  ds, ax
  mov  es, ax

  pop  ax

  cld

scroll_up_mode11_1_row:
  cmp  al, 0
  je   scroll_up_done

  push di
  push si
  push bx

scroll_up_mode11_loop:
  cmp  bx, 0
  je   scroll_up_mode11_1_row_done

  push cx
  push di
  push si

  rep  movsb

  pop  si
  pop  di
  pop  cx

  add  si, 80
  add  di, 80

  dec  bx
  jmp  scroll_up_mode11_loop

scroll_up_mode11_1_row_done:

  ; fill the last row with zeros
  push ax
  push bx
  mov  bl, 16

scroll_up_mode11_last_row_loop:
  push cx
  push di
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  add  di, 80

  dec  bl
  jnz  scroll_up_mode11_last_row_loop

  pop  bx
  pop  ax

  pop  bx
  pop  si
  pop  di

  dec  al
  jmp  scroll_up_mode11_1_row


scroll_up_mode13:

  ; Get the first and second window row offsets into DI and SI
  push ax

  ; Get the address offset in video ram for the top left character into DI
  mov  al, 40 ; characters per row
  mul  byte ch
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  al, cl
  adc  ah, 0
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  mov  di, ax

  add  ax, 2560
  mov  si, ax

  pop  ax

  ; bx = number of rows in the window to scroll
  mov  bl, dh
  sub  bl, ch
  mov  bh, 0
  shl  bx, 1
  shl  bx, 1
  shl  bx, 1

  ; cx = number of bytes to move
  push ax

  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  mov  cx, ax

  mov  ax, 0xa000
  mov  ds, ax
  mov  es, ax

  pop  ax

  cld

scroll_up_mode13_1_row:
  cmp  al, 0
  je   scroll_up_done

  push di
  push si
  push bx

scroll_up_mode13_loop:
  cmp  bx, 0
  je   scroll_up_mode13_1_row_done

  push cx
  push di
  push si

  rep  movsb

  pop  si
  pop  di
  pop  cx

  add  si, 320
  add  di, 320

  dec  bx
  jmp  scroll_up_mode13_loop

scroll_up_mode13_1_row_done:

  ; fill the last row with zeros
  push ax
  push bx
  mov  bl, 8

scroll_up_mode13_last_row_loop:
  push cx
  push di
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  add  di, 320

  dec  bl
  jnz  scroll_up_mode13_last_row_loop

  pop  bx
  pop  ax

  pop  bx
  pop  si
  pop  di

  dec  al
  jmp  scroll_up_mode13_1_row

scroll_up_done:
  pop  si
  pop  di
  pop  es
  pop  ds

  pop  dx
  pop  cx
  pop  bx
  pop  ax
  ret

scroll_down_window:
  ; AL = number of lines by which to scroll down (00h = clear entire window)
  ; BH = attribute used to write blank lines at top of window
  ; CH,CL = row,column of window's upper left corner
  ; DH,DL = row,column of window's lower right corner
  ; BP = video page offset

  push  ax
  push  bx
  push  cx
  push  dx

  push  ds
  push  es
  push  di
  push  si

  push  ax
  mov   ax, 0x40
  mov   ds, ax
  pop   ax

  cmp   byte [vid_mode-bios_data], 0x04
  je    scroll_dn_gfx320
  cmp   byte [vid_mode-bios_data], 0x05
  je    scroll_dn_gfx320
  cmp   byte [vid_mode-bios_data], 0x06
  je    scroll_dn_gfx640
  cmp   byte [vid_mode-bios_data], 0x11
  je    scroll_dn_mode11
  cmp   byte [vid_mode-bios_data], 0x13
  je    scroll_dn_mode13

  ; Get screen RAM address into es and ds
  push  ax

  mov   ax, 0xb800
  mov   es, ax
  mov   ds, ax

  pop   ax

  ; Get the last and second last window row offsets into DI and SI
  push  ax
  push  ds

  mov  ax, 0x40
  mov  ds, ax

  mov  ah, 0
  mov  al, [vid_cols-bios_data]  ; ax = bytes per row
  mul  dh  ; ax = window bottom row screen word offset
  add  al, dl
  adc  ah, 0  ; ax = window bottom right screen word offset
  shl  ax, 1  ; convert to byte address
  add  ax, bp  ; add video page offset

  mov  di, ax
  sub  al, [vid_cols-bios_data]
  sbb  ah, 0
  sub  al, [vid_cols-bios_data]
  sbb  ah, 0
  mov  si, ax

  pop  ds
  pop  ax

  ; bl = number of rows in the window to scroll
  mov  bl, dh
  sub  bl, ch

  ; cx = number of words to move
  push  ax
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax
  pop  ax

  std

  scroll_down_1_row:
  cmp  al, 0
  je   scroll_down_done

  push di
  push si
  push bx

scroll_down_loop:
  cmp  bl, 0
  je   scroll_down_1_row_done

  push cx
  push di
  push si

  rep  movsw

  pop  si
  pop  di
  pop  cx

  push ax
  push ds

  mov  ax, 0x40
  mov  ds, ax

  mov  ax, si
  sub  al, [vid_cols-bios_data]
  sbb  ah, 0
  sub  al, [vid_cols-bios_data]
  sbb  ah, 0
  mov  si, ax

  mov  ax, di
  sub  al, [vid_cols-bios_data]
  sbb  ah, 0
  sub  al, [vid_cols-bios_data]
  sbb  ah, 0
  mov  di, ax

  pop  ds
  pop  ax

  dec  bl
  jmp  scroll_down_loop

scroll_down_1_row_done:

  ; fill the last row with the bh attribute, null character
  push ax
  push cx
  mov  al, 0
  mov  ah, bh
  rep  stosw
  pop  cx
  pop  ax

  pop  bx
  pop  si
  pop  di

  dec  al
  jmp  scroll_down_1_row


scroll_dn_gfx320:

  ; Get the last and second last window row offsets into DI and SI
  push ax

  ; Get the address offset in video ram for the top left character into DI
  mov  al, 80 ; bytes per row
  mul  byte dh
  shl  ax, 1
  shl  ax, 1
  add  al, dl
  adc  ah, 0
  add  al, dl
  adc  ah, 0
  add  ax, 240
  mov  di, ax

  sub  ax, 320
  mov  si, ax

  pop  ax

  ; bl = number of rows in the window to scroll
  mov  bl, dh
  sub  bl, ch
  shl  bl, 1
  shl  bl, 1

  ; cx = number of words to move
  push ax
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax
  pop  ax

  std

scroll_dn_gfx320_1_row:
  cmp  al, 0
  je   scroll_down_done

  push di
  push si
  push bx

scroll_dn_gfx320_loop:
  cmp   bl, 0
  je    scroll_dn_gfx320_1_row_done

  push  ax

  push  cx
  push  di
  push  si

  mov   ax, 0xb800
  mov   ds, ax
  mov   es, ax
  rep   movsw

  pop   si
  pop   di
  pop   cx

  push  cx
  push  di
  push  si

  mov   ax, 0xba00
  mov   ds, ax
  mov   es, ax
  rep   movsw

  pop   si
  pop   di
  pop   cx

  sub   si, 80
  sub   di, 80

  pop   ax

  dec   bl
  jmp   scroll_dn_gfx320_loop

scroll_dn_gfx320_1_row_done:

  ; fill the last row with zeros
  push ax
  push bx
  mov  bl, 4

scroll_dn_gfx320_last_row_loop:
  push cx
  push di
  mov  ax, 0xb800
  mov  es, ax
  mov  ax, 0
  rep  stosw
  pop  di
  pop  cx

  push cx
  push di
  mov  ax, 0xba00
  mov  es, ax
  mov  ax, 0
  rep  stosw
  pop  di
  pop  cx

  sub  di, 80

  dec  bl
  jnz  scroll_dn_gfx320_last_row_loop

  pop  bx
  pop  ax

  pop  bx
  pop  si
  pop  di

  dec  al
  jmp  scroll_dn_gfx320_1_row


scroll_dn_gfx640:

  ; Get the first and second window row offsets into DI and SI
  push ax

  ; Get the address offset in video ram for the bottom right character into DI
  mov  al, 80 ; bytes per row
  mul  byte dh
  shl  ax, 1
  shl  ax, 1
  add  al, dl
  adc  ah, 0
  add  ax, 240
  mov  di, ax

  sub  ax, 320
  mov  si, ax

  pop  ax

  ; bl = number of rows in the window to scroll
  mov  bl, dh
  sub  bl, ch
  shl  bl, 1
  shl  bl, 1

  ; cx = number of bytes to move
  push ax
  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax
  pop  ax

  std

scroll_dn_gfx640_1_row:
  cmp  al, 0
  je   scroll_down_done

  push di
  push si
  push bx

scroll_dn_gfx640_loop:
  cmp  bl, 0
  je   scroll_dn_gfx640_1_row_done

  push ax

  push cx
  push di
  push si

  mov  ax, 0xb800
  mov  ds, ax
  mov  es, ax
  rep  movsb

  pop  si
  pop  di
  pop  cx

  push cx
  push di
  push si

  mov  ax, 0xba00
  mov  ds, ax
  mov  es, ax
  rep  movsb

  pop  si
  pop  di
  pop  cx

  sub  si, 80
  sub  di, 80

  pop  ax

  dec  bl
  jmp  scroll_dn_gfx640_loop

scroll_dn_gfx640_1_row_done:

  ; fill the last row with zeros
  push ax
  push bx
  mov  bl, 4

scroll_dn_gfx640_last_row_loop:
  push cx
  push di
  mov  ax, 0xb800
  mov  es, ax
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  push cx
  push di
  mov  ax, 0xba00
  mov  es, ax
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  sub  di, 80

  dec  bl
  jnz  scroll_dn_gfx640_last_row_loop

  pop  bx
  pop  ax

  pop  bx
  pop  si
  pop  di

  dec  al
  jmp  scroll_dn_gfx640_1_row

scroll_dn_mode11:

  ; Get the first and second window row offsets into DI and SI
  push ax

  ; Get the address offset in video ram for the bottom right character into DI
  mov  al, 80 ; bytes per row
  mul  byte dh
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  al, dl
  adc  ah, 0
  add  ax, 1200
  mov  di, ax

  sub  ax, 1280
  mov  si, ax

  pop  ax

  ; bl = number of rows in the window to scroll
  mov  bl, dh
  sub  bl, ch
  mov  bh, 0
  shl  bx, 1
  shl  bx, 1
  shl  bx, 1
  shl  bx, 1

  ; cx = number of bytes to move
  push ax

  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  mov  cx, ax

  mov  ax, 0xa000
  mov  ds, ax
  mov  es, ax

  pop  ax

  std

scroll_dn_mode11_1_row:
  cmp  al, 0
  je   scroll_down_done

  push di
  push si
  push bx

scroll_dn_mode11_loop:
  cmp  bx, 0
  je   scroll_dn_mode11_1_row_done

  push cx
  push di
  push si

  rep  movsb

  pop  si
  pop  di
  pop  cx

  sub  si, 80
  sub  di, 80

  dec  bx
  jmp  scroll_dn_mode11_loop

scroll_dn_mode11_1_row_done:

  ; fill the last row with zeros
  push ax
  push bx
  mov  bl, 16

scroll_dn_mode11_last_row_loop:
  push cx
  push di
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  sub  di, 80

  dec  bl
  jnz  scroll_dn_mode11_last_row_loop

  pop  bx
  pop  ax

  pop  bx
  pop  si
  pop  di

  dec  al
  jmp  scroll_dn_mode11_1_row

scroll_dn_mode13:

  ; Get the first and second window row offsets into DI and SI
  push ax

  ; Get the address offset in video ram for the top left character into DI
  mov  al, 40 ; characters per row
  mul  byte dh
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  al, dl
  adc  ah, 0
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  ax, 2240
  mov  di, ax

  sub  ax, 2560
  mov  si, ax

  pop  ax

  ; bx = number of rows in the window to scroll
  mov  bl, dh
  sub  bl, ch
  mov  bh, 0
  shl  bx, 1
  shl  bx, 1
  shl  bx, 1

  ; cx = number of bytes to move
  push ax

  mov  ax, 0
  mov  al, dl
  sub  al, cl
  inc  al
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  mov  cx, ax

  mov  ax, 0xa000
  mov  ds, ax
  mov  es, ax

  pop  ax

  std

scroll_dn_mode13_1_row:
  cmp  al, 0
  je   scroll_down_done

  push di
  push si
  push bx

scroll_dn_mode13_loop:
  cmp  bx, 0
  je   scroll_dn_mode13_1_row_done

  push cx
  push di
  push si

  rep  movsb

  pop  si
  pop  di
  pop  cx

  sub  si, 320
  sub  di, 320

  dec  bx
  jmp  scroll_dn_mode13_loop

scroll_dn_mode13_1_row_done:

  ; fill the last row with zeros
  push ax
  push bx
  mov  bl, 8

scroll_dn_mode13_last_row_loop:
  push cx
  push di
  mov  ax, 0
  rep  stosb
  pop  di
  pop  cx

  sub  di, 320

  dec  bl
  jnz  scroll_dn_mode13_last_row_loop

  pop  bx
  pop  ax

  pop  bx
  pop  si
  pop  di

  dec  al
  jmp  scroll_dn_mode13_1_row


scroll_down_done:
  pop  si
  pop  di
  pop  es
  pop  ds

  pop  dx
  pop  cx
  pop  bx
  pop  ax
  ret

clear_screen:
; Clear video memory with attribute in BH

  push  ax
  push  bx
  push  cx
  push  ds
  push  es
  push  di
  push  si

  mov  ax, 0x40
  mov  ds, ax

  mov  al, [disp_page-bios_data]
  mov  ah, 0
  shl  ax, 1
  mov  si, ax

  mov  byte [si+curpos_x-bios_data], 0
  mov  byte [cs:crt_curpos_x], 0
  mov  byte [si+curpos_y-bios_data], 0
  mov  byte [cs:crt_curpos_y], 0

  cmp  byte [vid_mode-bios_data], 4
  je   clear_gfx
  cmp  byte [vid_mode-bios_data], 5
  je   clear_gfx
  cmp  byte [vid_mode-bios_data], 6
  je   clear_gfx
  cmp  byte [vid_mode-bios_data], 0x11
  je   clear_gfx2
  cmp  byte [vid_mode-bios_data], 0x13
  je   clear_gfx2

  cld
  mov  al, [vid_page_size+1-bios_data]
  mul  byte [disp_page-bios_data]
  mov  ah, al
  mov  al, 0
  shr  ax, 1
  shr  ax, 1
  shr  ax, 1
  shr  ax, 1
  add  ax, 0xb800
  mov  es, ax
  mov  di, 0
  mov  al, 0
  mov  ah, bh
  mov  cx, [vid_page_size-bios_data]
  rep  stosw
  jmp  clear_done

  clear_gfx:
  cld
  mov  ax, 0xb800
  mov  es, ax
  mov  di, 0
  mov  ax, 0
  mov  cx, 4000
  rep  stosw

  mov  ax, 0xba00
  mov  es, ax
  mov  di, 0
  mov  ax, 0
  mov  cx, 4000
  rep  stosw
  jmp  clear_done

  clear_gfx2:
  cld
  mov  ax, 0xa000
  mov  es, ax
  mov  di, 0
  mov  ax, 0
  mov  cx, 0x8000
  rep  stosw

  mov  ax, 0xa800
  mov  es, ax
  mov  di, 0
  mov  ax, 0
  mov  cx, 0x8000
  rep  stosw
  jmp  clear_done

clear_done:
  call  set_crtc_cursor_pos

  pop  si
  pop  di
  pop  es
  pop  ds
  pop  cx
  pop  bx
  pop  ax

  ret

put_cga320_char:
  ; Character is in AL
  ; Colour is in AH

  push ax
  push bx
  push cx
  push ds
  push es
  push di
  push bp

  ; when bit 7 of AH is set the character is XORed with the contents of the current display
  ; remove bit 7 from AH and set CX = 1 if dest will be xored
  mov  ch, 0x00
  mov  cl, ah
  and  cl, 0x80
  and  ah, 0x7f
  push cx         ; save for later use

  ; Get the colour mask into BH
  cmp  ah, 1
  jne  put_cga320_char_c2
  mov  bh, 0x55
  jmp  put_cga320_char_cdone
put_cga320_char_c2:
  cmp  ah, 2
  jne  put_cga320_char_c3
  mov  bh, 0xAA
  jmp  put_cga320_char_cdone
put_cga320_char_c3:
  mov  bh, 0xFF

put_cga320_char_cdone:
  ; Get glyph character top offset into bp and character segment into cx
  test al, 0x80
  jne  put_cga320_char_high

  ; Characters 0 .. 127 are always in ROM
  mov  ah, 0
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  ax, cga_glyphs
  mov  bp, ax

  mov  cx, cs

  jmp  put_cga320_char_vidoffset

put_cga320_char_high:
  ; Characters 128 .. 255 get their address from interrupt vector 1F
  and  al, 0x7F
  mov  ah, 0
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  mov  bp, ax

  mov  ax, 0
  mov  ds, ax
  mov  ax, [ds:0x7c]
  add  bp, ax

  mov  cx, [ds:0x7e]

put_cga320_char_vidoffset:
  mov  ax, 0x40
  mov  ds, ax

  ; Get the address offset in video ram for the top of the character into DI
  mov  al, 80 ; bytes per row
  mul  byte [ds:curpos_y-bios_data]
  shl  ax, 1
  shl  ax, 1
  add  al, [ds:curpos_x-bios_data]
  adc  ah, 0
  add  al, [ds:curpos_x-bios_data]
  adc  ah, 0
  mov  di, ax

  ; get segment for character data into ds
  mov  ds, cx

  ; get video RAM address for even lines into es
  mov  ax, 0xb800
  mov  es, ax

  ; cx is 1 if dest needs to be xored instead of replaced
  pop cx

  push  di

  jcxz put_cga320_char_copy1

  ; XOR instead of copy
  mov   bl, byte [ds:bp]
  call  put_cga320_char_double
  xor   [es:di], ax
  add   di, 80
  mov   bl, byte [ds:bp+2]
  call  put_cga320_char_double
  xor   [es:di], ax
  add   di, 80
  mov   bl, byte [ds:bp+4]
  call  put_cga320_char_double
  xor   [es:di], ax
  add   di, 80
  mov   bl, byte [ds:bp+6]
  call  put_cga320_char_double
  xor   [es:di], ax
  jmp   put_cga320_char_oddlines

  ; copy instead of XOR
put_cga320_char_copy1:
  cld
  mov   bl, byte [ds:bp]
  call  put_cga320_char_double
  stosw
  add   di, 78
  mov   bl, byte [ds:bp+2]
  call  put_cga320_char_double
  stosw
  add   di, 78
  mov   bl, byte [ds:bp+4]
  call  put_cga320_char_double
  stosw
  add   di, 78
  mov   bl, byte [ds:bp+6]
  call  put_cga320_char_double
  stosw

put_cga320_char_oddlines:

  ; get video RAM address for odd lines into es
  mov  ax, 0xba00
  mov  es, ax

  pop  di

  jcxz put_cga320_char_copy2

  ; XOR instead of copy
  mov  bl, byte [ds:bp+1]
  call put_cga320_char_double
  xor  [es:di], ax
  add  di, 80
  mov  bl, byte [ds:bp+3]
  call put_cga320_char_double
  xor  [es:di], ax
  add  di, 80
  mov  bl, byte [ds:bp+5]
  call put_cga320_char_double
  xor  [es:di], ax
  add  di, 80
  mov  bl, byte [ds:bp+7]
  call put_cga320_char_double
  xor  [es:di], ax
  jmp  put_cga320_char_done

  ; copy instead of XOR
put_cga320_char_copy2:
  mov  bl, byte [ds:bp+1]
  call put_cga320_char_double
  stosw
  add  di, 78
  mov  bl, byte [ds:bp+3]
  call put_cga320_char_double
  stosw
  add  di, 78
  mov  bl, byte [ds:bp+5]
  call put_cga320_char_double
  stosw
  add  di, 78
  mov  bl, byte [ds:bp+7]
  call put_cga320_char_double
  stosw

put_cga320_char_done:
  pop  bp
  pop  di
  pop  es
  pop  ds
  pop  cx
  pop  bx
  pop  ax
  ret

; Translate character glyph into CGA 320 format
put_cga320_char_double:
  ; BL = character bit pattern
  ; BH = colour mask
  ; AX is set to double width character bit pattern

  mov  ax, 0
  test bl, 0x80
  je   put_chachar_bit6
  or   al, 0xc0
put_chachar_bit6:
  test bl, 0x40
  je   put_chachar_bit5
  or   al, 0x30
put_chachar_bit5:
  test bl, 0x20
  je   put_chachar_bit4
  or   al, 0x0c
put_chachar_bit4:
  test bl, 0x10
  je   put_chachar_bit3
  or   al, 0x03
put_chachar_bit3:
  test bl, 0x08
  je   put_chachar_bit2
  or   ah, 0xc0
put_chachar_bit2:
  test bl, 0x04
  je   put_chachar_bit1
  or   ah, 0x30
put_chachar_bit1:
  test bl, 0x02
  je   put_chachar_bit0
  or   ah, 0x0c
put_chachar_bit0:
  test bl, 0x01
  je   put_chachar_done
  or   ah, 0x03
put_chachar_done:
  and  al, bh
  and  ah, bh
  ret

put_cga640_char:
  ; Character is in AL
  ; Colour is in AH

  push  ax
  push  bx
  push  cx
  push  ds
  push  es
  push  di
  push  bp

  ; when bit 7 of AH is set the character is XORed with the contents of the current display
  mov   ch, 0x00
  mov   cl, ah
  and   cl, 0x80
  push  cx         ; save for later use

  ; Get glyph character top offset into bp and character segment into cx
  test  al, 0x80
  jne   put_cga640_char_high

  ; Characters 0 .. 127 are always in ROM
  mov   ah, 0
  shl   ax, 1
  shl   ax, 1
  shl   ax, 1
  add   ax, cga_glyphs
  mov   bp, ax

  mov   cx, cs

  jmp   put_cga640_char_vidoffset

put_cga640_char_high:
  ; Characters 128 .. 255 get their address from interrupt vector 1F
  and  al, 0x7f
  mov  ah, 0
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  mov  bp, ax

  mov  ax, 0
  mov  ds, ax
  mov  ax, [ds:0x7c]
  add  bp, ax

  mov  cx, [ds:0x7e]

put_cga640_char_vidoffset:
  mov  ax, 0x40
  mov  ds, ax

  ; Get the address offset in video ram for the top of the character into DI
  mov  al, 80 ; bytes per row
  mul  byte [ds:curpos_y-bios_data]
  shl  ax, 1
  shl  ax, 1
  add  al, [ds:curpos_x-bios_data]
  adc  ah, 0
  mov  di, ax

  ; get segment for character data into ds
  mov  ds, cx

  ; get video RAM address for even lines into ds
  mov  ax, 0xb800
  mov  es, ax

  ; cx is 1 if dest needs to be xored instead of replaced
  pop  cx

  push di

  jcxz put_cga640_char_copy1

  ; XOR instead of copy
  mov  al, byte [ds:bp]
  xor  byte [es:di], al
  add  di, 80
  mov  al, byte [ds:bp+2]
  xor  byte [es:di], al
  add  di, 80
  mov  al, byte [ds:bp+4]
  xor  byte [es:di], al
  add  di, 80
  mov  al, byte [ds:bp+6]
  xor  byte [es:di], al
  jmp  put_cga640_char_oddlines

  ; copy instead of XOR
put_cga640_char_copy1:
  cld
  mov  al, byte [ds:bp]
  stosb
  add  di, 79
  mov  al, byte [ds:bp+2]
  stosb
  add  di, 79
  mov  al, byte [ds:bp+4]
  stosb
  add  di, 79
  mov  al, byte [ds:bp+6]
  stosb

put_cga640_char_oddlines:

  ; get video RAM address for odd lines into ds
  mov ax, 0xba00
  mov es, ax

  pop  di

  jcxz put_cga640_char_copy2

  ; XOR instead of copy
  mov  al, byte [ds:bp+1]
  xor  byte [es:di], al
  add  di, 80
  mov  al, byte [ds:bp+3]
  xor  byte [es:di], al
  add  di, 80
  mov  al, byte [ds:bp+5]
  xor  byte [es:di], al
  add  di, 80
  mov  al, byte [ds:bp+7]
  xor  byte [es:di], al
  jmp  put_cga640_char_done

  ; copy instead of XOR
put_cga640_char_copy2:
  mov  al, byte [ds:bp+1]
  stosb
  add  di, 79
  mov  al, byte [ds:bp+3]
  stosb
  add  di, 79
  mov  al, byte [ds:bp+5]
  stosb
  add  di, 79
  mov  al, byte [ds:bp+7]
  stosb

put_cga640_char_done:
  pop  bp
  pop  di
  pop  es
  pop  ds
  pop  cx
  pop  bx
  pop  ax
  ret

put_mode11_char:
  ; Character is in AL
  push ax
  push bx
  push cx
  push ds
  push es
  push di
  push bp

  ; Get glyph character top offset into bp and character segment into cx
  test al, 0x80
  jne  put_mode11_char_high

  ; Characters 0 .. 127 are always in ROM
  mov  ah, 0
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  ax, cga_glyphs
  mov  bp, ax

  mov  cx, cs

  jmp  put_mode11_char_vidoffset

put_mode11_char_high:
  ; Characters 128 .. 255 get their address from interrupt vector 1F
  and  al, 0x7f
  mov  ah, 0
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  mov  bp, ax

  mov  ax, 0
  mov  ds, ax
  mov  ax, [ds:0x7c]
  add  bp, ax

  mov  cx, [ds:0x7e]

put_mode11_char_vidoffset:
  mov  ax, 0x40
  mov  ds, ax

  ; Get the address offset in video ram for the top of the character into DI
  mov  al, 80 ; bytes per row
  mul  byte [ds:curpos_y-bios_data]
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  al, [ds:curpos_x-bios_data]
  adc  ah, 0
  mov  di, ax

  ; get segment for character data into ds
  mov  ds, cx

  ; get video RAM address for even lines into ds
  mov  ax, 0xa000
  mov  es, ax

  cld

  mov  al, byte [ds:bp]
  stosb
  add  di, 79
  stosb
  add  di, 79

  mov  al, byte [ds:bp+1]
  stosb
  add  di, 79
  stosb
  add  di, 79

  mov  al, byte [ds:bp+2]
  stosb
  add  di, 79
  stosb
  add  di, 79

  mov  al, byte [ds:bp+3]
  stosb
  add  di, 79
  stosb
  add  di, 79

  mov  al, byte [ds:bp+4]
  stosb
  add  di, 79
  stosb
  add  di, 79

  mov  al, byte [ds:bp+5]
  stosb
  add  di, 79
  stosb
  add  di, 79

  mov  al, byte [ds:bp+6]
  stosb
  add  di, 79
  stosb
  add  di, 79

  mov  al, byte [ds:bp+7]
  stosb
  add  di, 79
  stosb
  add  di, 79

  pop  bp
  pop  di
  pop  es
  pop  ds
  pop  cx
  pop  bx
  pop  ax
  ret

put_mode13_char:
  ; Character is in AL
  ; Colour is in AH
  push ax
  push bx
  push cx
  push dx
  push ds
  push es
  push di
  push bp

  mov  bh, ah
  push bx

  ; Get glyph character top offset into BP and character segment into CX
  test al, 0x80
  jne  put_mode13_char_high

  ; Characters 0 .. 127 are always in ROM
  mov  ah, 0
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  add  ax, cga_glyphs
  mov  bp, ax

  mov  cx, cs

  jmp  put_mode13_char_vidoffset

put_mode13_char_high:
  ; Characters 128 .. 255 get their address from interrupt vector 1F
  and  al, 0x7F
  mov  ah, 0
  shl  ax, 1
  shl  ax, 1
  shl  ax, 1
  mov  bp, ax

  mov  ax, 0
  mov  ds, ax
  mov  ax, [ds:0x7c]
  add  bp, ax

  mov  cx, [ds:0x7e]

put_mode13_char_vidoffset:
  mov  ax, 0x40
  mov  ds, ax

  ; Get the address offset in video ram for the top of the character into DI
  mov  ax, 2560 ; bytes per row
  mov  bl, [ds:curpos_y-bios_data]
  mov  bh, 0
  mul  bx
  mov  bl, [ds:curpos_x-bios_data]
  shl  bx, 1
  shl  bx, 1
  shl  bx, 1
  add  ax, bx
  mov  di, ax

  ; get segment for character data into ds
  mov  ds, cx

  ; get video RAM address for even lines into es
  mov  ax, 0xa000
  mov  es, ax

  pop  bx

  mov  cl, 8
put_mode13_char_loop:
  mov  bl, byte [ds:bp]

  mov  ch, 0x80
put_mode13_char_line_loop:
  test bl, ch
  jz   put_mode13_char_clear
  mov  al, bh
  jmp  put_mode13_char_pixel
put_mode13_char_clear:
  mov  al, 0x00
put_mode13_char_pixel:
  stosb

  shr  ch, 1
  jnz  put_mode13_char_line_loop

  inc  bp
  add  di, 312
  dec  cl
  jnz  put_mode13_char_loop

put_mode13_char_done:
  pop  bp
  pop  di
  pop  es
  pop  ds
  pop  dx
  pop  cx
  pop  bx
  pop  ax
  ret



; ************************* INT 70h handler - MC146818 RTC
int70:
  push ds
  push ax

  ; load DS with BIOS data area
  mov  ax, 0x0040
  mov  ds, ax

  ; get interrupt flags (clear on read)
  mov  al, 0x0b       ; RTC register B
  out  RTC_SEL, al
  in   al, RTC_REG
  xchg al, ah         ; save into AH

  ; get interrupt enabled flags
  mov  al, 0x0c       ; RTC register C
  out  RTC_SEL, al
  in   al, RTC_REG

  ; match reported flags with enabled interrupts
  and  ah, al

  ; periodic interrupt?
  test ah, 0x40
  jz   int70_checkAlarm ; no, bypass

  ; handle Periodic Interrupt
  ; subtract 976us to wait count
  sub  word [wait_count_lo - bios_data], 976
  sbb  word [wait_count_hi - bios_data], 0
  jnc  int70_checkAlarm ; bypass if result is > 0

  ; it is the time for disable periodic interrupt
  ; reset PIE in register B
  mov  al, 0x0b
  out  RTC_SEL, al
  in   al, RTC_REG
  and  al, 0xbf
  out  RTC_REG, al
  ; reset wait flag
  mov  byte [wait_active_flag - bios_data], 0x00
  ; reset user wait flag
  push bx
  lds  bx, [usr_waitflag_off - bios_data] ; load DS from usr_waitfalg_seg and BX from usr_waitflag_off
  mov  byte [bx], 0x80
  pop  bx
  ; note: from here DS is no more valid

int70_checkAlarm:
  ; alarm interrupt?
  test ah, 0x20
  jz   int70_exit       ; no, exit
  ; handle Alarm Interrupt
  int  0x4a

int70_exit:
  call reportEOIB
  pop  ax
  pop  ds
  iret



; ************************* INT 74h handler - PS/2 mouse
int74:
  push ax
  push bx
  push cx
  push ds

  ; load EBDA segment to DS
  mov  ax, EBDA_SEG
  mov  ds, ax

  ; read mouse data from 8042
  in   al, 0x60

  ; put data index into BX
  mov  bl, [EBDA_FLAGS1]
  and  bl, 7
  xor  bh, bh

  ; save read data
  mov  [EBDA_PACKET + bx], al
  inc  bx

  ; save new data index
  and  byte [EBDA_FLAGS1], 0xf8
  or   byte [EBDA_FLAGS1], bl

  ; compare with packet size
  mov  al, [EBDA_FLAGS2]
  and  al, 7
  cmp  bl, al
  jl   int74_exit

  ; packet complete, mouse driver installed?
  test byte [EBDA_FLAGS2], 0x80
  jz   int74_reset          ; no, just reset index and exit

  ; packet complete push first 4 datas
  mov  cx, 4
  mov  bx, EBDA_PACKET
  xor  ah, ah
int74_push:
  mov  al, [bx]
  push ax
  inc  bx
  loop int74_push

  ; call mouse driver
  call far [EBDA_DRIVER_OFFSET]

  ; remove parameters
  mov  cx, 4
int74_pop:
  pop  ax
  loop int74_pop

  ; reset index
int74_reset:
  and  byte [EBDA_FLAGS1], 0xf8

int74_exit:
  call reportEOIB
  pop  ds
  pop  cx
  pop  bx
  pop  ax
  iret


; Reaches up into the stack before the end of an interrupt handler, and sets the carry flag

reach_stack_stc:

  push bp
  mov  bp, sp
  or   word [bp+6], 1
  pop  bp
  iret

; Reaches up into the stack before the end of an interrupt handler, and clears the carry flag

reach_stack_clc:

  push bp
  mov  bp, sp
  and  word [bp+6], 0xfffe
  pop  bp
  iret

; Reaches up into the stack before the end of an interrupt handler, and returns with the current
; setting of the carry flag

reach_stack_carry:

  jc  reach_stack_stc
  jmp reach_stack_clc


; BIOS interupt redirect for 100% compatible BIOS
int09_redirect_start:
  jmp [cs:int_table +9*4]
int09_redirect_end:



; ****************************************************************************************
; That's it for the code. Now, the data tables follow.
; ****************************************************************************************



; Standard PC-compatible BIOS data area - to copy to 40:0

bios_data:

com1addr         dw  0x03F8              ; 40:00h
com2addr         dw  0x02F8
com3addr         dw  0x0000
com4addr         dw  0x0000
lpt1addr         dw  0x0378              ; 40:08h
lpt2addr         dw  0
lpt3addr         dw  0
lpt4addr         dw  0                   ; 40:0e

; bit 0     : diskette available for boot
; bit 2     : PS/2 mouse present
; bit 4-5   : 80x25 color (10)
; bit 7-6   : number of disk drivers (00 = 1, 01 = 2)
; bit 9-11  : number of serial ports (010 = 2)
; bit 14-15 : number of printer adapters
equip            dw  0b0001010001100101  ; 40:10h
;                      5432109876543210

                 db  0
memsize          dw  MEMSIZE
                 db  0
                 db  0
keyflags1        db  0
keyflags2        db  0
                 db  0
kbbuf_head       dw  kbbuf-bios_data
kbbuf_tail       dw  kbbuf-bios_data
kbbuf:
times 32         db  'X'
drivecal         db  0b00000011         ; 40:3eh
diskmotor        db  0                  ; 40:3fh
motorshutoff     db  0x07               ; 40:40h
disk_laststatus  db  0                  ; 40:41h
times 7          db  0

bios_data_vid_start: ; // offset 0x49
vid_mode         db  0x03
vid_cols         dw  80
vid_page_size    dw  0x1000
vid_page_offset  dw  0
cursor_pos:
curpos_x         db  0
curpos_y         db  0
times 7          dw  0 ; cursor pos for pages
cur_v_end        db  0x07
cur_v_start      db  0x06
disp_page        db  0
crtport          dw  0x3d4
vid_3x8          db  0x09
vid_3x9          db  0
vid_rom_offset   dw  0
vid_rom_segaddr  dw  0
biod_data_vid_end:

last_intr        db  0
clk_dtimer:
clk_dtimer_lo    dw  0   ; 0x6c
clk_dtimer_hi    dw  0   ; 0x6e
clk_rollover     db  0   ; 0x70
ctrl_break       db  0
soft_rst_flg     dw  0x1234
                 db  0   ; 0x74
num_hd           db  0   ; 0x75
                 db  0   ; 0x76
                 db  0   ; 0x77
                 dd  0   ; 0x78
com1_timeout     db  27  ; 40:7c  500 ms
com2_timeout     db  27  ; 40:7d  500 ms
com3_timeout     db  27  ; 40:7e  500 ms
com4_timeout     db  27  ; 40:7f  500 ms
kbbuf_start_ptr  dw  0x001e     ; 40:80
kbbuf_end_ptr    dw  0x003e     ; 40:82
vid_rows         db  24         ; 40:84
                 db  8          ; 40:85 scan lines per character
                 db  0          ; 40:86
vidmode_opt      db  0x96       ; 40:87    0x70
                 db  0x09       ; 40:88    0x89
                 db  0x81       ; 40:89    0x51
video_card       db  0x0c       ; 40:8a    0x0c
                 db  0          ; 40:8b
                 db  0          ; 40:8c
                 db  0          ; 40:8d
                 db  0          ; 40:8e
                 db  0          ; 40:8f
                 db  0
                 db  0
                 db  0
                 db  0
                 db  0
                 db  0
kb_mode          db  0x10    ; 40:96, bit 4 = 1, 101/102 enhanced keyboard
kb_led           db  0x10    ; 40:97, bit 4 = 1, ACK received (always on)
usr_waitflag_off dw  0       ; 40:98
usr_waitfalg_seg dw  0       ; 40:9a
wait_count_lo    dw  0       ; 40:9c
wait_count_hi    dw  0       ; 40:9e
wait_active_flag db  0       ; 40:a0

ending:
times (0xff-($-com1addr)) db  0

; additional BIOS variables (non standard)
boot_device      db  0
crt_curpos_x     db  0
crt_curpos_y     db  0


; Interrupt vector table - to copy to 0:0

int_table   dw int0
            dw 0xf000
            dw int1
            dw 0xf000
            dw int2
            dw 0xf000
            dw int3
            dw 0xf000
            dw int4
            dw 0xf000
            dw int5
            dw 0xf000
            dw int6
            dw 0xf000
            dw int7
            dw 0xf000
            dw int8
            dw 0xf000
            dw int9
            dw 0xf000
            dw inta
            dw 0xf000
            dw intb
            dw 0xf000
            dw intc
            dw 0xf000
            dw intd
            dw 0xf000
            dw inte
            dw 0xf000
            dw intf
            dw 0xf000

            dw int10
            dw 0xf000
            dw int11
            dw 0xf000
            dw int12
            dw 0xf000
            dw int13
            dw 0xf000
            dw int14
            dw 0xf000
            dw int15
            dw 0xf000
            dw int16
            dw 0xf000
            dw int17
            dw 0xf000
            dw int18
            dw 0xf000
            dw int19
            dw 0xf000
            dw int1a
            dw 0xf000
            dw int1b
            dw 0xf000
            dw int1c
            dw 0xf000
            dw video_init_table   ; int1d
            dw 0xf000
            dw int1e
            dw 0xf000
            dw cga_glyphs + 1024  ; int1f
            dw 0xf000

            dw 0x0000   ; int20
            dw 0x0000
            dw 0x0000   ; int21
            dw 0x0000
            dw 0x0000   ; int22
            dw 0x0000
            dw 0x0000   ; int23
            dw 0x0000
            dw 0x0000   ; int24
            dw 0x0000
            dw 0x0000   ; int25
            dw 0x0000
            dw 0x0000   ; int26
            dw 0x0000
            dw 0x0000   ; int27
            dw 0x0000
            dw 0x0000   ; int28
            dw 0x0000
            dw 0x0000   ; int29
            dw 0x0000
            dw 0x0000   ; int2a
            dw 0x0000
            dw 0x0000   ; int2b
            dw 0x0000
            dw 0x0000   ; int2c
            dw 0x0000
            dw 0x0000   ; int2d
            dw 0x0000
            dw 0x0000   ; int2e
            dw 0x0000
            dw 0x0000   ; int2f
            dw 0x0000

            dw 0x0000   ; int30
            dw 0x0000
            dw 0x0000   ; int31
            dw 0x0000
            dw 0x0000   ; int32
            dw 0x0000
            dw 0x0000   ; int33
            dw 0x0000
            dw 0x0000   ; int34
            dw 0x0000
            dw 0x0000   ; int35
            dw 0x0000
            dw 0x0000   ; int36
            dw 0x0000
            dw 0x0000   ; int37
            dw 0x0000
            dw 0x0000   ; int38
            dw 0x0000
            dw 0x0000   ; int39
            dw 0x0000
            dw 0x0000   ; int3a
            dw 0x0000
            dw 0x0000   ; int3b
            dw 0x0000
            dw 0x0000   ; int3c
            dw 0x0000
            dw 0x0000   ; int3d
            dw 0x0000
            dw 0x0000   ; int3e
            dw 0x0000
            dw 0x0000   ; int3f
            dw 0x0000

            dw 0x0000   ; int40
            dw 0x0000
            dw int41    ; int41
            dw 0xf000
            dw 0x0000   ; int42
            dw 0x0000
            dw 0x0000   ; int43
            dw 0x0000
            dw 0x0000   ; int44
            dw 0x0000
            dw 0x0000   ; int45
            dw 0x0000
            dw int46    ; int46
            dw 0xf000
            dw 0x0000   ; int47
            dw 0x0000
            dw 0x0000   ; int48
            dw 0x0000
            dw 0x0000   ; int49
            dw 0x0000
            dw 0x0000   ; int4a
            dw 0x0000
            dw 0x0000   ; int4b
            dw 0x0000
            dw 0x0000   ; int4c
            dw 0x0000
            dw 0x0000   ; int4d
            dw 0x0000
            dw 0x0000   ; int4e
            dw 0x0000
            dw 0x0000   ; int4f
            dw 0x0000

            dw 0x0000   ; int50
            dw 0x0000
            dw 0x0000   ; int51
            dw 0x0000
            dw 0x0000   ; int52
            dw 0x0000
            dw 0x0000   ; int53
            dw 0x0000
            dw 0x0000   ; int54
            dw 0x0000
            dw 0x0000   ; int55
            dw 0x0000
            dw 0x0000   ; int56
            dw 0x0000
            dw 0x0000   ; int57
            dw 0x0000
            dw 0x0000   ; int58
            dw 0x0000
            dw 0x0000   ; int59
            dw 0x0000
            dw 0x0000   ; int5a
            dw 0x0000
            dw 0x0000   ; int5b
            dw 0x0000
            dw 0x0000   ; int5c
            dw 0x0000
            dw 0x0000   ; int5d
            dw 0x0000
            dw 0x0000   ; int5e
            dw 0x0000
            dw 0x0000   ; int5f
            dw 0x0000

            dw 0x0000   ; int60
            dw 0x0000
            dw 0x0000   ; int61
            dw 0x0000
            dw 0x0000   ; int62
            dw 0x0000
            dw 0x0000   ; int63
            dw 0x0000
            dw 0x0000   ; int64
            dw 0x0000
            dw 0x0000   ; int65
            dw 0x0000
            dw 0x0000   ; int66
            dw 0x0000
            dw 0x0000   ; int67
            dw 0x0000
            dw 0x0000   ; int68
            dw 0x0000
            dw 0x0000   ; int69
            dw 0x0000
            dw 0x0000   ; int6a
            dw 0x0000
            dw 0x0000   ; int6b
            dw 0x0000
            dw 0x0000   ; int6c
            dw 0x0000
            dw 0x0000   ; int6d
            dw 0x0000
            dw 0x0000   ; int6e
            dw 0x0000
            dw 0x0000   ; int6f
            dw 0x0000

            dw int70    ; int70
            dw 0xf000
            dw 0x0000   ; int71
            dw 0x0000
            dw 0x0000   ; int72
            dw 0x0000
            dw 0x0000   ; int73
            dw 0x0000
            dw int74    ; int74
            dw 0xf000
            dw 0x0000   ; int75
            dw 0x0000
            dw 0x0000   ; int76
            dw 0x0000
            dw 0x0000   ; int77
            dw 0x0000
            dw 0x0000   ; int78
            dw 0x0000
            dw 0x0000   ; int79
            dw 0x0000
            dw 0x0000   ; int7a
            dw 0x0000
            dw 0x0000   ; int7b
            dw 0x0000
            dw 0x0000   ; int7c
            dw 0x0000
            dw 0x0000   ; int7d
            dw 0x0000
            dw 0x0000   ; int7e
            dw 0x0000
            dw 0x0000   ; int7f
            dw 0x0000

itbl_size   dw $-int_table


video_init_table:
  ; 6845 register values for 40x25 modes
  db  0x39, 0x28, 0x2d, 0x10, 0x1f, 0x06, 0x19, 0x1c, 0x02, 0x07, 0x66, 0x07, 0x00, 0x00, 0x00, 0x00
  ; 6845 register values for 80x25 modes
  db  0x72, 0x50, 0x5a, 0x10, 0x1f, 0x06, 0x19, 0x1c, 0x02, 0x07, 0x66, 0x07, 0x00, 0x00, 0x00, 0x00
  ; 6845 register values for graphics modes
  db  0x39, 0x28, 0x2d, 0x10, 0x7f, 0x06, 0x64, 0x70, 0x02, 0x07, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00
  ; 6845 register values for 80x25 monochrome modes
  db  0x72, 0x50, 0x5a, 0x10, 0x1f, 0x06, 0x19, 0x1c, 0x02, 0x07, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00
  ; wSize40x25
  dw  0x3E8
  ; wSize80x25
  dw  0x7d0
  ; wSizeLoRes
  dw  0x3e80
  ; wSizeHiRes
  dw  0x3e80
  ; abClmCnts (Text columns in each mode)
  db  0x28, 0x28, 0x50, 0x50, 0x28, 0x28, 0x50, 0x00
  ; abModeCodes (port 3d8 values for each mode)
  db  0x2c, 0x28, 0x22, 0x29, 0x0a, 0x0e, 0x1a, 0x00

vid_static_table:
  db 0x7f ; bits 0 .. 7 = modes 00h .. 07h supported
  db 0x00 ; bits 0 .. 7 = modes 08h .. 0fh supported
  db 0x0a ; bits 0 .. 3 = modes 10h .. 13h supported
  db 0x00 ; IBM reserved
  db 0x00 ; IBM reserved
  db 0x00 ; IBM reserved
  db 0x00 ; IBM reserved
  db 0x01 ; scan lines suppported: bit 0 = 200, 1 = 350, 2 = 400
  db 0x08 ; total number of character blocks in text mode
  db 0x08 ; maximum number of character blocks in text mode
  dw 0x046c ; misc support flags
  dw 0x00 ; reserved
  db 0x08 ; save pointer function flags
  db 0x00 ; reserved


; CGA Character set patterns

cga_glyphs:
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x7e, 0x81, 0xa5, 0x81, 0xbd, 0x99, 0x81, 0x7e,
  db 0x7e, 0xff, 0xdb, 0xff, 0xc3, 0xe7, 0xff, 0x7e,
  db 0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00,
  db 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00,
  db 0x3c, 0x3c, 0x18, 0xff, 0xe7, 0x18, 0x3c, 0x00,
  db 0x10, 0x38, 0x7c, 0xfe, 0xee, 0x10, 0x38, 0x00,
  db 0x00, 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00, 0x00,
  db 0xff, 0xff, 0xe7, 0xc3, 0xc3, 0xe7, 0xff, 0xff,
  db 0x00, 0x3c, 0x66, 0x42, 0x42, 0x66, 0x3c, 0x00,
  db 0xff, 0xc3, 0x99, 0xbd, 0xbd, 0x99, 0xc3, 0xff,
  db 0x1e, 0x06, 0x0a, 0x78, 0xcc, 0xcc, 0x78, 0x00,
  db 0x3c, 0x42, 0x42, 0x42, 0x3c, 0x18, 0x7e, 0x18,
  db 0x08, 0x0c, 0x0a, 0x0a, 0x08, 0x78, 0xf0, 0x00,
  db 0x18, 0x14, 0x1a, 0x16, 0x72, 0xe2, 0x0e, 0x1c,
  db 0x10, 0x54, 0x38, 0xee, 0x38, 0x54, 0x10, 0x00,
  db 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x00,
  db 0x02, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x02, 0x00,
  db 0x18, 0x3c, 0x5a, 0x18, 0x5a, 0x3c, 0x18, 0x00,
  db 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x00,
  db 0x7f, 0xdb, 0xdb, 0xdb, 0x7b, 0x1b, 0x1b, 0x00,
  db 0x1c, 0x22, 0x38, 0x44, 0x44, 0x38, 0x88, 0x70,
  db 0x00, 0x00, 0x00, 0x00, 0x7e, 0x7e, 0x7e, 0x00,
  db 0x18, 0x3c, 0x5a, 0x18, 0x5a, 0x3c, 0x18, 0x7e,
  db 0x18, 0x3c, 0x5a, 0x18, 0x18, 0x18, 0x18, 0x00,
  db 0x18, 0x18, 0x18, 0x18, 0x5a, 0x3c, 0x18, 0x00,
  db 0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00,
  db 0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xff, 0x00,
  db 0x00, 0x24, 0x42, 0xff, 0x42, 0x24, 0x00, 0x00,
  db 0x00, 0x10, 0x38, 0x7c, 0xfe, 0xfe, 0x00, 0x00,
  db 0x00, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x30, 0x78, 0x78, 0x30, 0x30, 0x00, 0x30, 0x00,
  db 0x6c, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x6c, 0x6c, 0xfe, 0x6c, 0xfe, 0x6c, 0x6c, 0x00,
  db 0x10, 0x7c, 0xd0, 0x7c, 0x16, 0xfc, 0x10, 0x00,
  db 0x00, 0x66, 0xac, 0xd8, 0x36, 0x6a, 0xcc, 0x00,
  db 0x38, 0x4c, 0x38, 0x78, 0xce, 0xcc, 0x7a, 0x00,
  db 0x30, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x18, 0x30, 0x60, 0x60, 0x60, 0x30, 0x18, 0x00,
  db 0x60, 0x30, 0x18, 0x18, 0x18, 0x30, 0x60, 0x00,
  db 0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00,
  db 0x00, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x10, 0x20,
  db 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00,
  db 0x02, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x00,
  db 0x7c, 0xce, 0xde, 0xf6, 0xe6, 0xe6, 0x7c, 0x00,
  db 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x7e, 0x00,
  db 0x7c, 0xc6, 0x06, 0x1c, 0x70, 0xc0, 0xfe, 0x00,
  db 0x7c, 0xc6, 0x06, 0x3c, 0x06, 0xc6, 0x7c, 0x00,
  db 0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x1e, 0x00,
  db 0xfe, 0xc0, 0xfc, 0x06, 0x06, 0xc6, 0x7c, 0x00,
  db 0x7c, 0xc6, 0xc0, 0xfc, 0xc6, 0xc6, 0x7c, 0x00,
  db 0xfe, 0xc6, 0x86, 0x0c, 0x18, 0x30, 0x30, 0x00,
  db 0x7c, 0xc6, 0xc6, 0x7c, 0xc6, 0xc6, 0x7c, 0x00,
  db 0x7c, 0xc6, 0xc6, 0x7e, 0x06, 0xc6, 0x7c, 0x00,
  db 0x00, 0x30, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00,
  db 0x00, 0x30, 0x00, 0x00, 0x00, 0x30, 0x10, 0x20,
  db 0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x00,
  db 0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00,
  db 0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0x00,
  db 0x78, 0xcc, 0x0c, 0x18, 0x30, 0x00, 0x30, 0x00,
  db 0x7c, 0x82, 0x9e, 0xa6, 0x9e, 0x80, 0x7c, 0x00,
  db 0x7c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00,
  db 0xfc, 0x66, 0x66, 0x7c, 0x66, 0x66, 0xfc, 0x00,
  db 0x7c, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x7c, 0x00,
  db 0xfc, 0x66, 0x66, 0x66, 0x66, 0x66, 0xfc, 0x00,
  db 0xfe, 0x62, 0x68, 0x78, 0x68, 0x62, 0xfe, 0x00,
  db 0xfe, 0x62, 0x68, 0x78, 0x68, 0x60, 0xf0, 0x00,
  db 0x7c, 0xc6, 0xc6, 0xc0, 0xce, 0xc6, 0x7e, 0x00,
  db 0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00,
  db 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
  db 0x1e, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78, 0x00,
  db 0xe6, 0x66, 0x64, 0x78, 0x6c, 0x66, 0xe6, 0x00,
  db 0xf0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0x00,
  db 0x82, 0xc6, 0xee, 0xfe, 0xd6, 0xc6, 0xc6, 0x00,
  db 0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc6, 0x00,
  db 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
  db 0xfc, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xf0, 0x00,
  db 0x7c, 0xc6, 0xc6, 0xc6, 0xd6, 0xde, 0x7c, 0x06,
  db 0xfc, 0x66, 0x66, 0x7c, 0x66, 0x66, 0xe6, 0x00,
  db 0x7c, 0xc6, 0xc0, 0x7c, 0x06, 0xc6, 0x7c, 0x00,
  db 0x7e, 0x5a, 0x5a, 0x18, 0x18, 0x18, 0x3c, 0x00,
  db 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
  db 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00,
  db 0xc6, 0xc6, 0xd6, 0xfe, 0xee, 0xc6, 0x82, 0x00,
  db 0xc6, 0x6c, 0x38, 0x38, 0x38, 0x6c, 0xc6, 0x00,
  db 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x3c, 0x00,
  db 0xfe, 0xc6, 0x8c, 0x18, 0x32, 0x66, 0xfe, 0x00,
  db 0x78, 0x60, 0x60, 0x60, 0x60, 0x60, 0x78, 0x00,
  db 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x02, 0x00,
  db 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x00,
  db 0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
  db 0x30, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x76, 0x00,
  db 0xe0, 0x60, 0x60, 0x7c, 0x66, 0x66, 0x7c, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc6, 0x7c, 0x00,
  db 0x1c, 0x0c, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0x7c, 0x00,
  db 0x1c, 0x36, 0x30, 0x78, 0x30, 0x30, 0x78, 0x00,
  db 0x00, 0x00, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0x78,
  db 0xe0, 0x60, 0x6c, 0x76, 0x66, 0x66, 0xe6, 0x00,
  db 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00,
  db 0x00, 0x0c, 0x00, 0x1c, 0x0c, 0x0c, 0xcc, 0x78,
  db 0xe0, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0xe6, 0x00,
  db 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
  db 0x00, 0x00, 0xcc, 0xfe, 0xd6, 0xd6, 0xd6, 0x00,
  db 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
  db 0x00, 0x00, 0xdc, 0x66, 0x66, 0x7c, 0x60, 0xf0,
  db 0x00, 0x00, 0x7c, 0xcc, 0xcc, 0x7c, 0x0c, 0x1e,
  db 0x00, 0x00, 0xde, 0x76, 0x60, 0x60, 0xf0, 0x00,
  db 0x00, 0x00, 0x7c, 0xc0, 0x7c, 0x06, 0x7c, 0x00,
  db 0x10, 0x30, 0xfc, 0x30, 0x30, 0x34, 0x18, 0x00,
  db 0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
  db 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00,
  db 0x00, 0x00, 0xc6, 0xd6, 0xd6, 0xfe, 0x6c, 0x00,
  db 0x00, 0x00, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0x00,
  db 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x7c,
  db 0x00, 0x00, 0xfc, 0x98, 0x30, 0x64, 0xfc, 0x00,
  db 0x0e, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0e, 0x00,
  db 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00,
  db 0xe0, 0x30, 0x30, 0x18, 0x30, 0x30, 0xe0, 0x00,
  db 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0x00,
  db 0x7c, 0xc6, 0xc0, 0xc0, 0xc6, 0x7c, 0x18, 0x70,
  db 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
  db 0x0e, 0x10, 0x7c, 0xc6, 0xfe, 0xc0, 0x7c, 0x00,
  db 0x7c, 0x82, 0x38, 0x0c, 0x7c, 0xcc, 0x76, 0x00,
  db 0xcc, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x76, 0x00,
  db 0xe0, 0x10, 0x78, 0x0c, 0x7c, 0xcc, 0x76, 0x00,
  db 0x30, 0x30, 0x78, 0x0c, 0x7c, 0xcc, 0x76, 0x00,
  db 0x00, 0x00, 0x7c, 0xc0, 0xc0, 0x7c, 0x18, 0x70,
  db 0x7c, 0x82, 0x7c, 0xc6, 0xfe, 0xc0, 0x7c, 0x00,
  db 0xc6, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0x7c, 0x00,
  db 0xe0, 0x10, 0x7c, 0xc6, 0xfe, 0xc0, 0x7c, 0x00,
  db 0x66, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00,
  db 0x7c, 0x82, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00,
  db 0xe0, 0x10, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00,
  db 0xc6, 0x00, 0x7c, 0xc6, 0xfe, 0xc6, 0xc6, 0x00,
  db 0x38, 0x38, 0x7c, 0xc6, 0xfe, 0xc6, 0xc6, 0x00,
  db 0x0e, 0x10, 0xfe, 0x60, 0x78, 0x60, 0xfe, 0x00,
  db 0x00, 0x00, 0x7c, 0x12, 0x7e, 0xd0, 0x7e, 0x00,
  db 0x7e, 0xc8, 0xc8, 0xfe, 0xc8, 0xc8, 0xce, 0x00,
  db 0x7c, 0x82, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
  db 0xc6, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
  db 0xe0, 0x10, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
  db 0x7c, 0x82, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
  db 0xe0, 0x10, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
  db 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x7c,
  db 0xc6, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
  db 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
  db 0x18, 0x7c, 0xd6, 0xd0, 0xd6, 0x7c, 0x18, 0x00,
  db 0x38, 0x6c, 0x60, 0xf0, 0x60, 0xf2, 0xdc, 0x00,
  db 0x66, 0x3c, 0x18, 0x7e, 0x18, 0x7e, 0x18, 0x00,
  db 0xf8, 0xcc, 0xf8, 0xc4, 0xcc, 0xde, 0xcc, 0x06,
  db 0x0e, 0x1b, 0x18, 0x3c, 0x18, 0x18, 0xd8, 0x70,
  db 0x0e, 0x10, 0x78, 0x0c, 0x7c, 0xcc, 0x76, 0x00,
  db 0x0e, 0x10, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00,
  db 0x0e, 0x10, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
  db 0x0e, 0x10, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
  db 0x66, 0x98, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x00,
  db 0x66, 0x98, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0x00,
  db 0x38, 0x0c, 0x3c, 0x34, 0x00, 0x7e, 0x00, 0x00,
  db 0x38, 0x6c, 0x6c, 0x38, 0x00, 0x7c, 0x00, 0x00,
  db 0x30, 0x00, 0x30, 0x60, 0xc6, 0xc6, 0x7c, 0x00,
  db 0x00, 0x00, 0xfe, 0xc0, 0xc0, 0xc0, 0x00, 0x00,
  db 0x00, 0x00, 0xfe, 0x06, 0x06, 0x06, 0x00, 0x00,
  db 0xc0, 0xc8, 0xd0, 0xfe, 0x46, 0x8c, 0x1e, 0x00,
  db 0xc0, 0xc8, 0xd0, 0xec, 0x5c, 0xbe, 0x0c, 0x00,
  db 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,
  db 0x00, 0x36, 0x6c, 0xd8, 0x6c, 0x36, 0x00, 0x00,
  db 0x00, 0xd8, 0x6c, 0x36, 0x6c, 0xd8, 0x00, 0x00,
  db 0x00, 0x55, 0x00, 0x55, 0x00, 0x55, 0x00, 0x55,
  db 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55,
  db 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0xf8, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18,
  db 0x36, 0x36, 0x36, 0x36, 0xf6, 0x36, 0x36, 0x36,
  db 0x00, 0x00, 0x00, 0x00, 0xfe, 0x36, 0x36, 0x36,
  db 0x00, 0x00, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18,
  db 0x36, 0x36, 0xf6, 0x06, 0xf6, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x00, 0x00, 0xfe, 0x06, 0xf6, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0xf6, 0x06, 0xfe, 0x00, 0x00, 0x00,
  db 0x36, 0x36, 0x36, 0x36, 0xfe, 0x00, 0x00, 0x00,
  db 0x18, 0x18, 0xf8, 0x18, 0xf8, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0xf8, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0x1f, 0x00, 0x00, 0x00,
  db 0x18, 0x18, 0x18, 0x18, 0xff, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0xff, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0x1f, 0x18, 0x18, 0x18,
  db 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
  db 0x18, 0x18, 0x18, 0x18, 0xff, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18,
  db 0x36, 0x36, 0x36, 0x36, 0x37, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x37, 0x30, 0x3f, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3f, 0x30, 0x37, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0xf7, 0x00, 0xff, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xff, 0x00, 0xf7, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x37, 0x30, 0x37, 0x36, 0x36, 0x36,
  db 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
  db 0x36, 0x36, 0xf7, 0x00, 0xf7, 0x36, 0x36, 0x36,
  db 0x18, 0x18, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
  db 0x36, 0x36, 0x36, 0x36, 0xff, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xff, 0x00, 0xff, 0x18, 0x18, 0x18,
  db 0x00, 0x00, 0x00, 0x00, 0xff, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x36, 0x36, 0x3f, 0x00, 0x00, 0x00,
  db 0x18, 0x18, 0x1f, 0x18, 0x1f, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18,
  db 0x00, 0x00, 0x00, 0x00, 0x3f, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x36, 0x36, 0xff, 0x36, 0x36, 0x36,
  db 0x18, 0x18, 0xff, 0x18, 0xff, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0xf8, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x1f, 0x18, 0x18, 0x18,
  db 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  db 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
  db 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  db 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  db 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x74, 0xcc, 0xc8, 0xdc, 0x76, 0x00,
  db 0x78, 0xcc, 0xd8, 0xcc, 0xc6, 0xc6, 0xdc, 0x40,
  db 0xfe, 0x62, 0x60, 0x60, 0x60, 0x60, 0xf0, 0x00,
  db 0x00, 0x02, 0x7e, 0xec, 0x6c, 0x6c, 0x48, 0x00,
  db 0xfe, 0x62, 0x30, 0x18, 0x30, 0x62, 0xfe, 0x00,
  db 0x00, 0x00, 0x7e, 0xd0, 0xc8, 0xc8, 0x70, 0x00,
  db 0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xf8, 0x80,
  db 0x00, 0x00, 0x7e, 0xd8, 0x18, 0x18, 0x10, 0x00,
  db 0x38, 0x10, 0x7c, 0xd6, 0xd6, 0x7c, 0x10, 0x38,
  db 0x7c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x7c, 0x00,
  db 0x7c, 0xc6, 0xc6, 0xc6, 0x6c, 0x28, 0xee, 0x00,
  db 0x3c, 0x22, 0x18, 0x7c, 0xcc, 0xcc, 0x78, 0x00,
  db 0x00, 0x00, 0x66, 0x99, 0x99, 0x66, 0x00, 0x00,
  db 0x00, 0x06, 0x7c, 0x9e, 0xf2, 0x7c, 0xc0, 0x00,
  db 0x00, 0x00, 0x7c, 0xc0, 0xf8, 0xc0, 0x7c, 0x00,
  db 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00,
  db 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0x00,
  db 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x7e, 0x00,
  db 0x30, 0x18, 0x0c, 0x18, 0x30, 0x00, 0x7c, 0x00,
  db 0x18, 0x30, 0x60, 0x30, 0x18, 0x00, 0x7c, 0x00,
  db 0x0e, 0x1b, 0x1b, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0xd8, 0xd8, 0x70,
  db 0x00, 0x18, 0x00, 0x7e, 0x00, 0x18, 0x00, 0x00,
  db 0x00, 0x76, 0xdc, 0x00, 0x76, 0xdc, 0x00, 0x00,
  db 0x38, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
  db 0x0f, 0x0c, 0x0c, 0x0c, 0xec, 0x6c, 0x3c, 0x00,
  db 0xd8, 0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00,
  db 0xf0, 0x30, 0xc0, 0xf0, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3c, 0x3c, 0x3c, 0x3c, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

vga_glyphs:
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7e, 0x81, 0xa5, 0x81, 0x81, 0xbd, 0x99, 0x81, 0x81, 0x7e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7e, 0xff, 0xdb, 0xff, 0xff, 0xc3, 0xe7, 0xff, 0xff, 0x7e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x6c, 0xfe, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x18, 0x3c, 0x3c, 0xe7, 0xe7, 0xe7, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x18, 0x3c, 0x7e, 0xff, 0xff, 0x7e, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xc3, 0xc3, 0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x66, 0x42, 0x42, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0xff, 0xff, 0xff, 0xff, 0xff, 0xc3, 0x99, 0xbd, 0xbd, 0x99, 0xc3, 0xff, 0xff, 0xff, 0xff, 0xff,
  db 0x00, 0x00, 0x1e, 0x0e, 0x1a, 0x32, 0x78, 0xcc, 0xcc, 0xcc, 0xcc, 0x78, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3f, 0x33, 0x3f, 0x30, 0x30, 0x30, 0x30, 0x70, 0xf0, 0xe0, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7f, 0x63, 0x7f, 0x63, 0x63, 0x63, 0x63, 0x67, 0xe7, 0xe6, 0xc0, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x18, 0x18, 0xdb, 0x3c, 0xe7, 0x3c, 0xdb, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfe, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x02, 0x06, 0x0e, 0x1e, 0x3e, 0xfe, 0x3e, 0x1e, 0x0e, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7f, 0xdb, 0xdb, 0xdb, 0x7b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x7c, 0xc6, 0x60, 0x38, 0x6c, 0xc6, 0xc6, 0x6c, 0x38, 0x0c, 0xc6, 0x7c, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xfe, 0xfe, 0xfe, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x7e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x6c, 0xfe, 0x6c, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x38, 0x7c, 0x7c, 0xfe, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0xfe, 0xfe, 0x7c, 0x7c, 0x38, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x18, 0x3c, 0x3c, 0x3c, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x66, 0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00,
  db 0x18, 0x18, 0x7c, 0xc6, 0xc2, 0xc0, 0x7c, 0x06, 0x06, 0x86, 0xc6, 0x7c, 0x18, 0x18, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0xc2, 0xc6, 0x0c, 0x18, 0x30, 0x60, 0xc6, 0x86, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x38, 0x6c, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x30, 0x30, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x02, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xd6, 0xd6, 0xc6, 0xc6, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0x06, 0x06, 0x3c, 0x06, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x0c, 0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x0c, 0x0c, 0x1e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xfe, 0xc0, 0xc0, 0xc0, 0xfc, 0x06, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x38, 0x60, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xfe, 0xc6, 0x06, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x06, 0x06, 0x0c, 0x78, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0x0c, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xde, 0xde, 0xde, 0xdc, 0xc0, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x66, 0x66, 0xfc, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xc0, 0xc0, 0xc2, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xf8, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x62, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xde, 0xc6, 0xc6, 0x66, 0x3a, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0xcc, 0x78, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xe6, 0x66, 0x66, 0x6c, 0x78, 0x78, 0x6c, 0x66, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xf0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xc6, 0xee, 0xfe, 0xfe, 0xd6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xc6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xde, 0x7c, 0x0c, 0x0e, 0x00, 0x00,
  db 0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0x66, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0x60, 0x38, 0x0c, 0x06, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7e, 0x7e, 0x5a, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xd6, 0xd6, 0xfe, 0xee, 0x6c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xc6, 0xc6, 0x6c, 0x7c, 0x38, 0x38, 0x7c, 0x6c, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xfe, 0xc6, 0x86, 0x0c, 0x18, 0x30, 0x60, 0xc2, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0x70, 0x38, 0x1c, 0x0e, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
  db 0x00, 0x30, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xe0, 0x60, 0x60, 0x78, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x1c, 0x0c, 0x0c, 0x3c, 0x6c, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x1c, 0x36, 0x32, 0x30, 0x78, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0xcc, 0x78, 0x00,
  db 0x00, 0x00, 0xe0, 0x60, 0x60, 0x6c, 0x76, 0x66, 0x66, 0x66, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x18, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x06, 0x06, 0x00, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c, 0x00,
  db 0x00, 0x00, 0xe0, 0x60, 0x60, 0x66, 0x6c, 0x78, 0x78, 0x6c, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xec, 0xfe, 0xd6, 0xd6, 0xd6, 0xd6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xf0, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0x0c, 0x1e, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x76, 0x66, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0x60, 0x38, 0x0c, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x10, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x30, 0x30, 0x36, 0x1c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xd6, 0xd6, 0xd6, 0xfe, 0x6c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0x6c, 0x38, 0x38, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x0c, 0xf8, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xcc, 0x18, 0x30, 0x60, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x0e, 0x18, 0x18, 0x18, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x70, 0x18, 0x18, 0x18, 0x0e, 0x18, 0x18, 0x18, 0x18, 0x70, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xc0, 0xc0, 0xc2, 0x66, 0x3c, 0x18, 0x70, 0x00, 0x00,
  db 0x00, 0x00, 0xcc, 0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x0c, 0x18, 0x30, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x10, 0x38, 0x6c, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xcc, 0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x60, 0x30, 0x18, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x38, 0x6c, 0x38, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x7c, 0x18, 0x70, 0x00, 0x00,
  db 0x00, 0x10, 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xc6, 0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x60, 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x66, 0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x18, 0x3c, 0x66, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x60, 0x30, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0xc6, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x38, 0x6c, 0x38, 0x10, 0x38, 0x6c, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x0c, 0x18, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x62, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xec, 0x36, 0x36, 0x7e, 0xd8, 0xd8, 0x6e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3e, 0x6c, 0xcc, 0xcc, 0xfe, 0xcc, 0xcc, 0xcc, 0xcc, 0xce, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x10, 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xc6, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x60, 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x30, 0x78, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x60, 0x30, 0x18, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xc6, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x0c, 0x78, 0x00,
  db 0x00, 0xc6, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x18, 0x18, 0x7c, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x7c, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x38, 0x6c, 0x64, 0x60, 0xf0, 0x60, 0x60, 0x60, 0x60, 0xe6, 0xfc, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0xf8, 0xcc, 0xcc, 0xf8, 0xc4, 0xcc, 0xde, 0xcc, 0xcc, 0xcc, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x0e, 0x1b, 0x18, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x18, 0xd8, 0x70, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x18, 0x30, 0x60, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x0c, 0x18, 0x30, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x18, 0x30, 0x60, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x18, 0x30, 0x60, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x76, 0xdc, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00,
  db 0x76, 0xdc, 0x00, 0xc6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x3c, 0x6c, 0x6c, 0x3e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x38, 0x6c, 0x6c, 0x38, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x30, 0x30, 0x00, 0x30, 0x30, 0x60, 0xc0, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xc0, 0xc0, 0xc0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x60, 0xe0, 0x62, 0x66, 0x6c, 0x18, 0x30, 0x60, 0xdc, 0x86, 0x0c, 0x18, 0x3e, 0x00, 0x00,
  db 0x00, 0x60, 0xe0, 0x62, 0x66, 0x6c, 0x18, 0x30, 0x66, 0xce, 0x9a, 0x3f, 0x06, 0x06, 0x00, 0x00,
  db 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x3c, 0x3c, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x6c, 0xd8, 0x6c, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x6c, 0x36, 0x6c, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44,
  db 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,
  db 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xf8, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0xf6, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0xf6, 0x06, 0xf6, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x06, 0xf6, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0xf6, 0x06, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0xf8, 0x18, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1f, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x30, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x30, 0x37, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0xf7, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xf7, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x30, 0x37, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0xf7, 0x00, 0xf7, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x1f, 0x18, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0xff, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0xff, 0x18, 0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  db 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  db 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  db 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0xd8, 0xd8, 0xd8, 0xdc, 0x76, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x78, 0xcc, 0xcc, 0xcc, 0xd8, 0xcc, 0xc6, 0xc6, 0xc6, 0xcc, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xfe, 0xc6, 0xc6, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0xfe, 0xc6, 0x60, 0x30, 0x18, 0x18, 0x30, 0x60, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8, 0x70, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xc0, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x7e, 0x18, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0x6c, 0x6c, 0x6c, 0x6c, 0xee, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x1e, 0x30, 0x18, 0x0c, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0xdb, 0xdb, 0xdb, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x03, 0x06, 0x7e, 0xdb, 0xdb, 0xf3, 0x7e, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x1c, 0x30, 0x60, 0x60, 0x7c, 0x60, 0x60, 0x60, 0x30, 0x1c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0xfe, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x0e, 0x1b, 0x1b, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
  db 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xd8, 0xd8, 0xd8, 0x70, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x7e, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x38, 0x6c, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x0f, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0xec, 0x6c, 0x6c, 0x3c, 0x1c, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x6c, 0x36, 0x36, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x3c, 0x66, 0x0c, 0x18, 0x32, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00,
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00


; ROM BIOS compatibility entry points:
; ===================================
;   $e05b ; POST Entry Point
;   $e2c3 ; NMI Handler Entry Point
;   $e3fe ; INT 13h Fixed Disk Services Entry Point
;   $e401 ; Fixed Disk Parameter Table
;   $e6f2 ; INT 19h Boot Load Service Entry Point
;   $e6f5 ; Configuration Data Table
;   $e729 ; Baud Rate Generator Table
;   $e739 ; INT 14h Serial Communications Service Entry Point
;   $e82e ; INT 16h Keyboard Service Entry Point
; * $e987 ; INT 09h Keyboard Service Entry Point
;   $ec59 ; INT 13h Diskette Service Entry Point
;   $ef57 ; INT 0Eh Diskette Hardware ISR Entry Point
;   $efc7 ; Diskette Controller Parameter Table
;   $efd2 ; INT 17h Printer Service Entry Point
;   $f045 ; INT 10 Functions 0-Fh Entry Point
;   $f065 ; INT 10h Video Support Service Entry Point
;   $f0a4 ; MDA/CGA Video Parameter Table (INT 1Dh)
;   $f841 ; INT 12h Memory Size Service Entry Point
;   $f84d ; INT 11h Equipment List Service Entry Point
;   $f859 ; INT 15h System Services Entry Point
; * $fa6e ; Character Font for 320x200 & 640x200 Graphics (lower 128 characters)
;   $fe6e ; INT 1Ah Time-of-day Service Entry Point
;   $fea5 ; INT 08h System Timer ISR Entry Point
;   $fef3 ; Initial Interrupt Vector Offsets Loaded by POST
;   $ff53 ; IRET Instruction for Dummy Interrupt Handler
;   $ff54 ; INT 05h Print Screen Service Entry Point
; * $fff0 ; Power-up Entry Point
; * $fff5 ; ASCII Date ROM was built - 8 characters in MM/DD/YY
; * $fffe ; System Model ID

  absolute 0xe6f5
confDataTable:
  resb 0x1b

; In a 100% compatible BIOS this is where the int09 (keyboard) handler lives.
  absolute 0xe987
int09_bios_redirect:
  resb (int09_redirect_end - int09_redirect_start)

; In a 100% compatible bios this is where the CGA ROM appears.
; The first 128 characters of the above table gets copied here when the BIOS starts.
  absolute 0xfa6e
glyphs8x8:
  resb 1024

