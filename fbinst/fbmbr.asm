; MASM port of ref/fbinst/fbmbr.S.
;
; This file is intentionally kept as a flat 16-bit image source. Build it with
; ml.exe /c /omf and extract the _TEXT LEDATA bytes from the OMF object.

OPTION CASEMAP:NONE

.model tiny
.code
org 0

FB_MAGIC_WORD		EQU 4246h
FB_MAGIC_TEXT		EQU "FBBF"
FB_MENU_FILE		EQU "fb.cfg"

FBM_TYPE_MENU		EQU 1
FBM_TYPE_TEXT		EQU 2
FBM_TYPE_TIMEOUT	EQU 3
FBM_TYPE_DEFAULT	EQU 4
FBM_TYPE_COLOR		EQU 5

FBS_TYPE_BULDR		EQU 2
FBS_TYPE_SYSLINUX	EQU 3
FBS_TYPE_LINUX		EQU 4
FBS_TYPE_MSDOS		EQU 5
FBS_TYPE_FREEDOS	EQU 6
FBS_TYPE_CHAIN		EQU 7
FBS_TYPE_GRLDR		EQU 8

COLOR_NORMAL		EQU 7

OFS_max_sec		EQU 01ADh
OFS_lba			EQU 01AEh
OFS_bootdrv		EQU 01AFh
OFS_spt			EQU 01B0h
OFS_heads		EQU 01B1h
OFS_boot_base		EQU 01B2h
OFS_fb_magic		EQU 01B4h
OFS_mbr_table		EQU 01B8h

OFS_boot_size		EQU 0200h
OFS_flags		EQU 0202h
OFS_ver_major		EQU 0204h
OFS_ver_minor		EQU 0205h
OFS_list_used		EQU 0206h
OFS_list_size		EQU 0208h
OFS_pri_size		EQU 020Ah
OFS_ext_size		EQU 020Ch

VER_MAJOR		EQU 1
VER_MINOR		EQU 7

KEY_ESCAPE		EQU 011Bh
DATA_BUF_SEG		EQU 8000h
LIST_BUF_SEG		EQU 1000h
CODE_START		EQU 2000h
DOT_SIZE		EQU 1024
IOSYS_CM_MAGIC		EQU 4D43h

A MACRO sym
	EXITM <(CODE_START + sym - start)>
ENDM

start:
	jmp	start_7c00

org 010h
	db	2

org 018h
	dw	003Fh
	dw	00FFh

org 024h
	db	080h

IFDEF DEBUG
org 028h
ELSE
org 060h
ENDIF

start_7c00:
	xor	bx, bx
	mov	ds, bx
	mov	bp, bx

	mov	ss, bx
	mov	sp, 7C00h

	push	es
	push	di

	mov	ax, 0200h
	mov	es, ax
	inc	ax

	mov	cx, 1
	mov	si, cx
	xor	dh, dh
	call	safe_int13
	call	check_mbr
	mov	di, WORD PTR [A(lba)]

	pusha
	push	((CODE_START + 0200h) SHR 4)

	cmp	BYTE PTR [A(max_sec)], 080h
	jae	start_lba_fail

	mov	ah, 041h
	mov	bx, 055AAh
	int	13h
	jc	start_lba_fail
	sub	bx, 0AA55h
	jnz	start_lba_fail
	test	cl, 1
	jz	start_lba_fail

	pop	ax
	mov	es, ax
	call	lba_mode
	popa
	dec	dh
	jmp	start_load_stage2

start_lba_fail:
	pop	es

try_chs:
	popa

	inc	dh
	call	safe_int13
	push	ax
	call	calc_lba
	mov	BYTE PTR [A(spt)], al
	pop	ax

	xchg	ch, dh
	call	safe_int13
	call	calc_lba
	div	BYTE PTR [A(spt)]
	mov	BYTE PTR [A(heads)], al

start_load_stage2:
	mov	BYTE PTR [A(jump_patch)], err_int13 - jump_ofs
	mov	ax, WORD PTR [A(boot_base)]
	inc	ax
	call	read_sectors
	db	0EAh
	dw	A(start_2000)
	dw	0

