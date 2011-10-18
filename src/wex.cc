// ---------------------------------------------------------------------------
// $Id: wex.cc 9080 2011-03-23 10:34:23Z pberck $
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
#include <vector>
#include <algorithm>
#include <iterator>

#include "math.h"

#include "qlog.h"
#include "Config.h"
#include "util.h"
#include "runrunrun.h"
#include "Multi.h"
#include "ngrams.h"
#include "wex.h"

// ---------------------------------------------------------------------------
//  Code.
// ---------------------------------------------------------------------------

#ifdef TIMBL
// wopr -r multi -p kvs:ibases.kvs,filename:zin.04,lexicon:austen.txt.ng2.lex
//
int multi( Logfile& l, Config& c ) {
  l.log( "multi" );
  const std::string& filename         = c.get_value( "filename" );
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  const std::string& counts_filename  = c.get_value( "counts" );
  const std::string& kvs_filename     = c.get_value( "kvs" );
  const std::string& timbl            = c.get_value( "timbl" );
  // PJB: Should be a context string, also l2r2 &c.
  int                ws               = stoi( c.get_value( "ws", "3" ));
  bool               to_lower         = stoi( c.get_value( "lc", "0" )) == 1;
  std::string        output_filename  = filename + ".px" + to_str(ws);
  std::string        pre_s            = c.get_value( "pre_s", "<s>" );
  std::string        suf_s            = c.get_value( "suf_s", "</s>" );
  int                skip             = 0;
  Timbl::TimblAPI   *My_Experiment;
  std::string        distrib;
  std::vector<std::string> distribution;
  std::string        result;
  double             distance;

  l.inc_prefix();
  l.log( "filename:   "+filename );
  l.log( "lexicon:    "+lexicon_filename );
  l.log( "counts:     "+counts_filename );
  l.log( "kvs:        "+kvs_filename );
  l.log( "timbl:      "+timbl );
  l.log( "ws:         "+to_str(ws) );
  l.log( "lowercase:  "+to_str(to_lower) );
  l.log( "OUTPUT:     "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename.c_str() );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load inputfile." );
    return -1;
  }
  std::ofstream file_out( output_filename.c_str(), std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write output file." );
    return -1;
  }

  // Load lexicon. NB: hapaxed lexicon is different? Or add HAPAX entry?
  //
  int wfreq;
  unsigned long total_count = 0;
  unsigned long N_1 = 0; // Count for p0 estimate.
  std::map<std::string,int> wfreqs; // whole lexicon
  std::ifstream file_lexicon( lexicon_filename.c_str() );
  if ( ! file_lexicon ) {
    l.log( "NOTICE: cannot load lexicon file." );
    //return -1;
  } else {
    // Read the lexicon with word frequencies.
    // We need a hash with frequence - countOfFrequency, ffreqs.
    //
    l.log( "Reading lexicon." );
    std::string a_word;
    while ( file_lexicon >> a_word >> wfreq ) {
      wfreqs[a_word] = wfreq;
      total_count += wfreq;
      if ( wfreq == 1 ) {
	++N_1;
      }
    }
    file_lexicon.close();
    l.log( "Read lexicon (total_count="+to_str(total_count)+")." );
  }

  // If we want smoothed counts, we need this file...
  // Make mapping <int, double> from c to c* ?
  //
  std::map<int,double> c_stars;
  int Nc0;
  double Nc1; // this is c*
  int count;
  std::ifstream file_counts( counts_filename.c_str() );
  if ( ! file_counts ) {
    l.log( "NOTICE: cannot read counts file, no smoothing will be applied." ); 
  } else {
    while( file_counts >> count >> Nc0 >> Nc1 ) {
      c_stars[count] = Nc1;
    }
    file_counts.close();
  }

  // The P(new_word) according to GoodTuring-3.pdf
  // We need the filename.cnt for this, because we really need to
  // discount the rest if we assign probability to the unseen words.
  //
  // We need to esitmate the total number of unseen words. Same as
  // vocab, i.e assume we saw half? Ratio of N_1 to total_count?
  //
  // We need to load .cnt file as well...
  //
  double p0 = 0.00001; // Arbitrary low probability for unknown words.
  if ( total_count > 0 ) { // Better estimate if we have a lexicon
    p0 = (double)N_1 / ((double)total_count * total_count);
  }

  // read kvs
  //
  std::map<int, Classifier*> ws_classifier; // size -> classifier.
  std::map<int, Classifier*>::iterator wsci;
  std::vector<Classifier*> cls;
  std::vector<Classifier*>::iterator cli;
  if ( kvs_filename != "" ) {
    l.log( "Reading classifiers." );
    std::ifstream file_kvs( kvs_filename.c_str() );
    if ( ! file_kvs ) {
      l.log( "ERROR: cannot load kvs file." );
      return -1;
    }
    read_classifiers_from_file( file_kvs, cls );
    l.log( to_str((int)cls.size()) );
    file_kvs.close();
    for ( cli = cls.begin(); cli != cls.end(); cli++ ) {
      l.log( (*cli)->id );
      (*cli)->init();
      int c_ws = (*cli)->get_ws();
      ws_classifier[c_ws] = *cli;
    }
    l.log( "Read classifiers." );
  }

  std::vector<std::string>::iterator vi;
  std::ostream_iterator<std::string> output( file_out, " " );

  std::string a_line;
  std::vector<std::string> results;
  std::vector<std::string> targets;
  std::vector<std::string>::iterator ri;
  const Timbl::ValueDistribution *vd;
  const Timbl::TargetValue *tv;
  std::vector<Multi*> multivec( 20 );
  std::vector<std::string> words;

  while( std::getline( file_in, a_line )) {

    // How long is the sentence? initialise Multi.h in advance,
    // one per word in the sentence?
    // Initialise correct target as well...
    //
    int multi_idx = 0;
    multivec.clear();
    words.clear();
    Tokenize( a_line, words, ' ' );
    int sentence_size = words.size();
    for ( int i = 0; i < sentence_size; i++ ) {
      multivec[i] = new Multi("");
      multivec[i]->set_target( words[i] );
    }

    // We loop over classifiers, vote which result we take.
    //
    for ( cli = cls.begin(); cli != cls.end(); cli++ ) {

      Classifier *classifier = *cli;
      l.log( "Classifier: " + (*cli)->id );
      int win_s = (*cli)->get_ws();
      //l.log( "win_s="+to_str(win_s) );

      Timbl::TimblAPI *timbl = classifier->get_exp();

      //      pattern target  lc    rc  backoff
      window( a_line, a_line, win_s, 0, false, results ); 

      // For each classifier, make data and run. We need to specify how to
      // make data. We have the ws parameter in the Classifier.
      //
      // If we get different size patterns (first 1, then 2, then 3, 3, 3) we
      // can choose he right classifier based on the size of the pattern.
      // So maybe we need yet-another-window function which does that
      // (essentially patterns without the _ markers).
      //
      // Add a function to the kvs file to make data?
      //
      // We need a data structure to gather all the results and probability
      // values... (classifier, classification, distr?, prob....)
      //
      multi_idx = 0;
      for ( ri = results.begin(); ri != results.end(); ri++ ) {
	std::string cl = *ri;
	file_out << cl << std::endl;

	tv = timbl->Classify( cl, vd );
	if ( ! tv ) {
	  l.log( "ERROR: Timbl returned a classification error, aborting." );
	  break;
	}
	std::string answer = tv->Name();

	int cnt = vd->size();
	int distr_count = vd->totalSize();

	l.log( to_str(multi_idx) + ": " + cl + "/" + answer + " "+to_str(cnt) );

	multivec[multi_idx]->add_string( answer );
	multivec[multi_idx]->add_answer( classifier, answer );

	++multi_idx;
      } // results
      results.clear();
    } // classifiers

    for ( int i = 0; i < sentence_size; i++ ) {
      //l.log( words[i] + ": " + multivec[i]->get_answer() );
      l.log( multivec[i]->get_answers() );
    }
  }

  file_out.close();
  file_in.close();

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}
#else
int multi( Logfile& l, Config& c ) {
  l.log( "Timbl support not built in." );  
  return -1;
}
#endif

