
#include <multitasking.h>

using namespace myos;
using namespace myos::common;

void printf(char* str);
void printfHex32(uint32_t);
void printfDec(int);

// default return value
int default_val = 0;

#define PRINT_FLAG

#define DEFAULT_SCHEDULE
//#define THIRD_STRATEGY_PRIORITY_SCHEDULE
//#define DYNAMIC_PRIORITY_SCHEDULE


Task::Task(GlobalDescriptorTable *gdt, void entrypoint())
{
    cpustate = (CPUState*)(stack + 4096 - sizeof(CPUState));
    
    cpustate -> eax = 0;
    cpustate -> ebx = 0;
    cpustate -> ecx = 0;
    cpustate -> edx = 0;

    cpustate -> esi = 0;
    cpustate -> edi = 0;
    cpustate -> ebp = 0;
    
    cpustate -> eip = (uint32_t)entrypoint;
    cpustate -> cs = gdt->CodeSegmentSelector();

    cpustate -> eflags = 0x202;

    pid = 0;
}

Task::Task(GlobalDescriptorTable *gdt)
{
    cpustate = (CPUState*)(stack + 4096 - sizeof(CPUState));
    
    cpustate -> eax = 0;
    cpustate -> ebx = 0;
    cpustate -> ecx = 0;
    cpustate -> edx = 0;

    cpustate -> esi = 0;
    cpustate -> edi = 0;
    cpustate -> ebp = 0;
    
    cpustate -> eip = 0;
    cpustate -> cs = gdt->CodeSegmentSelector();

    cpustate -> eflags = 0x202;

    pid = 0;
}

Task::Task()
{
    cpustate = (CPUState*)(stack + 4096 - sizeof(CPUState));
    
    cpustate -> eax = 0;
    cpustate -> ebx = 0;
    cpustate -> ecx = 0;
    cpustate -> edx = 0;

    cpustate -> esi = 0;
    cpustate -> edi = 0;
    cpustate -> ebp = 0;
    
    cpustate -> eip = 0;
    cpustate -> cs = 0;

    cpustate -> eflags = 0x202;

    pid = 0;
}

Task::~Task()
{
}

TaskManager::TaskManager(GlobalDescriptorTable *gdt)
{
    numTasks = 0;
    currentTask = -1;
    terminatedTaskCount = 0;
    processTable.numProcesses = 0;
    for(int i = 0; i < 256; i++)
    {
        tasks[i].cpustate -> cs = gdt->CodeSegmentSelector();
    }
}

TaskManager::~TaskManager()
{
}

bool TaskManager::AddTask(Task* task)
{
    if(numTasks >= 256)
        return false;

    for (int i = 1; i < 256; i++)
    {
        if (processTable.processes[i].GetPID() == 0)
        {
            task->pid = i;
            break;
        }
    }

    Process* process = &(processTable.processes[task->pid]);
    
    process->SetPID(task->pid);
    process->SetPPID(1);
    process->SetState(READY);
    process->SetNumChildren(0);

    if (task->pid != 1)
    {
        Process* parent = &(processTable.processes[process->GetPPID()]);
        parent->AddChild(task->pid);
    }

    tasks[numTasks++].Set(task);
    return true;
}

bool TaskManager::AddTaskWithPriority(Task* task, common::uint32_t priority)
{
    if(numTasks >= 256)
        return false;

    for (int i = 1; i < 256; i++)
    {
        if (processTable.processes[i].GetPID() == 0)
        {
            task->pid = i;
            break;
        }
    }

    Process* addedProcess = &(processTable.processes[task->pid]);
    
    addedProcess->SetPID(task->pid);
    addedProcess->SetPPID(1);
    addedProcess->SetState(READY);
    addedProcess->SetNumChildren(0);
    addedProcess->priority = priority;
    addedProcess->streak = 0;

    if (task->pid != 1)
    {
        Process* parent = &(processTable.processes[addedProcess->GetPPID()]);
        parent->AddChild(task->pid);
    }

    tasks[numTasks++].Set(task);
    return true;
}

