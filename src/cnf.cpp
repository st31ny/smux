// cnf.cpp
#include "cnf.h"

using namespace smux_client;

static void set_symmetric_file(cnf::channel& chan, std::unique_ptr<cnf::file_def> fl)
{
    chan.type = channel_type::symmetric;
    chan.io = std::move(fl);
    chan.in.reset(nullptr);
    chan.out.reset(nullptr);
}

static void set_in_file(cnf::channel& chan, std::unique_ptr<cnf::file_def> fl)
{
    chan.type = channel_type::read_only; // assume this for now
    // if a symmetric master file is defined, move it to out
    if(chan.io)
    {
        chan.out = std::move(chan.io);
        chan.io.reset(nullptr);
    }
    chan.in = std::move(fl);
    if(chan.out) // if out is defined, too, type is seperate
        chan.type = channel_type::separate;
}

static void set_out_file(cnf::channel& chan, std::unique_ptr<cnf::file_def> fl)
{
    chan.type = channel_type::write_only; // assume this for now
    // if a symmetric master file is defined, move it to in
    if(chan.io)
    {
        chan.in = std::move(chan.io);
        chan.io.reset(nullptr);
    }
    chan.out = std::move(fl);
    if(chan.in) // if in is defined, too, type is seperate
        chan.type = channel_type::separate;
}

void cnf::set_master_file(std::unique_ptr<cnf::file_def> fl)
{
    set_symmetric_file(_master_file, std::move(fl));
}

void cnf::set_master_file_in(std::unique_ptr<cnf::file_def> fl)
{
    set_in_file(_master_file, std::move(fl));
}

void cnf::set_master_file_out(std::unique_ptr<cnf::file_def> fl)
{
    set_out_file(_master_file, std::move(fl));
}

void cnf::reset_master()
{
    _master_file = channel{};
}

void cnf::set_channel_file(smux_channel ch, std::unique_ptr<cnf::file_def> fl)
{
    set_symmetric_file(_channels[ch], std::move(fl));
}

void cnf::set_channel_file_in(smux_channel ch, std::unique_ptr<cnf::file_def> fl)
{
    set_in_file(_channels[ch], std::move(fl));
}

void cnf::set_channel_file_out(smux_channel ch, std::unique_ptr<cnf::file_def> fl)
{
    set_out_file(_channels[ch], std::move(fl));
}

void cnf::reset_channel(smux_channel ch)
{
    _channels.erase(ch);
}
