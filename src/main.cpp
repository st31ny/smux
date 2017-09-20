// main.cpp
#include <iostream>
#include <string>
#include <chrono>
#include <stdlib.h>
#include <unistd.h>
#include <smux.hpp>

#include "file_factory.h"
#include "rt.h"
#include "cnf_argv.h"

using namespace smux_client;

void print_file(std::ostream& os, cnf::file_def const& fd)
{
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

void print_channel(std::ostream& os, cnf::channel const& ch)
{
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
        print_file(os, *ch.io);
        os << ">";
    }
    if(ch.in)
    {
        os << "<in:";
        print_file(os, *ch.in);
        os << ">";
    }
    if(ch.out)
    {
        os << "<out:";
        print_file(os, *ch.out);
        os << ">";
    }
    os << "}";
}

int main(int argc, const char* argv[])
{
    std::unique_ptr<smux_client::cnf_argv> conf(new smux_client::cnf_argv);
    conf->parse(argc, argv);
    std::cout << "parsed config options:\n"
        << "\nhelp lvl: " << conf->help_level()
        << "\ndebug lvl: " << conf->debug_level()
        << "\nmaster file: ";
    print_channel(std::cout, conf->master());
    std::cout << "\n";
    std::cout << "channels:\n";
    for(auto const& ch : conf->channels())
    {
        std::cout << " " << static_cast<unsigned>(ch.first) << " ";
        print_channel(std::cout, ch.second);
    std::cout << "\n";
    }
    return 0;
}

int main_testsmux(int argc, const char* argv[])
{
    using namespace smux_client;
    auto fac = file_factory::get();

    std::clog << "open master-in" << std::endl;
    auto master_in = fac->create("stdin", file_mode::in, {});
    std::clog << "open master-out" << std::endl;
    auto master_out = fac->create("stdout", file_mode::out, {});

    std::clog << "open ch0" << std::endl;
    auto file0_out = fac->create("file", file_mode::out, {"file-ch0-out"});
    auto file0_in = fac->create("file", file_mode::in, {"file-ch0-in"});

    std::clog << "open ch1" << std::endl;
    auto file1_out = fac->create("file", file_mode::out, {"file-ch1-out"});
    auto file1_in = fac->create("file", file_mode::in, {"file-ch1-in"});

    std::clog << "starting re..." << std::endl;

    std::unique_ptr<runtime_system> rt(new runtime_system(std::move(master_in), std::move(master_out)));
    rt->add_channel(0, std::move(file0_in), std::move(file0_out));
    rt->add_channel(1, std::move(file1_in), std::move(file1_out));

    rt->run();

    return 0;
}

int main_smux(int argc, const char* argv[])
{
    if(argc != 2 || (*argv[1] != 'e' && *argv[1] != 'd'))
    {
        std::cerr << "Usage:\n"
            << argv[0] << " (e|d)\n"
            << " e    ... encode (to different channels, changing on character @)\n"
            << " d    ... decode\n"
            << std::flush;
        return 1;
    }
    bool fEncode = true;
    if(*argv[1] == 'd')
        fEncode = false;

    smux::connection con;

    con.smux()->proto.esc = 'X';

    con.set_write_fn([](const void* buf, size_t count) { return write(STDOUT_FILENO, buf, count); });
    con.set_read_fn([](void* buf, size_t count) { return read(STDIN_FILENO, buf, count); } );

    if(fEncode)
    {
        smux::ostream out(con);
        // keep sending
        std::string str;
        while(std::cin >> str)
        {
            std::clog << "send on channel " << static_cast<unsigned>(out.channel()) << std::endl;
            out << str << std::flush;
            if(str.find('@') != std::string::npos)
                out.channel((out.channel() + 1) % 5);
        }
    } else
    {
        smux::istream in(con);

        // keep receiving
        while(1)
        {
            std::string str;
            while(in >> str)
            {
                std::clog << "receive on channel " << static_cast<unsigned>(in.channel()) << std::endl;
                std::cout << str << std::endl;
            }
            in.reset();
        }
    }

    return 0;
}
