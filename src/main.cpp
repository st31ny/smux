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

int main(int argc, const char* argv[])
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
    if(conf.master().io) // symmetric master file
    {
        std::clog << "open master-io" << std::endl;
        io = fac->create(*conf.master().io);
        rt.reset(new runtime_system(std::move(io)));
    } else // asymmetric
    {
        if(conf.master().in)
        {
            std::clog << "open master-in" << std::endl;
            in = fac->create(*conf.master().in);
        }
        if(conf.master().out)
        {
            std::clog << "open master-out" << std::endl;
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
        std::clog << "open ch " << static_cast<unsigned>(fl_def.first) << "\n";
        if(fl_def.second.io) // symmetric file
        {
            std::clog << ">> io\n";
            io = fac->create(*fl_def.second.io);
            rt->add_channel(fl_def.first, std::move(io));
        } else
        {
            if(fl_def.second.in)
            {
                std::clog << ">>in\n";
                in = fac->create(*fl_def.second.in);
            }
            if(fl_def.second.out)
            {
                std::clog << ">>out\n";
                out = fac->create(*fl_def.second.out);
            }
            if(in || out)
                rt->add_channel(fl_def.first, std::move(in), std::move(out));
        }
        std::clog << std::endl;
    }

    return rt;
}
