// main.cpp
#include <iostream>
#include <string>
#include <chrono>
#include <stdlib.h>
#include <unistd.h>
#include <smux.hpp>

int main(int argc, const char* argv[])
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

    smux::ostream out(con);
    if(fEncode)
    {
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
        char buf[32];
        ssize_t ret;
        smux_channel ch;

        // keep receiving
        while(1)
        {
            ret = smux_recv(con.smux(), &ch, buf, sizeof(buf) / sizeof(*buf));
            if(ret > 0)
            {
                std::clog << "receive on channel " << static_cast<unsigned>(ch) << std::endl;
                std::cout.write(buf, ret);
                std::cout << std::endl;
            } else
            if(ret < 0)
            {
                std::cerr << "receiving failed" << std::endl;
                return 1;
            }
        }
    }

    return 0;
}
