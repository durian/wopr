// $Id: qlog.h 14653 2012-04-17 10:12:10Z sloot $

/*****************************************************************************
 * Copyright 2005 - 2011 Peter Berck                                                *
 *                                                                           *
 * This file is part of PETeR.                                               *
 *                                                                           *
 * PETeR is free software; you can redistribute it and/or modify it          *
 * under the terms of the GNU General Public License as published by the     *
 * Free Software Foundation; either version 2 of the License, or (at your    *
 * option) any later version.                                                *
 *                                                                           *
 * PETeR is distributed in the hope that it will be useful, but WITHOUT      *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or     *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License      *
 * for more details.                                                         *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with PETeR; if not, write to the Free Software Foundation,          *
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA               *
 *****************************************************************************/

// ----------------------------------------------------------------------------

#ifndef _qlog_h
#define _qlog_h

#include <string>
#include <vector>

// C includes
//
#include <sys/time.h>
#include <unistd.h>

// ----------------------------------------------------------------------------

class Logfile {
  private:
  std::string prefix;
  std::string time_format;
  static pthread_mutex_t mtx;
  void init();

  timeval       mark;
  timeval       genesis;
  timeval       clck;
  timeval       tmr;
  long          clck_mu_sec;

 public:
  Logfile();
  Logfile(std::string);
  ~Logfile();

  void append( const std::string& );
  void log( const std::string& );
  void log( const std::string&, const std::string&);
  void log( const std::string&, int);
  void log_raw( const std::string& );
  void log_begin( const std::string& );
  void log_end( const std::string& );
  void set_prefix( const std::string& );
  std::string get_prefix();
  void inc_prefix();
  void dec_prefix();
  void lock();
  void unlock();
  void get_raw( timeval& );
  void set_mark();
  void DBG( const std::string& );

  long clock_start();
  long clock_elapsed();
  long clock_mu_sec_elapsed();
  long clock_mu_secs();
  long clock_m_secs();

};

#endif
