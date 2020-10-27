/*
 * libstored, a Store for Embedded Debugger.
 * Copyright (C) 2020  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libstored/util.h>

#include <cstring>
#include <cinttypes>

namespace stored {

/*!
 * \brief Like \c ::strncpy(), but without padding and returning the length of the string.
 * \ingroup libstored_util
 */
size_t strncpy(char* __restrict__ dst, char const* __restrict__ src, size_t len) {
	if(len == 0)
		return 0;

	stored_assert(dst);
	stored_assert(src);

	size_t res = 0;
	for(; res < len && src[res]; res++)
		dst[res] = src[res];

	return res;
}

/*!
 * \brief Like \c ::strncmp(), but handles non zero-terminated strings.
 * \ingroup libstored_util
 */
int strncmp(char const* __restrict__ str1, size_t len1, char const* __restrict__ str2, size_t len2) {
	stored_assert(str1);
	stored_assert(str2);

	size_t i = 0;
	for(; i < len1 && i < len2; i++) {
		char c1 = str1[i];
		char c2 = str2[i];

		if(c1 < c2)
			return -1;
		else if(c1 > c2)
			return 1;
		else if(c1 == 0)
			// early-terminate same strings
			return 0;
	}

	if(len1 == len2)
		// same strings
		return 0;
	else if(i == len1)
		// str1 was shortest
		return str2[i] ? -1 : 0;
	else
		// str2 was shortest
		return str1[i] ? 1 : 0;
}

/*!
 * \brief Swap endianness of the given buffer.
 * \ingroup libstored_util
 */
void swap_endian(void* buffer, size_t len) {
	char* buffer_ = static_cast<char*>(buffer);
	for(size_t i = 0; i < len / 2; i++) {
		char c = buffer_[i];
		buffer_[i] = buffer_[len - i - 1];
		buffer_[len - i - 1] = c;
	}
}

/*!
 * \brief \c memcpy() with endianness swapping.
 * \ingroup libstored_util
 */
void memcpy_swap(void* __restrict__ dst, void const* __restrict__ src, size_t len) {
	char* dst_ = static_cast<char*>(dst);
	char const* src_ = static_cast<char const*>(src);

	for(size_t i = 0; i < len; i++)
		dst_[i] = src_[len - i - 1];
}

/*!
 * \brief memcmp() with endianness swapping.
 * \ingroup libstored_util
 */
int memcmp_swap(void const* a, void const* b, size_t len) {
	unsigned char const* a_ = static_cast<unsigned char const*>(a);
	unsigned char const* b_ = static_cast<unsigned char const*>(b);

	size_t i = 0;
	for(; i < len; i++)
		if(a_[i] != b_[len - i - 1])
			goto diff;

	return 0;

diff:
	return a_[i] < b_[i] ? -1 : 1;
}

/*!
 * \brief Converts the given buffer to a string literal.
 *
 * This comes in handy for verbose output of binary data, like protocol messages.
 *
 * \ingroup libstored_util
 */
std::string string_literal(void const* buffer, size_t len, char const* prefix) {
	std::string s;
	if(prefix)
		s += prefix;

	uint8_t const* b = static_cast<uint8_t const*>(buffer);
	char buf[16];
	for(size_t i = 0; i < len; i++) {
		switch(b[i]) {
		case '\0': s += "\\0"; break;
		case '\r': s += "\\r"; break;
		case '\n': s += "\\n"; break;
		case '\t': s += "\\t"; break;
		case '\\': s += "\\\\"; break;
		default:
			if(b[i] < 0x20 || b[i] >= 0x7f) {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
				snprintf(buf, sizeof(buf), "\\x%02" PRIx8, b[i]);
				s += buf;
			} else {
				s += (char)b[i];
			}
		}
	}

	return s;
}

} // namespace

