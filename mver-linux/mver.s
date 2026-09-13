# mver.s -- the Malaise version manager, Linux edition, AT&T-syntax x86-64.
#
# Manages the version of the Malaise toolchain. There is one version. It is
# 0.9. Every other version resolves to 0.9, with a reason -- same design as
# mver/mver.applescript (macOS) and mver-win/mver.ps1 (Windows). This one
# talks to the kernel directly: no libc, no _start argument-handling
# convention beyond what the kernel itself puts on the stack at entry
# (argc, argv[], envp[]), no dependency this binary carries with it beyond
# the Linux x86-64 syscall table. See mver-linux/README.md for why that
# makes it exactly as Linux-only as mver/ is macOS-only, and more so.
#
# Assembled with `as --64`, linked with `ld` -- no crt0, no libc, `_start`
# is the real ELF entry point. Every register clobbered by a `call` here is
# caller-saved by convention (there are no callee-saved registers in this
# file); state that must survive a call lives in the .bss globals below,
# not in a register.

.text
.global _start

# ---- helpers -----------------------------------------------------------

# strlen: rdi = ptr -> rax = length. does not modify rdi.
strlen:
    xor %rax, %rax
.Lsl_loop:
    cmpb $0, (%rdi,%rax)
    je .Lsl_done
    inc %rax
    jmp .Lsl_loop
.Lsl_done:
    ret

# write_str: rdi = null-terminated ptr -> writes to stdout.
write_str:
    call strlen
    mov %rdi, %rsi
    mov %rax, %rdx
    mov $1, %rdi
    mov $1, %rax
    syscall
    ret

# streq: rdi, rsi = null-terminated ptrs -> rax = 1 if equal else 0.
streq:
.Lseq_loop:
    movb (%rdi), %al
    cmpb (%rsi), %al
    jne .Lseq_ne
    testb %al, %al
    jz .Lseq_eq
    inc %rdi
    inc %rsi
    jmp .Lseq_loop
.Lseq_ne:
    xor %eax, %eax
    ret
.Lseq_eq:
    mov $1, %eax
    ret

# append_str: rdi = dest write point, rsi = null-terminated src -> copies
# src (incl. terminator) to dest; rax = pointer to the new terminator.
append_str:
.Lap_loop:
    movb (%rsi), %al
    movb %al, (%rdi)
    testb %al, %al
    jz .Lap_done
    inc %rsi
    inc %rdi
    jmp .Lap_loop
.Lap_done:
    mov %rdi, %rax
    ret

# find_env: rdi = name ptr (no '=') -> rax = ptr to value, or 0.
find_env:
    mov envpp(%rip), %r8
.Lfe_outer:
    mov (%r8), %r9
    test %r9, %r9
    jz .Lfe_notfound
    mov %rdi, %r10
    mov %r9, %r11
.Lfe_cmp:
    movb (%r10), %al
    testb %al, %al
    jz .Lfe_nameend
    cmpb (%r11), %al
    jne .Lfe_next
    inc %r10
    inc %r11
    jmp .Lfe_cmp
.Lfe_nameend:
    cmpb $'=', (%r11)
    jne .Lfe_next
    lea 1(%r11), %rax
    ret
.Lfe_next:
    add $8, %r8
    jmp .Lfe_outer
.Lfe_notfound:
    xor %eax, %eax
    ret

# find_home: -> rax = HOME value ptr, or "" if unset.
find_home:
    lea s_HOME(%rip), %rdi
    call find_env
    test %rax, %rax
    jnz .Lfh_ok
    lea empty_str(%rip), %rax
.Lfh_ok:
    ret

# getcwd_str: fills cwd_buf (kernel null-terminates it) -> rax = ptr, or "."
getcwd_str:
    lea cwd_buf(%rip), %rdi
    mov $4096, %rsi
    mov $79, %rax
    syscall
    cmp $0, %rax
    jg .Lgc_ok
    lea dot_str(%rip), %rax
    ret