calc_lba:
	mov	ax, WORD PTR [CODE_START + 0200h + 01FEh]
	cmp	ax, 0AA55h
	jnz	calc_lba_done
	call	check_mbr
	mov	ax, WORD PTR [CODE_START + 0200h + OFS_lba]
calc_lba_done:
	sub	ax, di
	ret

check_mbr:
	cmp	WORD PTR es:[OFS_fb_magic], FB_MAGIC_WORD
	jnz	halt
	ret

read_int13_fail:
	pusha
	xor	ax, ax
	int	13h
	popa

	cmp	al, 7
	jbe	read_int13_fail_small
	mov	al, 7
	jmp	read_int13_fail_store

read_int13_fail_small:
	cmp	al, 1
	db	076h
jump_patch:
	db	try_chs - jump_ofs
jump_ofs:
	mov	al, 1

read_int13_fail_store:
	cmp	dh, 0FFh
	jnz	read_int13_fail_save
	mov	BYTE PTR [si + 2], al
read_int13_fail_save:
	mov	BYTE PTR [A(max_sec)], al
	jmp	read_int13

read_sectors:
	sub	ax, di
	sbb	bp, 0

	cmp	dh, 0FFh
	jnz	chs_mode

lba_mode:
	mov	cx, ax

lba_mode_cont:
	push	si

	xor	ax, ax
	push	ax
	push	ax
	push	bp
	push	cx
	push	es
	push	bx

	mov	al, BYTE PTR [A(max_sec)]
	cmp	ax, si
	jbe	lba_mode_count_ok
	mov	ax, si
lba_mode_count_ok:
	push	ax
	push	0010h

	mov	si, sp
	mov	ah, 042h
	call	read_int13
	add	sp, 16

	pop	si
	ret

err_int13:
halt:
	hlt
	jmp	halt

chs_mode:
	push	dx
	mov	dx, bp

	mov	cx, ax
	mov	al, BYTE PTR [A(spt)]
	mul	BYTE PTR [A(heads)]
	xchg	ax, cx
	div	cx

	mov	cx, ax
	mov	ax, dx
	div	BYTE PTR [A(spt)]

	pop	dx
	mov	dh, al

	shl	ch, 6
	or	ch, ah
	xchg	cl, ch

chs_mode_cont:
	movzx	ax, BYTE PTR [A(spt)]
	sub	al, cl
	and	al, 03Fh
	cmp	al, BYTE PTR [A(max_sec)]
	jbe	chs_mode_count1
	mov	al, BYTE PTR [A(max_sec)]
chs_mode_count1:
	cmp	ax, si
	jbe	chs_mode_count2
	mov	ax, si
chs_mode_count2:
	mov	ah, 2
	inc	cl

read_int13:
	call	test_int13
	jc	read_int13_fail
	xor	ah, ah
	ret

test_int13:
	pusha
	stc
	int	13h
	sti

IFDEF DEBUG_INT13
	pushf
	pusha
	push	es

	shl	ax, 9
	add	bx, ax
	mov	cx, 5

test_int13_dump_loop:
	dec	bx
	dec	bx
	push	WORD PTR es:[bx]
	loop	test_int13_dump_loop

	push	WORD PTR es:[OFS_lba]

	mov	cl, 16
test_int13_dump_hex_loop:
	call	dump_hex
	loop	test_int13_dump_hex_loop

	popf
ENDIF

	popa
	ret

safe_int13:
	call	test_int13
	jc	err_int13
	ret

IFDEF DEBUG
dump_hex:
	pusha
	mov	bp, sp

	mov	ah, 0Eh
	xor	bx, bx
	mov	cx, 4
	mov	dx, WORD PTR [bp + 18]
dump_hex_loop:
	rol	dx, 4
	mov	al, dl
	and	al, 0Fh
	cmp	al, 10
	jb	dump_hex_digit
	sub	al, ('0' - 'A' + 10)
dump_hex_digit:
	add	al, '0'
	int	10h
	loop	dump_hex_loop
	mov	al, ' '
	int	10h

	popa
	ret	2
