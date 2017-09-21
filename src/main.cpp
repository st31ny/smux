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

void print_config(std::ostream& os, smux_client::cnf const& conf);

/**
 * \brief                   create and configure the runtime system
 * \param conf              channel configuration
 * \return                  ready runtime system
 * \throw config_error
 * \throw system_error
 */
static std::unique_ptr<smux_client::runtime_system> load_rt(smux_client::cnf const& conf);

int main(int argc, const char* argv[])
{
    using namespace smux_client;

    // parse config
    std::unique_ptr<cnf_argv> conf(new cnf_argv);
    conf->parse(argc, argv);
    print_config(std::clog, *conf);

    // create/configure the runtime system
    auto rt = load_rt(*conf);

    // pass control to the rt
    rt->run();

    return 0;
}

static std::unique_ptr<smux_client::runtime_system> load_rt(smux_client::cnf const& conf)
{
    using namespace smux_client;

    // create runtime enviroment
    auto fac = file_factory::get();
    std::unique_ptr<runtime_system> rt;
    std::unique_ptr<file> io, in, out;
    if(conf.master().io) // symmetric master file
    {
        io = fac->create(*conf.master().io);
        rt.reset(new runtime_system(std::move(io)));
    } else // asymmetric
    {
        if(conf.master().in)
            in = fac->create(*conf.master().in);
        if(conf.master().out)
            out = fac->create(*conf.master().out);
        if(in || out)
            rt.reset(new runtime_system(std::move(in), std::move(out)));
        else // neither master in nor master out defined... what are we doing here?
            throw config_error("master file definition required");
    }

    // create/add all files
    for(auto const& fl_def : conf.channels())
    {
        if(fl_def.second.io) // symmetric file
        {
            io = fac->create(*fl_def.second.io);
            rt->add_channel(fl_def.first, std::move(io));
        } else
        {
            if(fl_def.second.in)
                in = fac->create(*fl_def.second.in);
            if(fl_def.second.out)
                out = fac->create(*fl_def.second.out);
            if(in || out)
                rt->add_channel(fl_def.first, std::move(in), std::move(out));
        }
    }

    return rt;
}

void print_file(std::ostream& os, smux_client::file_def const& fd)
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

void print_channel(std::ostream& os, smux_client::cnf::channel const& ch)
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

void print_config(std::ostream& os, smux_client::cnf const& conf)
{
    os << "master: ";
    print_channel(os, conf.master());
    os << "\n" << "channels:\n";
    for(auto const& ch : conf.channels())
    {
        os << " " << static_cast<unsigned>(ch.first) << " ";
        print_channel(os, ch.second);
    os << "\n";
    }
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
