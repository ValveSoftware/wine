/*
 * Copyright 2024 Ziqing Hui for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


#if 0
#pragma makedep unix
#endif

#include "media-converter.h"

GST_ELEMENT_REGISTER_DECLARE(protonvideoconverter);
GST_ELEMENT_REGISTER_DECLARE(protonaudioconverter);
GST_ELEMENT_REGISTER_DECLARE(protonaudioconverterbin);
GST_ELEMENT_REGISTER_DECLARE(protondemuxer);

GST_DEBUG_CATEGORY(media_converter_debug);

static void get_dirname(const char *path, char *result)
{
    size_t i;

    for (i = strlen(path) - 1; i > 0; --i)
    {
        if (path[i] == '\\' || path[i] == '/')
            break;
    }

    memcpy(result, path, i);
    result[i] = 0;
}

static void path_concat(char *result, const char *a, const char *b)
{
    size_t a_len, b_offset;

    a_len = strlen(a);
    b_offset = a_len;
    memcpy(result, a, a_len);

    if (result[a_len - 1] != '/')
    {
        result[a_len] = '/';
        ++b_offset;
    }

    strcpy(result + b_offset, b);
}

static bool create_all_dir(const char *dir)
{
    char *ptr, buffer[4096] = {0};

    strcpy(buffer, dir);
    if (buffer[strlen(dir) - 1] != '/')
        buffer[strlen(dir)] = '/';

    ptr = strchr(buffer + 1, '/');
    while (ptr)
    {
        *ptr = '\0';

        if (mkdir(buffer, 0777) < 0)
        {
            if (errno != EEXIST)
            {
                GST_ERROR("Failed to make directory %s. %s.", buffer, strerror(errno));
                return false;
            }
        }
        *ptr = '/';

        ptr = strchr(ptr + 1, '/');
    }
    return true;
}

static int create_file(const char *file_name)
{
    int fd, ret = CONV_OK;
    char dir[4096];

    get_dirname(file_name, dir);
    if (access(dir, F_OK) < 0 && !create_all_dir(dir))
        return CONV_ERROR_PATH_NOT_FOUND;

    if ((fd = open(file_name, O_CREAT,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) < 0)
    {
        GST_WARNING("Failed to open file %s with O_CREAT. %s.", file_name, strerror(errno));
        return CONV_ERROR_OPEN_FAILED;
    }

    if (futimens(fd, NULL) < 0)
    {
        GST_WARNING("Failed to set file %s timestamps. %s.", file_name, strerror(errno));
        ret = CONV_ERROR;
    }

    close(fd);

    return ret;
}

bool open_file(const char *file_name, int open_flags, int *out_fd)
{
    int fd;

    if ((fd = open(file_name, open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) < 0)
    {
        GST_WARNING("Failed to open %s with flags %d. %s.", file_name, open_flags, strerror(errno));
        return false;
    }

    *out_fd = fd;
    return true;
}

bool get_file_size(int fd, uint64_t *file_size)
{
    struct stat file_info;

    if (fstat(fd, &file_info) < 0)
    {
        GST_WARNING("Failed to fstat fd %d. %s.", fd, strerror(errno));
        return false;
    }

    *file_size = file_info.st_size;
    return true;
}

bool complete_read(int file, void *buffer, size_t size)
{
    size_t current_read = 0;
    ssize_t read_size;
    errno = 0;

    while (current_read < size)
    {
        read_size = read(file, ((char *)buffer) + current_read, size - current_read);
        if(read_size <= 0)
        {
            if(errno != EINTR && errno != EAGAIN)
                return false;
        }
        else
        {
            current_read += read_size;
        }
    }

    return current_read == size;
}

bool complete_write(int file, const void *buffer, size_t size)
{
    size_t current_write = 0;
    ssize_t write_size;
    errno = 0;

    while (current_write < size)
    {
        write_size = write(file, (const char *)buffer + current_write, size - current_write);
        if(write_size < 0)
        {
            if(errno != EINTR && errno != EAGAIN)
                return false;
        }
        else
        {
            current_write += write_size;
        }
    }

    return current_write == size;
}

uint32_t crc32(uint32_t crc, const uint8_t *ptr, size_t buf_len)
{
    static const uint32_t s_crc_table[256] =
    {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535,
        0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd,
        0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d,
        0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
        0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac,
        0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
        0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
        0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb,
        0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea,
        0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce,
        0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
        0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409,
        0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739,
        0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2, 0x1e01f268,
        0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0,
        0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8,
        0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
        0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703,
        0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
        0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae,
        0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6,
        0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d,
        0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5,
        0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
        0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
    };
    uint32_t crc32 = (uint32_t)crc ^ 0xffffffff;
    const uint8_t *buffer = (const uint8_t *)ptr;

    while (buf_len >= 4)
    {
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ buffer[0]) & 0xff];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ buffer[1]) & 0xff];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ buffer[2]) & 0xff];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ buffer[3]) & 0xff];
        buffer += 4;
        buf_len -= 4;
    }

    while (buf_len)
    {
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ buffer[0]) & 0xff];
        ++buffer;
        --buf_len;
    }

    return ~crc32;
}

int create_placeholder_file(const char *file_name)
{
    const char *shader_path;
    char path[1024];
    int ret;

    if ((shader_path = getenv("STEAM_COMPAT_SHADER_PATH")))
    {
        path_concat(path, shader_path, file_name);
        if ((ret = create_file(path)) < 0)
            GST_ERROR("Failed to create %s file, ret %d.", file_name, ret);
    }
    else
    {
        GST_ERROR("Env STEAM_COMPAT_SHADER_PATH not set.");
        ret = CONV_ERROR_ENV_NOT_SET;
    }

    return ret;
}

int dump_fozdb_open(struct dump_fozdb *db, bool create, const char *file_path_env, int num_tags)
{
    char *dump_file_path;

    if (db->fozdb)
        return CONV_OK;

    if (!(dump_file_path = getenv(file_path_env)))
    {
        GST_ERROR("Env %s not set.", file_path_env);
        return CONV_ERROR_ENV_NOT_SET;
    }

    if (create)
        create_file(dump_file_path);

    return fozdb_create(dump_file_path, O_RDWR, false, num_tags, &db->fozdb);
}

void dump_fozdb_close(struct dump_fozdb *db)
{
    if (db->fozdb)
    {
        fozdb_release(db->fozdb);
        db->fozdb = NULL;
    }
}

bool media_converter_init(void)
{
    GST_DEBUG_CATEGORY_INIT(media_converter_debug,
            "protonmediaconverter", GST_DEBUG_FG_YELLOW, "Proton media converter");

    if (!GST_ELEMENT_REGISTER(protonvideoconverter, NULL))
    {
        GST_ERROR("Failed to register protonvideoconverter.");
        return false;
    }

    if (!GST_ELEMENT_REGISTER(protonaudioconverter, NULL))
    {
        GST_ERROR("Failed to register protonaudioconverter.");
        return false;
    }

    if (!GST_ELEMENT_REGISTER(protonaudioconverterbin, NULL))
    {
        GST_ERROR("Failed to register protonaudioconverterbin.");
        return false;
    }

    if (!GST_ELEMENT_REGISTER(protondemuxer, NULL))
    {
        GST_ERROR("Failed to register protondemuxer.");
        return false;
    }

    return true;
}
