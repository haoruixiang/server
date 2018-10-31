#ifndef __H_NET_BUFF_H
#define __H_NET_BUFF_H
#include <sys/uio.h>
#include <vector>
#include <string>
class HNetBuff
{
public:
    HNetBuff():buffer(1024),reader(0),writer(0),id(0),ptr(0){
    };
    ~HNetBuff(){
    };
    int ReadFd(int fd){
        char extrabuf[65536];
        struct iovec vec[2];
        const size_t writable = WritableBytes();
        vec[0].iov_base = Begin() + writer;
        vec[0].iov_len = writable;
        vec[1].iov_base = extrabuf;
        vec[1].iov_len = sizeof(extrabuf);
        const ssize_t n = ::readv(fd, vec, 2);
        if (n < 0) {
        } else if (static_cast<size_t>(n) <= writable) {
            writer += n;
        } else {
            writer = buffer.size();
            Append(extrabuf, n - writable);
        }
        return n;
    };
    size_t ReadableBytes() const {
        return writer - reader; 
    };
    size_t WritableBytes() const {
        return buffer.size() - writer;
    };
    void Append(const std::string& str) {
        Append(str.c_str(), str.size());
    };
    void Append(const char* data, size_t len) {
        EnsureWritableBytes(len);
        std::copy(data, data+len, BeginWrite());
        writer += len;
    };
    void EnsureWritableBytes(size_t len) {
        if (WritableBytes() < len) {
            MakeSpace(len);
        }
    }; 
    void MakeSpace(size_t len) {
        if (WritableBytes() + reader < len) {
            buffer.resize(writer + len);
        } else {
            size_t readable = ReadableBytes();
            std::copy(Begin()+reader, Begin()+writer, Begin());
            reader = 0;
            writer = readable;
        }
    };
    const char* Peek() const {
        return Begin() + reader;
    };
    void Retrieve(size_t len) {
        if (len < ReadableBytes()) {
            reader += len;
        } else {
            reader = 0;
            writer = 0;
        }
    };
    char* BeginWrite() {
        return Begin() + writer;
    };
    char* Begin() {
        return &*buffer.begin();
    };
    const char* Begin() const {
        return &*buffer.begin();
    };
    std::vector<char>  buffer;
    size_t             reader;
    size_t             writer;
    uint64_t           id;
    void*              ptr;
};

#endif