void TaskManager::PrintProcessTable()
{
    printf("\nProcess table:\n");
    for (int i = 0; i < 256; i++)
    {
        if (processTable.processes[i].GetPID() != 0)
        {
            printf("Priority: ");
            printfDec(processTable.processes[i].priority);
            printf(" |Streak: ");
            printfDec(processTable.processes[i].streak);
            printf(" |PID: ");
            printfDec(processTable.processes[i].GetPID());
            printf(" |PPID: ");
            printfDec(processTable.processes[i].GetPPID());
            printf(" |State: ");
            
            if (processTable.processes[i].GetState() == READY) {
                printf("READY");
            } else if (processTable.processes[i].GetState() == RUNNING) {
                printf("RUNNING");
            } else if (processTable.processes[i].GetState() == TERMINATED) {
                printf("TERMINATED");
            } else if (processTable.processes[i].GetState() == BLOCKED) {
                printf("BLOCKED");
            }

            printf(" RetVal: ");
            int* val = processTable.processes[i].GetReturnValue();
            if (*val < 0) 
            {
                printf("-");
                printfDec(*val * -1);
            }
            else
                printfDec(*val);
            printf(" #Child: ");
            printfDec(processTable.processes[i].GetNumChildren());
            printf("\n");
        }
    }
    printf("\n");
}

void TaskManager::Terminate(int* returnValue)
{
    if (tasks[currentTask].pid == 1)
        return;
    
    Process* process = &(processTable.processes[tasks[currentTask].pid]);
    process->SetReturnValue(returnValue);

    // set state to terminated
    process->SetState(TERMINATED);
    terminatedTaskCount++;

    while(true);
}

#ifdef DEFAULT_SCHEDULE
CPUState* TaskManager::Schedule(CPUState* cpustate)
{
    if(numTasks - terminatedTaskCount <= 0)
        return cpustate;

    if(currentTask >= 0){
        tasks[currentTask].cpustate = cpustate;
    }
        
    uint32_t from = -1;

    if (numTasks - terminatedTaskCount > 1 && currentTask >= 0)
    {
        from = tasks[currentTask].pid;
    }

    if (tasks[currentTask].pid != 0)
    {
        Process* process = &(processTable.processes[tasks[currentTask].pid]);
        if (process->GetState() == RUNNING)
            process->SetState(READY);
    }

    ProcessState state = TERMINATED;
    do
    {   
        if(++currentTask >= numTasks){
            currentTask =currentTask % numTasks;
        }
            
        if (processTable.processes[tasks[currentTask].pid].GetState() == BLOCKED)
        {
            Process* process = &(processTable.processes[tasks[currentTask].pid]);
            if (processTable.processes[process->waitPid].GetState() == TERMINATED)
            {
                process->SetState(READY);
                process->waitPid = 0;
            }
        }

        state = processTable.processes[tasks[currentTask].pid].GetState();
    } while (state == TERMINATED || state == BLOCKED);


    uint32_t to = -1;

    if (numTasks - terminatedTaskCount > 1 && currentTask >= 0)
    {
        to = tasks[currentTask].pid;
    }

    if(from != -1 && to != -1){
        printf("Switching task from ");
        printfDec(from);
        printf(" to ");
        printfDec(to);
        printf("\n");
    }

    Process* process = &(processTable.processes[tasks[currentTask].pid]);
    if(process->GetState() == READY)
        process->SetState(RUNNING);

    PrintProcessTable();

    return tasks[currentTask].cpustate; 
}
#endif


#ifdef THIRD_STRATEGY_PRIORITY_SCHEDULE
CPUState* TaskManager::Schedule(CPUState* cpustate)
{
    if (numTasks - terminatedTaskCount <= 0)
        return cpustate;

    if (currentTask >= 0)
        tasks[currentTask].cpustate = cpustate;

    bool printFlag = false;

    #ifdef PRINT_FLAG
    if (numTasks - terminatedTaskCount > 1 && currentTask >= 0)
    {
        printf("Switching task from ");
        printfDec(tasks[currentTask].pid);
        printFlag = true;
    }
    #endif

    if (tasks[currentTask].pid != 0)
    {
        Process* process = &(processTable.processes[tasks[currentTask].pid]);
        if (process->GetState() == RUNNING)
            process->SetState(READY);
    }

    ProcessState state = TERMINATED;
    int lowestPriorityTask = -1;
    int lowestPriority = 255;

    for (int i = 0; i < numTasks; ++i)
    {
        if (processTable.processes[tasks[i].pid].GetState() != TERMINATED &&
            processTable.processes[tasks[i].pid].priority < lowestPriority)
        {
            lowestPriorityTask = i;
            lowestPriority = processTable.processes[tasks[i].pid].priority;
        }
    }

    currentTask = lowestPriorityTask;

    #ifdef PRINT_FLAG
    if (numTasks - terminatedTaskCount > 1 && currentTask >= 0 && printFlag)
    {
        printf(" to ");
        printfDec(tasks[currentTask].pid);
        printf("\n");
        printFlag = false;
    }
    #endif

    Process* process = &(processTable.processes[tasks[currentTask].pid]);
    if (process->GetState() == READY)
        process->SetState(RUNNING);


    PrintProcessTable();

    return tasks[currentTask].cpustate;
}
#endif

