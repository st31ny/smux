/// \file debug.h
#ifndef _DEBUG_H_INCLUDED_
#define _DEBUG_H_INCLUDED_

#include <ostream>

#include "cnf.h"

namespace smux_client
{
    void print_config(std::ostream& os, smux_client::cnf const& conf);
    void print_file_def(std::ostream& os, smux_client::file_def const& fd);
    void print_channel_def(std::ostream& os, smux_client::cnf::channel const& ch);
}

#endif // ifndef _DEBUG_H_INCLUDED_
