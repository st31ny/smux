// smux.c
#include <smux.h>

// adjust ring buffer index
#define ADJRBI(i, s) ((i) >= (s) ? (i)-(s) : (i))
// used elements in the buffer (given head, tail and size)
#define RBUSED(h, t, s) ((h)>=(t) ? (h)-(t)+1 : (s)-(t)+(h)+1)

// protocol properties
#define PROTO_CHANNEL_BYTES 1
#define PROTO_SIZE_BYTES 2
#define PROTO_MAX_SIZE ((1l<<PROTO_SIZE_BYTES*8)-1)

void smux_init(struct smux_config *config)
{
    // just zero-initialize everything
    static struct smux_config zero_config;
    *config = zero_config;

    // non-zero stuff
    config->proto.esc = 'X';
}

void smux_free(struct smux_config *config)
{
    (void)config;
    // no clean-up needed currently
}

ssize_t smux_send(struct smux_config *config, smux_channel ch, const void *buf, size_t count)
{
    char *write_buf = (char*)config->buffer.write_buf;
    unsigned write_buf_head = config->_internal.write_buf_head;
    unsigned write_buf_tail = config->_internal.write_buf_tail;
    size_t write_buf_size = config->buffer.write_buf_size;
    size_t write_buf_used = RBUSED(write_buf_head, write_buf_tail, write_buf_size);
    unsigned size_field = 0; // size field position in write_buf
    char esc = config->proto.esc;

    char* input_buf = (char*)buf;
    size_t count_copied = 0; // copied chars from input_buf

    // catch trivial case
    if(count <= 0) return 0;

    // limit size
    if(count > PROTO_MAX_SIZE)
        count = PROTO_MAX_SIZE;

    // insert channel & size field
    if(ch != 0)
    {
        // enough space for escape byte, the channel and size fields?
        if(write_buf_used + 1 + PROTO_CHANNEL_BYTES + PROTO_SIZE_BYTES >= write_buf_size - 1)
            return 0;

        // esc char
        write_buf[write_buf_head] = esc;
        write_buf_head = ADJRBI(write_buf_head + 1, write_buf_size);
        // channel field (1 byte)
        write_buf[write_buf_head] = (char)ch;
        write_buf_head = ADJRBI(write_buf_head + 1, write_buf_size);
        // keep space for size field (2 bytes)
        size_field = write_buf_head;
        write_buf_head = ADJRBI(write_buf_head + 2, write_buf_size);
    }

    // copy the data in while not full
    for(; (count_copied < count) && (ADJRBI(write_buf_head + 1, write_buf_size) != write_buf_tail); count_copied++)
    {
        if(input_buf[count_copied] == esc)
        {
            // enough space for two characters?
            if(ADJRBI(write_buf_head + 2, write_buf_size) == write_buf_tail)
                break;
            // escape the escape char with esc + '\0'
            write_buf[write_buf_head] = esc;
            write_buf_head = ADJRBI(write_buf_head + 1, write_buf_size);
            write_buf[write_buf_head] = 0;
            write_buf_head = ADJRBI(write_buf_head + 1, write_buf_size);
        } else
        {
            write_buf[write_buf_head] = input_buf[count_copied];
            write_buf_head = ADJRBI(write_buf_head + 1, write_buf_size);
        }
    }

    // write size field (big endian)
    if(ch != 0)
    {
        write_buf[size_field] = (char)(count_copied >> 8);
        size_field = ADJRBI(size_field + 1, write_buf_size);
        write_buf[size_field] = (char)(count_copied & 0xFF);
    }

    // write head index back
    config->_internal.write_buf_head = write_buf_head;

    return count_copied;
}

ssize_t smux_recv(struct smux_config *config, smux_channel *ch, void *buf, size_t count)
{
}

ssize_t smux_flush(struct smux_config *config)
{
    char *write_buf = (char*)config->buffer.write_buf;
    unsigned write_buf_head = config->_internal.write_buf_head;
    unsigned write_buf_tail = config->_internal.write_buf_tail;
    size_t write_buf_size = config->buffer.write_buf_size;
    smux_write_fn write_fn = config->buffer.write_fn;
    int fd = config->buffer.write_fd;

    ssize_t ret;
    unsigned end;

    if(write_fn)
    {
        while(write_buf_head != write_buf_tail)
        {
            // end of area to transmit
            end = write_buf_tail < write_buf_head ? write_buf_head : write_buf_size;

            ret = write_fn(fd, (void*)(write_buf + write_buf_tail), end - write_buf_tail);
            if(ret <= 0)
                break;

            write_buf_tail = ADJRBI(write_buf_tail + ret, write_buf_size);
        }

        // write new tail index back
        config->_internal.write_buf_tail = write_buf_tail;

        if(ret < 0) // error?
            return ret;
    }
    return RBUSED(write_buf_head, write_buf_tail, write_buf_size);
}