ENDIF

org OFS_max_sec
max_sec		db	63
lba		db	0
bootdrv		db	0
spt		db	63
heads		db	255
boot_base	dw	0
fb_magic	db	"FBBF"

org 0200h - 2
	dw	0AA55h

boot_size	dw	0
flags		dw	0
ver_major	db	VER_MAJOR
ver_minor	db	VER_MINOR
list_used	dw	0
list_size	dw	0
pri_size	dw	0
ext_size	dd	0

read_sectors_cont_1:
	push	ax
	shl	ax, 5
	push	bx
	mov	bx, es
	add	bx, ax
	mov	es, bx
	pop	bx
	pop	ax

read_sectors_cont_2:
	sub	si, ax
	jnz	read_sectors_more
	ret

read_sectors_more:
	cmp	dh, 0FFh
	jnz	read_sectors_more_chs
	add	cx, ax
	adc	bp, 0
	jmp	near ptr lba_mode_cont

read_sectors_more_chs:
	dec	cl
	add	al, cl
	and	al, 03Fh
	and	cl, 0C0h
	cmp	al, BYTE PTR [A(spt)]
	jz	read_sectors_next_head
	or	cl, al
	jmp	near ptr chs_mode_cont

read_sectors_next_head:
	inc	dh
	cmp	dh, BYTE PTR [A(heads)]
	jz	read_sectors_wrap_cylinder
	jmp	near ptr chs_mode_cont
read_sectors_wrap_cylinder:
	xor	dh, dh
	inc	ch
	jz	read_sectors_wrap_high_cylinder
	jmp	near ptr chs_mode_cont
read_sectors_wrap_high_cylinder:
	add	cl, 040h
	jmp	near ptr chs_mode_cont

fail:
	call	print_string
	jmp	near ptr halt

print_string:
	cld
	pusha
	mov	cx, 1
	mov	bx, WORD PTR cs:[bp - 6]

print_string_loop:
	lodsb
	or	al, al
	jz	print_string_done
	cmp	al, 10
	jz	print_string_tty
	cmp	al, 13
	jz	print_string_tty
	mov	ah, 9
	int	10h
print_string_tty:
	mov	ah, 0Eh
	int	10h
	jmp	print_string_loop

print_string_done:
	popa
	ret

copy_sectors:
	push	es
	push	ds

	push	ds
	pop	es
	lea	si, [bp - 56]
	mov	cx, WORD PTR [bp - 58]
	cmp	ch, 2
	jnz	copy_sectors_begin
	shl	ax, 9
	mov	cx, ax
	mov	al, 1

copy_sectors_begin:
	mov	ah, 087h

copy_sectors_loop:
	pusha
	shr	cx, 1
	int	15h
	popa

	jnc	copy_sectors_ok
	mov	si, A(err_int15)
	jmp	fail

copy_sectors_ok:
	add	WORD PTR [bp - 38], 512
	add	WORD PTR [bp - 30], cx
	jnc	copy_sectors_next
	inc	BYTE PTR [bp - 28]
	jnz	copy_sectors_next
	inc	BYTE PTR [bp - 25]
copy_sectors_next:
	dec	al
	jnz	copy_sectors_loop

	pop	WORD PTR [bp - 38]
	pop	es
	ret

check_file:
	mov	WORD PTR [bp - 8], si
	push	LIST_BUF_SEG
	pop	ds
	xor	si, si

check_file_loop:
	lodsw
	or	al, al
	jnz	check_file_item

	push	cs
	pop	ds
	stc
	ret

check_file_item:
	push	si
	add	si, 12
	mov	bx, WORD PTR cs:[bp - 8]

check_file_name_loop:
	lodsb
	mov	ah, al
	xor	al, BYTE PTR cs:[bx]
	jz	check_file_name_match_char
	cmp	al, 020h
	jnz	check_file_name_nomatch
check_file_name_match_char:
	inc	bx
	or	ah, ah
	jnz	check_file_name_loop
	mov	WORD PTR cs:[bp - 8], bx

