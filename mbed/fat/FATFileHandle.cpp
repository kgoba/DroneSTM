/* mbed Microcontroller Library
 * Copyright (c) 2006-2012 ARM Limited
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "rtos.h"
#include "ff.h"
#include "ffconf.h"
#include "mbed_debug.h"

#include "FATFileHandle.h"

FATFileHandle::FATFileHandle(FIL fh, PlatformMutex * mutex): _mutex(mutex) {
    _fh = fh;
}

int FATFileHandle::close() {
    lock();
    int retval = f_close(&_fh);
    unlock();
    delete this;
    return retval;
}

ssize_t FATFileHandle::write(const void* buffer, size_t length) {
    lock();
    UINT n;
    FRESULT res = f_write(&_fh, buffer, length, &n);
    if (res) {
        debug_if(FFS_DBG, "f_write() failed: %d", res);
        unlock();
        return -1;
    }
    unlock();
    return n;
}

ssize_t FATFileHandle::read(void* buffer, size_t length) {
    lock();
    debug_if(FFS_DBG, "read(%d)\n", length);
    UINT n;
    FRESULT res = f_read(&_fh, buffer, length, &n);
    if (res) {
        debug_if(FFS_DBG, "f_read() failed: %d\n", res);
        unlock();
        return -1;
    }
    unlock();
    return n;
}

int FATFileHandle::isatty() {
    return 0;
}

off_t FATFileHandle::lseek(off_t position, int whence) {
    lock();
    if (whence == SEEK_END) {
        position += _fh.fsize;
    } else if(whence==SEEK_CUR) {
        position += _fh.fptr;
    }
    FRESULT res = f_lseek(&_fh, position);
    if (res) {
        debug_if(FFS_DBG, "lseek failed: %d\n", res);
        unlock();
        return -1;
    } else {
        debug_if(FFS_DBG, "lseek OK, returning %i\n", _fh.fptr);
        unlock();
        return _fh.fptr;
    }
}

int FATFileHandle::fsync() {
    lock();
    FRESULT res = f_sync(&_fh);
    if (res) {
        debug_if(FFS_DBG, "f_sync() failed: %d\n", res);
        unlock();
        return -1;
    }
    unlock();
    return 0;
}

off_t FATFileHandle::flen() {
    lock();
    off_t size = _fh.fsize;
    unlock();
    return size;
}

void FATFileHandle::lock() {
    _mutex->lock();
}

void FATFileHandle::unlock() {
    _mutex->unlock();
}
