#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <assert.h>
#include <unistd.h>

#include "buf.h"
#include "db.h"
#include <pwd.h>


#include "BMTester.h"

//extern "C" int getpid();
//extern "C" int unlink( const char* );


BMTester::BMTester() : TestDriver( "buftest" )
{}


BMTester::~BMTester()
{}

//----------------------------------------------------
// test 1
//      Testing pinPage, unpinPage, and whether a dirty page 
//      is written to disk
//----------------------------------------------------

int BMTester::test1() 
{
	Status st;
	Page*	pg;
	int	first,last;
	char data[200]; 
	first = 5;
	last = first + NUMBUF + 5;

	cout << "--------------------- Test 1 ----------------------\n";
        st = OK;
	for (int i=first;i<=last;i++){
		if (MINIBASE_BM->pinPage(i,pg,0)!=OK)  {
       		        st = FAIL;
			MINIBASE_SHOW_ERRORS();
        	}
        	cout<<"after pinPage" << i <<endl;
		sprintf(data,"This is test 1 for page %d\n",i);
		strcpy((char*)pg,data);
		if (MINIBASE_BM->unpinPage(i,1)!=OK) {
            		st = FAIL;
			MINIBASE_SHOW_ERRORS();
        	}
        	cout<<"after unpinPage"<< i << endl;
    	}

	cout << "\n" << endl;

    	for (int i=first;i<=last;i++){
       		if (MINIBASE_BM->pinPage(i,pg,0)!=OK) {
            		st = FAIL;
			MINIBASE_SHOW_ERRORS();
        	}
        	cout<<"PAGE["<<i<<"]: "<<(char *)pg;
        	sprintf(data,"This is test 1 for page %d\n",i);
        	if (strcmp(data,(char*)pg)) {
            		st = FAIL;
            		cerr << "Error: page content incorrect!\n";
        	}
        	if (MINIBASE_BM->unpinPage(i)!=OK)  {
            		st = FAIL;
			MINIBASE_SHOW_ERRORS();
        	}
        }
	minibase_errors.clear_errors();
	return st == OK;
}

//-----------------------------------------------------------
// test 2
//------------------------------------------------------------

int BMTester::test2()
{
    return TRUE;
}

//---------------------------------------------------------
// test 3
//      Testing  newPage,pinPage, freePage, error protocol
//---------------------------------------------------------


int BMTester::test3() 
{
  Status st;
  int pages[30];
  Page* pagesptrs[30];
  Page* pgptr;
  int i;
  
  cout << "--------------------- Test 3 ----------------------\n";
  st = OK;
  // Allocate 10 pages from database
  for ( i = 0; i < 10; i++)
    {
      if (MINIBASE_BM->newPage(pages[i], pagesptrs[i]) !=OK) {
        st = FAIL;
	cout << "\tnewPage failed...\n";
      }
    }
  

  // Pin first 10 pages a second time 
  for (i = 0; i < 10; i++)
    {
      cout << "Pinning page " << i << " " << pages[i] << endl;
      if (MINIBASE_BM->pinPage(pages[i], pgptr)!= OK) {
        st = FAIL;
	cout << "\tpinPage failed...\n";
      }
      if (pgptr != pagesptrs[i]) {
        st = FAIL;
	cout << "\tPinning error in a second time ...\n";
      }
    }


  // Try to free pinned pages
  for (i = 5; i < 10; i++)
    {
      cout << "Freeing page " << pages[i] << endl;
      if (MINIBASE_BM->freePage(pages[i]) == OK) {
        st = FAIL;
	cerr << "Error: pinned page freed!\n";
      }
    }

  // Now free page 0 thru 9 by first unpinning each page twice


  for (i = 0; i < 10; i++)
  {
   
    if (MINIBASE_BM->unpinPage(pages[i]) !=OK) {
      st = FAIL;
      MINIBASE_SHOW_ERRORS();
    }
    if (MINIBASE_BM->unpinPage(pages[i]) !=OK) {
      st = FAIL;
      MINIBASE_SHOW_ERRORS();
    }
    if (MINIBASE_BM->freePage(pages[i]) !=OK) {
      st = FAIL;
      MINIBASE_SHOW_ERRORS();
    }
    cout << "free  page " << pages[i] << endl;
  }

  // Get 14 more pages
  for (i = 10; i < 24; i++)
  {
    if(MINIBASE_BM->newPage(pages[i], pagesptrs[i])!=OK) {
      st = FAIL;
      MINIBASE_SHOW_ERRORS();
    }
     cout << "new  page " << i << "," << pages[i] << endl;
  }
  minibase_errors.clear_errors();
  return st == OK;
}




//-------------------------------------------------------------
// test 4
//-------------------------------------------------------------

int BMTester::test4(){
  return TRUE;
}

//-------------------------------------------------------------
// test 5
//-------------------------------------------------------------

int BMTester::test5(){
  return TRUE;
}

//----------------------------------------------------------
// Test 6
//-----------------------------------------------------------

int BMTester::test6()
{
  return TRUE;
}

const char* BMTester::testName()
{
    return "Buffer Management";
}


void BMTester::runTest( Status& status, TestDriver::testFunction test )
{
    minibase_globals = new SystemDefs( status, dbpath, logpath, 
				  NUMBUF+50, 500, NUMBUF, "Clock" );
    if ( status == OK )
      {
        TestDriver::runTest(status,test);
        delete minibase_globals; 
	minibase_globals = 0;
      }

    char* newdbpath;
    char* newlogpath;
    char remove_logcmd[50];
    char remove_dbcmd[50];

    newdbpath = new char[ strlen(dbpath) + 20];
    newlogpath = new char[ strlen(logpath) + 20];
    strcpy(newdbpath,dbpath);
    strcpy(newlogpath, logpath);

    sprintf(remove_logcmd, "/bin/rm -rf %s", logpath);
    sprintf(remove_dbcmd, "/bin/rm -rf %s", dbpath);
    system(remove_logcmd);
    system(remove_dbcmd);
    sprintf(newdbpath, "%s", dbpath);
    sprintf(newlogpath, "%s", logpath);


    unlink( newdbpath );
    unlink( newlogpath );

    delete newdbpath; delete newlogpath;

}


Status BMTester::runTests()
{
    return TestDriver::runTests();
}


Status BMTester::runAllTests()
{
    return TestDriver::runAllTests();
}
