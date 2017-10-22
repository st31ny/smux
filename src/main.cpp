// main.cpp
#include <iostream>
#include <string>
#include <stdlib.h>
#include <cstring>

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include <smux.hpp>

#include "file_factory.h"
#include "rt.h"
#include "cnf_argv.h"
#include "debug.h"

/**
 * \brief                   create and configure the runtime system
 * \param conf              channel configuration
 * \return                  ready runtime system
 * \throw config_error
 * \throw system_error
 */
static std::unique_ptr<smux_client::runtime_system> load_rt(smux_client::cnf const& conf);

// runtime system
static std::unique_ptr<smux_client::runtime_system> rt;

// signal handling
static void signal_handler(int, siginfo_t *, void *)
{
    if(rt)
        rt->shutdown();
}
static void setup_signals()
{
    // catch signals to allow clean shutdown
    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    if(sigaction(SIGTERM, &sa, 0) || sigaction(SIGINT, &sa, 0) ||
            sigaction(SIGPIPE, &sa, 0) || sigaction(SIGHUP, &sa, 0))
        std::cerr << "registering signal handler failed: " << std::strerror(errno) << std::endl;

    // ignore SIGCHLD and clean up children automatically
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_NOCLDWAIT;
    if(sigaction(SIGCHLD, &sa, 0))
        std::cerr << "registering signal handler failed: " << std::strerror(errno) << std::endl;
}

void print_help(std::ostream& os, char const* pgrm_name);


int main(int argc, char* argv[])
{
    using namespace smux_client;

    std::clog << "Welcome to smux!" << std::endl;

    // setup signal handlers
    setup_signals();

    // parse config
    std::unique_ptr<cnf_argv> conf(new cnf_argv);
    try
    {
        conf->parse(argc, argv);
    } catch(config_error& e)
    {
        std::cerr << "parsing configuration failed: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
    if(conf->help_level())
    {
        print_help(std::clog, argv[0]);
        return 0;
    }
    print_config(std::clog, *conf);

    // create/configure the runtime system
    try
    {
        rt = load_rt(*conf);
    } catch(config_error& e)
    {
        std::cerr << "file configuration erronous: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    } catch(system_error& e)
    {
        std::cerr << "file creation failure: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    // pass control to the rt
    try
    {
        rt->run();
    } catch(system_error& e)
    {
        std::cerr << "main loop exited: " << e.what() << std::endl;
    }

    return 0;
}

static std::unique_ptr<smux_client::runtime_system> load_rt(smux_client::cnf const& conf)
{
    using namespace smux_client;

    // create runtime enviroment
    auto fac = file_factory::get();
    std::unique_ptr<runtime_system> rt;
    std::unique_ptr<file> io, in, out;
    std::clog << ">> open master\n";
    if(conf.master().io) // symmetric master file
    {
        std::clog << "{io}";
        io = fac->create(*conf.master().io);
        rt.reset(new runtime_system(std::move(io)));
    } else // asymmetric
    {
        if(conf.master().in)
        {
            std::clog << "{in}";
            in = fac->create(*conf.master().in);
        }
        if(conf.master().out)
        {
            std::clog << "{out}";
            out = fac->create(*conf.master().out);
        }
        if(in || out)
            rt.reset(new runtime_system(std::move(in), std::move(out)));
        else // neither master in nor master out defined... what are we doing here?
            throw config_error("master file definition required");
    }

    // create/add all files
    for(auto const& fl_def : conf.channels())
    {
        std::clog << ">> open ch " << static_cast<unsigned>(fl_def.first) << "\n";
        if(fl_def.second.io) // symmetric file
        {
            std::clog << "{io}";
            io = fac->create(*fl_def.second.io);
            rt->add_channel(fl_def.first, std::move(io));
        } else
        {
            if(fl_def.second.in)
            {
                std::clog << "{in}";
                in = fac->create(*fl_def.second.in);
            }
            if(fl_def.second.out)
            {
                std::clog << "{out}";
                out = fac->create(*fl_def.second.out);
            }
            if(in || out)
                rt->add_channel(fl_def.first, std::move(in), std::move(out));
        }
        std::clog << std::flush;
    }

    return rt;
}

void print_help(std::ostream& os, char const* pgrm_name)
{
    os << "Copyright 2017 Maximilian Stein <m@steiny.biz>\n"
        << "smux is a library and an application for multiplexing multiple binary data\n"
        << "streams over a single channel.\n"
        << "It was influenced by, and could be seen as an extension to the great socat(1)\n"
        << "tool, with which it tightly integrates to allow greatest possible flexibility.\n"
        << "\nUsage:\n"
        << "(1) " << pgrm_name << " -m <file definition> {-c <channel definition>}\n"
        << "(2) " << pgrm_name << " -h\n\n"
        << "Options:\n"
        << " -h         Print this help message and exit\n"
        << " -m <fd>    Specify the master file definition\n"
        << " -c <cd>    Add a channel definition\n"
        << "\n"
        << "File definition:\n"
        << "(1) <file type>[:<argument>]\n"
        << "(2) <file type>[:<argument>]%<file type>[:argument]\n"
        << "  Form 1 specifies a file for reading and writing. In form 2, two separate file\n"
        << "  definitions are given and seperated by '%'. The left hand side is used for\n"
        << "  reading and the right hand side for writing. Omitting either part is possible\n"
        << "  and creates a unidirectional channel.\n"
        << "\n"
        << "Channel definition:\n"
        << "    <channel number>=<file definition>\n"
        << "  Channel number must be between 0 and 255.\n"
        << "\n"
        << "File types:\n"
        << "  stdio                         Read from stdin and write to stdout\n"
        << "  file:<file name>              Open the file <file name> for reading/writing\n"
        << "                                (depending on channel definition) with open(2).\n"
        << "                                If used for writing, 'O_APPEND' and 'O_CREAT'\n"
        << "                                flags are set.\n"
        << "  exec:<program command line>   Executes the specified program and uses stdin\n"
        << "                                and stdout for communication (depending on\n"
        << "                                read/write mode in channel definition only one\n"
        << "                                of the two).\n"
        << "                                The stdout of the program is used as input for\n"
        << "                                the specified channel and data received on a\n"
        << "                                specific channel is forwarded to the stdin of\n"
        << "                                the program.\n"
        << "  socat:<address>               Wrapper for 'exec' to use with socat. Depending\n"
        << "                                on the read/write mode of the channel translates\n"
        << "                                to one of the three file definitions:\n"
        << "                                   'exec:socat -d -d stdin <address>'\n"
        << "                                            (channel is write only)\n"
        << "                                   'exec:socat -d -d stdout <address>'\n"
        << "                                            (channel is read only)\n"
        << "                                   'exec:socat -d -d stdio <address>'\n"
        << "                                            (channel is read/write)\n"
        << "                                For details about the usage of socat, refer to\n"
        << "                                its manpage.\n"
        << std::flush;
}