check_file_name_nomatch:
	pop	si
	jz	check_file_found
	mov	al, BYTE PTR [si - 2]
	xor	ah, ah
	add	si, ax
	call	normalize_dssi
	jmp	check_file_loop

check_file_found:
	cmp	WORD PTR [si + 2], 0
	jnz	check_file_block_size
	mov	ax, WORD PTR [si]
	cmp	ax, WORD PTR cs:[A(pri_size)]

check_file_block_size:
	mov	cx, 512
	jae	check_file_sector_count
	dec	cx
	dec	cx

check_file_sector_count:
	push	dx
	mov	ax, WORD PTR [si + 4]
	mov	dx, WORD PTR [si + 6]
	div	cx
	or	dx, dx
	jz	check_file_sector_count_ok
	inc	ax
check_file_sector_count_ok:
	pop	dx

	push	ds
	push	cs
	pop	ds
	mov	WORD PTR [bp - 58], cx

	mov	bx, WORD PTR [bp - 60]
	or	bx, bx
	jz	check_file_no_progress

	mov	WORD PTR [bp - 62], bx
	mov	bx, si
	mov	si, A(loading_message)
	call	print_string
	lea	si, [bx + 12]
	pop	ds
	push	ds
	call	print_string
	push	cs
	pop	ds
	mov	si, bx

check_file_no_progress:
	pop	es
	clc
	ret

find_file:
	call	check_file
	jnc	find_file_done
	mov	si, A(err_no_file)
	jmp	fail
find_file_done:
	ret

load_file:
	call	find_file

load_data:
	push	es
	push	bp

	push	ax
	mov	ax, WORD PTR es:[si]
	mov	bp, WORD PTR es:[si + 2]
	pop	si

	xor	bx, bx
	push	DATA_BUF_SEG
	pop	es

	call	read_sectors

load_data_loop:
	mov	bx, bp
	pop	bp

	cmp	WORD PTR [bp - 60], 0
	jz	load_data_no_dot
	sub	WORD PTR [bp - 62], ax
	jnc	load_data_no_dot
	push	si
	mov	si, A(dot_message)
	call	print_string
	mov	si, WORD PTR [bp - 60]
	add	WORD PTR [bp - 62], si
	pop	si
load_data_no_dot:
	pusha
	call	copy_sectors
	popa

	push	bp
	mov	bp, bx
	xor	bx, bx
	call	read_sectors_cont_2
	or	si, si
	jnz	load_data_loop

	pop	bp
	pop	es
	ret

start_2000:
	mov	si, WORD PTR [A(boot_size)]
	and	BYTE PTR [A(max_sec)], 07Fh
	push	si

start_2000_load_loop:
	call	read_sectors_cont_1
	or	si, si
	jnz	start_2000_load_loop

	pop	ax
	push	ax

	shl	ax, 9
	add	ax, CODE_START + 0200h + 64

	mov	bp, ax
	mov	WORD PTR [bp - 6], COLOR_NORMAL
	mov	WORD PTR [bp - 60], si

	xor	ax, ax
	mov	es, ax
	mov	cx, 24
	push	di
	lea	di, [bp - 56]
	cld
	rep	stosw
	pop	di

	dec	ax
	mov	WORD PTR [bp - 40], ax
	mov	WORD PTR [bp - 32], ax
	mov	al, 093h
	mov	BYTE PTR [bp - 35], al
	mov	BYTE PTR [bp - 27], al

	pop	ax
	push	ax
	dec	ax
	mov	WORD PTR [bp - 38], CODE_START + 0400h
	mov	WORD PTR [bp - 30], CODE_START + 0400h - 2
	mov	WORD PTR [bp - 58], 510
	call	copy_sectors

	pop	ax

	pop	WORD PTR [bp - 4]
	pop	WORD PTR [bp - 2]

	jmp	start_2000_after_guard

org 0400h - 2

