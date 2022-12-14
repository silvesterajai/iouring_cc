#pragma once

#include <cstring>
#include <string>

namespace iouring {
// Intentionally value-compatible with Abseil's StatusCode.
//
enum class StatusCode : int
{
    kOk = 0,
    kCancelled = 1,
    kUnknown = 2,
    kInvalidArgument = 3,
    kDeadlineExceeded = 4,
    kNotFound = 5,
    kAlreadyExists = 6,
    kPermissionDenied = 7,
    kResourceExhausted = 8,
    kFailedPrecondition = 9,
    kAborted = 10,
    kOutOfRange = 11,
    kUnimplemented = 12,
    kInternal = 13,
    kUnavailable = 14,
    kDataLoss = 15,
    kUnauthenticated = 16,
    // ...
    // This range reserved for future allocation of Abseil status codes.
    // ...
    kClosed = 100,
    kUnimplemented2 = 101,
};

class [[nodiscard("Status is disabled")]] Status
{
public:
    /// Construct an OK instance.
    Status()
        : code_(StatusCode::kOk)
    {}

    /// Construct an instance with associated code and error_message.
    /// It is an error to construct an OK status with non-empty \a error_message.
    Status(StatusCode code, const std::string &error_message)
        : code_(code)
        , error_message_(error_message)
    {}

    /// @brief Construct an instance with `code`,  `error_message` and
    /// `error_details`. It is an error to construct an OK status with non-empty
    /// `error_message` and/or `error_details`.
    Status(StatusCode code, const std::string &error_message, const std::string &error_details)
        : code_(code)
        , error_message_(error_message)
        , binary_error_details_(error_details)
    {}

    /// Copy the specified status.
    inline Status(const Status &s) = default;

    /// Copy the specified status.
    inline Status &operator=(const Status &s) = default;

    /// Move the specified status.
    inline Status(Status && s) noexcept = default;

    /// Move the specified status.
    inline Status &operator=(Status &&s) noexcept = default;

    /// Return a success status
    [[nodiscard]] static Status OkStatus()
    {
        return Status();
    }

    /// Return the instance's error code.
    StatusCode Code() const
    {
        return code_;
    }
    /// Return the instance's error message.
    std::string ErrorMessage() const
    {
        static const std::string no_message = "";
        return Ok() ? no_message : error_message_;
    }
    /// Return the (binary) error details.
    std::string ErrorDetails() const
    {
        static const std::string no_message = "";
        return Ok() ? no_message : binary_error_details_;
    }

    /// Is the status OK?
    [[nodiscard]] bool Ok() const
    {
        return code_ == StatusCode::kOk;
    }

    /// Construct an instance with `code` and `error_message`
    static Status FromArgs(StatusCode code, const std::string &error_message)
    {
        return Status(code, error_message);
    }

    /// Construct an instance with `code`,  `error_message` and `error_details`.
    static Status FromArgs(StatusCode code, const std::string &error_message, const std::string &error_details)
    {
        return Status(code, error_message, error_details);
    }

    /// These convenience functions create an Status object with an error
    static Status AbortedError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kAborted, error_message);
    }
    static Status AlreadyExistsError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kAlreadyExists, error_message);
    }
    static Status CancelledError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kCancelled, error_message);
    }
    static Status DataLossError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kDataLoss, error_message);
    }
    static Status DeadlineExceededError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kDeadlineExceeded, error_message);
    }
    static Status FailedPreconditionError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kFailedPrecondition, error_message);
    }
    static Status InternalError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kInternal, error_message);
    }
    static Status InvalidArgumentError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kInvalidArgument, error_message);
    }
    static Status NotFoundError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kNotFound, error_message);
    }
    static Status OutOfRangeError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kOutOfRange, error_message);
    }
    static Status PermissionDeniedError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kPermissionDenied, error_message);
    }
    static Status ResourceExhaustedError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kResourceExhausted, error_message);
    }
    static Status UnauthenticatedError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kUnauthenticated, error_message);
    }
    static Status UnavailableError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kUnavailable, error_message);
    }
    static Status UnimplementedError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kUnimplemented, error_message);
    }
    static Status UnknownError(const std::string &error_message)
    {
        return FromArgs(StatusCode::kUnknown, error_message);
    }
    [[nodiscard]] bool IsAborted()
    {
        return Code() == StatusCode::kAborted;
    }
    [[nodiscard]] bool IsAlreadyExists()
    {
        return Code() == StatusCode::kAlreadyExists;
    }
    [[nodiscard]] bool IsCancelled()
    {
        return Code() == StatusCode::kCancelled;
    }
    [[nodiscard]] bool IsDataLoss()
    {
        return Code() == StatusCode::kDataLoss;
    }
    [[nodiscard]] bool IsDeadlineExceeded()
    {
        return Code() == StatusCode::kDeadlineExceeded;
    }
    [[nodiscard]] bool IsFailedPrecondition()
    {
        return Code() == StatusCode::kFailedPrecondition;
    }
    [[nodiscard]] bool IsInternal()
    {
        return Code() == StatusCode::kInternal;
    }
    [[nodiscard]] bool IsInvalidArgument()
    {
        return Code() == StatusCode::kInvalidArgument;
    }
    [[nodiscard]] bool IsNotFound()
    {
        return Code() == StatusCode::kNotFound;
    }
    [[nodiscard]] bool IsOutOfRange()
    {
        return Code() == StatusCode::kOutOfRange;
    }
    [[nodiscard]] bool IsPermissionDenied()
    {
        return Code() == StatusCode::kPermissionDenied;
    }
    [[nodiscard]] bool IsResourceExhausted()
    {
        return Code() == StatusCode::kResourceExhausted;
    }
    [[nodiscard]] bool IsUnauthenticated()
    {
        return Code() == StatusCode::kUnauthenticated;
    }
    [[nodiscard]] bool IsUnavailable()
    {
        return Code() == StatusCode::kUnavailable;
    }
    [[nodiscard]] bool IsUnimplemented()
    {
        return Code() == StatusCode::kUnimplemented;
    }
    [[nodiscard]] bool IsUnknown()
    {
        return Code() == StatusCode::kUnknown;
    }

    static StatusCode ErrnoToStatusCode(int error_number)
    {
        switch (error_number)
        {
        case 0:
            return StatusCode::kOk;
        case EINVAL: // Invalid argument
            return StatusCode::kInvalidArgument;
        case ENODEV: // No such device
        case ENOENT: // No such file or directory
            return StatusCode::kNotFound;
        default:
            return StatusCode::kUnknown;
        }
    }

    [[nodiscard]] static Status ErrnoToStatus(int error_number, const std::string &message)
    {
        return Status(ErrnoToStatusCode(error_number), message, strerror(error_number));
    }

private:
    StatusCode code_;
    std::string error_message_;
    std::string binary_error_details_;
};
} // namespace iouring