.Lgc_ok:
    lea cwd_buf(%rip), %rax
    ret

# mkdir_dir: rdi = path -> mkdir(path, 0755); ignores the result, same as
# every other mver implementation's "create the directory if needed".
mkdir_dir:
    mov $0x1ED, %esi
    mov $83, %rax
    syscall
    ret

# read_file: rdi = path -> rax = ptr to trimmed contents in file_buf, or 0
# if the file does not exist (or open failed for any other reason: this
# tool does not distinguish, same as the interpreter's OPEN, invariant "post-spec").
read_file:
    xor %esi, %esi
    xor %edx, %edx
    mov $2, %rax
    syscall
    cmp $0, %rax
    jl .Lrf_fail
    mov %rax, %r8
    lea file_buf(%rip), %rsi
    mov $255, %rdx
    mov %r8, %rdi
    xor %eax, %eax
    syscall
    mov %rax, %r9
    mov %r8, %rdi
    mov $3, %rax
    syscall
    lea file_buf(%rip), %rsi
.Ltrim:
    test %r9, %r9
    jz .Ltrimdone
    movb -1(%rsi,%r9), %al
    cmpb $10, %al
    je .Ltrimdec
    cmpb $13, %al
    je .Ltrimdec
    cmpb $' ', %al
    je .Ltrimdec
    jmp .Ltrimdone
.Ltrimdec:
    dec %r9
    jmp .Ltrim
.Ltrimdone:
    movb $0, (%rsi,%r9)
    lea file_buf(%rip), %rax
    ret
.Lrf_fail:
    xor %eax, %eax
    ret

# write_file: rdi = path, rsi = null-terminated content -> writes it,
# creating/truncating the file. No error path: OPEN in this ecosystem
# never fails outward (invariant, "post-spec additions").
write_file:
    mov %rsi, %r9
    mov $0x241, %esi
    mov $0x1A4, %edx
    mov $2, %rax
    syscall
    cmp $0, %rax
    jl .Lwf_done
    mov %rax, %r8
    mov %r9, %rdi
    call strlen
    mov %rax, %rdx
    mov %r9, %rsi
    mov %r8, %rdi
    mov $1, %rax
    syscall
    mov %r8, %rdi
    mov $3, %rax
    syscall
.Lwf_done:
    ret

# dirname_of: rdi = a path (typically argv[0]) -> writes the directory part
# (no trailing slash) into path_buf; rax = ptr to the new terminator. No
# slash in the input means path_buf becomes ".".
dirname_of:
    mov %rdi, %rsi
    mov $-1, %r9
    xor %r8, %r8
.Ldn_scan:
    movb (%rsi,%r8), %al
    testb %al, %al
    jz .Ldn_scandone
    cmpb $'/', %al
    jne .Ldn_next
    mov %r8, %r9
.Ldn_next:
    inc %r8
    jmp .Ldn_scan
.Ldn_scandone:
    cmp $-1, %r9
    jne .Ldn_hasslash
    lea path_buf(%rip), %rdi
    lea dot_str(%rip), %rsi
    call append_str
    ret
.Ldn_hasslash:
    lea path_buf(%rip), %rdi
    xor %r10, %r10
.Ldn_copy:
    cmp %r10, %r9
    je .Ldn_copydone
    movb (%rsi,%r10), %al
    movb %al, (%rdi,%r10)
    inc %r10
    jmp .Ldn_copy
.Ldn_copydone:
    movb $0, (%rdi,%r10)
    lea (%rdi,%r10), %rax
    ret

# find_source: -> rax = ptr to a descriptor of where the version was read
# from (env var name, a file path, or "default"). Always ends at 0.9
# regardless, same as resolveVersion in mver.applescript; this port does
# not reproduce that one's "X said <other value>, using 0.9" flourish --
# see mver-linux/README.md.
find_source:
    lea s_MVER_VERSION(%rip), %rdi
    call find_env
    test %rax, %rax
    jz .Lfs_local
    lea s_MVER_VERSION(%rip), %rax
    ret