start_2000_after_guard:
	add	ax, WORD PTR [A(boot_base)]
	inc	ax
	xor	si, si
	push	LIST_BUF_SEG
	pop	es
	mov	WORD PTR es:[si], ax
	mov	WORD PTR es:[si + 2], si
	mov	WORD PTR [bp - 30], si

	add	BYTE PTR [bp - 28], LIST_BUF_SEG SHR 12
	mov	WORD PTR [bp - 30], si
	mov	BYTE PTR [bp - 36], DATA_BUF_SEG SHR 12
	mov	ax, WORD PTR [A(list_used)]
	call	load_data

	mov	WORD PTR [bp - 30], bp
	mov	BYTE PTR [bp - 28], 0

	mov	si, A(menu_file)
	push	si
	call	check_file
	pop	si
	jc	start_2000_default
	call	load_file
	jmp	parse_menu

start_2000_default:
	call	clear_dest
	mov	si, A(default_loader)
	jmp	boot_buldr

find_first_item:
	mov	si, bp
	jmp	find_item_start

find_next_item:
	mov	al, BYTE PTR [si - 2]
find_item_skip:
	xor	ah, ah
	add	si, ax

find_item_start:
	cld
	lodsw
	cmp	ah, cl
	jz	find_item_done
	or	al, al
	jnz	find_item_skip
	stc
find_item_done:
	ret

check_key:
	mov	ah, 1
	push	ax
	int	16h
	pop	ax
	jnz	check_key_read

	mov	ah, 011h
	push	ax
	int	16h
	pop	ax
	jnz	check_key_read
	xor	ax, ax
	ret

check_key_read:
	dec	ah
	int	16h
	ret

check_menu:
	call	check_key
	or	ax, ax
	jz	check_menu_none

	pusha
	mov	bx, ax
	mov	cl, FBM_TYPE_MENU
	call	find_first_item

check_menu_loop:
	jc	check_menu_not_found
	cmp	WORD PTR [si], bx
	jz	check_menu_found
	call	find_next_item
	jmp	check_menu_loop

check_menu_not_found:
	popa

check_menu_none:
	stc
	ret

check_menu_found:
	add	sp, 16
	ret

check_timeout:
	push	dx

	cmp	bl, 0FFh
	jnz	check_timeout_count

check_timeout_wait_forever:
	call	check_menu
	jnc	check_timeout_done
	hlt
	jmp	check_timeout_wait_forever

check_timeout_count:
	xor	ax, ax
	int	1Ah

	mov	al, 18
	mul	bl
	add	dx, ax
	adc	cx, 0
	mov	bx, dx
	mov	si, cx

check_timeout_loop:
	call	check_menu
	jnc	check_timeout_done

	xor	ax, ax
	int	1Ah

	or	al, al
	jz	check_timeout_no_midnight
	sub	bx, 00B0h
	sbb	si, 0018h
check_timeout_no_midnight:
	cmp	si, cx
	jnz	check_timeout_cmp_done
	cmp	bx, dx
check_timeout_cmp_done:
	jb	check_timeout_done

	hlt
	jmp	check_timeout_loop

check_timeout_done:
	pop	dx
	ret

parse_menu:
	mov	cl, FBM_TYPE_TIMEOUT
	call	find_first_item
	mov	bl, 0
	jc	parse_menu_no_timeout_item
	mov	bl, BYTE PTR [si]
parse_menu_no_timeout_item:

	call	check_menu
	jnc	boot_item
	cmp	ax, KEY_ESCAPE
	jnz	parse_menu_no_escape
	mov	bl, 0FFh
parse_menu_no_escape:

	or	bl, bl
	jz	no_timeout

	cld
	mov	si, bp

parse_menu_display_loop:
	lodsw
	or	al, al
	jz	parse_menu_display_done
	cmp	ah, FBM_TYPE_COLOR
	jnz	parse_menu_not_color
	mov	bh, BYTE PTR [si]
	mov	BYTE PTR [bp - 6], bh
parse_menu_not_color:
	cmp	ah, FBM_TYPE_TEXT
	jnz	parse_menu_next_display
	call	print_string
parse_menu_next_display:
	xor	ah, ah
	add	si, ax
	jmp	parse_menu_display_loop

