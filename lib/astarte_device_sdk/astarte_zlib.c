/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "astarte_zlib.h"

#include "heap.h"

/************************************************
 *         Static functions declaration         *
 ***********************************************/

/**
 * @brief Custom allocation function for zlib.
 *
 * @param[in] opaque User data for the allocation function.
 * @param[in] items Number of items to allocate.
 * @param[in] size Size for each item to allocate.
 * @return The allocated memory region, NULL upon failure.
 */
static voidpf zcalloc(voidpf opaque, uInt items, uInt size);
/**
 * @brief Custom deallocation function for zlib.
 *
 * @param[in] opaque User data for the allocation function.
 * @param[in] address Address of the memory to deallocate.
 */
static void zcfree(voidpf opaque, voidpf address);

/************************************************
 *         Global functions definitions         *
 ***********************************************/

// NOLINTBEGIN: This function is pretty much identical to the one contained in zlib.
int ZEXPORT astarte_zlib_compress(
    Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    z_stream stream;
    int err;
    const uInt max = (uInt) -1;
    uLong left;

    left = *destLen;
    *destLen = 0;

    stream.zalloc = (alloc_func) zcalloc;
    stream.zfree = (free_func) zcfree;
    stream.opaque = (voidpf) 0;

    int windowBits = 9; // Smallest possible window
    int memLevel = 1; // Smallest possible memory usage
    err = deflateInit2(
        &stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits, memLevel, Z_DEFAULT_STRATEGY);
    if (err != Z_OK)
        return err;

    stream.next_out = dest;
    stream.avail_out = 0;
    stream.next_in = (z_const Bytef *) source;
    stream.avail_in = 0;

    do {
        if (stream.avail_out == 0) {
            stream.avail_out = left > (uLong) max ? max : (uInt) left;
            left -= stream.avail_out;
        }
        if (stream.avail_in == 0) {
            stream.avail_in = sourceLen > (uLong) max ? max : (uInt) sourceLen;
            sourceLen -= stream.avail_in;
        }
        err = deflate(&stream, sourceLen ? Z_NO_FLUSH : Z_FINISH);
    } while (err == Z_OK);

    *destLen = stream.total_out;
    deflateEnd(&stream);
    return err == Z_STREAM_END ? Z_OK : err;
}
// NOLINTEND

// NOLINTBEGIN: This function is pretty much identical to the one contained in zlib.
int ZEXPORT astarte_zlib_uncompress(
    Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    z_stream stream;
    int err;
    const uInt max = (uInt) -1;
    uLong len, left;
    Byte buf[1]; /* for detection of incomplete stream when *destLen == 0 */

    len = sourceLen;
    if (*destLen) {
        left = *destLen;
        *destLen = 0;
    } else {
        left = 1;
        dest = buf;
    }

    stream.next_in = (z_const Bytef *) source;
    stream.avail_in = 0;
    stream.zalloc = (alloc_func) zcalloc;
    stream.zfree = (free_func) zcfree;
    stream.opaque = (voidpf) 0;

    err = inflateInit(&stream);
    if (err != Z_OK)
        return err;

    stream.next_out = dest;
    stream.avail_out = 0;

    do {
        if (stream.avail_out == 0) {
            stream.avail_out = left > (uLong) max ? max : (uInt) left;
            left -= stream.avail_out;
        }
        if (stream.avail_in == 0) {
            stream.avail_in = len > (uLong) max ? max : (uInt) len;
            len -= stream.avail_in;
        }
        err = inflate(&stream, Z_NO_FLUSH);
    } while (err == Z_OK);

    sourceLen -= len + stream.avail_in;
    if (dest != buf)
        *destLen = stream.total_out;
    else if (stream.total_out && err == Z_BUF_ERROR)
        left = 1;

    inflateEnd(&stream);
    return err == Z_STREAM_END                          ? Z_OK
        : err == Z_NEED_DICT                            ? Z_DATA_ERROR
        : err == Z_BUF_ERROR && left + stream.avail_out ? Z_DATA_ERROR
                                                        : err;
}
// NOLINTEND

/************************************************
 *         Static functions definitions         *
 ***********************************************/

static voidpf zcalloc(voidpf opaque, unsigned items, unsigned size)
{
    (void) opaque;
    return sizeof(uInt) > 2 ? (voidpf) astarte_malloc(items * size)
                            : (voidpf) astarte_calloc(items, size);
}

static void zcfree(voidpf opaque, voidpf address)
{
    (void) opaque;
    astarte_free(address);
}
