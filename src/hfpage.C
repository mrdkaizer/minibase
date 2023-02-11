// **********************************************
//     Heap Fle Page Class
//     $Id: hfpage.C,v 1.2 1997/01/04 08:53:49 flisakow Exp $
//
//     9/16/01 KMO: Minor modification to ::available_space() so that
//                  it checks whether there is a free slot available.
//                  if so, ::available_space() returns freespace
//                  rather than freespace - sizeof(slot_t).
//    
// **********************************************

#include <iostream>
#include <stdlib.h>
#include <memory.h>

#include "hfpage.h"
#include "new_error.h"
#include "buf.h"
#include "db.h"

static const char *hpErrMsgs[] = {
    "invalid slot number",
};

static error_string_table hpTable( HEAPPAGE, hpErrMsgs );

HFPage::HFPage() {
}

// **********************************************************
// page class constructor

void HFPage::init(PageId pageNo)
{

    nextPage = prevPage = INVALID_PAGE;

    slotCnt  = 0;                // no slots in use
    curPage  = pageNo;

    usedPtr   = sizeof(data);    // offset of used space in data array

    freeSpace = sizeof(data) + sizeof(slot_t); // amount of space available
                                               // (initially one unused slot)
}

// **********************************************************
// dump page utlity
void HFPage::dumpPage()
{
    int i;

    cout << "dumpPage, this: " << this << endl;
    cout << "curPage= " << curPage << ", nextPage=" << nextPage << endl;
    cout << "usedPtr=" << usedPtr << ",  freeSpace=" << freeSpace
         << ", slotCnt=" << slotCnt << endl;
    
    for (i=0; i < slotCnt; i++) {
        cout << "slot["<< i <<"].offset=" << slot[i].offset
             << ", slot["<< i << "].length=" << slot[i].length << endl; 
    }
}

// **********************************************************
PageId HFPage::getPrevPage()
{
    return prevPage;
}

// **********************************************************
void HFPage::setPrevPage(PageId pageNo)
{

    prevPage = pageNo;
}

// **********************************************************
void HFPage::setNextPage(PageId pageNo)
{

    nextPage = pageNo;
}

// **********************************************************
PageId HFPage::getNextPage()
{
    return nextPage;
}

// **********************************************************
// Add a new record to the page. Returns OK if everything went OK
// otherwise, returns DONE if sufficient space does not exist
// RID of the new record is returned via rid parameter.
Status HFPage::insertRecord(char* recPtr, int recLen, RID& rid)
{
    RID tmpRid;
    int spaceNeeded = recLen + sizeof(slot_t);

    // Start by checking if sufficient space exists.
    // This is an upper bound check. May not actually need a slot
    // if we can find an empty one.

    if (spaceNeeded > freeSpace) {
        return DONE;
    } else {

        // look for an empty slot

        int i;
        for (i=0; i < slotCnt; i++) {
            if (slot[i].length == EMPTY_SLOT)
                break;
        }

        // at this point we have either found an empty slot 
        // or i will be equal to slotCnt.  In either case,
        // we can just use i as the slot index

        // adjust free space


        if (i == slotCnt) {

              // using a new slot
            freeSpace -= spaceNeeded;
            slotCnt++;

        } else {

              // reusing an existing slot 
            freeSpace -= recLen;
        }

        // use existing value of slotCnt as the index into slot array
        // use before incrementing because constructor sets the initial
        // value to 0


        usedPtr -= recLen;    // adjust usedPtr

        slot[i].offset = usedPtr;
        slot[i].length = recLen;


        memcpy(&data[usedPtr],recPtr,recLen); // copy data onto the data page


        tmpRid.pageNo = curPage;
        tmpRid.slotNo = i;
        rid = tmpRid;


        return OK;
    }
}

// **********************************************************
// Delete a record from a page. Returns OK if everything went okay.
// Compacts remaining records but leaves a hole in the slot array.
// Use memmove() rather than memcpy() as space may overlap.
Status HFPage::deleteRecord(const RID& rid)
{

    int slotNo = rid.slotNo;


    // first check if the record being deleted is actually valid
    if ((slotNo >= 0) && (slotNo < slotCnt) && (slot[slotNo].length > 0)) {


        // valid slot

        // The records always need to be compacted, as they are
        // not necessarily stored on the page in the order that
        // they are listed in the slot index.  
        
        int offset = slot[slotNo].offset; // offset of record being deleted
        int recLen = slot[slotNo].length; // length of record being deleted

        char* newSpot = &(data[usedPtr + recLen]);

            // calculate the num of bytes to move
        int sze = offset - usedPtr;
        memmove(newSpot, &(data[usedPtr]), sze); // shift bytes to the right


        // now need to adjust offsets of all valid slots that refer 
        // to the left of the record being removed. (by the size of the hole)

        int i;
        for (i = 0; i < slotCnt; i++) {
            if ((slot[i].length >= 0)
                   && (slot[i].offset < slot[slotNo].offset))
                slot[i].offset += recLen;
        }

        usedPtr   += recLen;   // move used Ptr forward
        freeSpace += recLen;   // increase freespace by size of hole

        slot[slotNo].length = EMPTY_SLOT;  // mark slot free
        slot[slotNo].offset =  0;

        // shrink the slot array
        for ( i = slotCnt-1; i >= 0; i--) {
             if (slot[i].length == EMPTY_SLOT) {
                 slotCnt--;
                 freeSpace += sizeof(slot_t);
             } else {
                 break;
             }
        }

        return OK;
        
    } else {
        return MINIBASE_FIRST_ERROR( HEAPFILE, INVALID_SLOTNO );
    }
}

