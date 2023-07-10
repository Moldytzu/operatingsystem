#include <sys/sys.h>
#include <misc/logger.h>

// pid (rsi = pid, rdx = info, r8 = retVal/inputVal)
void pid(uint64_t pid, uint64_t info, uint64_t retVal, uint64_t r9, sched_task_t *task)
{
    uint64_t *retAddr = PHYSICAL(retVal);
    sched_task_t *t = schedGet(pid);
    if (!t) // check if the task exists
    {
        *retAddr = UINT64_MAX;
        return;
    }

    switch (info)
    {
    case 0:                  // get pid state
        *retAddr = t->state; // give the state in which that pid is in
        break;
    case 1:                        // get pid enviroment
        if (!IS_MAPPED(retVal)) // available only in the allocated memory
            break;
        memcpy(PHYSICAL(retVal), t->enviroment, 4096); // copy the enviroment
        break;
    case 2:                        // set pid enviroment
        if (!IS_MAPPED(retVal)) // available only in the allocated memory
            break;
        memcpy(t->enviroment, PHYSICAL(retVal), 4096); // copy the enviroment
        break;
    case 3:                        // get current pid
        if (!IS_MAPPED(retVal)) // prevent crashing
            return;
        *retAddr = task->id; // the id
        break;
    case 4:                        // get current working directory
        if (!IS_MAPPED(retVal)) // available only in the allocated memory
            break;

        task->cwd[0] = '/'; // make sure the cwd starts with /

        memcpy(PHYSICAL(retVal), task->cwd, min(strlen(task->cwd), 512)); // copy the buffer
        break;
    case 5:                        // set current working directory
        if (!IS_MAPPED(retVal)) // available only in the allocated memory
            break;

        zero(task->cwd, 512);
        memcpy(task->cwd, PHYSICAL(retVal), min(strlen(PHYSICAL(retVal)), 512)); // copy the buffer
        break;
    default:
        break;
    }
}