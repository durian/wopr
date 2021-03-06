// ---------------------------------------------------------------------------
// $Id: main.cc 15913 2013-04-03 14:11:50Z sloot $
// ---------------------------------------------------------------------------

/*****************************************************************************
 * Copyright 2007-2013 Peter Berck                                           *
 *                                                                           *
 * This file is part of wopr.                                                *
 *                                                                           *
 * wopr is free software; you can redistribute it and/or modify it           *
 * under the terms of the GNU General Public License as published by the     *
 * Free Software Foundation; either version 2 of the License, or (at your    *
 * option) any later version.                                                *
 *                                                                           *
 * wopr is distributed in the hope that it will be useful, but WITHOUT       *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or     *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License      *
 * for more details.                                                         *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with wpred; if not, write to the Free Software Foundation,          *
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA               *
 *****************************************************************************/

// ---------------------------------------------------------------------------
//  Includes.
// ---------------------------------------------------------------------------

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <map>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <getopt.h>

#include <stdlib.h>  
#include <string.h>  

#ifdef HAVE_ICU
#include "unicode/uversion.h"
#endif

#include "qlog.h"
#include "util.h"
#include "Config.h"
#include "runrunrun.h"

// ---------------------------------------------------------------------------
//  Detested globals
// ---------------------------------------------------------------------------

volatile int ctrl_c = 0;

// ---------------------------------------------------------------------------
//  Code.
// ---------------------------------------------------------------------------

/*!
  Handler for control.C
*/
extern "C" void ctrl_c_handler( int sig ) {
  ctrl_c = 1;
}

// ---------------------------------------------------------------------------
//  Subroutines
// ---------------------------------------------------------------------------

