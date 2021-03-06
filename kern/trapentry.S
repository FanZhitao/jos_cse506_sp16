/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushq $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushq $0;							\
	pushq $(num);							\
	jmp _alltraps


/*
 * Lab 3: Your code here for generating entry points for the different traps.
   Find the PUSHA,POPA macros in kern/macro.h. Since amd64 doesn't support
   pusha,popa so you will find these macros handy.
 */
.set num, 64

// Generate trap handler, all calling _alltraps
.altmacro
.macro GEN_TRAP_HANDLERS n
	.if \n > 0
		GEN_TRAP_HANDLERS %(n-1)
	.endif

	// CPU push error code for exception
	//  13 - general protection
	.if \n == 13
		TRAPHANDLER(XTRPX_\n, \n)
	//  14 - page fault
	.elseif \n == 14
		TRAPHANDLER(XTRPX_\n, \n)
	.else
		TRAPHANDLER_NOEC(XTRPX_\n, \n)
	.endif
.endm

// Set entry
.macro SET_ENTRY name
	.quad \name
.endm

// Generate handler table
.altmacro
.macro GEN_TRAP_TABLE n
	.globl entrytable
	.align 2;	// align by 4 bytes
	entrytable:
	GEN_TRAP_TABLE_0 \n
.endm

.macro GEN_TRAP_TABLE_0 n
	.if \n > 0
		GEN_TRAP_TABLE_0 %(n-1)
	.endif
	SET_ENTRY XTRPX_\n
.endm


.text
	// Expand macro here!
	GEN_TRAP_HANDLERS %(num)


/*
 * Lab 3: Your code here for _alltraps
 *
 * Hint: Be sure to review the x64 calling convention from lab1
 *       for how to pass a pointer to the trapframe.
 */

_alltraps:

	// Prepare stack like TrapFrame
	//  1.Top: ss, rsp, eflags, cs, rip (old regs pushed by CPU)
	//  2.Mid: errorcode, trapno (pushed by TRAPHANDLER[_NOEC])
	//  3.Bottom: ds, es, rax, rbx ... r15 (pushed here)
	subq $16, %rsp
	mov %ds, 8(%rsp)
	mov %es, 0(%rsp)
	PUSHA

	// CPU only switch CS/SS as per what's in kernel's TSS,
	//  so we have to take care of DS/ES/FS/GS switch by ourselves
	mov $0x10, %eax
	mov %eax, %ds
	mov %eax, %es
	
	// Call by x64 convention
	mov %rsp, %rdi
	call trap

	// Should not return here...

.data
	GEN_TRAP_TABLE %(num)
