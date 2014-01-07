// $Id$

// This file is originally from Moses with the following copyright information:
/***********************************************************************
travatar - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#ifndef INPUT_FILE_STREAM_H__
#define INPUT_FILE_STREAM_H__

#include <cstdlib>
#include <fstream>
#include <string>

namespace travatar
{

/** Used in place of std::istream, can read zipped files if it ends in .gz
*/
class InputFileStream : public std::istream
{
protected:
  std::streambuf *m_streambuf;
  std::ifstream *ifs_;
public:

  InputFileStream(std::string filePath);
  ~InputFileStream();

  void Close();
};

}

#endif
