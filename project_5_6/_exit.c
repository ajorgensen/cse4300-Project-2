#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <array.h>

void _exit(int exitCode)
{

	//kprintf("Exiting thread with code %d\n", exitCode);
	*(curthread->t_exit) = exitCode;
	thread_exit();
}
