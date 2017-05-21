// main.cpp
#include <iostream>
#include <string>
#include <chrono>
#include <stdlib.h>
#include <unistd.h>
#include <smux.h>


int main(int argc, const char* argv[])
{
    if(argc != 2 || (*argv[1] != 'e' && *argv[1] != 'd'))
    {
        std::cerr << "Usage:\n"
            << argv[0] << " (e|d) file\n"
            << " e    ... encode (to different channels, changing on character @)\n"
            << " d    ... decode\n"
            << std::flush;
        return 1;
    }
    bool fEncode = true;
    if(*argv[1] == 'd')
        fEncode = false;

    size_t bufSize = 16;

    smux_config sconfig;
    smux_init(&sconfig);

    sconfig.proto.esc = 'X';

    sconfig.buffer.write_buf = malloc(bufSize);
    sconfig.buffer.write_buf_size = bufSize;
    sconfig.buffer.write_fn = write;
    sconfig.buffer.write_fd = STDOUT_FILENO;

    sconfig.buffer.read_buf = malloc(bufSize);
    sconfig.buffer.read_buf_size = bufSize;
    sconfig.buffer.read_fn = read;
    sconfig.buffer.read_fd = STDIN_FILENO;

    smux_channel ch = 0;
    if(fEncode)
    {
        // keep sending
        std::string str;
        while(std::cin >> str)
        {
            std::clog << "send on channel " << static_cast<unsigned>(ch) << std::endl;
            unsigned begin = 0;
            unsigned end = str.size();
            while(begin < end)
            {
                begin += smux_send(&sconfig, ch, str.data() + begin, end - begin);
                if(smux_flush(&sconfig) < 0)
                {
                    std::cerr << "flushing failed" << std::endl;
                    return 1;
                }
            }
            if(str.find('@') != std::string::npos)
                ch = (ch + 1) % 5;
        }
    } else
    {
        char buf[32];
        ssize_t ret;

        // keep receiving
        while(1)
        {
            ret = smux_recv(&sconfig, &ch, buf, sizeof(buf) / sizeof(*buf));
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

    smux_free(&sconfig);
    return 0;
}
