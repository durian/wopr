// $Id: woprsm.cc 3329 2009-12-10 07:49:45Z pberck $

// ---------------------------------------------------------------------------
//  Includes.
// ---------------------------------------------------------------------------

#include <string>
#include <iostream>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/msg.h>

#include <stdlib.h>  
#include <string.h>  

// ---------------------------------------------------------------------------
//  Code.
// ---------------------------------------------------------------------------

struct wopr_msgbuf {
  long mtype;
  int verbose;
  int foo;
};

/*!
  \brief Main.
*/
int main( int argc, char* argv[] ) {

  key_t wopr_id = 88;
  if ( argc == 2 ) {
    wopr_id = atoi( argv[1] );
  }

  struct wopr_msgbuf wmb = {2, 0, 0}; // verbosity, stop

  int msqid = msgget( wopr_id, 0666 | IPC_CREAT );
  
  msgsnd( msqid, &wmb, sizeof(wmb), 0 );

  return 0;
}

// ---------------------------------------------------------------------------
