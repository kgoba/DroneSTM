//------------------------------------------------------------------------/
// Sample code of OS dependent controls for FatFs                         /
// (C)ChaN, 2012                                                          /
//------------------------------------------------------------------------/
 
#include "ff.h"
#include "rtos.h"
 
#if _FS_REENTRANT
/*-----------------------------------------------------------------------/
/ Create a Synchronization Object
/------------------------------------------------------------------------/
/ This function is called in f_mount function to create a new
/  synchronization object, such as semaphore and mutex. When a FALSE is
/  returned, the f_mount function fails with FR_INT_ERR.
*/
 
int ff_cre_syncobj (    /* 1:Function succeeded, 0:Could not create due to any error */
    BYTE vol,           /* Corresponding logical drive being processed */
    _SYNC_t *sobj       /* Pointer to return the created sync object */
)
{
    (*sobj) = new Semaphore(1);
 
    return 1;
}
 
 
 
/*------------------------------------------------------------------------*/
/* Delete a Synchronization Object                                        */
/*------------------------------------------------------------------------*/
/* This function is called in f_mount function to delete a synchronization
/  object that created with ff_cre_syncobj function. When a FALSE is
/  returned, the f_mount function fails with FR_INT_ERR.
*/
 
int ff_del_syncobj (    /* 1:Function succeeded, 0:Could not delete due to any error */
    _SYNC_t sobj        /* Sync object tied to the logical drive to be deleted */
)
{
    delete sobj;
    return 1;
}
 
 
 
/*------------------------------------------------------------------------*/
/* Request Grant to Access the Volume                                     */
/*------------------------------------------------------------------------*/
/* This function is called on entering file functions to lock the volume.
/  When a FALSE is returned, the file function fails with FR_TIMEOUT.
*/
 
int ff_req_grant (  /* TRUE:Got a grant to access the volume, FALSE:Could not get a grant */
    _SYNC_t sobj    /* Sync object to wait */
)
{
    bool ret;
 
    ret = (sobj->wait(_FS_TIMEOUT) > 0);
 
    return ret;
}
 
 
 
/*------------------------------------------------------------------------*/
/* Release Grant to Access the Volume                                     */
/*------------------------------------------------------------------------*/
/* This function is called on leaving file functions to unlock the volume.
*/
 
void ff_rel_grant (
    _SYNC_t sobj    /* Sync object to be signaled */
)
{
    sobj->release();
}
 
#endif
 
 
 
 
#if _USE_LFN == 3   /* LFN with a working buffer on the heap */
/*------------------------------------------------------------------------*/
/* Allocate a memory block                                                */
/*------------------------------------------------------------------------*/
/* If a NULL is returned, the file function fails with FR_NOT_ENOUGH_CORE.
*/
 
void* ff_memalloc ( /* Returns pointer to the allocated memory block */
    UINT size       /* Number of bytes to allocate */
)
{
    return malloc(size);
}
 
 
/*------------------------------------------------------------------------*/
/* Free a memory block                                                    */
/*------------------------------------------------------------------------*/
 
void ff_memfree (
    void* mblock    /* Pointer to the memory block to free */
)
{
    free(mblock);
}
 
#endif
