#ifndef NETWORK_BASE_NONCOPYABLE_H
#define NETWORK_BASE_NONCOPYABLE_H

namespace network
{
    class noncopyable
    {
        public:
            // 禁止拷贝函数
            noncopyable(const noncopyable&) = delete;

            // 禁止赋值
            void operator=(const noncopyable&) = delete;

        protected:        
            noncopyable() = default;
            ~noncopyable()= default;
    };
};

#endif