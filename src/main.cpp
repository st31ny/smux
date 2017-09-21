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
#include "debug.h"

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
        std::clog << "open master-io" << std::endl;
        io = fac->create(*conf.master().io);
        rt.reset(new runtime_system(std::move(io)));
    } else // asymmetric
    {
        std::clog << "open master-in" << std::endl;
        if(conf.master().in)
            in = fac->create(*conf.master().in);
        std::clog << "open master-out" << std::endl;
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
        std::clog << "open ch " << static_cast<unsigned>(fl_def.first) << " ";
        if(fl_def.second.io) // symmetric file
        {
            std::clog << "io";
            io = fac->create(*fl_def.second.io);
            rt->add_channel(fl_def.first, std::move(io));
        } else
        {
            std::clog << "in ";
            if(fl_def.second.in)
                in = fac->create(*fl_def.second.in);
            std::clog << "out";
            if(fl_def.second.out)
                out = fac->create(*fl_def.second.out);
            if(in || out)
                rt->add_channel(fl_def.first, std::move(in), std::move(out));
        }
        std::clog << std::endl;
    }

    return rt;
}
