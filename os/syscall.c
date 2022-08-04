#include "syscall.h"
#include "console.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d str = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

uint64 sys_read(int fd, uint64 va, uint64 len)
{
	debugf("sys_read fd = %d str = %x, len = %d", fd, va, len);
	if (fd != STDIN)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	for (int i = 0; i < len; ++i) {
		int c = consgetc();
		str[i] = c;
	}
	copyout(p->pagetable, va, str, len);
	return len;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_gettimeofday(uint64 val, int _tz)
{
	struct proc *p = curr_proc();
	uint64 cycle = get_cycle();
	TimeVal t;
	t.sec = cycle / CPU_FREQ;
	t.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	copyout(p->pagetable, val, (char *)&t, sizeof(TimeVal));
	return 0;
}
// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
int mmap(void* start, unsigned long long len, int port, int flag, int fd){
	uint64 n, va0;
	if((((port) & (~0x7)) != 0 )|| (((port) & (0x7)) == 0)){
		return -1;
	}
	va0 = PGROUNDDOWN((uint64)start);
	if(va0 != (uint64)start) return -1;
	port = port << 1;
	port = port | PTE_U;
	// printf("max page: %d \n", curr_proc()->max_page);
	// curr_proc()->max_page += 2;
	while(len > 0){
		n = PGSIZE;
		if(n > len) n = len;

		void* pa = kalloc();
		
		if(pa == (void*)0) return -1;
		int ret = mappages(curr_proc()->pagetable, va0, n, (uint64)pa, port );
		if(ret == -1) return -1;
		curr_proc()->max_page++;
		va0 += n;
		len -= n;
	}
	// printf("max page: %d \n", curr_proc()->max_page);
	return 0;
}
int munmap(void* start, unsigned long long len){
	uint64 n, va0;
	va0 = PGROUNDDOWN((uint64)start);
	int dofree = 1;
	int size = 1;
	while(len > 0){
		n = PGSIZE;
		if(n > len) n = len;
		if(walkaddr(curr_proc()->pagetable, va0) == 0){
			return -1;
		}
		uvmunmap(curr_proc()->pagetable, va0, size, dofree);
		curr_proc()->max_page--;
		va0 += n;
		len -= n;
	}


	return 0;
}


/*
* LAB1: you may need to define sys_task_info here
*/
int sys_task_info(TaskInfo *ti){
	TaskInfo temp;
	temp.status = Running;
	int curtime = (get_cycle() % CPU_FREQ) * 1000 / CPU_FREQ;
	temp.time = curtime- curr_proc()->time;
	for (int i = 0; i < MAX_SYSCALL_NUM; i++)
	{
		temp.syscall_times[i] = curr_proc()->syscall_times[i];
	}
	copyout(curr_proc()->pagetable, (uint64)ti, (char*)&temp,sizeof(TaskInfo));
	// printf("start time : %d, end time : %d, dtime: %d \n", curr_proc()->time, curtime, ti->time);
	return 0;
}

uint64 sys_getpid()
{
	return curr_proc()->pid;
}

uint64 sys_getppid()
{
	struct proc *p = curr_proc();
	return p->parent == NULL ? IDLE_PID : p->parent->pid;
}

uint64 sys_clone()
{
	debugf("fork!\n");
	return fork();
}

uint64 sys_exec(uint64 va)
{
	struct proc *p = curr_proc();
	char name[200];
	copyinstr(p->pagetable, name, va, 200);
	debugf("sys_exec %s\n", name);
	return exec(name);
}

uint64 sys_wait(int pid, uint64 va)
{
	struct proc *p = curr_proc();
	int *code = (int *)useraddr(p->pagetable, va);
	return wait(pid, code);
}

uint64 sys_spawn(uint64 va)
{
	// TODO: your job is to complete the sys call
	return -1;
}

uint64 sys_set_priority(long long prio){
    // TODO: your job is to complete the sys call
    return -1;
}


extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	/*
	* LAB1: you may need to update syscall counter for task info here
	*/
	curr_proc()->syscall_times[id]++; // update syscall nums
	
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_read:
		ret = sys_read(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday(args[0], args[1]);
		break;
	case SYS_getpid:
		ret = sys_getpid();
		break;
	case SYS_getppid:
		ret = sys_getppid();
		break;
	case SYS_clone: // SYS_fork
		ret = sys_clone();
		break;
	case SYS_execve:
		ret = sys_exec(args[0]);
		break;
	case SYS_wait4:
		ret = sys_wait(args[0], args[1]);
		break;
	case SYS_spawn:
		ret = sys_spawn(args[0]);
		break;
	case SYS_mmap:
		ret = mmap((void*)args[0], args[1], args[2],args[3],args[4]);
		break;
	case SYS_munmap:
		ret = munmap((void*)args[0], args[1]);
		break;	
	/*
	* LAB1: you may need to add SYS_taskinfo case here
	*/
	case SYS_task_info:
		ret = sys_task_info((TaskInfo *)args[0]); // argments need to be decide
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
