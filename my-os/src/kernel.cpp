
#include <common/types.h>
#include <gdt.h>
#include <hardwarecommunication/interrupts.h>
#include <hardwarecommunication/pci.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <multitasking.h>
#include <syscalls.h>

// change scheduling type in multitasking.cpp
// #define PRIORITY_FLAG 
#define NON_PRIORITY_FLAG

#define PART_A

// #define PART_B_First_Strategy
// #define PART_B_Second_Strategy
// #define PART_B_Third_Strategy
//#define PART_B_DYNAMIC_STRATEGY

//#define PART_C_First_Strategy

// #define FORK_TEST_1
// #define FORK_TEST_2
// #define GETPID_TEST
// #define WAITPID_TEST
// #define EXECVE_TEST


#define PRINT_MODE

using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;
using namespace myos::gui;

void printf(char* str);
bool myos::common::readIOFlag = false;
bool myos::common::criticalFlag = false;

class PrintfKeyboardEventHandler : public KeyboardEventHandler
{
public:
    void OnKeyDown(char c)
    {
        if (readIOFlag)
        {
            char* foo = " ";
            foo[0] = c;

            printf(foo);
            
        }

    }
};

GlobalDescriptorTable gdt;
TaskManager taskManager(&gdt);
InterruptManager interrupts(0x20, &gdt, &taskManager);
SyscallHandler syscalls(&interrupts, 0x80);
PrintfKeyboardEventHandler keyboardhandler;
KeyboardDriver keyboard(&interrupts, &keyboardhandler);

void critical_in();
void critical_out();
void sleep(unsigned int seconds);

void printf(char* str)
{
    static uint16_t* VideoMemory = (uint16_t*)0xb8000;

    static uint8_t x=0,y=0;

    for(int i = 0; str[i] != '\0'; ++i)
    {
        switch(str[i])
        {
            case '\n':
                x = 0;
                y++;
                break;
            default:
                VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | str[i];
                x++;
                break;
        }

        if(x >= 80)
        {
            x = 0;
            y++;
        }

        if(y >= 25)
        {
            for(y = 0; y < 25; y++)
                for(x = 0; x < 80; x++)
                    VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | ' ';
            x = 0;
            y = 0;
        }
    }
}

uint32_t fork(CPUState* cpu)
{    
    return taskManager.Fork_1(cpu);
}

uint32_t fork2(CPUState* cpu)
{    
    return taskManager.Fork_2(cpu);
}

void wait(CPUState* cpu)
{
    taskManager.Wait(cpu);
}

void execve(CPUState* cpu)
{
    taskManager.Exec(cpu);
}

void printfHex(uint8_t key)
{
    char* foo = "00";
    char* hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0xF];
    foo[1] = hex[key & 0xF];
    printf(foo);
}

void printfHex32(uint32_t key)
{
    printfHex((key >> 24) & 0xFF);
    printfHex((key >> 16) & 0xFF);
    printfHex((key >> 8) & 0xFF);
    printfHex(key & 0xFF);
}

void printfChar(char c)
{
    char* foo = " ";
    foo[0] = c;
    printf(foo);
}

void printfDec(int num)
{
    if (num == 0)
    {
        printf("0");
        return;
    }

    char buffer[32];
    int i = 0;
    bool negativeFlag = false;

    if (num < 0)
    {
        num = -num;
        negativeFlag = true;
    }

    while (num != 0)
    {
        buffer[i] = "0123456789"[num % 10];
        num /= 10;
        i++;
    }

    if (negativeFlag)
    {
        buffer[i] = '-';
        i++;
    }

    buffer[i] = 0;

    char temp;
    
    int j = 0;
    i--;

    while (j < i)
    {
        temp = buffer[j];
        buffer[j++] = buffer[i];
        buffer[i--] = temp;
    }

    printf(buffer);
}

void critical_in()
{
    // interrupts.Deactivate();
    criticalFlag = true;
}

void critical_out()
{
    // interrupts.Activate();
    criticalFlag = false;
}

class MouseToConsole : public MouseEventHandler
{
    int8_t x, y;
public:
    
    MouseToConsole()
    {
        uint16_t* VideoMemory = (uint16_t*)0xb8000;
        x = 40;
        y = 12;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);        
    }
    
    virtual void OnMouseMove(int xoffset, int yoffset)
    {
        static uint16_t* VideoMemory = (uint16_t*)0xb8000;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);

        x += xoffset;
        if(x >= 80) x = 79;
        if(x < 0) x = 0;
        y += yoffset;
        if(y >= 25) y = 24;
        if(y < 0) y = 0;

        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);
    }
    
};

void fork(int *pid)
{
    asm("int $0x80" : "=c" (*pid) : "a" (1));
}