// Fill a keyword value.
//
int read_kv_from_file( std::ifstream& file,
		       std::map<std::string, std::string>& res )  {
  std::string a_line;
  while( std::getline( file, a_line )) {
    if ( a_line.length() == 0 ) {
      continue;
    }
    int pos = a_line.find( ':', 0 );
    if ( pos != std::string::npos ) {
      std::string lhs = trim(a_line.substr( 0, pos ));
      std::string rhs = trim(a_line.substr( pos+1 ));
      res[lhs] = rhs;
    }
  }
  
  return 0;
}

// Read the classifiers.
// classifier: <NAME> begins a new classifier "block"
// ibasefile: <FILE> the trained instance base
// ws: <N> windowing size, only used in old multi function
// timbl: <PARAMS> how the instance based was trained
// testfile: <FILE> file to run through the classifier
// weight: <N> weight for this classifier when merging
// distfile: <FILE> file with (global) distribution to merge
//
int read_classifiers_from_file( std::ifstream& file,
				std::vector<Classifier*>& cl )  {
  std::string a_line;
  Classifier* c = NULL;
  
  while( std::getline( file, a_line )) {
    if ( a_line.length() == 0 ) {
      continue;
    }
    int pos = a_line.find( ':', 0 );
    if ( pos != std::string::npos ) {
      std::string lhs = trim(a_line.substr( 0, pos ));
      std::string rhs = trim(a_line.substr( pos+1 ));

      // Create a new one. If c != NULL, we store it in the cl vector.
      //
      if ( lhs == "classifier" ) {
	if ( c != NULL ) {
	  //store
	  cl.push_back( c );
	  c = NULL;
	}
	c = new Classifier( rhs );
	c->set_weight( 1.0 );
	c->set_testfile( "NONE" );
      } else if ( lhs == "ibasefile" ) {
	// store this ibasefile
	if ( c != NULL ) {
	  c->set_ibasefile( rhs );
	  c->set_type( 1 );
	}
      } else if ( lhs == "ws" ) {
	// store this window size
	if ( c != NULL) {
	    c->set_ws( stoi(rhs) );
	}
      } else if ( lhs == "timbl" ) {
	// e.g: "-a1 +vdb +D +G"
	if ( c != NULL) {
	    c->set_timbl( rhs );
	}
      } else if ( lhs == "testfile" ) {
	if ( c != NULL) {
	    c->set_testfile( rhs );
	}
      } else if ( lhs == "weight" ) {
	if ( c != NULL) {
	  c->set_weight( stod(rhs) );
	}
      } else if ( lhs == "distprob" ) {
	if ( c != NULL) {
	  c->set_distprob( stod(rhs) );
	}
      } else if ( lhs == "distfile" ) {
	if ( c != NULL) {
	  c->set_distfile( rhs );
	  c->set_type( 2 );
	}
      } else if ( lhs == "gatetrigger" ) {
	if ( c != NULL) {
	  c->set_gatetrigger( rhs );
	  c->set_type( 3 );
	}
      } else if ( lhs == "gatepos" ) {
	if ( c != NULL) {
	  c->set_gatepos( stoi(rhs) );
	  c->set_type( 3 );
	}
      } else if ( lhs == "gatedefault" ) {
	if ( c != NULL) {
	  c->set_type( 4 );
	}
      }
    }
  }
  if ( c != NULL ) {
    cl.push_back( c );
  }
  
  return 0;
}

