// lib_test.h
#ifndef _LIB_TEST_H_
#define _LIB_TEST_H_

#include <boost/test/unit_test.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/mpl/list.hpp>
namespace utf = boost::unit_test;

#include <cstring>
#include <smux.h>

class TestLibFixture
{
    public:
        smux_config_send sender;
        smux_config_recv receiver;
        char read_buf[32];
        char write_buf[32];

        char const* reader_dat;
        unsigned reader_dat_len = 0;
        // measurement for testing
        unsigned reader_dat_req;
        ssize_t reader_dat_ret;
        unsigned reader_called = 0;

        char* writer_buf;
        unsigned writer_buf_len = 0;
        // measurement for testing
        unsigned writer_req;
        ssize_t writer_ret;
        unsigned writer_called = 0;

        TestLibFixture()
        {
            smux_init(&sender, &receiver);
            receiver.buffer.read_buf = read_buf;
            receiver.buffer.read_buf_size = sizeof(read_buf);
            receiver.buffer.read_fn = read_fn;
            receiver.buffer.read_fd = reinterpret_cast<void*>(this);
            sender.buffer.write_buf = write_buf;
            sender.buffer.write_buf_size = sizeof(write_buf);
            sender.buffer.write_fn = write_fn;
            sender.buffer.write_fd = reinterpret_cast<void*>(this);
        }

        static ssize_t read_fn(void *fd, void *buf, size_t count)
        {
            TestLibFixture* f = reinterpret_cast<TestLibFixture*>(fd);
            ssize_t len = count > f->reader_dat_len ? f->reader_dat_len : count;
            std::memcpy(buf, f->reader_dat, len);
            f->reader_dat += len;
            f->reader_dat_len -= len;

            f->reader_dat_req = count;
            f->reader_dat_ret = f->reader_dat_len > 0 ? len + 1 : len;
            f->reader_called += 1;
            return f->reader_dat_ret;
        }

        static ssize_t write_fn(void *fd, const void *buf, size_t count)
        {
            TestLibFixture* f = reinterpret_cast<TestLibFixture*>(fd);
            ssize_t len = count > f->writer_buf_len ? f->writer_buf_len : count;
            std::memcpy(f->writer_buf, buf, len);
            f->writer_buf += len;
            f->writer_buf_len -= len;

            f->writer_req = count;
            f->writer_ret = len;
            f->writer_called += 1;
            return len;
        }

        ~TestLibFixture()
        {
            smux_free(&sender, &receiver);
        }
};

#endif
