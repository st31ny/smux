// main.cpp
#include <iostream>
#include <string>
#include <chrono>
#include <stdlib.h>
#include <unistd.h>
#include <smux.hpp>

#include "file_factory.h"
#include "rt.h"

int main(int argc, const char* argv[])
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
