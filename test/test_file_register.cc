#include "logger.h"
#include "test_include.h"
#include <filesystem>

template<int N>
class FileRegisterTestFixture : public ::testing::Test
{
protected:
    static const int kRegisteredFdSize = N;
protected:
    FileRegisterTestFixture() : directory_("/tmp/iouring_cc/filereg")
    {
        RemoveDirectory();
        std::filesystem::create_directories(directory_);
    }
    virtual ~FileRegisterTestFixture()
    {
        RemoveDirectory();
    }
    void RemoveDirectory()
    {
        for(uint i = 0; i < fds_.size(); ++i)
        {
            if (fds_[i] >= 0)
            {
                std::string filename = fmt::format("filereg.{}", i);
                ::close(fds_[i]);
                ::unlink(filename.c_str());
            }
        }
        std::filesystem::remove_all(directory_);
    }
    std::vector<fd_t> Openfiles(const int &count)
    {
        std::vector<fd_t> fds_local;
        for(int i = 0; i < count; ++i)
        {
            std::string filename = fmt::format("filereg.{}", fds_.size() + i);
            auto fd = ::open(filename.c_str(), O_RDWR | O_CREAT, 0644);
            fds_local.push_back(fd);
        }
        fds_.insert(fds_.end(), fds_local.begin(), fds_local.end());
        return fds_local;
    }

    fd_t FileDescriptor(uint32_t index)
    {
        return fds_[index];
    }

    virtual void SetUp() override
    {
        IoUringParams params;
        params.SetMaxFdSlots(kRegisteredFdSize);
        io_uring_ = std::make_unique<IoUring>(params);
    }

    IoUring &Ring()
    {
        return *(io_uring_.get());
    }
    

protected:
    std::string directory_;
    std::vector<fd_t> fds_;
    std::unique_ptr<IoUring> io_uring_;
};

using FileRegisterTestFixtureFixedSize = FileRegisterTestFixture<16>;
TEST_F(FileRegisterTestFixtureFixedSize, Basic)
{
    auto fd_count = kRegisteredFdSize;
    auto fds = Openfiles(fd_count);
    auto ret = io_uring_->RegisterFiles(fds);
    EXPECT_TRUE(ret.Ok()) << ret.ErrorMessage() << ret.ErrorDetails();
    ret = io_uring_->UnregisterFiles();
    EXPECT_TRUE(ret.Ok()) << ret.ErrorMessage() << ret.ErrorDetails();
}

TEST_F(FileRegisterTestFixtureFixedSize, BasicFail)
{
    auto fd_count = kRegisteredFdSize + 10;
    auto fds = Openfiles(fd_count);
    auto ret = io_uring_->RegisterFiles(fds);
    EXPECT_FALSE(ret.Ok()) << ret.ErrorMessage();
    ret = io_uring_->UnregisterFiles();
    EXPECT_TRUE(ret.Ok()) << ret.ErrorMessage();
}

TEST_F(FileRegisterTestFixtureFixedSize, ReadWrite)
{
    auto fd_count = kRegisteredFdSize;
    auto fds = Openfiles(fd_count);
    auto ret = io_uring_->RegisterFiles(fds);

    auto count = 1024;
    char bufs_write[count];
    char bufs_read[count];
    memset(bufs_write, 0x20, count);

    auto write_fut = iouring::Write(Ring(), FileDescriptor(0), bufs_write, count, 0);
    auto write_ret = write_fut.Get();
    EXPECT_EQ(count, write_ret);

    auto read_fut = iouring::Read(Ring(), FileDescriptor(0), bufs_read, count, 0);
    auto read_ret = read_fut.Get();
    EXPECT_EQ(count, read_ret);
    EXPECT_EQ(0, memcmp(bufs_write, bufs_read, count));

    auto stats = Ring().Stats();
    EXPECT_EQ(stats.ops_[Operation::IORING_OP_READ], 1);
    EXPECT_EQ(stats.ops_[Operation::IORING_OP_WRITE], 1);
    EXPECT_EQ(stats.sqe_flags_[IOSQE_FIXED_FILE_BIT], 2); //One for write and one for read

}

