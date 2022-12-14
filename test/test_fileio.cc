#include "io_request.h"
#include "iouring/buffer.h"
#include "iouring/io_uring.h"
#include "iouring/io_uring_utils.h"
#include "iouring/io_uring_api.h"
#include <gtest/gtest.h>
#include <memory>

using namespace iouring;

class FileIoTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override {}

    void SetUp(std::string filename)
    {
        IoUringParams params;
        params.AddProvidedBuffer(8193, 1024, 10)
              .ShouldSupportOpProvideBuffers()
              .ShouldSupportFeatFastPoll();
        io_uring_ = std::make_unique<IoUring>(params);
        filename_ = filename;
        fd_ = open(filename_.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        IOURING_ASSERT((fd_ > 0), "Failed to open file");
    }

    virtual void TearDown() override
    {
        if (fd_ > 0)
        {
            close(fd_);
            unlink(filename_.c_str());
        }
        fd_ = -1;
    }
    void Reopen()
    {
        if (fd_ > 0)
        {
            close(fd_);
            fd_ = -1;
        }
        SetUp(filename_);
    }
    IoUring &Ring()
    {
        return *io_uring_.get();
    }
protected:
    std::unique_ptr<IoUring> io_uring_;
    std::string tmp_dir_;
    std::string filename_;
    os_fd_t fd_;
};

void FillWriteData(std::vector<std::vector<uint8_t>> &write_datas)
{
    int entry = (int)'a';
    for (auto &write_data : write_datas)
    {
       for (auto &d : write_data)
           d = entry;
       ++entry;
    }
}

void FillWriteData(std::vector<uint8_t> &write_data)
{
    std::vector<std::vector<uint8_t>> write_datas = {write_data};
    FillWriteData(write_datas);
}

void ToIovecs(std::vector<std::vector<uint8_t>> &datas, std::vector<struct iovec> &iovecs)
{
    int index = 0;
    for (auto &data : datas)
    {
        iovecs[index].iov_base = data.data();
        iovecs[index].iov_len = data.size();
        index++;
    }
}

TEST_F(FileIoTestFixture, ReadSingle)
{
    SetUp("/tmp/testfileio.txt");
    std::vector<uint8_t> data(10);
    FillWriteData(data);
    IoCompletionCb callback = [&](ssize_t res, IoCompletion* comp) { 
        EXPECT_EQ(10, res);
        EXPECT_EQ(10, comp->Res());
        EXPECT_EQ(fd_, ((DefaultCompletion*)comp)->Fd());
    };
    auto write_promise = iouring::Write(Ring(), fd_, data.data(), data.size(), 0, callback);
    EXPECT_EQ(10, write_promise.Get());
    std::vector<uint8_t> read_data(10);
    auto read_promise = iouring::Read(Ring(), fd_, read_data.data(), read_data.size(), 0, [&](ssize_t res, IoCompletion* comp) {
        EXPECT_EQ(10, res);
        EXPECT_EQ(10, comp->Res());
        EXPECT_EQ(fd_, ((DefaultCompletion*)comp)->Fd());
    });
    EXPECT_EQ(10, read_promise.Get());
    EXPECT_EQ(data, read_data);
}

TEST_F(FileIoTestFixture, ReadSingleNoCb)
{
    SetUp("/tmp/testfileio.txt");
    std::vector<uint8_t> data(10);
    FillWriteData(data);
    auto write_promise = iouring::Write(Ring(), fd_, data.data(), data.size(), 0);
    EXPECT_EQ(10, write_promise.Get());
    std::vector<uint8_t> read_data(10);
    auto read_promise = iouring::Read(Ring(), fd_, read_data.data(), read_data.size(), 0);
    EXPECT_EQ(10, read_promise.Get());
    EXPECT_EQ(data, read_data);
}

TEST_F(FileIoTestFixture, ReadVector)
{
    SetUp("/tmp/testfileio.txt");
    std::vector<std::vector<uint8_t>> write_datas = {std::vector<uint8_t>(10), std::vector<uint8_t>(15)};
    FillWriteData(write_datas);
    std::vector<struct iovec> iovecs(2);
    ToIovecs(write_datas, iovecs);
    IoCompletionCb callback = [&](ssize_t res, IoCompletion* comp) { 
        EXPECT_EQ(25, res);
        EXPECT_EQ(25, comp->Res());
    };
    auto write_promise = iouring::WriteVector(Ring(), fd_, iovecs, 0, 0, callback);
    EXPECT_EQ(25, write_promise.Get());
    std::vector<std::vector<uint8_t>> read_datas = {std::vector<uint8_t>(10), std::vector<uint8_t>(15)};
    std::vector<struct iovec> iovecs_rd(2);
    ToIovecs(read_datas, iovecs_rd);
    auto read_promise = iouring::ReadVector(Ring(), fd_, iovecs_rd, 0, 0, [](ssize_t res, IoCompletion* comp) { 
        EXPECT_EQ(25, res);
        EXPECT_EQ(25, comp->Res());
    });
    EXPECT_EQ(25, read_promise.Get());
    EXPECT_EQ(write_datas[0], read_datas[0]);
    EXPECT_EQ(write_datas[1], read_datas[1]);
}
