/* statx.c -- part of Nemo file creation date extension
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#define _GNU_SOURCE
#define _ATFILE_SOURCE
#include <config.h>
#include <time.h>
#include <linux/fcntl.h>        // for AT_FDCWD, AT_NO_AUTOMOUNT
#include <linux/stat.h>         // for statx, STATX_BTIME, statx_timestamp
#include <string.h>             // for memset
#include <syscall.h>            // for __NR_statx
#include <unistd.h>             // for syscall, ssize_t
#include <stdio.h>
#include <errno.h>

#if NATIVE_STATX
/* native statx call */
static __attribute__((unused))
ssize_t statx (int dfd, const char *filename, unsigned flags,
          unsigned int mask, struct statx *buffer)
{
    return syscall (__NR_statx, dfd, filename, flags, mask, buffer);
}

#else
/* statx wrapper/compatibility */

/* this code works ony with x86 and x86_64 */
#if __x86_64__
#define __NR_statx 332
#else
#define __NR_statx 383
#endif

#define STATX_BTIME             0x00000800U     /* Want/got stx_btime */

struct statx_timestamp {
    __s64   tv_sec;
    __u32   tv_nsec;
    __s32   __reserved;
};

struct statx {
    /* 0x00 */
    __u32                   stx_mask;       /* What results were written [uncond] */
    __u32                   stx_blksize;    /* Preferred general I/O size [uncond] */
    __u64                   stx_attributes; /* Flags conveying information about the file [uncond] */
    /* 0x10 */
    __u32                   stx_nlink;      /* Number of hard links */
    __u32                   stx_uid;        /* User ID of owner */
    __u32                   stx_gid;        /* Group ID of owner */
    __u16                   stx_mode;       /* File mode */
    __u16                   __spare0[1];
    /* 0x20 */
    __u64                   stx_ino;        /* Inode number */
    __u64                   stx_size;       /* File size */
    __u64                   stx_blocks;     /* Number of 512-byte blocks allocated */
    __u64                   stx_attributes_mask; /* Mask to show what's supported in stx_attributes */
    /* 0x40 */
    struct statx_timestamp  stx_atime;      /* Last access time */
    struct statx_timestamp  stx_btime;      /* File creation time */
    struct statx_timestamp  stx_ctime;      /* Last attribute change time */
    struct statx_timestamp  stx_mtime;      /* Last data modification time */
    /* 0x80 */
    __u32                   stx_rdev_major; /* Device ID of special file [if bdev/cdev] */
    __u32                   stx_rdev_minor;
    __u32                   stx_dev_major;  /* ID of device containing file [uncond] */
    __u32                   stx_dev_minor;
    /* 0x90 */
    __u64                   __spare2[14];   /* Spare space for future expansion */
    /* 0x100 */
};

#define statx(a,b,c,d,e) syscall(__NR_statx,(a),(b),(c),(d),(e))

#endif // NATIVE_STATX


time_t
get_file_btime (const char *path)
{
    static int not_implemented = 0;

    int flags = AT_NO_AUTOMOUNT;
    unsigned int mask = STATX_BTIME;
    struct statx stxbuf;
    long ret = 0;
    time_t btime;

    btime = 0;

    if (not_implemented)
    {
        return btime;
    }

    memset (&stxbuf, 0xbf, sizeof(stxbuf));
    errno = 0;

    ret = statx (AT_FDCWD, path, flags, mask, &stxbuf);

    if (ret < 0)
    {
        if (errno == ENOSYS)
        {
            printf("nemo-creation-date: kernel needs to be (>= 4.15) - file creation dates not available\n");
            not_implemented = 1;
        }

        return btime;
    }

    btime = (&stxbuf)->stx_btime.tv_sec;

    return btime;
}