using FileRegisterTestFixtureZeroSize = FileRegisterTestFixture<0>;
TEST_F(FileRegisterTestFixtureZeroSize, RegisterExternal)
{
    auto fd_count = 10;
    auto fds = Openfiles(fd_count);
    auto count = io_uring_->RegisterFilesExternal(fds, 0);
    EXPECT_EQ(fd_count, count);
    auto ret = io_uring_->UnregisterFiles();
    EXPECT_TRUE(ret.Ok()) << ret.ErrorMessage() << ret.ErrorDetails();
}

TEST_F(FileRegisterTestFixtureZeroSize, RegisterExternalFail)
{
    auto fd_count = 10;
    auto fds = Openfiles(fd_count);
    auto count = io_uring_->RegisterFilesExternal(fds, 0);
    EXPECT_EQ(fd_count, count);
    fds = Openfiles(fd_count);
    count = io_uring_->RegisterFilesExternal(fds, fd_count);
    EXPECT_EQ(-16, count);
    count = io_uring_->RegisterFilesExternal(fds, 0, true);
    EXPECT_EQ(fd_count, count);
    auto ret = io_uring_->UnregisterFiles();
    EXPECT_TRUE(ret.Ok()) << ret.ErrorMessage() << ret.ErrorDetails();
}

TEST_F(FileRegisterTestFixtureZeroSize, ReadWrite)
{
    auto fd_count = 10;
    auto fds = Openfiles(fd_count);
    auto count = io_uring_->RegisterFilesExternal(fds, 0);
    EXPECT_EQ(fd_count, count);

    count = 1024;
    char bufs_write[count];
    char bufs_read[count];
    memset(bufs_write, 0x20, count);

    auto write_fut = iouring::Write(Ring(), 0, bufs_write, count, 0, NoOpIoCompletionCb, IoApiCommonVars().SqeFlag(IOSQE_FIXED_FILE));
    auto write_ret = write_fut.Get();
    EXPECT_EQ(count, write_ret);

    auto read_fut = iouring::Read(Ring(), 0, bufs_read, count, 0, NoOpIoCompletionCb, IoApiCommonVars().SqeFlag(IOSQE_FIXED_FILE));
    auto read_ret = read_fut.Get();
    EXPECT_EQ(count, read_ret);
    EXPECT_EQ(0, memcmp(bufs_write, bufs_read, count));

    auto stats = Ring().Stats();
    EXPECT_EQ(stats.ops_[Operation::IORING_OP_READ], 1);
    EXPECT_EQ(stats.ops_[Operation::IORING_OP_WRITE], 1);
    EXPECT_EQ(stats.sqe_flags_[IOSQE_FIXED_FILE_BIT], 2); //One for write and one for read

}

TEST_F(FileRegisterTestFixtureZeroSize, RegisterFileAsync)
{
    bool supported = Ring().OpsSupported({Operation::IORING_OP_FILES_UPDATE});
    if (!supported)
    {
        LOG_DEBUG("IORING_OP_FILES_UPDATE is not supported.");
        return;
    }
    auto fd_count = 10;
    auto fds = Openfiles(fd_count);
    auto count = io_uring_->RegisterFilesExternal(fds, 0);
    fds = Openfiles(fd_count);
    auto register_fut = iouring::FilesUpdate(Ring(), fds, 0);
    EXPECT_EQ(register_fut.Get(), count);

    count = 1024;
    char bufs_write[count];
    char bufs_read[count];
    memset(bufs_write, 0x20, count);

    auto write_fut = iouring::Write(Ring(), 0, bufs_write, count, 0, NoOpIoCompletionCb, IoApiCommonVars().SqeFlag(IOSQE_FIXED_FILE));
    auto write_ret = write_fut.Get();
    EXPECT_EQ(count, write_ret);

    auto read_fut = iouring::Read(Ring(), 0, bufs_read, count, 0, NoOpIoCompletionCb, IoApiCommonVars().SqeFlag(IOSQE_FIXED_FILE));
    auto read_ret = read_fut.Get();
    EXPECT_EQ(count, read_ret);
    EXPECT_EQ(0, memcmp(bufs_write, bufs_read, count));

    auto stats = Ring().Stats();
    EXPECT_EQ(stats.ops_[Operation::IORING_OP_FILES_UPDATE], 1);
    EXPECT_EQ(stats.ops_[Operation::IORING_OP_READ], 1);
    EXPECT_EQ(stats.ops_[Operation::IORING_OP_WRITE], 1);
    EXPECT_EQ(stats.sqe_flags_[IOSQE_FIXED_FILE_BIT], 2); //One for write and one for read

}