parse_menu_display_done:
	mov	BYTE PTR [bp - 6], COLOR_NORMAL
	call	check_timeout
	jnc	boot_item

no_timeout:
	mov	cl, FBM_TYPE_DEFAULT
	call	find_first_item
	mov	bl, 0
	jc	no_timeout_no_default
	mov	bl, BYTE PTR [si]
no_timeout_no_default:

	mov	cl, FBM_TYPE_MENU
	call	find_first_item

no_timeout_default_loop:
	jc	no_timeout_no_menu
	sub	bl, 1
	jc	boot_item
	call	find_next_item
	jmp	no_timeout_default_loop

no_timeout_no_menu:
	mov	si, A(err_no_menu)
	jmp	fail

boot_item:
	call	clear_dest

	mov	al, BYTE PTR [si + 2]
	add	si, 3
	cmp	al, FBS_TYPE_BULDR
	jnz	boot_not_buldr

boot_buldr:
	mov	BYTE PTR [bp - 28], 2
	call	load_file
	call	setup_mbr
	db	0EAh
	dw	0
	dw	2000h

boot_not_buldr:
	cmp	al, FBS_TYPE_GRLDR
	jnz	boot_not_grldr

boot_grldr:
	mov	BYTE PTR [bp - 28], 2
	call	load_file
	call	setup_mbr
	mov	dl, 023h
	push	dx
	db	0EAh
	dw	0
	dw	2000h

boot_not_grldr:
	cmp	al, FBS_TYPE_SYSLINUX
	jnz	boot_not_syslinux

boot_syslinux:
	mov	WORD PTR [bp - 30], 7C00h
	call	load_file
	call	copy_bs
	call	setup_mbr
	db	0EAh
	dw	7C00h
	dw	0

boot_not_syslinux:
	cmp	al, FBS_TYPE_LINUX
	jnz	boot_not_linux

boot_linux:
	mov	WORD PTR [bp - 60], DOT_SIZE
	call	find_file
	push	ax
	mov	BYTE PTR [bp - 28], 9
	mov	ax, 1
	call	load_data_inc
	mov	bx, 9000h
	push	es
	mov	es, bx
	mov	al, BYTE PTR es:[01F1h]
	pop	es
	inc	ax
	shl	ax, 9
	mov	cx, ax
	add	ax, WORD PTR [bp - 58]
	dec	ax
	push	dx
	xor	dx, dx
	div	WORD PTR [bp - 58]
	pop	dx

	push	ax
	dec	ax
	call	load_data_inc
	inc	ax

	push	dx
	mul	WORD PTR [bp - 58]
	pop	dx
	sub	ax, cx

	mov	BYTE PTR [bp - 28], 10h
	mov	cx, 0
	xchg	cx, WORD PTR [bp - 30]

	jz	boot_linux_aligned
	push	WORD PTR [bp - 58]
	mov	WORD PTR [bp - 58], ax
	sub	cx, ax
	mov	WORD PTR [bp - 38], cx
	inc	BYTE PTR [bp - 36]
	push	si
	mov	ax, 1
	call	copy_sectors
	pop	si
	dec	BYTE PTR [bp - 36]
	pop	WORD PTR [bp - 58]
boot_linux_aligned:
	pop	cx
	pop	ax

	sub	ax, cx
	jz	boot_linux_kernel_done
	call	load_data_inc
boot_linux_kernel_done:

	call	print_newline

	mov	si, WORD PTR [bp - 8]
	cmp	BYTE PTR [si], 0
	jz	boot_linux_no_initrd
	call	clear_dest
	mov	BYTE PTR [bp - 25], 2
	push	bx
	call	find_file
	pop	bx
	push	bx
	push	es
	push	WORD PTR es:[si + 6]
	push	WORD PTR es:[si + 4]
	mov	es, bx
	mov	BYTE PTR es:[0218h + 3], 2
	pop	WORD PTR es:[021Ch]
	pop	WORD PTR es:[021Ch + 2]
	pop	es
	call	load_data
	call	print_newline

	pop	bx

	mov	si, WORD PTR [bp - 8]
	dec	si
