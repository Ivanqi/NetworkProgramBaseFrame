#ifndef EVENT_ENDIAN_H
#define EVENT_ENDIAN_H

#include <stdint.h>
#include <endian.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"

/**
 * 参考资料：https://linux.die.net/man/3/htobe64
 * 函数列表
 *  #include <endian.h>
 *  uint16_t htobe16(uint16_t host_16bits);
 *  uint16_t htole16(uint16_t host_16bits);
 *  uint16_t be16toh(uint16_t big_endian_16bits);
 *  uint16_t le16toh(uint16_t little_endian_16bits);
 *
 *  uint32_t htobe32(uint32_t host_32bits);
 *  uint32_t htole32(uint32_t host_32bits);
 *  uint32_t be32toh(uint32_t big_endian_32bits);
 *  uint32_t le32toh(uint32_t little_endian_32bits);
 *
 *  uint64_t htobe64(uint64_t host_64bits);
 *  uint64_t htole64(uint64_t host_64bits);
 *  uint64_t be64toh(uint64_t big_endian_64bits);
 *  uint64_t le64toh(uint64_t little_endian_64bits);
 *
 * 描述
 *  这些函数将整数值的字节编码从当前CPU（“主机”）使用的字节顺序转换为little-endian和big-endian字节顺序
 *  每个函数名称中的数字nn表示该函数处理的整数的大小，可以是16位，32位或64位。
 *
 *  名称形式为“ htobenn”的函数将从主机字节顺序转换为大端顺序
 *
 *  名称形式为“ htolenn”的函数从主机字节顺序转换为小端顺序
 *
 *  名称形式为“ benntoh”的函数将从big-endian顺序转换为主机字节顺序。
 *
 *  名称形式为“ lenntoh”的函数会从little-endian顺序转换为主机字节顺序。
 */

// 从主机字节顺序转换为大端顺序
inline uint64_t hostToNetwork64(uint64_t host64)
{
    return htobe64(host64);
}

inline uint32_t hostToNetwork32(uint32_t host32)
{
    return htobe32(host32);
}

inline uint16_t hostToNetwork16(uint16_t host16)
{
    return htobe16(host16);
}

inline uint64_t networkToHost64(uint64_t net64)
{
    return be64toh(net64);
}

inline uint32_t networkToHost32(uint32_t net64)
{
    return be32toh(net64);
}

// 从big-endian顺序转换为主机字节顺序
inline uint16_t networkToHost16(uint16_t net16)
{
    return be16toh(net16);
}

#pragma GCC diagnostic pop

#endif