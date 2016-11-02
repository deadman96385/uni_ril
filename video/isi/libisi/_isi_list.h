/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2004 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
 *
 * $D2Tech$ $Rev: 24709 $ $Date: 2014-02-20 17:27:12 +0800 (Thu, 20 Feb 2014) $
 */

#ifndef _ISI_LIST_H_
#define _ISI_LIST_H_

#include "_isi_port.h"

typedef struct ISIL_Entry{
    struct ISIL_Entry *pNext;
    struct ISIL_Entry *pPrev;
#ifdef ISI_LIST_DEBUG
    uint32    inUse;
#endif
} ISIL_ListEntry;

typedef struct {
    ISIL_ListEntry *pHead;
    ISIL_ListEntry *pTail;
    vint            isBackwards;
    ISI_SemId       lock;
    char           *listName;
    int             cnt;
} ISIL_List;

void ISIL_initEntry(
    ISIL_ListEntry *pEntry);

void ISIL_initList(
    ISIL_List    *pDLL);

ISIL_ListEntry * ISIL_dequeue(
    ISIL_List *pDLL);

int ISIL_enqueue(
    ISIL_List *pDLL, 
    ISIL_ListEntry *pEntry);


#endif