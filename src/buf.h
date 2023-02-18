///////////////////////////////////////////////////////////////////////////////
/////////////  The Header File for the Buffer Manager /////////////////////////
///////////////////////////////////////////////////////////////////////////////


#ifndef BUF_H
#define BUF_H

#include "db.h"
#include "page.h"
#include<list>

#define NUMBUF 20   
// Default number of frames, artifically small number for ease of debugging.

#define HTSIZE 7
// Hash Table size


class Descriptor {
public:
    PageId page_number = INVALID_PAGE;
    int pin_count = 0;
    bool dirtybit = false;
};


/*******************ALL BELOW are purely local to buffer Manager********/

// You should create enums for internal errors in the buffer manager.
enum bufErrCodes  {
    PAGE_NOT_FOUND,
    PIN_NUMBER_ERROR,
    BUFFER_FULL_ERROR,
    PINNED_PAGE_FREE_ERROR,
    CANDIDATE_REMOVAL_ERROR
};

class Replacer;

class BufMgr {

private: // fill in this area
    Descriptor* bufDesc;
    int bufferSize;
    
    list<PageId> frameIndexer;
    list<int> loved;
    list<int> hated;

public:

    Page* bufPool; // The actual buffer pool

    BufMgr (int numbuf, Replacer *replacer = 0); 
    // Initializes a buffer manager managing "numbuf" buffers.
	// Disregard the "replacer" parameter for now. In the full 
  	// implementation of minibase, it is a pointer to an object
	// representing one of several buffer pool replacement schemes.

    ~BufMgr();           // Flush all valid dirty pages to disk

    PageId findEmptyPos();

    PageId findPage(PageId pageId);

    int findReplacePos();

    Status pinPage(PageId PageId_in_a_DB, Page*& page, int emptyPage=0);
        // Check if this page is in buffer pool, otherwise
        // find a frame for this page, read in and pin it.
        // also write out the old page if it's dirty before reading
        // if emptyPage==TRUE, then actually no read is done to bring
        // the page

    Status unpinPage(PageId globalPageId_in_a_DB, int dirty, int hate);
        // hate should be TRUE if the page is hated and FALSE otherwise
        // if pincount>0, decrement it and if it becomes zero,
        // put it in a group of replacement candidates.
        // if pincount=0 before this call, return error.

    Status newPage(PageId& firstPageId, Page*& firstpage, int howmany=1); 
        // call DB object to allocate a run of new pages and 
        // find a frame in the buffer pool for the first page
        // and pin it. If buffer is full, ask DB to deallocate 
        // all these pages and return error

    Status freePage(PageId globalPageId); 
        // user should call this method if it needs to delete a page
        // this routine will call DB to deallocate the page 

    Status flushPage(PageId pageid);
        // Used to flush a particular page of the buffer pool to disk
        // Should call the write_page method of the DB class

    Status flushAllPages();
	// Flush all pages of the buffer pool to disk, as per flushPage.

    /* DO NOT REMOVE THIS METHOD */    
    Status unpinPage(PageId globalPageId_in_a_DB, int dirty=FALSE)
        //for backward compatibility with the libraries
    {
      return unpinPage(globalPageId_in_a_DB, dirty, FALSE);
    }

    Status removeFromCandidate(int pagePos);
};

#endif