int forkWithExec(void entrypoint())
{
    int ret;
    asm("int $0x80" : "=a" (ret) : "a" (5), "b" ((uint32_t)entrypoint));
    return ret;
}

int sysgetpid()
{
    int pid;

    critical_in();
    pid = taskManager.GetCurrentPid();
    critical_out();

    return pid;
}


void sys_waitpid(uint32_t pid)
{
    printf("sys_waitpid() called for pid: ");
    printfDec(pid);
    asm("int $0x80" : : "a" (2), "b" (pid));
}

void sys_exit(int* ret)
{
    taskManager.Terminate(ret);
}

void execHere()
{
    int ret = 0;
    printf("EXEC TEST FUNCTION !");
    sys_exit(&ret);
}

void taskD()
{
    int pid;

    pid = forkWithExec(&execHere);
    sys_waitpid(pid);

    int ret = 0;
    sys_exit(&ret);
}

void taskWait()
{
    int pid;
    int ret = 0;
    pid = forkWithExec(&taskD);
    sys_waitpid(pid);

    sys_exit(&ret);
}

inline unsigned long long rdtsc() {
    unsigned int lo, hi;
    // Use inline assembly to read the TSC
    __asm__ __volatile__ (
        "rdtsc"
        : "=a"(lo), "=d"(hi)
    );
    return ((unsigned long long)hi << 32) | lo;
}

void custom_sleep(double seconds){
    // Get the CPU frequency in Hz (cycles per second)
    // This value is specific to your CPU and should be measured or set appropriately
    const double cpu_frequency = 2.9e9; // Example: 2.5 GHz

    // Calculate the number of cycles to wait for
    unsigned long long wait_cycles = static_cast<unsigned long long>(seconds * cpu_frequency);

    // Get the start time in cycles
    unsigned long long start_cycles = rdtsc();

    // Busy-wait loop until the required number of cycles has passed
    while (rdtsc() - start_cycles < wait_cycles) {
        // Do nothing, just loop
    }
}

void taskE(){
    int pid = sysgetpid();
    int i=0;
    while(true)
    {
        printf("Task-E | PID : ");
        printfDec(pid);
        printf(" | i =");
        printfDec(i++);
        printf("\n");
        custom_sleep(1.0);
    }
}

void taskC()
{
    int pid;
    pid = sysgetpid();
    printf("TASKC\n");
    printf("PID: ");
    printfHex32(pid);
    printf("\n");

    while(true);
}


void sysprintf(char* str)
{
    asm("int $0x80" : : "a" (4), "b" (str));
}

void sysexecve(void entry())
{
    asm("int $0x80" : : "a" (3), "b" (entry));
}

void taskExecve()
{
    printf("- Before Exec -\n");
    sysexecve(&execHere);
    printf("- After Exec - \n");

    while(true);
}

void execforGrandChild(){
    printf("EXEC is done for Grand Child!\n");
}

void execforChild(){
    int ret = 0;
    printf("EXEC is done for Child!\n");
    int thisPID = taskManager.GetCurrentPid();
    printf("PID in EXEC CHILD : ");
    printfHex32(thisPID);
    printf("\n");
    
    sys_waitpid(thisPID);
    sys_exit(&ret);
}

void taskFork()
{
    int pid = 0;
    fork(&pid);
    printf("\n");
    if(pid == 0) {
            printf("\n>>>>>>>>>>>>>> PID is : ");
            printfDec(pid);
            printf(" in CHILD\n");            
    }
    else {
            printf("\n<<<<<<<<<<<<<< PID is : ");
            printfDec(taskManager.GetCurrentPid());
            printf(" in PARENT\n");
    }

    // printf("PID in FORK : ");
    // printfHex32(pid);
    sys_exit(&pid);
    // while(true);

}

void printArray(int array[], int n){
    printf("Array: {");
    for(int i=0; i<n; i++){
        printfDec(array[i]);
        if(i+1!=n) printf(",");
    }
    printf("\n");
}

int32_t randomNumberGenerator(int number)
{
        uint32_t time;

        // get the time
        asm volatile("rdtsc" : "=a" (time) : : "edx");
        
        if (time < 0)
            time = -time;

        int randomNumber = time % number;

        return randomNumber;
}

void taskB()
{
    int pid = sysgetpid();
    //pid = sysgetpid();
    //printf("TASKB\n");
    printf("PID of Task-B : ");
    printfHex32(pid);
    printf("\n");

    int i=0;
    while(true)
    {
        printf("|");
        printfDec(i++);
        printf(" |");
        printf(" Task-B |");
        printf("\n");
        sleep(10);
    }
}

