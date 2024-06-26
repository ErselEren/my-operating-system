 
#ifndef __MYOS__MULTITASKING_H
#define __MYOS__MULTITASKING_H

#include <common/types.h>
#include <gdt.h>

namespace myos
{
    struct CPUState
    {
        common::uint32_t eax;
        common::uint32_t ebx;
        common::uint32_t ecx;
        common::uint32_t edx;

        common::uint32_t esi;
        common::uint32_t edi;
        common::uint32_t ebp;

        common::uint32_t error;

        common::uint32_t eip;
        common::uint32_t cs;
        common::uint32_t eflags;
        common::uint32_t esp;
        common::uint32_t ss;        
    } __attribute__((packed));
    
    
    class Task
    {
    friend class TaskManager;
    private:
        common::uint8_t stack[4096]; // 4 KiB
        CPUState* cpustate;

        // process ID
        common::uint32_t pid;
    public:
        Task(GlobalDescriptorTable *gdt, void entrypoint());

        Task(GlobalDescriptorTable *gdt);

        Task();
        void setEntryPoint(void entrypoint());
        ~Task();

        // get entry point
        void* GetEntryPoint();

        // set the task
        void Set(Task* task);
    };
    
    // process states
    enum ProcessState {
        RUNNING,
        BLOCKED,
        READY,
        TERMINATED
    };

    // process class
    class Process {
    
    friend class TaskManager;
    
    private:
        // process ID
        common::uint32_t pid;

        // parent process ID
        common::uint32_t ppid;

        // process waiting for
        common::uint32_t waitPid;
    
        // child processes
        common::uint32_t children[256];

        // return value
        int* retval;

        // process state
        ProcessState state;

        // number of children
        common::uint32_t numChildren;
    public:
        Process(common::uint32_t pid, common::uint32_t ppid);
        Process();
        ~Process();
        bool IsChild(common::uint32_t pid);
        void AddChild(common::uint32_t pid);
        void RemoveChild(common::uint32_t pid);
        common::uint32_t GetPID();
        common::uint32_t GetPPID();
        int* GetReturnValue();
        ProcessState GetState();
        common::uint32_t GetNumChildren();

        // setters
        void SetPID(common::uint32_t pid);
        void SetPPID(common::uint32_t ppid);
        void SetReturnValue(int* retval);
        void SetState(ProcessState state);
        void SetNumChildren(common::uint32_t numChildren);
        common::uint32_t priority;
        common::uint32_t streak;

    };

    // inactive processes has 0 pid
    struct ProcessTable
    {
        Process processes[256];
        int numProcesses;
    };

    class TaskManager
    {
    private:
        Task tasks[256];
        int numTasks;
        int currentTask;
        int terminatedTaskCount;
        ProcessTable processTable;
    public:
        TaskManager(GlobalDescriptorTable *gdt);
        ~TaskManager();
        bool AddTask(Task* task);
        bool AddTaskWithPriority(Task* task, common::uint32_t priority);
        CPUState* Schedule(CPUState* cpustate);
        CPUState* OriginalSchedule(CPUState* cpustate);
        void PrintProcessTable();
        void Terminate(int* returnValue);
        common::uint32_t Fork_1(CPUState* cpustate);
        common::uint32_t Fork_2(CPUState* cpustate);
        void Exec(CPUState* cpustate);
        void Wait(CPUState* cpustate);
        common::uint32_t GetCurrentPid();

    };
}


#endif