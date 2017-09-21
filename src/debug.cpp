// debug.cpp
#include "debug.h"


void smux_client::print_file_def(std::ostream& os, smux_client::file_def const& fd)
{
    using namespace smux_client;
    os << fd.type << ":";
    switch(fd.mode)
    {
        case file_mode::io:
            os << "io"; break;
        case file_mode::in:
            os << "in"; break;
        case  file_mode::out:
            os << "out"; break;
        default:
            os << "unk"; break;
    }
    os << ":";
    for(auto const& arg : fd.args)
    {
        os << arg << ":";
    }
}

void smux_client::print_channel_def(std::ostream& os, smux_client::cnf::channel const& ch)
{
    using namespace smux_client;
    switch(ch.type)
    {
        case channel_type::none:
            os << "n"; break;
        case channel_type::separate:
            os << "s"; break;
        case channel_type::read_only:
            os << "r"; break;
        case channel_type::write_only:
            os << "w"; break;
        case channel_type::symmetric:
            os << "y"; break;
        default:
            os << "u"; break;
    }
    os << "{";
    if(ch.io)
    {
        os << "<io:";
        print_file_def(os, *ch.io);
        os << ">";
    }
    if(ch.in)
    {
        os << "<in:";
        print_file_def(os, *ch.in);
        os << ">";
    }
    if(ch.out)
    {
        os << "<out:";
        print_file_def(os, *ch.out);
        os << ">";
    }
    os << "}";
}

void smux_client::print_config(std::ostream& os, smux_client::cnf const& conf)
{
    using namespace smux_client;
    os << "master: ";
    print_channel_def(os, conf.master());
    os << "\n" << "channels:\n";
    for(auto const& ch : conf.channels())
    {
        os << " " << static_cast<unsigned>(ch.first) << " ";
        print_channel_def(os, ch.second);
    os << "\n";
    }
}
