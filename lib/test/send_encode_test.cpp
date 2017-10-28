// send_encode_test.cpp
#include "lib_test.h"

struct SmuxWriter
{
    SmuxWriter(TestLibFixture* fixture)
        : _f(fixture)
    {}

    // return num of written chars
    virtual size_t write(char* buf, size_t count) = 0;

    TestLibFixture* _f;
};

/* not impl
struct WriteBufWriter : public SmuxWriter
{
    using SmuxWriter::SmuxWriter;

    size_t write(char* buf, size_t count);
};
*/

struct WriteFnWriter : public SmuxWriter
{
    using SmuxWriter::SmuxWriter;

    size_t write(char* buf, size_t count)
    {
        _f->writer_buf = buf;
        _f->writer_buf_len = count;
        _f->writer_called = 0;
        smux_write(&_f->sender);
        BOOST_TEST(_f->writer_called >= 1);
        return count - _f->writer_buf_len;
    }
};

typedef boost::mpl::list</*WriteBufWriter, */ WriteFnWriter> writers;


BOOST_FIXTURE_TEST_SUITE(write_encode, TestLibFixture);

BOOST_AUTO_TEST_CASE_TEMPLATE(basic_encode, W, writers)
{
    W writer(this);
    size_t ret;

    char buf[100];
    size_t count = sizeof(buf)/sizeof(*buf);

    // send 1st chunk
    ret = smux_send(&sender, 0, "ABC\x01""DEF", 7);
    BOOST_TEST(ret == 7);

    // send 2nd chunk
    ret = smux_send(&sender, 0x42, "123\x01", 4);
    BOOST_TEST(ret == 4);

    // send 3rd chunk
    ret = smux_send(&sender, 0, "GH", 2);
    BOOST_TEST(ret == 2);

    // write everything out
    ret = writer.write(buf, count);
    BOOST_TEST(ret == 19);
    buf[ret] = 0;
    BOOST_TEST(buf == "ABC\x01\x00""DEF\x01\x42\x00\x04""123\x01\x00""GH");

    // send 4th chunk
    ret = smux_send(&sender, 255, "abcd", 4);
    BOOST_TEST(ret == 4);

    // write it out
    ret = writer.write(buf, count);
    BOOST_TEST(ret == 8);
    buf[ret] = 0;
    BOOST_TEST(buf == "\x01\xff\x00\x04""abcd");

    // internal buffer should be empty now
    BOOST_TEST(sender._internal.wb_head == sender._internal.wb_tail);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(send_overlong, W, writers)
{
    W writer(this);

    size_t ret;
    char buf[100];
    size_t count = sizeof(buf)/sizeof(*buf);

    // send giant chunk
    const char msg[] = "0123456789ABCDEFGHIJ\x01""123456789abcdefghij";
    size_t msg_len = sizeof(msg)/sizeof(*msg) - 1;
    char const* p = msg;
    size_t sz = msg_len;

    ret = smux_send(&sender, 0x42, p, sz);
    BOOST_TEST(ret == 26);
    p += ret;
    sz -= ret;

    // write it out
    ret = writer.write(buf, count);
    BOOST_TEST(ret == 31);
    BOOST_TEST(memcmp(buf, "\x01""\x42""\x00""\x1a""0123456789ABCDEFGHIJ\x01""\x00""12345", ret)==0);

    // send rest of stuff
    ret = smux_send(&sender, 0x42, p, sz);
    BOOST_TEST(ret == sz);

    // write it out
    ret = writer.write(buf, count);
    BOOST_TEST(ret == 18);
    BOOST_TEST(memcmp(buf, "\x01""\x42""\x00""\x0e""6789abcdefghij", ret) == 0);

    // internal buffer empty
    BOOST_TEST(sender._internal.wb_head == sender._internal.wb_tail);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(write_to_short_buf, W, writers)
{
    W writer(this);
    size_t ret;

    char buf[8];
    size_t count = sizeof(buf) / sizeof(*buf);

    const char msg[] = "0123456789ABCDEFGH";
    size_t msg_len = sizeof(msg)/sizeof(*msg) - 1;

    // send chunk
    ret = smux_send(&sender, 0x11, msg, msg_len);
    BOOST_TEST(ret == msg_len);

    // write it out, part 1
    ret = writer.write(buf, count);
    BOOST_TEST(ret == count);
    BOOST_TEST(memcmp(buf, "\x01\x11\x00\x12""0123", count) == 0);

    // write more stuff out
    ret = writer.write(buf, count);
    BOOST_TEST(ret == count);
    BOOST_TEST(memcmp(buf, "456789AB", count) == 0);

    // and the rest
    ret = writer.write(buf, count);
    BOOST_TEST(ret == 6);
    BOOST_TEST(memcmp(buf, "CDEFGH", ret) == 0);
}

BOOST_AUTO_TEST_SUITE_END();
