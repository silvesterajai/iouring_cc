#include "iouring/io_uring.h"
#include "iouring/io_uring_api.h"
#include "logger.h"
#include <gtest/gtest.h>
#include <memory>
using namespace iouring;

TEST(supported, check_supported)
{
    EXPECT_TRUE(IsIoUringSupported());
}

class IoUringTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override 
    {
        IoUringParams params;
        params.DisableSubConsumeThread();
        io_uring_ = std::make_unique<IoUring>(params);
    }
    IoUring &Ring()
    {
        return *io_uring_.get();
    }

    virtual void TearDown() override {}

protected:
    std::unique_ptr<IoUring> io_uring_;
};

TEST_F(IoUringTestFixture, construction)
{
    EXPECT_EQ(0, io_uring_->Flag());
    auto required_ops = {
        IORING_OP_POLL_ADD, // linux 5.1
        IORING_OP_READV,
        IORING_OP_WRITEV,
        IORING_OP_FSYNC,
        IORING_OP_SENDMSG, // linux 5.3
        IORING_OP_RECVMSG,
        IORING_OP_ACCEPT,
        IORING_OP_CONNECT,
        IORING_OP_READ, // linux 5.6
        IORING_OP_WRITE,
        IORING_OP_SEND,
        IORING_OP_RECV,
        IORING_OP_PROVIDE_BUFFERS,
    };
    EXPECT_TRUE(io_uring_->OpsSupported(required_ops));

    auto required_features = IORING_FEAT_SUBMIT_STABLE | IORING_FEAT_NODROP | IORING_FEAT_FAST_POLL;
    EXPECT_TRUE(io_uring_->FeaturesSupported(required_features));
}

TEST_F(IoUringTestFixture, register_eventfd)
{
    EXPECT_TRUE(io_uring_->IsEventFdRegistered());
}

TEST_F(IoUringTestFixture, cqe_advance)
{
    auto space_left = Ring().SqeSpaceLeft();
    EXPECT_EQ(space_left, IOURING_DEFAULT_CQE_ENTRIES);
    auto ready = Ring().CqeReady();
    EXPECT_EQ(ready, 0);
    auto noop_promise = iouring::Noop(Ring());
    space_left = Ring().SqeSpaceLeft();
    EXPECT_EQ(space_left, IOURING_DEFAULT_CQE_ENTRIES - 1);
    ready = Ring().CqeReady();
    EXPECT_EQ(ready, 0);
    auto status = Ring().FlushSubmissionRing();
    sleep(1);
    ready = Ring().CqeReady();
    EXPECT_EQ(ready, 1);
    space_left = Ring().SqeSpaceLeft();
    EXPECT_EQ(space_left, IOURING_DEFAULT_CQE_ENTRIES);
    status = Ring().ProcessComplitionsRing();
    ready = Ring().CqeReady();
    EXPECT_EQ(ready, 0);
}

