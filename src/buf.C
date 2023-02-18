/*****************************************************************************/
/*************** Implementation of the Buffer Manager Layer ******************/
/*****************************************************************************/


#include "buf.h"


// Define buffer manager error messages here
//enum bufErrCodes  {...};

// Define error message here
static const char* bufErrMsgs[] = { 
  "Sfalma sto pin...",
  "Prospathiseis na epeleutherosis selida pinned",
  "Gematos Buffer",
  "Den mpori na diagrafi i selida",
  "H selida den vrethike",
};

// Create a static "error_string_table" object and register the error messages
// with minibase system 
static error_string_table bufTable(BUFMGR,bufErrMsgs);

BufMgr::BufMgr (int numbuf, Replacer *replacer) {
  bufferSize = numbuf;
  bufPool = new Page[bufferSize];
  bufDesc = new Descriptor[bufferSize];
}


Status BufMgr::pinPage(PageId PageId_in_a_DB, Page*& page, int emptyPage) {
  int empty_no = INVALID_PAGE;
  int page_no = INVALID_PAGE;

  for (int i=0; i<bufferSize; i++){
    if (bufDesc[i].page_number == PageId_in_a_DB) {
      page_no = i;
    }
  }

  for (int i=0; i<bufferSize; i++){
    if (bufDesc[i].page_number == INVALID_PAGE) {
      empty_no = i;
    }
  }

  for (int i=0; i<bufferSize; i++){
    if (empty_no == INVALID_PAGE && bufDesc[i].page_number == INVALID_PAGE) {
      empty_no = i;
    }

    if (bufDesc[i].page_number == PageId_in_a_DB) {
      page_no = i;
    }
  }

  if (page_no != INVALID_PAGE){
    page = bufPool+page_no;

    bufDesc[page_no].pin_count++;
  } else if (empty_no != INVALID_PAGE){

    page = bufPool+empty_no;

    Status status = MINIBASE_DB->read_page(PageId_in_a_DB,page);
    if(status!=OK){
      return MINIBASE_CHAIN_ERROR(BUFMGR, status);
    }
    bufDesc[empty_no].dirtybit = FALSE;
    bufDesc[empty_no].page_number = PageId_in_a_DB;
    bufDesc[empty_no].pin_count = 1;

  } else {
    int page_no = INVALID_PAGE;
    if(!hated.empty()){
      page_no = hated.back();
      hated.pop_back();
    }
    else if(!loved.empty()){
      for(auto page = loved.begin(); page!=loved.end(); ++page) {
        if(bufDesc[*page].pin_count == 0){
          loved.erase(page);
          page_no = *page;
          break;
        }
      }
    }

    if (page_no == INVALID_PAGE) {
      return MINIBASE_FIRST_ERROR(BUFMGR, BUFFER_FULL_ERROR);
    }

    flushPage(bufDesc[page_no].page_number);

    bufDesc[page_no].dirtybit = FALSE;
    bufDesc[page_no].page_number = PageId_in_a_DB;
    bufDesc[page_no].pin_count = 1;

    page = bufPool+page_no;

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
  PageId page_no = INVALID_PAGE;
  for (int i=0; i<bufferSize; i++) {
    if (bufDesc[i].page_number == pageid) {
      page_no = i;
    }
  }

  if(page_no!=INVALID_PAGE && bufDesc[page_no].dirtybit){
    Status write_status = MINIBASE_DB->write_page(pageid, bufPool+page_no);
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
  PageId page_no = INVALID_PAGE;
  for (int i=0; i<bufferSize; i++) {
    if (bufDesc[i].page_number == page_num) {
      page_no = i;
    }
  }

  if(page_no == INVALID_PAGE) {
    return MINIBASE_FIRST_ERROR(BUFMGR, PAGE_NOT_FOUND);
  }
  if(bufDesc[page_no].pin_count<=0) {
    return MINIBASE_FIRST_ERROR(BUFMGR, PIN_NUMBER_ERROR);
  }
  if(bufDesc[page_no].pin_count==1) {
    if(hate){
      hated.push_back(page_no);
    }
    else{
      loved.push_back(page_no);
    }
  }
  bufDesc[page_no].pin_count--;
  bufDesc[page_no].dirtybit = dirty;

  return OK;
}

//*************************************************************
//** This is the implementation of freePage
//************************************************************

Status BufMgr::freePage(PageId globalPageId){
  Status status=OK;
  PageId page_no = INVALID_PAGE;
  for (int i=0; i<bufferSize; i++) {
    if (bufDesc[i].page_number == globalPageId) {
      page_no = i;
    }
  }
  if(page_no!=INVALID_PAGE && bufDesc[page_no].pin_count==0) {
    bufDesc[page_no].dirtybit = false;
    bufDesc[page_no].page_number = INVALID_PAGE;
    bufDesc[page_no].pin_count = 0;

    for(auto page = loved.begin();page!=loved.end();++page){
      if(*page==page_no){
        loved.erase(page);
        status = OK;
        break;
      }
    }

    for(auto page = hated.begin();page!=hated.end();++page){
      if(*page==page_no){
        hated.erase(page);
        status = OK;
        break;
      }
    }

    if (status!=OK) {
     return MINIBASE_FIRST_ERROR(BUFMGR, CANDIDATE_REMOVAL_ERROR);
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
