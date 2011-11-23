// ---------------------------------------------------------------------------
// $Id: $
// ---------------------------------------------------------------------------

/*****************************************************************************
 * Copyright 2007 - 2011 Peter Berck                                         *
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>
#include <iterator>

#include <math.h>
#include <unistd.h>
#include <stdio.h>

#include "qlog.h"
#include "util.h"
#include "Config.h"
#include "runrunrun.h"
#include "server.h"
#include "prededit.h"
#include "Context.h"

#include "MersenneTwister.h"

// Socket stuff

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

// ----

#ifdef TIMBL
#include "timbl/TimblAPI.h"
#endif

#ifdef TIMBLSERVER
#include "SocketBasics.h"
#endif


/*
  Predictive Editing.

  ./wopr -r pdt -p ibasefile:test/reuters.martin.tok.1e5.l2r0_-a1+D.ibase,timbl:"-a1 +D",filename:test/abc.txt
*/
#ifdef TIMBL
struct distr_elem {
  std::string name;
  double      freq;
  double      s_freq;
  bool operator<(const distr_elem& rhs) const {
    return freq > rhs.freq;
  }
};
void generate_next( Timbl::TimblAPI*, Config&, std::string, std::vector<distr_elem>& );
void generate_tree( Timbl::TimblAPI*, Config&, Context&, std::vector<std::string>&, int, std::string );

int pdt( Logfile& l, Config& c ) {
  l.log( "predictive editing" );
  const std::string& start           = c.get_value( "start", "" );
  const std::string  filename        = c.get_value( "filename", to_str(getpid()) ); // "test"file
  const std::string& ibasefile       = c.get_value( "ibasefile" );
  const std::string& timbl           = c.get_value( "timbl" );
  int                lc              = stoi( c.get_value( "lc", "2" ));
  int                rc              = stoi( c.get_value( "rc", "0" )); // should be 0
  int                mode            = stoi( c.get_value( "mode", "1" ));
  bool               show_counts     = stoi( c.get_value( "sc", "0" )) == 1;
  std::string        output_filename = filename + ".pdt";

  Timbl::TimblAPI   *My_Experiment;
  std::string        distrib;
  std::vector<std::string> distribution;
  double             distance;

  l.inc_prefix();
  l.log( "filename:   "+filename );
  l.log( "ibasefile:  "+ibasefile );
  l.log( "timbl:      "+timbl );
  l.log( "lc:         "+to_str(lc) );
  l.log( "rc:         "+to_str(rc) );
  l.log( "mode:       "+to_str(mode) );
  l.log( "OUTPUT:     "+output_filename );
  l.dec_prefix();

  try {
    My_Experiment = new Timbl::TimblAPI( timbl );
    if ( ! My_Experiment->Valid() ) {
      l.log( "Timbl Experiment is not valid." );
      return 1;      
    }
    (void)My_Experiment->GetInstanceBase( ibasefile );
    if ( ! My_Experiment->Valid() ) {
      l.log( "Timbl Experiment is not valid." );
      return 1;
    }
    // My_Experiment->Classify( cl, result, distrib, distance );
  } catch (...) {
    l.log( "Cannot create Timbl Experiment..." );
    return 1;
  }
  l.log( "Instance base loaded." );

  std::ifstream file_in( filename.c_str() );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load file." );
    return -1;
  }
  std::ofstream file_out( output_filename.c_str(), std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write output file." );
    return -1;
  }

  std::string a_line;
  std::string token;
  std::string result;
  std::vector<std::string> results;
  std::vector<std::string> targets;
  std::vector<std::string>::iterator ri;
  const Timbl::ValueDistribution *vd;
  const Timbl::TargetValue *tv;
  std::vector<std::string> words;
  int correct = 0;
  int wrong   = 0;
  int correct_unknown = 0;
  int correct_distr = 0;
  Timbl::ValueDistribution::dist_iterator it;
  int cnt;
  int distr_count;

  MTRand mtrand;

  std::string ectx; // empty context
  for ( int i = 0; i < lc+rc; i++ ) {
    ectx = ectx + "_ ";
  }

  int no_choice = 0;
  int total_choices = 0;
  // abf, average branching factor?

  std::stringstream s;
  int ctx_size = lc+rc;
  Context ctx(ctx_size);
  std::vector<distr_elem> res;
  std::string prev_word = "";

  //std::vector<std::string>::iterator it_endm1 = ctx.begin();
  //advance(it_end,4);

  while( std::getline( file_in, a_line ) ) { 
    if ( a_line == "" ) {
      continue;
    }
    s << a_line;

    // each word in sentence
    //
    while (s >> token) {

      // We got word 'token' (pretend we just pressed space).
      // Add to context, and call wopr.
      //
      ctx.push( token );
      std::cerr << ctx << std::endl;

      // check new token in previous result?
      //
      std::vector<distr_elem>::iterator fi;
      fi = res.begin();
      int cnt = 5;
      while ( (fi != res.end()) && (--cnt >= 0) ) { // cache only those?
	if (  (*fi).name == token ) {
	  l.log( "OK" );
	}
	fi++;
      }

      // TEST
      //
      // NADEEL: this cannot use right context...only advantage is tribl2...?
      //         unless we shift instance bases after hving generated the first 
      //         right context.
      //
      std::vector<std::string> strs;
      std::string t;
      generate_tree( My_Experiment, c, ctx, strs, 3, t );
      std::vector<std::string>::iterator si = strs.begin();
      while ( si != strs.end() ) {
	l.log( (*si) );
	si++;
      }
      strs.clear();
      
      // Stuff next 5 results in res.
      //
      res.clear();
      std::string instance = ctx.toString() + " ?";
      generate_next( My_Experiment, c, instance, res );

      sort( res.begin(), res.end() );

      fi = res.begin();
      cnt = 5;
      std::cerr << res.size() << " [ ";
      while ( (fi != res.end()) && (--cnt >= 0) ) { // cache only those?
	std::cerr << (*fi).name << ' ' << (*fi).freq << ' ';
	fi++;
      }
      std::cerr << "]\n";
      
    }
    s.clear();
  }

  return 0;
}  