// After discussion with Antal 09/12/09, mixing of distributions.
// 
struct distr_probs {
  std::string name; // PJB call this token
  long        freq;
  double      prob;
  bool operator<(const distr_probs& rhs) const {
    return prob > rhs.prob;
  }
};
#ifdef TIMBL
int multi_dist( Logfile& l, Config& c ) {
  l.log( "multi_dist" );
  const std::string& filename         = c.get_value( "filename" );
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  const std::string& counts_filename  = c.get_value( "counts" );
  const std::string& kvs_filename     = c.get_value( "kvs" );
  std::string        id               = c.get_value( "id", to_str(getpid()) );
  int                topn             = stoi( c.get_value( "topn", "1" ) );

  if ( contains_id(filename, id) == true ) {
    id = "";
  } else {
    id = "_"+id;
  }
  std::string        output_filename  = filename + id + ".md";

  Timbl::TimblAPI   *My_Experiment;
  std::string        distrib;
  std::vector<std::string> distribution;
  std::string        result;
  double             distance;

  l.inc_prefix();
  l.log( "filename:   "+filename );
  l.log( "lexicon:    "+lexicon_filename );
  l.log( "counts:     "+counts_filename );
  l.log( "kvs:        "+kvs_filename );
  l.log( "topn:       "+to_str(topn) );
  l.log( "id:         "+id );
  l.log( "OUTPUT:     "+output_filename );
  l.dec_prefix();

  std::ifstream file_in( filename.c_str() );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load inputfile." );
    return -1;
  }
  std::ofstream file_out( output_filename.c_str(), std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write output file." );
    file_in.close();
    return -1;
  }

  // Load lexicon. NB: hapaxed lexicon is different? Or add HAPAX entry?
  //
  int wfreq;
  unsigned long total_count = 0;
  unsigned long N_1 = 0; // Count for p0 estimate.
  std::map<std::string,int> wfreqs; // whole lexicon
  std::ifstream file_lexicon( lexicon_filename.c_str() );
  if ( ! file_lexicon ) {
    l.log( "NOTICE: cannot load lexicon file." );
    //return -1;
  } else {
    // Read the lexicon with word frequencies.
    // We need a hash with frequence - countOfFrequency, ffreqs.
    //
    l.log( "Reading lexicon." );
    std::string a_word;
    while ( file_lexicon >> a_word >> wfreq ) {
      wfreqs[a_word] = wfreq;
      total_count += wfreq;
      if ( wfreq == 1 ) {
	++N_1;
      }
    }
    file_lexicon.close();
    l.log( "Read lexicon (total_count="+to_str(total_count)+")." );
  }

  // If we want smoothed counts, we need this file...
  // Make mapping <int, double> from c to c* ?
  //
  std::map<int,double> c_stars;
  int Nc0;
  double Nc1; // this is c*
  int count;
  std::ifstream file_counts( counts_filename.c_str() );
  if ( ! file_counts ) {
    l.log( "NOTICE: cannot read counts file, no smoothing will be applied." ); 
  } else {
    while( file_counts >> count >> Nc0 >> Nc1 ) {
      c_stars[count] = Nc1;
    }
    file_counts.close();
  }

  // The P(new_word) according to GoodTuring-3.pdf
  // We need the filename.cnt for this, because we really need to
  // discount the rest if we assign probability to the unseen words.
  //
  // We need to esitmate the total number of unseen words. Same as
  // vocab, i.e assume we saw half? Ratio of N_1 to total_count?
  //
  // We need to load .cnt file as well...
  //
  double p0 = 0.00001; // Arbitrary low probability for unknown words.
  if ( total_count > 0 ) { // Better estimate if we have a lexicon
    p0 = (double)N_1 / ((double)total_count * total_count);
  }

  // read kvs
  // in multi_dist we don't need the size, we read a testfile instead.
  //
  std::vector<Classifier*> cls;
  std::vector<Classifier*>::iterator cli;
  int classifier_count = 0;
  if ( kvs_filename != "" ) {
    l.log( "Reading classifiers." );
    std::ifstream file_kvs( kvs_filename.c_str() );
    if ( ! file_kvs ) {
      l.log( "ERROR: cannot load kvs file." );
      return -1;
    }
    read_classifiers_from_file( file_kvs, cls );
    l.log( to_str((int)cls.size()) );
    file_kvs.close();
    for ( cli = cls.begin(); cli != cls.end(); cli++ ) {
      l.log( (*cli)->id );
      (*cli)->init();
      ++classifier_count;
      l.log( (*cli)->info_str() );
    }
    l.log( "Read classifiers. Starting classification." );
  }

  std::vector<std::string>::iterator vi;
  std::ostream_iterator<std::string> output( file_out, " " );

  std::string a_line;
  std::vector<std::string> targets;
  std::vector<std::string>::iterator ri;
  const Timbl::ValueDistribution *vd;
  const Timbl::TargetValue *tv;
  std::vector<std::string> words;
  std::vector<distr_probs> distr_vec;
  std::vector<distr_probs>::iterator dei;
  std::map<std::string, double> combined_distr;
  std::map<std::string, double>::iterator mi;
  std::vector<double> distr_counts(classifier_count); // to calculate probs
  long combined_correct = 0;

  while( std::getline( file_in, a_line )) { // in right instance format

    words.clear();
    Tokenize( a_line, words, ' ' );
    std::string target = words.at( words.size()-1 );

    file_out << a_line << " [";

    // We loop over classifiers. We need a summed_distribution.
    //
    int classifier_idx = 0;
    for ( cli = cls.begin(); cli != cls.end(); cli++ ) {

      Classifier *classifier   = *cli;
      Timbl::TimblAPI *timbl   = classifier->get_exp();
      double classifier_weight = classifier->get_weight();

      // get a line from the file, above cls loop!

      std::string cl = a_line;

      tv = timbl->Classify( cl, vd );
      if ( ! tv ) {
	l.log( "ERROR: Timbl returned a classification error, aborting." );
	break;
      }
      std::string answer = tv->Name();

      size_t md  = timbl->matchDepth();
      bool   mal = timbl->matchedAtLeaf();

      int cnt = vd->size(); // size of distr.
      int distr_count = vd->totalSize(); // sum of freqs in distr.
      distr_counts[classifier_idx] = distr_count;

      /*l.log( (*cli)->id + ":" + cl + "/" + answer + " " + 
	to_str(cnt) + "/" + to_str(distr_count) );*/
      //file_out << " " << (*cli)->id << ":" << answer;
      file_out << " " << answer;

      if ( answer == target ) {
	(*cli)->inc_correct();
      }

      Timbl::ValueDistribution::dist_iterator it = vd->begin();      
      while ( it != vd->end() ) {
	std::string tvs  = it->second->Value()->Name();
	long        wght = it->second->Weight(); // absolute frequency.
	double      p    = (double)wght / (double)distr_count;

	combined_distr[tvs] += (p * classifier_weight); // multiply with weight

	if ( tvs == answer ) {
	  file_out << " " << p << " " << md << " " << to_str(mal);
	}
	if ( tvs == target ) {
	  //was somewhere in target, keep score per classifier?
	}

	++it;
      }
      ++classifier_idx;
    } // classifiers
    
    file_out << " ] {";

    // sort combined, get top-n, etc.
    //
    for ( mi = combined_distr.begin(); mi != combined_distr.end(); mi++ ) {
      distr_probs  d;
      d.name   = mi->first;
      d.prob   = mi->second; // was d.freq
      distr_vec.push_back( d );
    }

    sort( distr_vec.begin(), distr_vec.end() );

    // Calculate the combined guess, and write top-n to output file.
    //
    int cntr = topn;
    std::string combined_guess = "";
    dei = distr_vec.begin();
    while ( dei != distr_vec.end() ) { 
      if ( --cntr >= 0 ) {
	//file_out << (*dei).name << ' ' << (*dei).freq << ' ';
	//l.log( (*dei).name + ' ' + to_str((*dei).prob) ); // was .freq
	file_out << " " << (*dei).name << " " << (*dei).prob;
	if ( cntr == 0 ) { // take the first (could be more with same p).
	  combined_guess = (*dei).name;
	}
      }
      dei++;
    }
    file_out << " }";
    if ( target == combined_guess ) {
      ++combined_correct;
    }
    file_out << std::endl;
    distr_vec.clear();
    combined_distr.clear();
  } // get_line
  
  file_out.close();
  file_in.close();
  
  for ( cli = cls.begin(); cli != cls.end(); cli++ ) {  
    Classifier *classifier = *cli;
    l.log( (*cli)->id+": "+ to_str((*cli)->get_correct()) );
  }
  l.log( "Combined: "+to_str(combined_correct) );
  
  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}