.Lfs_local:
    call getcwd_str
    lea path_buf(%rip), %rdi
    mov %rax, %rsi
    call append_str
    mov %rax, %rdi
    lea suf_localver(%rip), %rsi
    call append_str
    lea path_buf(%rip), %rdi
    call read_file
    test %rax, %rax
    jz .Lfs_global
    lea path_buf(%rip), %rax
    ret
.Lfs_global:
    call find_home
    mov %rax, home_ptr(%rip)
    lea path_buf(%rip), %rdi
    mov home_ptr(%rip), %rsi
    call append_str
    mov %rax, %rdi
    lea suf_dotmver(%rip), %rsi
    call append_str
    mov %rax, %rdi
    lea suf_version(%rip), %rsi
    call append_str
    lea path_buf(%rip), %rdi
    call read_file
    test %rax, %rax
    jz .Lfs_default
    lea path_buf(%rip), %rax
    ret
.Lfs_default:
    lea s_default(%rip), %rax
    ret

# resolve_reason: reads arg2_ptr -> rax = ptr to the reason string.
resolve_reason:
    mov arg2_ptr(%rip), %rdi
    lea v_10(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_10
    mov arg2_ptr(%rip), %rdi
    lea v_1(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_10
    mov arg2_ptr(%rip), %rdi
    lea v_2(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_2
    mov arg2_ptr(%rip), %rdi
    lea v_2_0(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_2
    mov arg2_ptr(%rip), %rdi
    lea v_3(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_3
    mov arg2_ptr(%rip), %rdi
    lea v_3_0(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_3
    mov arg2_ptr(%rip), %rdi
    lea v_4(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_4
    mov arg2_ptr(%rip), %rdi
    lea v_4_0(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_4
    mov arg2_ptr(%rip), %rdi
    lea v_7(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_7
    mov arg2_ptr(%rip), %rdi
    lea v_7_0(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_7
    mov arg2_ptr(%rip), %rdi
    lea v_latest(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_latest
    mov arg2_ptr(%rip), %rdi
    lea v_system(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Rr_system
    lea reason_default(%rip), %rax
    ret
.Rr_10:
    lea reason_10(%rip), %rax
    ret
.Rr_2:
    lea reason_2(%rip), %rax
    ret
.Rr_3:
    lea reason_3(%rip), %rax
    ret
.Rr_4:
    lea reason_4(%rip), %rax
    ret
.Rr_7:
    lea reason_7(%rip), %rax
    ret
.Rr_latest:
    lea reason_latest(%rip), %rax
    ret
.Rr_system:
    lea reason_system(%rip), %rax
    ret

# ---- entry point --------------------------------------------------------

_start:
    mov (%rsp), %rax
    mov %rax, argc(%rip)
    lea 8(%rsp), %rbx
    mov %rbx, argvp(%rip)
    lea 8(%rbx,%rax,8), %rcx
    mov %rcx, envpp(%rip)

    cmp $2, %rax
    jl .Lnoargs
    mov argvp(%rip), %rbx
    mov 8(%rbx), %rdx
    mov %rdx, cmd_ptr(%rip)
    jmp .Lhaveargs
.Lnoargs:
    lea default_cmd(%rip), %rdx
    mov %rdx, cmd_ptr(%rip)
.Lhaveargs:
    mov argc(%rip), %rax
    cmp $3, %rax
    jl .Lnoarg2
    mov argvp(%rip), %rbx
    mov 16(%rbx), %rdx
    mov %rdx, arg2_ptr(%rip)
    jmp .Ldispatch
.Lnoarg2:
    movq $0, arg2_ptr(%rip)
.Ldispatch:
    lea banner(%rip), %rdi
    call write_str

    mov cmd_ptr(%rip), %rdi
    lea s_version(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_version

    mov cmd_ptr(%rip), %rdi
    lea s_versions(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_versions

    mov cmd_ptr(%rip), %rdi
    lea s_install(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_install

    mov cmd_ptr(%rip), %rdi
    lea s_uninstall(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_uninstall

    mov cmd_ptr(%rip), %rdi
    lea s_remove(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_uninstall

    mov cmd_ptr(%rip), %rdi
    lea s_global(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_global

    mov cmd_ptr(%rip), %rdi
    lea s_local(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_local

    mov cmd_ptr(%rip), %rdi
    lea s_shell(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_shell

    mov cmd_ptr(%rip), %rdi
    lea s_rehash(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_rehash

    mov cmd_ptr(%rip), %rdi
    lea s_which(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_which

    mov cmd_ptr(%rip), %rdi
    lea s_init(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_init

    mov cmd_ptr(%rip), %rdi
    lea s_help(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_help

    mov cmd_ptr(%rip), %rdi
    lea s_help2(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_help

    mov cmd_ptr(%rip), %rdi
    lea s_help3(%rip), %rsi
    call streq
    test %rax, %rax
    jnz do_help

    jmp do_unknown

do_version:
    call find_source
    mov %rax, source_ptr(%rip)
    lea s_0_9(%rip), %rdi
    call write_str
    lea m_paren_open(%rip), %rdi
    call write_str
    mov source_ptr(%rip), %rdi
    call write_str
    lea m_paren_close_nl(%rip), %rdi
    call write_str
    jmp finish

do_versions:
    call find_source
    mov %rax, source_ptr(%rip)
    lea m_star_09(%rip), %rdi
    call write_str
    mov source_ptr(%rip), %rdi
    call write_str
    lea m_versions_rest(%rip), %rdi
    call write_str
    jmp finish

do_install:
    mov arg2_ptr(%rip), %rax
    test %rax, %rax
    jnz .Linst_have
    lea m_install_none(%rip), %rdi
    call write_str
    jmp finish
.Linst_have:
    mov %rax, %rdi
    lea s_0_9(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Linst_already
    mov arg2_ptr(%rip), %rdi
    call write_str
    lea m_is(%rip), %rdi
    call write_str
    call resolve_reason
    mov %rax, %rdi
    call write_str
    lea m_period_nl(%rip), %rdi
    call write_str
    lea m_resolving(%rip), %rdi
    call write_str
    jmp finish
.Linst_already:
    lea m_already(%rip), %rdi
    call write_str
    jmp finish

do_uninstall:
    lea m_refuse1(%rip), %rdi
    call write_str
    lea m_refuse2(%rip), %rdi
    call write_str
    jmp finish

do_global:
    call find_home
    mov %rax, home_ptr(%rip)
    lea path_buf(%rip), %rdi
    mov home_ptr(%rip), %rsi
    call append_str
    mov %rax, %rdi
    lea suf_dotmver(%rip), %rsi
    call append_str
    mov %rax, path_end(%rip)
    lea path_buf(%rip), %rdi
    call mkdir_dir
    mov path_end(%rip), %rdi
    lea suf_version(%rip), %rsi
    call append_str
    lea path_buf(%rip), %rdi
    lea s_0_9nl(%rip), %rsi
    call write_file
    lea m_global_set(%rip), %rdi
    call write_str
    lea path_buf(%rip), %rdi
    call write_str
    lea m_close_paren_period(%rip), %rdi
    call write_str
    mov arg2_ptr(%rip), %rax
    test %rax, %rax
    jz .Lg_done
    mov %rax, %rdi
    lea s_0_9(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Lg_done
    lea m_note1(%rip), %rdi
    call write_str
    mov arg2_ptr(%rip), %rdi
    call write_str
    lea m_note2_global(%rip), %rdi
    call write_str
.Lg_done:
    jmp finish

do_local:
    call getcwd_str
    lea path_buf(%rip), %rdi
    mov %rax, %rsi
    call append_str
    mov %rax, %rdi
    lea suf_localver(%rip), %rsi
    call append_str
    lea path_buf(%rip), %rdi
    lea s_0_9nl(%rip), %rsi
    call write_file
    lea m_local_set(%rip), %rdi
    call write_str
    lea path_buf(%rip), %rdi
    call write_str
    lea m_close_paren_period(%rip), %rdi
    call write_str
    mov arg2_ptr(%rip), %rax
    test %rax, %rax
    jz .Ll_done
    mov %rax, %rdi
    lea s_0_9(%rip), %rsi
    call streq
    test %rax, %rax
    jnz .Ll_done
    lea m_note1(%rip), %rdi
    call write_str
    mov arg2_ptr(%rip), %rdi
    call write_str
    lea m_note2_local(%rip), %rdi
    call write_str
.Ll_done:
    jmp finish

do_shell:
    lea m_shell(%rip), %rdi
    call write_str
    jmp finish

do_rehash:
    lea ts(%rip), %rdi
    xor %esi, %esi
    mov $35, %rax
    syscall
    lea m_rehash(%rip), %rdi
    call write_str
    jmp finish

do_which:
    mov argvp(%rip), %rbx
    mov (%rbx), %rdi
    call dirname_of
    mov %rax, %rdi
    lea suf_interp(%rip), %rsi
    call append_str
    lea path_buf(%rip), %rdi
    call write_str
    lea m_nl(%rip), %rdi
    call write_str
    jmp finish

do_init:
    call find_home
    mov %rax, home_ptr(%rip)
    lea m_init1(%rip), %rdi
    call write_str
    mov home_ptr(%rip), %rdi
    call write_str
    lea m_init_shims(%rip), %rdi
    call write_str
    mov argvp(%rip), %rbx
    mov (%rbx), %rdi
    call dirname_of
    mov %rax, %rdi
    lea suf_interp(%rip), %rsi
    call append_str
    lea m_init2(%rip), %rdi
    call write_str
    lea path_buf(%rip), %rdi
    call write_str
    lea m_init3(%rip), %rdi
    call write_str
    jmp finish

do_help:
    lea m_help(%rip), %rdi
    call write_str
    jmp finish

do_unknown:
    lea m_unknown1(%rip), %rdi
    call write_str
    mov cmd_ptr(%rip), %rdi
    call write_str
    lea m_unknown2(%rip), %rdi
    call write_str
    jmp finish

finish:
    mov $60, %rax
    mov $1, %rdi
    syscall

# ---- data ----------------------------------------------------------------

.data
ts:
    .quad 0
    .quad 300000000

.section .rodata

banner:            .asciz "mver: the Malaise version manager. the version is 0.9.\n"
default_cmd:       .asciz "version"

s_version:         .asciz "version"
s_versions:        .asciz "versions"
s_install:         .asciz "install"
s_uninstall:       .asciz "uninstall"
s_remove:          .asciz "remove"
s_global:          .asciz "global"
s_local:           .asciz "local"
s_shell:           .asciz "shell"
s_rehash:          .asciz "rehash"
s_which:           .asciz "which"
s_init:            .asciz "init"
s_help:            .asciz "help"
s_help2:           .asciz "--help"
s_help3:           .asciz "-h"

s_0_9:             .asciz "0.9"
s_0_9nl:           .asciz "0.9\n"
s_default:         .asciz "default"
s_HOME:            .asciz "HOME"
s_MVER_VERSION:    .asciz "MVER_VERSION"
empty_str:         .asciz ""
dot_str:           .asciz "."

suf_localver:      .asciz "/.mver-version"
suf_dotmver:       .asciz "/.mver"
suf_version:       .asciz "/version"
suf_interp:        .asciz "/../interpreter/malaise"

v_10:              .asciz "1.0"
v_1:               .asciz "1"
v_2:               .asciz "2"
v_2_0:             .asciz "2.0"
v_3:               .asciz "3"
v_3_0:             .asciz "3.0"
v_4:               .asciz "4"
v_4_0:             .asciz "4.0"
v_7:               .asciz "7"
v_7_0:             .asciz "7.0"
v_latest:          .asciz "latest"
v_system:          .asciz "system"

reason_10:         .asciz "postponed (RFC-0001 and everything downstream of it)"
reason_2:          .asciz "skipped; the version number has always been 0.9"
reason_3:          .asciz "postponed (it removes sigils and keeps January 0 1900)"
reason_4:          .asciz "a documentation target, not a release (see mdoc)"
reason_7:          .asciz "what mdoc believes is current; mdoc is one tool"
reason_latest:     .asciz "0.9; it is also the earliest"
reason_system:     .asciz "0.9; there is no system Malaise"
reason_default:    .asciz "not a released version; the released version is 0.9"

m_paren_open:          .asciz "  ("
m_paren_close_nl:      .asciz ")\n"
m_star_09:             .asciz "* 0.9     set by "
m_versions_rest:       .asciz "\n  1.0    (postponed: RFC-0001 and everything downstream of it)\n  3      (postponed: removes sigils, keeps January 0 1900)\n  4      (documentation target; mdoc compiles for this)\n  7      (mdoc reports this as current; mdoc is one tool)\n  system (0.9; there is no system Malaise, so this is 0.9 too)\n"
m_install_none:        .asciz "mver: install which version? there is one: 0.9.\n"
m_is:                  .asciz " is "
m_period_nl:           .asciz ".\n"
m_resolving:           .asciz "resolving to 0.9 and installing that.\n"
m_already:             .asciz "0.9 is already installed. it is the only version. it has always been the only version.\n"
m_refuse1:             .asciz "mver: refusing. 0.9 is the only version; removing it would leave zero,\n"
m_refuse2:             .asciz "and a literal zero prints E_MALAISE_ZERO (spec 2.1). the toolchain stays at 0.9.\n"
m_global_set:          .asciz "mver: global version set to 0.9 ("
m_local_set:           .asciz "mver: local version set to 0.9 ("
m_close_paren_period:  .asciz ").\n"
m_note1:               .asciz "(you asked for "
m_note2_global:        .asciz "; your choice is on file. it says 0.9.)\n"
m_note2_local:         .asciz "; recorded as 0.9.)\n"
m_shell:               .asciz "mver: set MVER_VERSION=0.9 in your shell. any other value is read at resolve time and then ignored.\n"
m_rehash:              .asciz "mver: rehashed. the shims directory contains one shim. it is unchanged.\n"
m_nl:                  .asciz "\n"
m_init1:               .asciz "# add to your shell profile, then restart your shell:\nexport PATH=\""
m_init_shims:          .asciz "/.mver/shims:$PATH\"\n"
m_init2:               .asciz "# the shim forwards to "
m_init3:               .asciz ", 0 ms faster than calling it directly.\n"
m_unknown1:            .asciz "mver: unknown command '"
m_unknown2:            .asciz "'. try: mver help.\n"
m_help:                .asciz "usage: mver <command>\n  version                  the resolved version and where it came from\n  versions                 every version; one is usable\n  install <v>              resolves <v> to 0.9, installs 0.9\n  uninstall <v>            refused (zero versions is an error)\n  global|local|shell <v>   set the version (to 0.9) at that scope\n  which                    path to the interpreter\n  rehash                   does nothing, briefly\n  init                     PATH snippet for the shims directory\n"

.bss
.lcomm argc, 8
.lcomm argvp, 8
.lcomm envpp, 8
.lcomm cmd_ptr, 8
.lcomm arg2_ptr, 8
.lcomm home_ptr, 8
.lcomm source_ptr, 8
.lcomm path_end, 8
.lcomm path_buf, 4096
.lcomm cwd_buf, 4096
.lcomm file_buf, 4096
