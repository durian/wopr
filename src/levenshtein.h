// $Id: levenshtein.h 16138 2013-05-28 08:38:07Z pberck $
//

/*****************************************************************************
 * Copyright 2008 - 2013 Peter Berck                                         *
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

#ifndef _LEVENSHTEIN_H
#define _LEVENSHTEIN_H

#include "elements.h"

int distance(const std::string&, const std::string&);
int levenshtein( Logfile&, Config& );
void distr_spelcorr( const Timbl::ValueDistribution *, const std::string&, std::map<std::string,int>&,
					 std::vector<distr_elem*>&,int, double, double, bool, int);
int correct( Logfile&, Config& );
int lev_distance(const std::string&, const std::string&, int, bool);
int server_sc( Logfile&, Config& );
int server_sc_nf( Logfile&, Config& );
int mcorrect( Logfile&, Config& );

#endif
