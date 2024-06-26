
#include <syscalls.h>



using namespace myos;
using namespace myos::common;
using namespace myos::hardwarecommunication;

enum SYSCALLS
{
    SYS_FORK = 1,
    SYS_WAIT = 2,
    SYS_EXECVE = 3,
    SYS_PRINT = 4,
    SYS_FORK_EXEC = 5
};

SyscallHandler::SyscallHandler(InterruptManager* interruptManager, uint8_t InterruptNumber)
:    InterruptHandler(interruptManager, InterruptNumber  + interruptManager->HardwareInterruptOffset())
{
}

SyscallHandler::~SyscallHandler()
{
}

uint32_t fork(CPUState* cpu);
uint32_t fork2(CPUState* cpu);
void printf(char* str);
void printfHex32(uint32_t);
void printf(char*);
void wait(CPUState* cpu);
void execve(CPUState* cpu);

uint32_t SyscallHandler::HandleInterrupt(uint32_t esp)
{
    CPUState* cpu = (CPUState*)esp;
    
    if (cpu->eax == SYS_FORK) {
        cpu->ecx = fork(cpu);
        return InterruptHandler::HandleInterrupt((uint32_t) cpu);
    }
    else if (cpu->eax == SYS_WAIT) {
        wait(cpu);
        return InterruptHandler::HandleInterrupt((uint32_t) cpu);
    }
    else if (cpu->eax == SYS_EXECVE) {
        execve(cpu);
        cpu->eax = 0;
        cpu->eip = cpu->ebx;
        esp = (uint32_t) cpu;
    }
    else if (cpu->eax == SYS_PRINT) {
        printf((char*)cpu->ebx);
    }
    else if (cpu->eax == SYS_FORK_EXEC) {
        cpu->eax = fork2(cpu);
        return InterruptHandler::HandleInterrupt((uint32_t) cpu);
    }
    else {
        // default case, nothing to do
    }

    return esp;
}
