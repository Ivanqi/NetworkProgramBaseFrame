#ifndef NETWORKER_BASE_EXCEPTION_H
#define NETWORKER_BASE_EXCEPTION_H
#include "Types.h"
#include <exception>

namespace networker
{
    class Exception : public std::exception
    {
        private:
            string message_;
            string stack_;

        public:
            Exception(string what);
            ~Exception() noexcept override = default;

            const char* what() const noexcept override
            {
                return message_.c_str();
            }

            const char* stackTrace() const noexcept
            {
                return stack_.c_str();
            }
    };
};

#endif