#else
int multi_dist( Logfile& l, Config& c ) {
  l.log( "Timbl support not built in." );  
  return -1;
}
#endif

#ifdef TIMBL
int multi_dist2( Logfile& l, Config& c ) {
  l.log( "multi_classifiers" );
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  const std::string& filename         = c.get_value( "filename" );
  const std::string& kvs_filename     = c.get_value( "kvs" );
  bool               do_combined      = stoi( c.get_value( "c", "0" )) == 1;
  int                topn             = stoi( c.get_value( "topn", "1" ) );
  std::string        id               = c.get_value( "id", to_str(getpid()) );
  std::string        output_filename  = kvs_filename + "_" + id + ".mc";

  std::string        distrib;
  std::vector<std::string> distribution;
  std::string        result;
  double             distance;

  l.inc_prefix();
  l.log( "lexicon:    "+lexicon_filename );
  l.log( "filename:   "+filename );
  l.log( "kvs:        "+kvs_filename );
  l.log( "combined:   "+to_str(do_combined) );
  l.log( "topn:       "+to_str(topn) );
  l.log( "id:         "+id );
  l.log( "OUTPUT:     "+output_filename );
  l.dec_prefix();

  std::ofstream file_out( output_filename.c_str(), std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write output file." );
    return -1;
  }

  // Load lexicon. NB: hapaxed lexicon is different? Or add HAPAX entry?
  //
  int wfreq;
  unsigned long total_count = 0;
  unsigned long N_1 = 0; // Count for p0 estimate.
  std::map<std::string,int> wfreqs; // whole lexicon
  std::ifstream file_lexicon( lexicon_filename.c_str() );
  if ( ! file_lexicon ) {
    l.log( "NOTICE: cannot load lexicon file." );
    //return -1;
  } else {
    // Read the lexicon with word frequencies.
    // We need a hash with frequence - countOfFrequency, ffreqs.
    //
    l.log( "Reading lexicon." );
    std::string a_word;
    while ( file_lexicon >> a_word >> wfreq ) {
      wfreqs[a_word] = wfreq;
      total_count += wfreq;
      if ( wfreq == 1 ) {
	++N_1;
      }
    }
    file_lexicon.close();
    l.log( "Read lexicon (total_count="+to_str(total_count)+")." );
  }

  // read kvs
  // in multi_dist we don't need the size, we read a testfile instead.
  //
  std::vector<Classifier*> cls;
  std::vector<Classifier*>::iterator cli;
  int classifier_count = 0;
  if ( kvs_filename != "" ) {
    l.log( "Reading classifiers." );
    std::ifstream file_kvs( kvs_filename.c_str() );
    if ( ! file_kvs ) {
      l.log( "ERROR: cannot load kvs file." );
      return -1;
    }
    read_classifiers_from_file( file_kvs, cls );
    file_kvs.close();
    for ( cli = cls.begin(); cli != cls.end(); cli++ ) {
      (*cli)->init();
      l.log( (*cli)->id + "/" + to_str((*cli)->get_type()) );
      //
      // Test file can be global (-p filename:), or set explicitly.
      //
      if ( ((*cli)->get_type() == 1) && ((*cli)->get_testfile() == "NONE") ) {
	(*cli)->set_testfile( filename );
      }
      if ( (*cli)->open_file() == false ) {
	l.log( "ERROR: cannot open testfile" );
	return -1;
      }
      //
      // Type=2 multiplies with weight specified in kvs.
      //
      if ( (*cli)->get_type() == 2 ) {
	if ( fabs((*cli)->get_weight() - 1.0) > 0.0001 ) {
	  l.log( "Merged will be multiplied." );
	  (*cli)->set_subtype(1);
	} else {
	  l.log( "Distribution will be added." );
	  (*cli)->set_subtype(2);
	}
      }

      ++classifier_count;
      l.log( (*cli)->info_str() );
    }
    l.log( "Read classifiers. Starting classification." );
  }

  // We loop over classifiers.
  //
  int classifier_idx = 0;
  bool go_on = true;
  std::string outline;
  std::string outtarget;
  std::map<std::string, double> combined_distr;
  long combined_correct = 0;

  while ( go_on ) {
    outline.clear();
    combined_distr.clear();

    for ( cli = cls.begin(); cli != cls.end(); cli++ ) {
      
      Classifier *classifier = *cli;
      go_on = classifier->classify_next();

      if ( go_on == true ) {

	md2    foo     = classifier->classification;
	double classifier_weight = classifier->get_weight();
	int    type    = classifier->get_type();
	int    subtype = classifier->get_subtype();

	//l.log( foo.answer + " : " + foo.cl );
	if ( classifier->get_type() == 1 ) {
	  outline   = outline + foo.answer + " " + to_str(foo.prob)+ " ";
	  outline   = outline + to_str(foo.md) + " " + to_str(foo.mal) + " ";
	}
	outtarget = foo.target;
	
	if ( do_combined == true ) {
	  // The distribution is in classification.distr
	  std::vector<md2_elem>::iterator dei;
	  dei = foo.distr.begin();
	  while ( dei != foo.distr.end() ) { 
	    //std::cout << " " << (*dei).token << " " << (*dei).prob;
	    if ( type == 1 ) { // merge normal classifier
	      combined_distr[(*dei).token] += ((*dei).prob * classifier_weight);
	    } else if ( type == 2 ) { // distribution
	      if ( subtype == 1 ) { // multiply
		combined_distr[(*dei).token] *= classifier_weight;
	      } else if ( subtype == 2 ) { // add
		//
		// We need some kind of normalisation here.
		// Maybe related to best in the real/merged classification?
		// Make these "just as important".
		//
		combined_distr[(*dei).token] += (*dei).prob;
	      }
	    } // other type, divide by index number etc.
	    ++dei;
	  }
	}
	
      } // go_on
      ++classifier_idx;
    } // classifiers

    if ( go_on == true ) {
      file_out << outtarget << " [ " << outline << "]";
      
      if ( do_combined == true ) {
	file_out << " {";
	// Calculate the combined guess, and write top-n to output file.
	//
	int cntr = topn;
	std::string combined_guess = "";
	std::vector<distr_probs> distr_vec;
	std::vector<distr_probs>::iterator dei;
	std::map<std::string, double>::iterator mi;
	std::vector<double> distr_counts(classifier_count);
	
	// Convert hash to vector so we can sort it.
	//
	for ( mi = combined_distr.begin(); mi != combined_distr.end(); mi++ ) {
	  distr_probs  d;
	  d.name   = mi->first;
	  d.prob   = mi->second;
	  distr_vec.push_back( d );
	}
	sort( distr_vec.begin(), distr_vec.end() );
	
	// Loop over sorted vector, take top-n.
	//
	dei = distr_vec.begin();
	combined_guess = (*dei).name; // take the first
	while ( dei != distr_vec.end() ) { 
	  if ( --cntr >= 0 ) {
	    //l.log( (*dei).name + ' ' + to_str((*dei).prob) );
	    file_out << " " << (*dei).name << " " << (*dei).prob;
	  }
	  dei++;
	}
	file_out << " }";
	if ( outtarget == combined_guess ) {
	  ++combined_correct;
	}
	distr_vec.clear();
	combined_distr.clear();
      } // do_combined
      file_out << std::endl;
    }
      
  } // while go_on

  file_out.close();
  
  for ( cli = cls.begin(); cli != cls.end(); cli++ ) {  
    Classifier *classifier = *cli;
    l.log( (*cli)->id+": "+ to_str((*cli)->get_correct()) + "/" +
	   to_str((*cli)->get_cc()) );
  }
  if ( do_combined == true ) {
    l.log( "Combined: "+to_str(combined_correct) );
  }

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}
#else
int multi_dist2( Logfile& l, Config& c ) {
  l.log( "Timbl support not built in." );  
  return -1;
}
#endif

