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
  "You are trying to free a pinned page",
  "Cannot remove page from candidates"
};

// Create a static "error_string_table" object and register the error messages
// with minibase system 
static error_string_table bufTable(BUFMGR,bufErrMsgs);

BufMgr::BufMgr (int numbuf, Replacer *replacer) {
  bufferSize = numbuf;
  bufPool = new Page[bufferSize];
  bufDesc = new Descriptor[bufferSize];
}

PageId BufMgr::findEmptyPos() {
  for (int i=0; i<bufferSize; i++) {
    if (bufDesc[i].page_number == INVALID_PAGE) {
      return i;
    }
  }
  return INVALID_PAGE;
}

PageId BufMgr::findPage(PageId pageId) {
  for (int i=0; i<bufferSize; i++) {
    if (bufDesc[i].page_number == pageId) {
      return i;
    }
  }
  return INVALID_PAGE;
}

int BufMgr::findReplacePos() {
  int replacePos = INVALID_PAGE;
  if(!hated.empty()){
    replacePos = hated.back();
    hated.pop_back();
  }
  else if(!loved.empty()){
    for(auto iter = loved.begin(); iter!=loved.end(); ++iter) {
      if(bufDesc[*iter].pin_count == 0){
        loved.erase(iter);
        replacePos = *iter;
        break;
      }
    }
  }

  return replacePos;
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
      return MINIBASE_FIRST_ERROR(BUFMGR, BUFFER_FULL_ERROR);
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
    return MINIBASE_FIRST_ERROR(BUFMGR, BUFFER_FULL_ERROR);
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
    return MINIBASE_FIRST_ERROR(BUFMGR, PAGE_NOT_FOUND);
  }
  if(bufDesc[pageIndex].pin_count<=0) {
    return MINIBASE_FIRST_ERROR(BUFMGR, PIN_NUMBER_ERROR);
  }
  if(bufDesc[pageIndex].pin_count==1) {
    if(hate){
      hated.push_back(pageIndex);
    }
    else{
      loved.push_back(pageIndex);
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

    status = removeFromCandidate(pagePos);
    if (status!=OK) {
      return status;
    }
  } else {
    return MINIBASE_FIRST_ERROR(BUFMGR, PINNED_PAGE_FREE_ERROR);
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

Status BufMgr::removeFromCandidate(int pagePos) {
  for(auto iter = loved.begin();iter!=loved.end();++iter){
    if(*iter==pagePos){
      loved.erase(iter);
      return OK;
    }
  }

  for(auto iter = hated.begin();iter!=hated.end();++iter){
    if(*iter==pagePos){
      hated.erase(iter);
      return OK;
    }
  }

  return MINIBASE_FIRST_ERROR(BUFMGR, CANDIDATE_REMOVAL_ERROR);
}