void taskA()
{
    int pid = sysgetpid();
    //pid = sysgetpid();
    //printf("TASKA\n");
    printf("PID of Task-A: ");
    printfHex32(pid);
    printf("\n");
    int i=0;
    while(true)
    {
        printf("|");
        printfDec(i++);
        printf(" |");
        printf(" Task-A |");
        printf("\n");
        sleep(10);
    }
}

void sleep(unsigned int seconds) {
    long int delay = 100000000;
    for(long int i = 0; i < seconds*delay; i++)
        i++;
}

void my_own_testGetPID(){
    printf("GET PID TEST \n");
    forkWithExec(&taskA);
    forkWithExec(&taskB);
}

void my_own_collatz(){   
    int n = randomNumberGenerator(100);
    int n2 = n;
    printf("CALCULATING COLLATZ > ");
    printfDec(n);
    printf(" : ");
    
    while(n!=1){
        if(n%2==0){
            n=n/2;
        }
        else{
            n=3*n+1;
        }
        printf("| Cltz(");

        printfDec(n2);
        printf(") = ");
        printfDec(n);
        printf(" ");
        custom_sleep(1.0);
    }

    int ret = 0;
    ret = sysgetpid();
    printf("COLLATZ ->");
    printfDec(n2);
    printf(" <- is done\n");
    printfDec(ret);
    sys_exit(&ret);
}

void long_running(){
    long long n = 1000;
    int result = 0;
    int printCounter = 0;
    for(int i=0; i<n; ++i){
        for(int j=0; j<n; ++j){
            result += i*j;
        }
        if(printCounter==50){
            printf(" ");
            printfDec(result);
            printCounter = 0;
        }
        printCounter++;
        sleep(1);
    }
    printf("result: ");
    printfDec(result);
    int ret = 0;
    ret = sysgetpid();
    sys_exit(&ret);
}

int binarySearchHelper(int array[], int left, int right, int x){
    while(left <= right){     
        printf("left: ");
        printfDec(left);
        printf(" right: ");
        printfDec(right);
        printf("\n");
        custom_sleep(3.0);
        int mid = left + (right - left) / 2;
        if(array[mid] == x){
            return mid;
        }
        if(array[mid] < x){
            left = mid + 1;
        }
        else{
            right = mid - 1;
        }
    }

    return -1;
}

void my_own_binarySearch(){
    int array[] = {10, 20, 30, 50, 60, 80, 100, 110, 130, 170};
    int n = sizeof(array)/sizeof(array[0]);
    int x = 110;

    int result = binarySearchHelper(array, 0, n-1, x);

    printArray(array, n);
    printf("Element: ");
    printfDec(x);
    printf(" is at index: ");
    printfDec(result);
    printf("\n");
    printf(">>>>>> Binary Search is done <<<<<<\n");

    int ret = 0;
    sys_exit(&ret);
}

int linearSearchHelper(int array[], int n, int x){
    for(int i=0; i<n; i++){
        printf("Index: ");
        printfDec(i);
        printf(" - ");
        printf("Searched Element: ");
        printfDec(array[i]);
        printf("\n");
        custom_sleep(3.0);
        if(array[i] == x){
            
            return i;
        }
        
    }
    return -1;
}

void my_own_linearSearch(){
    int array[] = {10, 20, 80, 30, 60, 50, 110, 100, 130, 170};
    int n = sizeof(array)/sizeof(array[0]);
    int x = 130;
    int result = linearSearchHelper(array, n, x);
    printArray(array, n);
    printf("Element: ");
    printfDec(x);
    printf(" is at index: ");
    printfDec(result);
    printf("\n");

    printf(">>>>>> Linear Search is done <<<<<<\n");

    int ret = 0;
    sys_exit(&ret);

}


typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; i++)
        (*i)();
}