#ifdef DYNAMIC_PRIORITY_SCHEDULE
CPUState* TaskManager::Schedule(CPUState* cpustate)
{
    if (numTasks - terminatedTaskCount <= 0)
        return cpustate;

    if (currentTask >= 0)
        tasks[currentTask].cpustate = cpustate;

    bool printFlag = false;

    #ifdef PRINT_FLAG
    if (numTasks - terminatedTaskCount > 1 && currentTask >= 0)
    {
        printf("Switching task from ");
        printfDec(tasks[currentTask].pid);
        printFlag = true;
    }
    #endif

    if (tasks[currentTask].pid != 0)
    {
        Process* process = &(processTable.processes[tasks[currentTask].pid]);
        if (process->GetState() == RUNNING)
            process->SetState(READY);
    }

    ProcessState state = TERMINATED;
    int lowestPriorityTask = -1;
    int lowestPriority = 255;
    int tempCurrentTaskIndex = currentTask;

    for (int i = 0; i < numTasks; ++i)
    {
        if (processTable.processes[tasks[i].pid].GetState() != TERMINATED &&
            processTable.processes[tasks[i].pid].priority < lowestPriority && processTable.processes[tasks[i].pid].streak < 2) // Exclude processes with a streak of 5
        {
            lowestPriorityTask = i;
            lowestPriority = processTable.processes[tasks[i].pid].priority;
        }

    }

    //if currentTask's streak is 2
    if(processTable.processes[tasks[currentTask].pid].streak >= 2){
        //reset streak of current task
        processTable.processes[tasks[currentTask].pid].streak = 0;
        //find highest priority task
        int highestPriorityTask = -1;
        int highestPriority = 0;
        for (int i = 1; i < numTasks; ++i)
        {
            if (processTable.processes[tasks[i].pid].GetState() != TERMINATED &&
                processTable.processes[tasks[i].pid].priority > highestPriority)
            {
                highestPriorityTask = i;
                highestPriority = processTable.processes[tasks[i].pid].priority;
            }

        }

        if(highestPriority > 254){
            processTable.processes[tasks[currentTask].pid].priority = 254;
        }
        else{
            processTable.processes[tasks[currentTask].pid].priority = highestPriority + 1;
        }  
    }


    if(lowestPriorityTask == currentTask){
        printf("=====Current task is the lowest priority task=====\n");
        //increment streak of currenttask
        processTable.processes[tasks[currentTask].pid].streak++;
    }
    else{
        //reset streak of current task
        processTable.processes[tasks[tempCurrentTaskIndex].pid].streak = 0;
    }

    // Set the next task based on priority
    currentTask = lowestPriorityTask;

    // Print switching message
    #ifdef PRINT_FLAG
    if (numTasks - terminatedTaskCount > 1 && currentTask >= 0 && printFlag)
    {
        printf(" to ");
        printfDec(tasks[currentTask].pid);
        printf("\n");
        printFlag = false;
    }
    #endif

    // Current task is in the running state
    Process* process = &(processTable.processes[tasks[currentTask].pid]);
    if (process->GetState() == READY)
        process->SetState(RUNNING);

    PrintProcessTable();

    return tasks[currentTask].cpustate;
}
#endif

common::uint32_t TaskManager::GetCurrentPid()
{
    return tasks[currentTask].pid;
}

common::uint32_t TaskManager::Fork_1(CPUState* cpustate)
{   
    // memory is full
    if (numTasks >= 256)
        return 0;

    for (int i = 0; i < sizeof(tasks[currentTask].stack); i++)
    {
        tasks[numTasks].stack[i] = tasks[currentTask].stack[i];
    }
    
    tasks[numTasks].cpustate = (CPUState*)(tasks[numTasks].stack + 4096 - sizeof(CPUState));
    common::uint32_t currentTaskOffset = ((common::uint32_t)tasks[currentTask].cpustate) - ((common::uint32_t)cpustate);
    tasks[numTasks].cpustate = (CPUState*)(((common::uint32_t)tasks[numTasks].cpustate) - currentTaskOffset);
 
    for (int i = tasks[currentTask].pid + 1; i < 256; i++)
    {
        if (processTable.processes[i].GetPID() == 0)
        {
            tasks[numTasks].pid = i;
            break;
        }
    }

    tasks[numTasks].cpustate -> ecx = 0;

    Process* parent = &(processTable.processes[tasks[currentTask].pid]);
    parent->AddChild(tasks[numTasks].pid);

    Process* child = &(processTable.processes[tasks[numTasks].pid]);
    child->SetPPID(tasks[currentTask].pid);

    Process* process = &(processTable.processes[tasks[numTasks].pid]);
    process->SetPID(tasks[numTasks].pid);
    process->SetPPID(tasks[currentTask].pid);
    process->SetState(READY);

    numTasks++;

    return tasks[numTasks-1].pid;
}