// **********************************************************
// returns RID of first record on page
Status HFPage::firstRecord(RID& firstRid)
{
    RID tmpRid;
    int i;

    // find the first non-empty slot

    for (i=0; i < slotCnt; i++) {
        if (slot[i].length != EMPTY_SLOT)
            break;
    }

    if ((i == slotCnt) || (slot[i].length == EMPTY_SLOT)) {
        return DONE;
    }

      // found a non-empty slot

    tmpRid.pageNo = curPage;
    tmpRid.slotNo = i;
    firstRid = tmpRid;

    return OK;
}

// **********************************************************
// returns RID of next record on the page
// returns DONE if no more records exist on the page; otherwise OK
Status HFPage::nextRecord (RID curRid, RID& nextRid)
{
    RID tmpRid;
    int i; 

   if (curRid.slotNo < 0 || curRid.slotNo >= slotCnt)
	return FAIL;

      // find the next non-empty slot
    for (i=curRid.slotNo+1; i < slotCnt; i++) {
        if (slot[i].length != EMPTY_SLOT)
            break;
    }

    if ((i >= slotCnt) || (slot[i].length == EMPTY_SLOT)) {
        return DONE;
    }

      // found a non-empty slot
    tmpRid.pageNo = curPage;
    tmpRid.slotNo = i;
    nextRid = tmpRid;

    return OK;
}

// **********************************************************
// returns length and copies out record with RID rid
Status HFPage::getRecord(RID rid, char* recPtr, int& recLen)
{
    int slotNo = rid.slotNo;
    int offset;

    if ((slotNo < slotCnt) && (slot[slotNo].length > 0)) {

        offset = slot[slotNo].offset;   // extract offset in data[]

         // copy out the record

        recLen = slot[slotNo].length;   // return length of record
        memcpy(recPtr, &(data[offset]), recLen);

        return OK;
    } else {
        return MINIBASE_FIRST_ERROR( HEAPFILE, INVALID_SLOTNO );
    }
}

// **********************************************************
// returns length and pointer to record with RID rid.  The difference
// between this and getRecord is that getRecord copies out the record
// into recPtr, while this function returns a pointer to the record
// in recPtr.
Status HFPage::returnRecord(RID rid, char*& recPtr, int& recLen)
{
    int slotNo = rid.slotNo;
    int offset;

    if ((slotNo < slotCnt) && (slot[slotNo].length > 0)) {

        offset = slot[slotNo].offset;  // extract offset in data[]
        recLen = slot[slotNo].length;  // return length of record
        recPtr = &(data[offset]);      // return pointer to record

        return OK;
    } else {
        return MINIBASE_FIRST_ERROR( HEAPFILE, INVALID_SLOTNO );
    }
}

// **********************************************************
// Returns the amount of available space on the heap file page.
// You will have to compare it with the size of the record to
// see if the record will fit on the page.
int HFPage::available_space(void)
{

    // look for an empty slot.  if one exists, then freeSpace 
    // bytes are available to hold a record.

    int i;
    for (i=0; i < slotCnt; i++) {
        if (slot[i].length == EMPTY_SLOT)
	  return freeSpace;
    }
  
    // no empty slot exists.  must reserve sizeof(slot_t) bytes
    // from freeSpace to hold new slot.

    return freeSpace - sizeof(slot_t);
}

// **********************************************************
// Returns True if the HFPage is empty, and False otherwise.
// It scans the slot directory looking for a non-empty slot.
bool HFPage::empty(void)
{
    int i;

      // look for an empty slot
    for (i=0; i < slotCnt; i++)
        if (slot[i].length != EMPTY_SLOT)
            return false;

    return true;
}

// **********************************************************
// Start from the begining of the slot directory.
// We maintain two offsets into the directory.
//   o first free slot       (ffs) 
//   o current scan position (current_scan_posn)
// Used slots after empty slots are copied to the ffs.
// Shifting the whole array is done rather than filling
// the holes since the array may be sorted...
#if 0
void HFPage::compact_slot_dir(void)
{
   int  current_scan_posn =  0;
   int  first_free_slot   = -1;   // An invalid position.
   bool move = false;             // Move a record? -- initially false


   while (current_scan_posn < slotCnt) {
       if ((slot[current_scan_posn].length == EMPTY_SLOT)
                && (move == false)) {
           move = true;
           first_free_slot = current_scan_posn;
       } else if ((slot[current_scan_posn].length != EMPTY_SLOT)
                 && (move == true)) {
//         cout << "Moving " << current_scan_posn << " --> "
//              << first_free_slot << endl;
           slot[first_free_slot].length = slot[current_scan_posn].length;
           slot[first_free_slot].offset = slot[current_scan_posn].offset;
     
             // Mark the current_scan_posn as empty
           slot[current_scan_posn].length = EMPTY_SLOT;

             // Now make the first_free_slot point to the next free slot.
           first_free_slot++;

                 // slot[current_scan_posn].length == EMPTY_SLOT !!
           while (slot[first_free_slot].length != EMPTY_SLOT)  
               first_free_slot++;
       }
       current_scan_posn++;
    }

   if (move == true) {
       // Adjust amount of free space on page and slotCnt
       freeSpace += sizeof(slot_t) * (slotCnt - first_free_slot);
       slotCnt = first_free_slot;
   }

}

#endif
// **********************************************************
