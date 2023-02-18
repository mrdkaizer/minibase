/*****************************************************************************/
/*************** Implementation of the Buffer Manager Layer ******************/
/*****************************************************************************/


#include "buf.h"


// Define buffer manager error messages here
//enum bufErrCodes  {...};

// Define error message here
static const char* bufErrMsgs[] = { 
  "Page does not exists",
  "Pin count error",
  "Bufferpool is full",
  "You are trying to free a pinned page"
};

// Create a static "error_string_table" object and register the error messages
// with minibase system 
static error_string_table bufTable(BUFMGR,bufErrMsgs);

// CONSTRUCTOR
BufMgr::BufMgr (int numbuf, Replacer *replacer) {
  bufferSize = numbuf;
  bufPool = new Page[bufferSize];
  bufDesc = new Descriptor[bufferSize];
}

// Returns an empty positions if exists in bufDesc
PageId BufMgr::findEmptyPos() {
  for (int i=0; i<bufferSize; i++) {
    if (bufDesc[i].page_number == INVALID_PAGE) {
      return i;
    }
  }
  return INVALID_PAGE;
}

// Return the position of a page in the bufDesc if exists.
PageId BufMgr::findPage(PageId pageId) {
  for (int i=0; i<bufferSize; i++) {
    if (bufDesc[i].page_number == pageId) {
      return i;
    }
  }
  return INVALID_PAGE;
}

// Find a page by a given status, used to help with replacement policy
PageId BufMgr::findFirstPageByStatus(int status) {
  int time;
  PageId page = INVALID_PAGE;

  for (int i=0; i<bufferSize; i++) {
    if (bufDesc[i].page_number != INVALID_PAGE && bufDesc[i].status == status && (status == HATED || bufDesc[i].pin_count == 0)) {
      if (page == INVALID_PAGE || (status == HATED && time < bufDesc[i].timestamp) || (status == LOVED && time > bufDesc[i].timestamp)) {
        page = i;
        time = bufDesc[i].timestamp;
      }
    }
  }

  return page;
}

// get the replacement page: 
// order 1 - MRU hated, 2 - LRU LOVED
int BufMgr::findReplacePos() {
  int firstHated = findFirstPageByStatus(HATED);
  int firstLoved = findFirstPageByStatus(LOVED);

  return firstHated != INVALID_PAGE ? firstHated : firstLoved;
}

Status BufMgr::pinPage(PageId PageId_in_a_DB, Page*& page, int emptyPage) {
  int firstEmptyPos = findEmptyPos();
  int pageIndex = findPage(PageId_in_a_DB);

  for (int i=0; i<bufferSize; i++) {
    if (firstEmptyPos == INVALID_PAGE && bufDesc[i].page_number == INVALID_PAGE) {
      firstEmptyPos = i;
    }

    if (bufDesc[i].page_number == PageId_in_a_DB) {
      pageIndex = i;
    }
  }

  if (pageIndex != INVALID_PAGE) {
    page = bufPool+pageIndex;

    bufDesc[pageIndex].pin_count++;
  } else if (firstEmptyPos != INVALID_PAGE){

    page = bufPool+firstEmptyPos;

    Status status = MINIBASE_DB->read_page(PageId_in_a_DB,page);
    if(status!=OK){
      return MINIBASE_CHAIN_ERROR(BUFMGR, status);
    }
    bufDesc[firstEmptyPos].dirtybit = FALSE;
    bufDesc[firstEmptyPos].page_number = PageId_in_a_DB;
    bufDesc[firstEmptyPos].pin_count = 1;

  } else {
    int replacePos = findReplacePos();

    if (replacePos == INVALID_PAGE) {
      return MINIBASE_FIRST_ERROR(BUFMGR, MEMERR);
    }

    if (bufDesc[replacePos].dirtybit == TRUE) {
      MINIBASE_DB->write_page(bufDesc[replacePos].page_number, bufPool+replacePos);
    }

    bufDesc[replacePos].dirtybit = FALSE;
    bufDesc[replacePos].page_number = PageId_in_a_DB;
    bufDesc[replacePos].pin_count = 1;

    page = bufPool+replacePos;

    MINIBASE_DB->read_page(PageId_in_a_DB, page);
  }

  return OK;
}//end pinPage


Status BufMgr::newPage(PageId& firstPageId, Page*& firstpage, int howmany) {
  Status status = MINIBASE_DB->allocate_page(firstPageId, howmany);
  if(status!=OK){
    return MINIBASE_CHAIN_ERROR(BUFMGR, status);
  }
  if(pinPage(firstPageId, firstpage)!=OK){
    status = MINIBASE_DB->deallocate_page(firstPageId,howmany);
    if(status!=OK){
      return MINIBASE_CHAIN_ERROR(BUFMGR, status);
    } 
    return MINIBASE_FIRST_ERROR(BUFMGR, MEMERR);
  }
  return OK;
}

Status BufMgr::flushPage(PageId pageid) {
  // put your code here
  int pageIndex = findPage(pageid);

  if(pageIndex!=INVALID_PAGE && bufDesc[pageIndex].dirtybit){
    Status write_status = MINIBASE_DB->write_page(pageid, bufPool+pageIndex);
    if(write_status!=OK){
      return MINIBASE_FIRST_ERROR(BUFMGR,write_status);
    }
  }
  return OK;
}
    
	  
//*************************************************************
//** This is the implementation of ~BufMgr
//************************************************************
BufMgr::~BufMgr(){
  flushAllPages();
  delete[] bufDesc;
  delete[] bufPool;
}


//*************************************************************
//** This is the implementation of unpinPage
//************************************************************

Status BufMgr::unpinPage(PageId page_num, int dirty=FALSE, int hate = FALSE) {
  int pageIndex = findPage(page_num);

  if(pageIndex == INVALID_PAGE) {
    return MINIBASE_FIRST_ERROR(BUFMGR, PAGENOTFOUNDERR);
  }
  if(bufDesc[pageIndex].pin_count<=0) {
    return MINIBASE_FIRST_ERROR(BUFMGR, PINCOUNTERR);
  }
  if(bufDesc[pageIndex].pin_count==1) {
    if(hate){
      bufDesc[pageIndex].status = HATED;
      bufDesc[pageIndex].timestamp = ++globalTime;
    }
    else{
      bufDesc[pageIndex].status = LOVED;
      bufDesc[pageIndex].timestamp = ++globalTime;
    }
  }
  bufDesc[pageIndex].pin_count--;
  bufDesc[pageIndex].dirtybit = dirty;

  return OK;
}

//*************************************************************
//** This is the implementation of freePage
//************************************************************

Status BufMgr::freePage(PageId globalPageId){
  Status status=OK;
  int pagePos = findPage(globalPageId);
  if(pagePos!=INVALID_PAGE && bufDesc[pagePos].pin_count==0) {
    bufDesc[pagePos].dirtybit = false;
    bufDesc[pagePos].page_number = INVALID_PAGE;
    bufDesc[pagePos].pin_count = 0;
    bufDesc[pagePos].status = UKNOWN;

    if (status!=OK) {
      return status;
    }
  } else {
    return MINIBASE_FIRST_ERROR(BUFMGR, FREEPINPAGEERR);
  }

  status = MINIBASE_DB->deallocate_page(globalPageId);
  if(status!=OK){
    return MINIBASE_CHAIN_ERROR(BUFMGR, status);
  }
  return OK;
}

Status BufMgr::flushAllPages(){
  for (int i = 0; i < bufferSize; i++) {
    if (bufDesc[i].page_number != INVALID_PAGE){
      flushPage(bufDesc[i].page_number);
    }
  }
  return OK;
}

