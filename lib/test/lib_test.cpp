// lib_test.cpp
#include <boost/test/unit_test.hpp>
namespace utf = boost::unit_test;

#include <cstring>

#include <smux.h>

class TestLibFixture
{
    public:
        smux_config smux;
        char read_buf[32];
        char write_buf[32];

        TestLibFixture()
        {
            smux_init(&smux);
            smux.buffer.read_buf = read_buf;
            smux.buffer.read_buf_size = sizeof(read_buf);
            smux.buffer.write_buf = write_buf;
            smux.buffer.write_buf_size = sizeof(write_buf);
        }

        ~TestLibFixture()
        {
            smux_free(&smux);
        }
};

BOOST_FIXTURE_TEST_SUITE(lib_test, TestLibFixture);

BOOST_AUTO_TEST_CASE(read_buf_test)
{
    size_t ret;
    // > ABC\x01DEF on channel 0
    // > 123\x01 on channel \x42
    // > GH on channel 0
    char muxed[] = "ABC\x01\x00""DEF\x01\x42\x00\x04""123\x01\x00""GH";
    unsigned size = sizeof(muxed) - 1;

    ret = smux_read_buf(&smux, muxed, size);
    BOOST_TEST(ret == size);

    char recv[32];
    smux_channel ch;

    // check first chunk
    ch = 1;
    ret = smux_recv(&smux, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 7);
    recv[ret] = 0;
    BOOST_TEST(ch == 0);
    BOOST_TEST(recv == "ABC\x01""DEF");

    // supply more data
    // > abcd on channel 255
    char muxed2[] = "\x01\xff\x00\x04""abcd";
    size = sizeof(muxed2) - 1;
    ret = smux_read_buf(&smux, muxed2, size);
    BOOST_TEST(size == ret);

    // check 2nd chunk
    ret = smux_recv(&smux, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 4);
    recv[ret] = 0;
    BOOST_TEST(ch == 0x42);
    BOOST_TEST(recv == "123\x01");

    // check 3rd chunk
    ret = smux_recv(&smux, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 2);
    recv[ret] = 0;
    BOOST_TEST(ch == 0);
    BOOST_TEST(recv =="GH");

    // check 4th chunk
    ret = smux_recv(&smux, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 4);
    recv[ret] = 0;
    BOOST_TEST(ch == 255);
    BOOST_TEST(recv == "abcd");

    // verify that buffer is empty now
    BOOST_TEST(smux._internal.read_buf_head == smux._internal.read_buf_tail);
}

BOOST_AUTO_TEST_CASE(read_buf_overlong)
{
    unsigned ret;
    char muxed[] = "1234567890\x01\x42\x00\x1E""123456789012345678901234567890";
    char* p = muxed;
    char* e = muxed + sizeof(muxed) - 1;

    ret = smux_read_buf(&smux, muxed, e - p);
    BOOST_TEST(ret == 31);
    p += ret;

    char recv[64];
    smux_channel ch;

    // read first chunk
    ch = 1;
    ret = smux_recv(&smux, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 10);
    recv[ret] = 0;
    BOOST_TEST(ch == 0);
    BOOST_TEST(recv == "1234567890");

    // supply some more data
    ret = smux_read_buf(&smux, p, 2);
    BOOST_TEST(ret == 2);
    p += ret;

    // read next chunk
    ret = smux_recv(&smux, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 19);
    recv[ret] = 0;
    BOOST_TEST(ch == 0x42);
    BOOST_TEST(recv == "1234567890123456789");

    // supply rest of data
    ret = smux_read_buf(&smux, p, e - p);
    BOOST_TEST(ret == 11);

    // read last chunk
    ch = 0;
    ret = smux_recv(&smux, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 11);
    recv[ret] = 0;
    BOOST_TEST(ch == 0x42);
    BOOST_TEST(recv == "01234567890");

    // buffer is empty now
    BOOST_TEST(smux._internal.read_buf_head == smux._internal.read_buf_tail);
}

BOOST_AUTO_TEST_CASE(read_into_short_buf)
{
    unsigned ret;
    char muxed[] = "ABCDEF\x01\x42\x00\x05""12345";
    unsigned size = sizeof(muxed) - 1;

    ret = smux_read_buf(&smux, muxed, size);
    BOOST_TEST(ret == size);

    char recv[5];
    size = sizeof(recv) - 1;
    smux_channel ch;

    // read 1st chunk
    ch = 1;
    ret = smux_recv(&smux, &ch, recv, size);
    BOOST_TEST(ret == size);
    recv[ret] = 0;
    BOOST_TEST(ch == 0);
    BOOST_TEST(recv == "ABCD");

    // read more of 1st chunk
    ch = 1;
    ret = smux_recv(&smux, &ch, recv, size);
    BOOST_TEST(ret == 2);
    recv[ret] = 0;
    BOOST_TEST(ch == 0);
    BOOST_TEST(recv == "EF");

    // read 2nd chunk
     ret = smux_recv(&smux, &ch, recv, size);
    BOOST_TEST(ret == size);
    recv[ret] = 0;
    BOOST_TEST(ch == 0x42);
    BOOST_TEST(recv == "1234");

    // read rest 2nd chunk
    ch = 0;
    ret = smux_recv(&smux, &ch, recv, size);
    BOOST_TEST(ret == 1);
    recv[ret] = 0;
    BOOST_TEST(ch == 0x42);
    BOOST_TEST(recv == "5");
}

BOOST_AUTO_TEST_SUITE_END();