common::uint32_t TaskManager::Fork_2(CPUState* cpustate)
{
    if (numTasks >= 256)
        return 0;

    tasks[numTasks].cpustate->eip = cpustate->ebx;

    for (int i = tasks[currentTask].pid + 1; i < 256; i++)
    {
        if (processTable.processes[i].GetPID() == 0)
        {
            tasks[numTasks].pid = i;
            break;
        }
    }

    Process* parent = &(processTable.processes[tasks[currentTask].pid]);
    parent->AddChild(tasks[numTasks].pid);

    Process* child = &(processTable.processes[tasks[numTasks].pid]);
    child->SetPPID(tasks[currentTask].pid);

    Process* process = &(processTable.processes[tasks[numTasks].pid]);
    process->SetPID(tasks[numTasks].pid);
    process->SetPPID(tasks[currentTask].pid);
    process->SetState(READY);

    numTasks++;

    return tasks[numTasks-1].pid;
}

void TaskManager::Wait(CPUState* cpustate)
{
    common::uint32_t pid = cpustate->ebx;

    if (tasks[currentTask].pid == cpustate->ebx || pid == 0 || pid == 1)
        return;

    if (!processTable.processes[tasks[currentTask].pid].IsChild(pid))
        return;

    Process* process = &(processTable.processes[tasks[currentTask].pid]);
    process->SetState(BLOCKED);
    process->waitPid = pid;
}

void TaskManager::Exec(CPUState* cpustate)
{
    tasks[currentTask].cpustate = cpustate;
    tasks[currentTask].cpustate -> eip = cpustate -> ebx;

    Process* process = &(processTable.processes[tasks[currentTask].pid]);
    process->SetState(READY);
}

Process::Process()
{
    this->pid = 0;
    this->ppid = 1;
    this->state = TERMINATED;
    this->retval = &default_val;
    this->waitPid = 0;

    for (int i = 0; i < 256; i++)
        this->children[i] = 0;
}

Process::Process(common::uint32_t pid, common::uint32_t ppid) 
{
    this->pid = pid;
    this->ppid = ppid;
    this->state = READY;
    this->retval = &default_val;
    this->waitPid = 0;

    for (int i = 0; i < 256; i++)
        this->children[i] = 0;
}

Process::~Process() 
{
}

void Process::AddChild(uint32_t pid) 
{
    if (this->numChildren >= 256 || pid >= 256 || pid < 0) {
        return;
    }

    this->children[pid] = pid;
    this->numChildren++;
}

void Process::RemoveChild(uint32_t pid) 
{
    if (this->numChildren >= 256 || pid >= 256 || pid < 0) {
        return;
    }

    this->children[pid] = 0;
    if (this->numChildren > 0)
        this->numChildren--;
}

bool Process::IsChild(uint32_t pid) 
{
    if (this->numChildren >= 256 || pid >= 256 || pid < 0) {
        return false;
    }

    if (this->children[pid] == pid)
        return true;
    else
        return false;
}

common::uint32_t Process::GetPID() {
    return this->pid;
}

common::uint32_t Process::GetPPID() {
    return this->ppid;
}

common::uint32_t Process::GetNumChildren() {
    return this->numChildren;
}

ProcessState Process::GetState() {
    return this->state;
}

int* Process::GetReturnValue() {
    return this->retval;
}

void Process::SetPID(common::uint32_t pid) {
    this->pid = pid;
}

void Process::SetPPID(common::uint32_t ppid) {
    this->ppid = ppid;
}

void Process::SetNumChildren(common::uint32_t numChildren) {
    this->numChildren = numChildren;
}

void Process::SetState(ProcessState state) {
    this->state = state;
}

void Process::SetReturnValue(int* retval) {
    this->retval = retval;
}

void Task::Set(Task* task)
{
    cpustate->eax = task->cpustate->eax;
    cpustate->ebx = task->cpustate->ebx;
    cpustate->ecx = task->cpustate->ecx;
    cpustate->edx = task->cpustate->edx;

    cpustate->esi = task->cpustate->esi;
    cpustate->edi = task->cpustate->edi;
    cpustate->ebp = task->cpustate->ebp;

    cpustate->eip = task->cpustate->eip;
    cpustate->cs = task->cpustate->cs;
    cpustate->eflags = task->cpustate->eflags;

    pid = task->pid;
}

void Task::setEntryPoint(void entrypoint())
{
    cpustate -> eip = (uint32_t)entrypoint;
}

void* Task::GetEntryPoint()
{
    return (void*)cpustate->eip;
}