// Should implement caching? Threading, ...
// Cache must be 'global', contained in c maybe, or a parameter.
//
void generate_next( Timbl::TimblAPI* My_Experiment, Config& c, std::string instance, std::vector<distr_elem>& distr_vec ) {

  std::string distrib;
  std::vector<std::string> distribution;
  double distance;
  double total_prplx = 0.0;
  const Timbl::ValueDistribution *vd;
  const Timbl::TargetValue *tv;
  std::string result;

  tv = My_Experiment->Classify( instance, vd, distance );
  if ( ! tv ) {
    //l.log( "ERROR: Timbl returned a classification error, aborting." );
    //error
  }
  
  result = tv->Name();	
  size_t res_freq = tv->ValFreq();
  
  double res_p = -1;
  bool target_in_dist = false;
  int target_freq = 0;
  int cnt = vd->size();
  int distr_count = vd->totalSize();
  
  // Grok the distribution returned by Timbl.
  //
  Timbl::ValueDistribution::dist_iterator it = vd->begin();
  while ( it != vd->end() ) {
    
    std::string tvs  = it->second->Value()->Name();
    double      wght = it->second->Weight(); // absolute frequency.

    distr_elem  d;
    d.name   = tvs;
    d.freq   = wght;
    d.s_freq = wght;
    distr_vec.push_back( d );

    ++it;
  } // end loop distribution

  // We can take the top-n here, and recurse into generate_next (add a depth parameter)
  // with each top-n new instance. OR write a wrapper fun.
  
}

// Recursive?
//
void generate_tree( Timbl::TimblAPI* My_Experiment, Config& c, Context& ctx, std::vector<std::string>& strs, int depth, std::string t ) {

  if ( depth == 0 ) {
    // here we should save/print the whole "string" (?)
    //std::cout << "STRING: " << t << std::endl;
    strs.push_back( t );
    return;
  }

  const Timbl::ValueDistribution *vd;
  const Timbl::TargetValue *tv;
  std::string result;
  Timbl::ValueDistribution::dist_iterator it = vd->begin();

  // generate top-n, for each, recurse one down.
  // Need a different res in generate_next, and keep another (kind of) res
  // in this generate_tree.
  //
  std::string instance = ctx.toString() + " ?";
  std::vector<distr_elem> res;
  generate_next( My_Experiment, c, instance, res );
  //std::cerr << "RESSIZE=" << res.size() << std::endl;

  sort( res.begin(), res.end() );
  std::vector<distr_elem>::iterator fi = res.begin();
  int cnt = 3;
  while ( (fi != res.end()) && (--cnt >= 0) ) { 
    
    //for ( int i = 5-depth; i > 0; i--) { std::cout << "  ";}
    //std::cerr << depth << ":" << cnt  << " " << ctx << " -> " << (*fi).name /* << ' ' << (*fi).freq */ << std::endl;

    // Make new context, recurse
    //
    Context new_ctx = Context(ctx);
    new_ctx.push(  (*fi).name );

    //std::cerr << "Recurse down to " << depth-1 << " with " << new_ctx << std::endl;
    generate_tree( My_Experiment, c, new_ctx, strs, depth-1, t+" "+(*fi).name );

    fi++;
  }
  
}
#else
int pdt( Logfile& l, Config& c ) {
  l.log( "No TIMBL support." );
  return -1;
}  
#endif

