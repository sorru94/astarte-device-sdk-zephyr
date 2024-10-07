/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file astarte_zlib.h
 * @brief Soft wrappers for zlib functions.
 *
 * @note If help is needed check out the RFC regargin the ZLIB format:
 * https://www.rfc-editor.org/rfc/rfc1950
 */

#ifndef ASTARTE_ZLIB_H
#define ASTARTE_ZLIB_H

#include <zlib.h>

// NOLINTBEGIN: This function is pretty much identical to the one contained in zlib.
/**
 * @brief Function equivalent to the `compress` function defined in zlib.h.
 *
 * @details The implementation is copied 1 to 1 from zlib, with some differences.
 * `defalteInit` has been replaced by `deflateInit2`. This allows to reduce the memory usage of zlib
 * by setting custom values for the `windowBits` and `memLevel` parameters.
 * This could also have been done by changing the values of the defines `MAX_WBITS` and
 * `DEF_MEM_LEVEL` in zconf.h. However this would have required to change the library sources and
 * has been avoided.
 * Furthermore, the allocation and deallocation functions from the Astarte library have been used in
 * place of the default ones.
 *
 * @note Since this function is almost a copy and paste from zlib.c, it will not be linted as every
 * other function in this library.
 *
 * @param dest See docstrings for `compress` in zlib.h
 * @param destLen See docstrings for `compress` in zlib.h
 * @param source See docstrings for `compress` in zlib.h
 * @param sourceLen See docstrings for `compress` in zlib.h
 * @return See docstrings for `compress` in zlib.h
 */
int ZEXPORT astarte_zlib_compress(
    Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen);
// NOLINTEND

// NOLINTBEGIN: This function is pretty much identical to the one contained in zlib.
/**
 * @brief Function equivalent to the `uncompress` function defined in zlib.h.
 *
 * @details The implementation is copied 1 to 1 from zlib, with a single difference.
 * The allocation and deallocation functions from the Astarte library have been used in place of the
 * default ones.
 *
 * @note Since this function is almost a copy and paste from zlib.c, it will not be linted as every
 * other function in this library.
 *
 * @param dest See docstrings for `compress` in zlib.h
 * @param destLen See docstrings for `compress` in zlib.h
 * @param source See docstrings for `compress` in zlib.h
 * @param sourceLen See docstrings for `compress` in zlib.h
 * @return See docstrings for `compress` in zlib.h
 */
int ZEXPORT astarte_zlib_uncompress(
    Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen);
// NOLINTEND

#endif /* ASTARTE_ZLIB_H */
