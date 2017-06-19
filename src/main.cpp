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