/*!
  \brief Main.
*/
int main( int argc, char* argv[] ) {

#ifdef CATCH
  /*
    Set signal handlers for SIGINT, SIGHUP and SIGTERM.
  */
  struct sigaction sig_a;
  sigset_t sig_m;

  sigemptyset( &sig_m );
  sig_a.sa_handler = ctrl_c_handler;
  sig_a.sa_mask    = sig_m;
  sig_a.sa_flags   = 0;

  if ( sigaction( SIGINT, &sig_a, NULL ) == -1 ) {
    std::cerr << "Cannot install SIGINT handler." << std::endl;
    return 1;
  }
  if ( sigaction( SIGHUP, &sig_a, NULL ) == -1 ) {
    std::cerr << "Cannot install SIGHUP handler." << std::endl;
    return 1;
  }
  if ( sigaction( SIGTERM, &sig_a, NULL ) == -1 ) {
    std::cerr << "Cannot install SIGTERM handler." << std::endl;
    return 1;
  }
#endif

  Logfile l;
  std::string blurb = "Starting " + std::string(PACKAGE) + " " +
                      std::string(VERSION);
  l.log( blurb );
#ifndef TRANSPLD2
  l.log( "Transpose is atomic." );
#endif
#ifdef TIMBL
  l.log( "Timbl support built in." );
  if ( TIMBL != "yes" ) {
    l.log( TIMBL );
  }
  l.log( "Based on "+Timbl::VersionName() );
#endif
#ifdef HAVE_FOLIA
  l.log( "Based on "+folia::VersionName() );
#endif
#ifdef HAVE_ICU
  l.log_begin( "ICU support, " );
  printf("version %s\n", U_ICU_VERSION);
#endif

  Config co;
  co.add_kv( "PID", to_str(getpid()) );
  int verbose = 0;
  int log     = 0;
  int c;
  std::vector<std::string>kv_pairs;
  while ( 1 ) {
    int option_index = 0;
    static struct option long_options[] = {
      //{"name, has_arg, *flag, val"} 
      //no_argument, required_argument, optional_argument
      {"config", required_argument, 0, 0},
      {"log", no_argument, 0, 0},
      {"script", required_argument, 0, 0},
      {"params", required_argument, 0, 0},
      {"run", required_argument, 0, 0},
      {"verbose", no_argument, 0, 0},
      {"version", no_argument, 0, 0},
      {"examples", no_argument, 0, 0},
      {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, "c:ls:p:r:veV", long_options, &option_index);
    if (c == -1) {
      break;
    }
	     
    switch ( c ) {
      
    case 0:
      if ( long_options[option_index].name == "verbose" ) {
	verbose = 1;
      }
      if ( long_options[option_index].name == "version" ) {
	return 0;
      }
      if ( long_options[option_index].name == "log" ) {
	log = 1;
      }
      if ( long_options[option_index].name == "config" ) {
	co.read_file( optarg );
	// Note that one can override settings by having them after.
      }
      if ( long_options[option_index].name == "script" ) {
	co.add_kv( "script_filename", optarg );
	co.add_kv( "run", "script" ); // NB, -s implies "-r script".
      }
      if ( long_options[option_index].name == "params" ) {
	Tokenize( optarg, kv_pairs, ',' );
	for ( int i = 0; i < kv_pairs.size(); i++ ) {
	  co.process_line( kv_pairs.at(i), false );
	}
	kv_pairs.clear();
      }
      if ( long_options[option_index].name == "run" ) {
	co.add_kv( "run", optarg );
	// Note that one can override settings by having them after.
	// Depending on the force parameter, which is false here, and
	// true in scripts.
      }
      break;
      
    case 'v':
      verbose = 1;
      break;

    case 'V':
      return 0;
      break;
      
    case 'c':
      co.read_file( optarg );
      break;

    case 'l':
      log = 1;
      break;

    case 's':
      co.add_kv( "script_filename", optarg );
      co.add_kv( "run", "script" ); // NB, -s implies "-r script".
      l.log( optarg );
      break;

    case 'p':
      Tokenize( optarg, kv_pairs, ',' );
      for ( int i = 0; i < kv_pairs.size(); i++ ) {
	co.process_line( kv_pairs.at(i), false );
      }
      kv_pairs.clear();
      break;

    case 'r':
      co.add_kv( "run", optarg );
      break;

    case 'e':
      l.log( "--" );
      l.log( "wopr -r make_ibase -p filename:data.l2r2,timbl:'-a1 +D'" );
      l.log( "wopr -r window_lr -p filename:data.txt,lc:3,rc:1" );
      l.log( "wopr -r ngram -p filename:data.txt,n:3" );
      l.log( "wopr -r tag -p ibasefile:data.ibase" );
      l.log( "wopr -r lexicon,window_lr -p filename:data.txt,lc:2,rc:0,pre_s:'<s>',suf_s:'</s>'" );
      l.log( "wopr -r tag -p ibasefile:data.txt.l4r0.ibase,n:5" );
      l.log( "wopr -r pplxs -p filename:data.test.l2r0,ibasefile:data.train.l2r0_-a1+D.ibase,timbl:'-a1 +D',lexicon:data.lex,topn:5,id:EXP8" );
      l.log( "wopr -r server4 -p ibasefile:austen.50e3.l2r0_-a1+D.ibase,timbl:'-a1 +D',lexicon:austen.50e3.p.lex,mode:1,verbose:2,resm:0,skm:0" );
      l.log( "wopr -r gt -p ibasefile:austen.txt.l3r0.ibase,filename:austen.txt.l3r0,topn:10" );
      l.log( "wopr -r server4 -p port:8888,ibasefile:rmt.5e3.l2r0_-a1+D.ibase,timbl:'-a1 +D',lexicon:rmt.5e3.lex,verbose:1,mode:1,keep:1" );
      l.log( "--" );
      break;

    default:
      //printf ("?? getopt returned character code 0%o ??\n", c);
      l.log( "ERROR: invalid option." );
      exit( 1 );
    }

  } // while
  //co.dump_kv();

  std::ofstream wopr_log;
  if ( ! wopr_log ) {
    l.log( "ERROR: cannot write wopr.log." );
    return -1;
  }

  if ( log == 1 ) {
    wopr_log.open( "wopr.log", std::ios::out | std::ios::app );
    if ( wopr_log ) {
      wopr_log << "# " << the_date_time() << ", wopr " << VERSION << " (";
      wopr_log << to_str(getpid()) << ")" << std::endl;
      wopr_log << "wopr";
      for (int i = 1; i < argc; i++) {
	wopr_log << " ";
	std::string s = std::string( argv[i] );
	Tokenize( s, kv_pairs, ',' );
	for ( int j = 0; j < kv_pairs.size(); j++ ) {
	  std::string kv = kv_pairs.at(j);
	  if ( kv.find( ' ', 0 ) != std::string::npos ) { // contains space
	    int pos = kv.find( ':', 0 );
	    if ( pos != std::string::npos ) {
	      std::string lhs = trim(kv.substr( 0, pos ));
	      std::string rhs = trim(kv.substr( pos+1 ));
	      wopr_log << lhs << ":'" << rhs << "'"; // single or double?
	    } else {
	      // error.
	    }
	  } /*space*/ else {
	    wopr_log << kv;
	  }
	  if ( j < kv_pairs.size()-1 ) {
	    wopr_log << ",";
	  }
	} /* j */
	kv_pairs.clear();
      } /* i */
      wopr_log << std::endl;
    }
    wopr_log.close();
  }

  l.log( "PID: "+to_str(getpid(),6,' ')+" PPID: "+to_str(getppid(),6,' ') );

  timeval tv;
  l.get_raw(tv);

  //int (*MyPtr)(Logfile*, Config*) = &run_exp;
  //int res = (*MyPtr)(l, co);
  // see: http://www.newty.de/fpt/fpt.html

  std::vector<std::string>funs;
  std::string fun_list = co.get_value( "run" );
  Tokenize( fun_list, funs, ',' );
  for ( int i = 0; i < funs.size(); i++ ) {
    l.log( "Running: "+funs.at(i) );
    int(*pt2Func2)(Logfile&, Config&) = get_function( funs.at(i) );
    int res = pt2Func2(l, co);
    l.log( "Result = "+to_str(res) );
  }

  timeval tv2;
  l.get_raw(tv2);
  long diff = tv2.tv_sec - tv.tv_sec;
  l.log( "Running for "+secs_to_str( diff ));

  if ( log == 1 ) {
    wopr_log.open( "wopr.log", std::ios::out | std::ios::app );
    if ( wopr_log ) {
      wopr_log << "# " << the_date_time() << ", " << secs_to_str( diff );
      wopr_log << " (" << to_str(getpid()) << ")" << std::endl;
      wopr_log << std::endl << std::endl;
    }
    wopr_log.close();
  }

  l.log( "Ready." );

  return 0;
}

// ---------------------------------------------------------------------------