boot_linux_no_initrd:
	inc	si

	mov	es, bx
	mov	WORD PTR es:[0228h], bx
	cld

boot_linux_cmd_loop:
	lodsb
	mov	BYTE PTR es:[bx], al
	inc	bx
	or	al, al
	jnz	boot_linux_cmd_loop

	mov	BYTE PTR es:[0210h], 7
	mov	WORD PTR es:[0224h], 9000h - 0200h
	or	BYTE PTR es:[0211h], 080h
	mov	BYTE PTR es:[0228h + 2], 9

	cli
	mov	bx, es

	mov	ss, bx
	mov	sp, bx

	mov	ds, bx
	mov	fs, bx
	mov	gs, bx

	db	0EAh
	dw	0
	dw	9020h

print_newline:
	mov	si, A(newline)
	call	print_string
	ret

clear_dest:
	xor	ax, ax
	mov	WORD PTR [bp - 30], ax
	mov	BYTE PTR [bp - 28], al
	mov	BYTE PTR [bp - 25], al
	ret

load_data_inc:
	pusha
	call	load_data
	popa
	add	WORD PTR es:[si], ax
	adc	WORD PTR es:[si + 2], 0
	ret

boot_not_linux:
	cmp	al, FBS_TYPE_MSDOS
	jnz	boot_not_msdos

boot_msdos:
	call	find_file
	mov	bx, DATA_BUF_SEG SHR 5
	sub	bx, ax
	push	ax
	push	ax
	mov	ax, bx
	shl	ax, 9
	mov	WORD PTR [bp - 30], ax
	mov	ax, bx
	shr	ax, 7
	mov	BYTE PTR [bp - 28], al
	pop	ax
	push	bx
	call	load_data
	call	copy_bs

	pop	bx
	pop	ax

	mov	si, A(iosys_trampoline_start)
	mov	cx, iosys_trampoline_end - iosys_trampoline_start
	mov	di, 0200h
	rep	movsb

	db	0EAh
	dw	0200h
	dw	DATA_BUF_SEG

boot_not_msdos:
	cmp	al, FBS_TYPE_FREEDOS
	jnz	boot_not_freedos

boot_freedos:
	mov	BYTE PTR [bp - 28], 2
	call	load_file

	mov	si, A(freedos_trampoline_start)
	mov	cx, freedos_trampoline_end - freedos_trampoline_start
	xor	di, di
	push	DATA_BUF_SEG
	pop	es
	rep	movsb
	db	0EAh
	dw	0
	dw	DATA_BUF_SEG

freedos_trampoline_start:
	push	2000h
	pop	ds
	push	60h
	pop	es
	xor	si, si
	mov	di, si
	mov	cx, 8000h
	rep	movsw
	xor	dh, dh
	mov	bx, dx
	xor	ax, ax
	mov	ds, ax
	mov	es, ax
	mov	ss, ax
	mov	sp, 400h
	db	0EAh
	dw	0
	dw	60h
freedos_trampoline_end:

boot_not_freedos:
	cmp	al, FBS_TYPE_CHAIN
	jnz	boot_no_type

boot_chain:
	mov	WORD PTR [bp - 30], 7C00h
	call	load_file
	call	setup_mbr
	db	0EAh
	dw	7C00h
	dw	0

boot_no_type:
	mov	si, A(err_no_type)
	jmp	fail

iosys_trampoline_start:
	mov	cx, es
	mov	ss, cx
	xor	sp, sp

	add	bx, 4
	sub	ax, 4
	shl	bx, 5

	mov	ds, bx
	push	70h
	pop	es

	mov	bx, ax

	xor	si, si
	xor	di, di

	push	dx

	cmp	WORD PTR [si], IOSYS_CM_MAGIC
	jz	iosys_expand

iosys_copy_loop:
	mov	cx, 256
	rep	movsw
	call	normalize_address
	dec	bx
	jnz	iosys_copy_loop

