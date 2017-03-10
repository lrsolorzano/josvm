/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>



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

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
   Find the PUSHA,POPA macros in kern/macro.h. Since amd64 doesn't support
   pusha,popa so you will find these macros handy.
 */
TRAPHANDLER_NOEC(XX_divide_handler, T_DIVIDE)
TRAPHANDLER_NOEC(XX_debug_handler, T_DEBUG)
TRAPHANDLER_NOEC(XX_nmi_handler, T_NMI)
TRAPHANDLER_NOEC(XX_brkpt_handler, T_BRKPT)
TRAPHANDLER_NOEC(XX_oflow_handler, T_OFLOW)
TRAPHANDLER_NOEC(XX_bound_handler, T_BOUND)
TRAPHANDLER_NOEC(XX_illop_handler, T_ILLOP)
TRAPHANDLER_NOEC(XX_device_handler, T_DEVICE)
TRAPHANDLER(XX_dblflt_handler, T_DBLFLT)
TRAPHANDLER(XX_tss_handler, T_TSS)
TRAPHANDLER(XX_segnp_handler, T_SEGNP)
TRAPHANDLER(XX_stack_handler, T_STACK)
TRAPHANDLER(XX_gpflt_handler, T_GPFLT)
TRAPHANDLER(XX_pgflt_handler, T_PGFLT)
TRAPHANDLER_NOEC(XX_fperr_handler, T_FPERR)
TRAPHANDLER(XX_align_handler, T_ALIGN)
TRAPHANDLER_NOEC(XX_mchk_handler, T_MCHK)
TRAPHANDLER_NOEC(XX_simderr_handler, T_SIMDERR)
TRAPHANDLER_NOEC(XX_syscall_handler, T_SYSCALL)
TRAPHANDLER_NOEC(XX_default_handler, T_DEFAULT)

/*
 * Lab 3: Your code here for _alltraps
 *
 * Hint: Be sure to review the x64 calling convention from lab1
 *       for how to pass a pointer to the trapframe.
 */

_alltraps:
	subq $8, %rsp;
	movw %ds, 0(%rsp);
	subq $8, %rsp;
	movw %es, 0(%rsp);
	PUSHA
	movw $GD_KD, %ax;
	movw %ax, %es;
	movw %ax, %ds;
	movq %rsp, %rdi;
	call trap  
// can trap return? should we do something here?