void InitProcess() {
    printf("Init process started\n");

    #ifdef PART_A
        void (*process[])() = {long_running, my_own_collatz};

        for (int i = 0; i < 3; ++i) {
            forkWithExec(process[0]);
            forkWithExec(process[1]);
        }
    #endif

    #ifdef PART_B_First_Strategy
        
        int randomNumber = randomNumberGenerator(4);
        printf("Random number: ");
        printfDec(randomNumber);
        printf("\n");
        custom_sleep(1.0);
        if(randomNumber == 0){
            printf("Collatz is chosen\n");
            custom_sleep(3.0);
            for(int i=0;i<10;i++){
                forkWithExec(&my_own_collatz);
            }
        }
        else if(randomNumber == 1){
            printf("Binary Search is chosen\n");
            custom_sleep(3.0);
            for(int i=0;i<10;i++)
                forkWithExec(&my_own_binarySearch);
        }
        else if(randomNumber == 2){
            printf("Linear Search is chosen\n");
            custom_sleep(3.0);
            for(int i=0;i<10;i++)
                forkWithExec(&my_own_linearSearch);
        }
        else if(randomNumber == 3){
            printf("Long Running is chosen\n");
            custom_sleep(3.0);
            for(int i=0;i<10;i++)
                forkWithExec(&long_running);
        }
    #endif

    #ifdef PART_B_Second_Strategy
        int processCount = 4;
        int randomIndex1 = randomNumberGenerator(processCount);
        int randomIndex2;
        uint32_t pid1 = 0;
        uint32_t pid2 = 0;
        
        do {
        randomIndex2 = randomNumberGenerator(processCount);
        } while (randomIndex2 == randomIndex1);


        //write switch case
        if(randomIndex1 == 0)printf("CollbinarySearchHelperatz is chosen\n");
        else if(randomIndex1 == 1)printf("Binary Search is chosen\n");
        else if(randomIndex1 == 2)printf("Linear Search is chosen\n");
        else if(randomIndex1 == 3)printf("Long Running is chosen\n");

        if(randomIndex2 == 0)printf("Collatz is chosen\n");
        else if(randomIndex2 == 1)printf("Binary Search is chosen\n");
        else if(randomIndex2 == 2)printf("Linear Search is chosen\n");
        else if(randomIndex2 == 3)printf("Long Running is chosen\n");

        void (*process[])() = {my_own_collatz, my_own_binarySearch, my_own_linearSearch,long_running};

        for (int i = 0; i < 3; ++i) {
            pid1 = forkWithExec(process[randomIndex1]);
            printf("PID1: ");
            printfDec(pid1);
            printf("\n");
            pid2 = forkWithExec(process[randomIndex2]);
            printf("PID2: ");
            printfDec(pid2);
            printf("\n");

        }

        sys_waitpid(pid1);
        sys_waitpid(pid2);

    #endif

    #ifdef PART_B_Third_Strategy
        printf("Third Strategy\n");
        custom_sleep(3.0);
        Task task_third_strategy1(&gdt, &my_own_collatz);
        Task task_third_strategy2(&gdt, &my_own_binarySearch);
        Task task_third_strategy3(&gdt, &my_own_linearSearch);
        Task task_third_strategy4(&gdt, &long_running);

        taskManager.AddTaskWithPriority(&task_third_strategy1, 10);
        taskManager.AddTaskWithPriority(&task_third_strategy2, 40);
        taskManager.AddTaskWithPriority(&task_third_strategy3, 30);
        taskManager.AddTaskWithPriority(&task_third_strategy4, 20);
    #endif

    #ifdef PART_B_DYNAMIC_STRATEGY
        Task task_priority1(&gdt, &my_own_collatz);
        Task task_priority2(&gdt, &my_own_collatz);
        Task task_priority3(&gdt, &my_own_collatz);
        taskManager.AddTaskWithPriority(&task_priority1, 100);
        taskManager.AddTaskWithPriority(&task_priority2, 110);
        taskManager.AddTaskWithPriority(&task_priority3, 90);
    #endif

    #ifdef PART_C_First_Strategy
        for(int i=0; i<3; i++){
            forkWithExec(&taskE);
        }
    #endif

    #ifdef FORK_TEST_1
        taskFork();
    #endif

    #ifdef FORK_TEST_2
        forkWithExec(&taskFork);
    #endif

    #ifdef GETPID_TEST
        my_own_testGetPID();
    #endif

    #ifdef WAITPID_TEST
        int pid = forkWithExec(&taskWait);
        sys_waitpid(pid);
    #endif

    #ifdef EXECVE_TEST
        forkWithExec(&taskExecve);
    #endif

    while (true);
}

void init() {
    Task task_init(&gdt, &InitProcess);
    
    #ifdef PRIORITY_FLAG
        taskManager.AddTaskWithPriority(&task_init,254);
    #endif
    
    #ifdef NON_PRIORITY_FLAG
        taskManager.AddTask(&task_init);
    #endif

}


extern "C" void kernelMain(const void* multiboot_structure, uint32_t /*multiboot_magic*/)
{
    printf(">>>>>>> Welcome <<<<<<<\n");
    
    init();
   
    DriverManager drvManager;


    drvManager.AddDriver(&keyboard);
    
    MouseToConsole mousehandler;
    MouseDriver mouse(&interrupts, &mousehandler);
    drvManager.AddDriver(&mouse);
    
    PeripheralComponentInterconnectController PCIController;
    PCIController.SelectDrivers(&drvManager, &interrupts);
        
    drvManager.ActivateAll();

    interrupts.Activate();
    
    while(1);
}