iosys_boot:
	pop	dx
	xor	ax, ax
	xor	bx, bx
	xor	bp, bp
	xor	di, di

	push	cs
	pop	ds

	mov	dh, 0F8h
	db	0EAh
	dw	0
	dw	70h

iosys_expand:
	push	ds
	lodsw
	push	si

	xor	bh, bh

iosys_expand_scan:
	lodsb
	mov	bl, al
	lodsw
	or	ax, ax
	jz	iosys_expand_scan_done
	add	ax, bx
	add	ax, bx
	add	si, ax
	call	normalize_address
	jmp	iosys_expand_scan

iosys_expand_scan_done:
	or	si, si
	jz	iosys_expand_same_seg
	mov	ax, ds
	inc	ax
	mov	ds, ax
iosys_expand_same_seg:
	xor	si, si
	lodsw
	cmp	ax, IOSYS_CM_MAGIC
	jnz	iosys_expand_error
	lodsw
	mov	WORD PTR cs:[iosys_expand_func - iosys_trampoline_start + 0200h], ax
	mov	WORD PTR cs:[iosys_expand_func - iosys_trampoline_start + 0200h + 2], ds

	pop	si
	pop	ds

iosys_expand_loop:
	call	normalize_address
	lodsb
	mov	bl, al
	lodsw
	or	ax, ax
	jz	iosys_boot
	or	bl, bl
	jnz	iosys_expand_compressed

	mov	cx, ax
	rep	movsb
	jmp	iosys_expand_loop

iosys_expand_compressed:
	lodsw
	mov	cx, ax
	shr	ax, 9
	and	cx, 01FFh
	jz	iosys_expand_sector_count_ok
	inc	ax
iosys_expand_sector_count_ok:
	mov	cx, ax
	lodsw
	lodsw
	lodsw
	xor	dx, dx

	db	09Ah
iosys_expand_func:
	dw	0, 0
	jc	iosys_expand_error

	dec	si
	jmp	iosys_expand_loop

iosys_expand_error:
	hlt
	jmp	iosys_expand_error

normalize_address:
	mov	ax, di
	and	di, 0Fh
	shr	ax, 4
	mov	cx, es
	add	ax, cx
	mov	es, ax

normalize_dssi:
	mov	ax, si
	and	si, 0Fh
	shr	ax, 4
	mov	cx, ds
	add	ax, cx
	mov	ds, ax
	ret

iosys_trampoline_end:

setup_mbr:
	push	ds
	pop	es
	mov	cx, 8
	mov	si, CODE_START + 01BEh
	mov	di, 0800h - 18
	push	di
	rep	movsw
	pop	si

	mov	es, WORD PTR [bp - 2]
	mov	di, WORD PTR [bp - 4]

	xor	dh, dh
	mov	BYTE PTR [A(bootdrv)], dl
	ret

copy_bs:
	pusha

	xor	di, di
	mov	ax, DATA_BUF_SEG
	mov	es, ax
	xor	bx, bx
	mov	si, 1

	mov	ax, WORD PTR [CODE_START + 01C6h]
	mov	bp, WORD PTR [CODE_START + 01C8h]

	push	bp
	push	ax

	call	read_sectors

	mov	cl, BYTE PTR [7C01h]
	xor	ch, ch

	mov	ax, WORD PTR [A(spt)]

	push	ds
	push	es
	pop	ds
	pop	es

	pop	WORD PTR [001Ch]
	pop	WORD PTR [001Eh]

	mov	BYTE PTR [0018h], al
	mov	BYTE PTR [001Ah], ah

	mov	si, 2
	mov	di, 7C02h

	cld
	rep	movsb

	push	ds
	push	es
	pop	ds
	pop	es

	popa
	ret

menu_file:
	db	FB_MENU_FILE, 0

default_loader:
	db	"buldr", 0

err_int15:
	db	"int15", 0

err_no_file:
	db	"no file", 0

err_no_menu:
	db	"no menu", 0

err_no_type:
	db	"no type", 0

loading_message:
	db	"Loading ", 0

dot_message:
	db	".", 0

newline:
	db	13, 10, 0

code_end:

end start
