// read_decode_test.cpp
#include "lib_test.h"

struct SmuxReader
{
    SmuxReader(TestLibFixture* fixture)
        : _f(fixture)
    {}

    // return num of read chars
    virtual size_t read(char const* data, size_t count) = 0;

    TestLibFixture* _f;
};

struct ReadBufReader : public SmuxReader
{
    using SmuxReader::SmuxReader;

    size_t read(char const* data, size_t count)
    {
        return smux_read_buf(&_f->receiver, data, count);
    }
};

struct ReadFnReader : public SmuxReader
{
    using SmuxReader::SmuxReader;

    size_t read(char const* data, size_t count)
    {
        _f->reader_dat = data;
        _f->reader_dat_len = count;
        _f->reader_called = 0;
        smux_read(&_f->receiver);
        BOOST_TEST(_f->reader_called >= 1);
        return count - _f->reader_dat_len;
    }
};

typedef boost::mpl::list<ReadBufReader, ReadFnReader> readers;


BOOST_FIXTURE_TEST_SUITE(read_decode, TestLibFixture);

BOOST_AUTO_TEST_CASE_TEMPLATE(read_buf_decode, R, readers)
{
    R reader(this);

    ssize_t ret;
    // > ABC\x01DEF on channel 0
    // > 123\x01 on channel \x42
    // > GH on channel 0
    char muxed[] = "ABC\x01\x00""DEF\x01\x42\x00\x04""123\x01\x00""GH";
    unsigned size = sizeof(muxed) - 1;

    ret = reader.read(muxed, size);
    BOOST_TEST(ret == size);

    char recv[32];
    smux_channel ch;

    // check first chunk
    ch = 1;
    ret = smux_recv(&receiver, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 7);
    recv[ret] = 0;
    BOOST_TEST(ch == 0);
    BOOST_TEST(recv == "ABC\x01""DEF");

    // supply more data
    // > abcd on channel 255
    char muxed2[] = "\x01\xff\x00\x04""abcd";
    size = sizeof(muxed2) - 1;
    ret = reader.read(muxed2, size);
    BOOST_TEST(size == ret);

    // check 2nd chunk
    ret = smux_recv(&receiver, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 4);
    recv[ret] = 0;
    BOOST_TEST(ch == 0x42);
    BOOST_TEST(recv == "123\x01");

    // check 3rd chunk
    ret = smux_recv(&receiver, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 2);
    recv[ret] = 0;
    BOOST_TEST(ch == 0);
    BOOST_TEST(recv =="GH");

    // check 4th chunk
    ret = smux_recv(&receiver, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 4);
    recv[ret] = 0;
    BOOST_TEST(ch == 255);
    BOOST_TEST(recv == "abcd");

    // verify that buffer is empty now
    BOOST_TEST(receiver._internal.rb_head == receiver._internal.rb_tail);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(read_buf_overlong_decode, R, readers)
{
    R reader(this);

    ssize_t ret;
    char muxed[] = "1234567890\x01\x42\x00\x1E""123456789012345678901234567890";
    char* p = muxed;
    char* e = muxed + sizeof(muxed) - 1;

    ret = reader.read(muxed, e - p);
    BOOST_TEST(ret == 31);
    p += ret;

    char recv[64];
    smux_channel ch;

    // read first chunk
    ch = 1;
    ret = smux_recv(&receiver, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 10);
    recv[ret] = 0;
    BOOST_TEST(ch == 0);
    BOOST_TEST(recv == "1234567890");

    // supply some more data
    ret = reader.read(p, 2);
    BOOST_TEST(ret == 2);
    p += ret;

    // read next chunk
    ret = smux_recv(&receiver, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 19);
    recv[ret] = 0;
    BOOST_TEST(ch == 0x42);
    BOOST_TEST(recv == "1234567890123456789");

    // supply rest of data
    ret = reader.read(p, e - p);
    BOOST_TEST(ret == 11);

    // read last chunk
    ch = 0;
    ret = smux_recv(&receiver, &ch, recv, sizeof(recv) - 1);
    BOOST_TEST(ret == 11);
    recv[ret] = 0;
    BOOST_TEST(ch == 0x42);
    BOOST_TEST(recv == "01234567890");

    // buffer is empty now
    BOOST_TEST(receiver._internal.rb_head == receiver._internal.rb_tail);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(read_into_short_buf_decode, R, readers)
{
    R reader(this);

    ssize_t ret;
    char muxed[] = "ABCDEF\x01\x42\x00\x05""12345";
    unsigned size = sizeof(muxed) - 1;

    ret = reader.read(muxed, size);
    BOOST_TEST(ret == size);

    char recv[5];
    size = sizeof(recv) - 1;
    smux_channel ch;

    // read 1st chunk
    ch = 1;
    ret = smux_recv(&receiver, &ch, recv, size);
    BOOST_TEST(ret == size);
    recv[ret] = 0;
    BOOST_TEST(ch == 0);
    BOOST_TEST(recv == "ABCD");

    // read more of 1st chunk
    ch = 1;
    ret = smux_recv(&receiver, &ch, recv, size);
    BOOST_TEST(ret == 2);
    recv[ret] = 0;
    BOOST_TEST(ch == 0);
    BOOST_TEST(recv == "EF");

    // read 2nd chunk
     ret = smux_recv(&receiver, &ch, recv, size);
    BOOST_TEST(ret == size);
    recv[ret] = 0;
    BOOST_TEST(ch == 0x42);
    BOOST_TEST(recv == "1234");

    // read rest 2nd chunk
    ch = 0;
    ret = smux_recv(&receiver, &ch, recv, size);
    BOOST_TEST(ret == 1);
    recv[ret] = 0;
    BOOST_TEST(ch == 0x42);
    BOOST_TEST(recv == "5");
}

BOOST_AUTO_TEST_SUITE_END();
