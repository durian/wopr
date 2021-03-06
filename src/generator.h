// $Id: generator.h 14654 2012-04-17 10:33:42Z sloot $
//

/*****************************************************************************
 * Copyright 2008 - 2011-2009 Peter Berck                                           *
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

#ifndef _GENERATOR_H
#define _GENERATOR_H

#ifdef TIMBL
# include "timbl/TimblAPI.h"
#endif

int generate( Logfile&, Config& );

#ifdef TIMBL
std::string generate_one( Config&, std::string&, int, int, const std::string&, Timbl::TimblAPI* );
std::string generate_xml( Config&, std::string&, std::string&, int, int, const std::string&, Timbl::TimblAPI* );
#endif

int generate_server( Logfile&, Config& );

#endif