#ifdef TIMBL
int multi_gated( Logfile& l, Config& c ) {
  l.log( "multi_gated" );
  const std::string& lexicon_filename = c.get_value( "lexicon" );
  const std::string& filename         = c.get_value( "filename" );
  const std::string& kvs_filename     = c.get_value( "kvs" );
  int                topn             = stoi( c.get_value( "topn", "1" ) );
  int                fco              = stoi( c.get_value( "fco", "0" ));
  std::string        id               = c.get_value( "id", to_str(getpid()) );

  std::string output_filename;
  if ( (id != "") && ( ! contains_id( filename, id)) ) { //was kvs_filename
    output_filename  = filename + "_" + id + ".mg";
  } else {
    output_filename  = filename + ".mg";
  }
  std::string stat_filename;
  if ( (id != "") && ( ! contains_id( filename, id)) ) {
    stat_filename  = filename + "_" + id + ".stat";
  } else {
    stat_filename  = filename + ".stat";
  }

  if ( file_exists( l, c, output_filename ) ) {
    l.log( "OUTPUT exists, not overwriting." );
    c.add_kv( "filename", output_filename );
    l.log( "SET filename to "+output_filename );
    return 0;
  }

  std::string        distrib;
  std::vector<std::string> distribution;
  std::string        result;
  double             distance;

  // specify default to be able to choose default classifier?

  l.inc_prefix();
  l.log( "lexicon:    "+lexicon_filename );
  l.log( "filename:   "+filename );
  l.log( "kvs:        "+kvs_filename );
  l.log( "fco:        "+to_str(fco) ); 
  l.log( "topn:       "+to_str(topn) );
  l.log( "id:         "+id );
  l.log( "OUTPUT:     "+output_filename );
  l.log( "OUTPUT:     "+stat_filename );
  l.dec_prefix();

  std::ofstream file_out( output_filename.c_str(), std::ios::out );
  if ( ! file_out ) {
    l.log( "ERROR: cannot write output file." );
    return -1;
  }

  // Load lexicon. NB: hapaxed lexicon is different? Or add HAPAX entry?
  //
  int wfreq;
  unsigned long total_count = 0;
  unsigned long N_1 = 0; // Count for p0 estimate.
  std::map<std::string,int> wfreqs; // whole lexicon
  std::ifstream file_lexicon( lexicon_filename.c_str() );
  if ( ! file_lexicon ) {
    l.log( "NOTICE: cannot load lexicon file." );
    //return -1;
  } else {
    // Read the lexicon with word frequencies.
    // We need a hash with frequence - countOfFrequency, ffreqs.
    //
    l.log( "Reading lexicon." );
    std::string a_word;
    while ( file_lexicon >> a_word >> wfreq ) {
      wfreqs[a_word] = wfreq;
      total_count += wfreq;
      if ( wfreq == 1 ) {
	++N_1;
      }
    }
    file_lexicon.close();
    l.log( "Read lexicon (total_count="+to_str(total_count)+")." );
  }

  // Read kvs
  //
  std::vector<Classifier*> cls;
  std::vector<Classifier*>::iterator cli;
  std::map<std::string,Classifier*> gated_cls; // reverse list
  int classifier_count = 0;
  Classifier* dflt = NULL;
  if ( kvs_filename != "" ) {
    l.log( "Reading classifiers." );
    std::ifstream file_kvs( kvs_filename.c_str() );
    if ( ! file_kvs ) {
      l.log( "ERROR: cannot load kvs file." );
      return -1;
    }
    read_classifiers_from_file( file_kvs, cls );
    file_kvs.close();
    l.log( "Read "+to_str(cls.size())+" classifiers from "+kvs_filename );

    for ( cli = cls.begin(); cli != cls.end(); cli++ ) {
      (*cli)->init();
      l.log( (*cli)->id + "/" + to_str((*cli)->get_type()) );
      //
      // Gated classifier, rest is ignored.
      //
      if ( (*cli)->get_type() == 3 ) {
	gated_cls[ (*cli)->get_gatetrigger() ] = (*cli);
      } else if ( (*cli)->get_type() == 4 ) { //Well, not the default one
	dflt = (*cli);
      }

      ++classifier_count;
      l.log( (*cli)->info_str() );
    }
    
    if ( dflt == NULL ) {
      l.log( "ERROR: no default classifier" );
      file_out.close();
      return -1;
    }
    l.log( "Read classifiers. Starting classification." );
  }

  std::ifstream file_in( filename.c_str() );
  if ( ! file_in ) {
    l.log( "ERROR: cannot load inputfile." );
    file_out.close();
    return -1;
  }

  std::string a_line;
  std::vector<std::string> words;
  int pos;
  std::map<std::string, Classifier*>::iterator gci;
  std::string gate;
  std::string target;
  int gates_triggered = 0;

  md2    multidist;
  double classifier_weight;
  int    type;
  int    subtype;
  std::vector<md2_elem>::iterator dei;
  Classifier* cl = NULL; // classifier used.
  std::map<std::string,int>::iterator wfi;

  while( std::getline( file_in, a_line )) {

    Tokenize( a_line, words, ' ' ); // instance

    // We could loop over the focus positions, to see if
    // we match. But we can't use a default one for each
    // position, or? Maybe fco should be a parameter. But
    // that defeats the purpose of having it saved in the
    // classifier. It also allows more than one classifier
    // to  be used for an instance, on different fco
    // positions... (hm, a multi-dist-gated version?)

    pos    = words.size()-1-fco;
    pos    = (pos < 0) ? 0 : pos;
    gate   = words[pos];
    target = words[words.size()-1];

    //l.log( a_line + " / " + gate );

    // Store the rsults per instance in ... ?
    // we have md2 (multi_dist2) for a Timbl distribution
    // from which we get our classification.

    // Have we got a classifier with this gate?
    //
    gci = gated_cls.find( gate );
    if ( gci != gated_cls.end() ) {
      cl = (*gci).second;
      ++gates_triggered;
    } else { // the default classifier.
      cl = dflt;
    }

    // Do the classification.
    //
    //l.log( cl->id + "/" + to_str(cl->get_type()) );
    cl->classify_one( a_line );
    
    multidist = cl->classification;
    type      = cl->get_type();
    subtype   = cl->get_subtype();
    
    //l.log( multidist.answer + " : " + to_str(multidist.prob) );

    // Print the instance plus classification
    //
    file_out << a_line << " " << multidist.answer << " ";

    // If the prob == 0, we could do a backoff to lexical probs if
    // we supplied a lexicon.
    // We could also calculate logs and pplxs....
    //
    std::string known = "k"; // unknown.
    if ( multidist.prob == 0 ) {
      known = "u";
      wfi = wfreqs.find(target);
      if ( wfi != wfreqs.end() ) {
	multidist.prob = (int)(*wfi).second / (double)total_count;
	known = "k";
      }
    }
    if ( multidist.prob == 0 ) {
      file_out << "0 ";
    } else {
      file_out << log2( multidist.prob ) << " ";
    }

    //file_out << "entropy" << " " << "word_lp" << " ";
    
    // Indicator if it was correct or not.
    // We have gated:correct/indist/wrong, and idem for default.
    //
    if ( cl->get_type() == 4 ) {
      file_out << "D ";
    } else {
      file_out << "G ";
    }
    file_out << cl->id << " ";

    if ( multidist.info == INFO_WRONG ) {
      file_out << "ic ";
    } else if ( multidist.info == INFO_CORRECT ) {
      file_out << "cg ";
    } else { // INFO_INDISTR is left
      file_out << "cd ";
    }

    file_out << known << " ";

    file_out << multidist.md << " " << multidist.mal << " ";

    file_out << multidist.cnt << " " << multidist.distr_count << " ";

    // For MRR, this makes output format different from px output again.
    //
    file_out << multidist.mrr << " ";

    // Loop over sorted vector, take top-n.
    //
    sort( multidist.distr.begin(), multidist.distr.end() );
    int cntr = topn;
    dei = multidist.distr.begin();
    file_out << "[";
    while ( dei != multidist.distr.end() ) { 
      if ( --cntr >= 0 ) {
	//l.log( (*dei).name + ' ' + to_str((*dei).prob) );
	file_out << " " << (*dei).token << " " << (*dei).freq;
      }
      dei++;
    }
    file_out << " ]";

    file_out << std::endl;

  }
  file_in.close();

  file_out.close();

  // Info per classifier, in a separate output file. Note
  // the the dflt one is saved in the file as well.
  //
  std::ofstream stat_out( stat_filename.c_str(), std::ios::out );
  if ( ! stat_out ) {
    l.log( "ERROR: cannot write stat output file." );
    return -1;
  }
  for ( cli = cls.begin(); cli != cls.end(); cli++ ) {  
    stat_out << (*cli)->id << " " 
	     << (*cli)->get_cc() << " "
	     << (*cli)->get_cg() << " " 
	     << (*cli)->get_cd() << " " 
	     << (*cli)->get_ic() << std::endl;
  }
  stat_out.close();

  l.log( "Gated classifications: "+to_str(gates_triggered) );

  c.add_kv( "filename", output_filename );
  l.log( "SET filename to "+output_filename );
  return 0;
}
#else
int multi_gated( Logfile& l, Config& c ) {
  l.log( "Timbl support not built in." );  
  return -1;
}
#endif

// ---------------------------------------------------------------------------
