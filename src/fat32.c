/* fat32.c — FAT32 文件系统读取 */

#include "kernel.h"

#pragma pack(1)
typedef struct {
    uint8  jmp[3];
    uint8  oem[8];
    uint16 bytes_per_sector;
    uint8  sectors_per_cluster;
    uint16 reserved_sectors;
    uint8  fat_count;
    uint16 root_entries;
    uint16 total_sectors_16;
    uint8  media_type;
    uint16 sectors_per_fat_16;
    uint16 sectors_per_track;
    uint16 heads;
    uint32 hidden_sectors;
    uint32 total_sectors_32;
    /* FAT32 扩展 */
    uint32 sectors_per_fat;
    uint16 flags;
    uint16 version;
    uint32 root_cluster;
    uint16 fsinfo_sector;
    uint16 backup_boot_sector;
    uint8  reserved[12];
    uint8  drive_number;
    uint8  nt_flags;
    uint8  signature;
    uint32 serial;
    uint8  label[11];
    uint8  system_id[8];
} fat_bpb_t;

typedef struct {
    uint8  name[11];
    uint8  attr;
    uint8  nt_reserved;
    uint8  create_time_tenth;
    uint16 create_time;
    uint16 create_date;
    uint16 access_date;
    uint16 first_cluster_hi;
    uint16 write_time;
    uint16 write_date;
    uint16 first_cluster_lo;
    uint32 file_size;
} fat_dir_entry_t;
#pragma pack()

struct fat32_fs {
    fat_bpb_t bpb;
    uint32    fat_start;
    uint32    data_start;
    uint32    total_clusters;
    uint8     sector_buf[512];
    uint32    fat_buf[128];  /* 缓存 128 个 FAT 项 */
    uint32    fat_buf_start;
};

fat32_fs_t *mounted_fs = NULL;

/* 从 LBA 读取一个扇区 */
static int read_sector(uint32 lba, uint8 *buf) {
    return ata_read_sector(lba, buf);
}

/* 读取 FAT 表项 */
static uint32 fat_read(fat32_fs_t *fs, uint32 cluster) {
    uint32 fat_offset = cluster * 4;
    uint32 fat_sector = fs->fat_start + fat_offset / 512;
    uint32 ent_offset = fat_offset % 512;

    read_sector(fat_sector, fs->sector_buf);
    return *(uint32*)(fs->sector_buf + ent_offset) & 0x0FFFFFFF;
}

/* 读取簇中指定偏移的数据 */
static int read_cluster_offset(fat32_fs_t *fs, uint32 cluster, uint32 offset, void *buf, uint32 size) {
    uint32 bytes_per_cluster = fs->bpb.sectors_per_cluster * 512;
    uint8 tmp[512];

    while (size > 0 && cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32 sector = fs->data_start + (cluster - 2) * fs->bpb.sectors_per_cluster + offset / 512;
        uint32 sec_off = offset % 512;
        uint32 chunk = 512 - sec_off;
        if (chunk > size) chunk = size;

        read_sector(sector, tmp);
        memcpy(buf, tmp + sec_off, chunk);

        buf = (uint8*)buf + chunk;
        size -= chunk;
        offset += chunk;

        if (offset >= bytes_per_cluster) {
            offset = 0;
            cluster = fat_read(fs, cluster);
            if (cluster >= 0x0FFFFFF8) break;
        }
    }
    return 0;
}

/* 读取整个簇 */
static int read_cluster(fat32_fs_t *fs, uint32 cluster, void *buf) {
    for (int i = 0; i < fs->bpb.sectors_per_cluster; i++) {
        uint32 sector = fs->data_start + (cluster - 2) * fs->bpb.sectors_per_cluster + i;
        read_sector(sector, (uint8*)buf + i * 512);
    }
    return 0;
}

fat32_fs_t* fat32_mount(void) {
    fat32_fs_t *fs = (fat32_fs_t*)kmalloc(sizeof(fat32_fs_t));
    if (!fs) return NULL;
    memset(fs, 0, sizeof(fat32_fs_t));

    /* 读取 MBR / VBR (LBA 0 = 分区引导扇区) */
    if (read_sector(0, (uint8*)&fs->bpb) != 0) {
        kfree(fs);
        return NULL;
    }

    /* 验证签名 */
    if (fs->bpb.signature != 0x28 && fs->bpb.signature != 0x29) {
        /* 尝试读取 MBR 分区表，获取第一个分区 */
        uint8 mbr[512];
        read_sector(0, mbr);
        /* 检查 MBR 签名 */
        if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
            kfree(fs);
            return NULL;
        }
        /* 分区表在 offset 446 */
        uint32 part_start = *(uint32*)(mbr + 454);
        if (part_start == 0) { kfree(fs); return NULL; }
        read_sector(part_start, (uint8*)&fs->bpb);
        if (fs->bpb.signature != 0x28 && fs->bpb.signature != 0x29) {
            kfree(fs);
            return NULL;
        }
    }

    /* 计算 FAT 和数据区起始 */
    fs->fat_start = fs->bpb.reserved_sectors;
    fs->data_start = fs->fat_start + fs->bpb.fat_count * fs->bpb.sectors_per_fat;
    fs->total_clusters = (fs->bpb.total_sectors_32 - fs->data_start) / fs->bpb.sectors_per_cluster;
    fs->fat_buf_start = 0xFFFFFFFF;  /* 无效标记 */

    mounted_fs = fs;
    return fs;
}

/* 读取目录项 */
static int read_dir(fat32_fs_t *fs, uint32 cluster, fat32_file_t *files, int max) {
    uint8 *buf = (uint8*)kmalloc(512 * fs->bpb.sectors_per_cluster);
    if (!buf) return 0;
    int count = 0;

    while (cluster >= 2 && cluster < 0x0FFFFFF8 && count < max) {
        read_cluster(fs, cluster, buf);
        fat_dir_entry_t *entries = (fat_dir_entry_t*)buf;
        int entries_per_cluster = (512 * fs->bpb.sectors_per_cluster) / sizeof(fat_dir_entry_t);

        for (int i = 0; i < entries_per_cluster && count < max; i++) {
            if (entries[i].name[0] == 0x00) goto done;
            if (entries[i].name[0] == 0xE5) continue;
            if (entries[i].attr == 0x0F) continue;  /* 跳过 LFN */

            /* 提取文件名 */
            char *dst = files[count].name;
            for (int j = 0; j < 8; j++) {
                if (entries[i].name[j] == ' ') break;
                *dst++ = entries[i].name[j];
            }
            if (entries[i].name[8] != ' ') {
                *dst++ = '.';
                for (int j = 8; j < 11; j++) {
                    if (entries[i].name[j] == ' ') break;
                    *dst++ = entries[i].name[j];
                }
            }
            *dst = '\0';

            files[count].size = entries[i].file_size;
            files[count].cluster = entries[i].first_cluster_lo | ((uint32)entries[i].first_cluster_hi << 16);
            files[count].is_dir = (entries[i].attr & 0x10) ? 1 : 0;
            files[count].cur_cluster = files[count].cluster;
            files[count].cur_offset = 0;
            files[count].fs = fs;
            count++;
        }
        cluster = fat_read(fs, cluster);
    }
done:
    kfree(buf);
    return count;
}

int fat32_readdir(fat32_fs_t *fs, const char *path, fat32_file_t *files, int max) {
    fat32_file_t dir_entries[64];
    int count = read_dir(fs, fs->bpb.root_cluster, dir_entries, 64);

    /* 如果 path 是 "/"，直接返回根目录 */
    if (path[0] == '/' && path[1] == '\0') {
        int n = count < max ? count : max;
        for (int i = 0; i < n; i++) files[i] = dir_entries[i];
        return n;
    }

    /* 简单路径解析: 只支持 "/dirname" 形式 */
    const char *name = path;
    if (*name == '/') name++;

    /* 在根目录中查找 */
    for (int i = 0; i < count; i++) {
        if (!dir_entries[i].is_dir) continue;
        if (strcmp(dir_entries[i].name, name) == 0) {
            return read_dir(fs, dir_entries[i].cluster, files, max);
        }
    }
    return -1;
}

fat32_file_t* fat32_open(fat32_fs_t *fs, const char *path) {
    fat32_file_t dir_entries[64];
    int count = read_dir(fs, fs->bpb.root_cluster, dir_entries, 64);

    const char *name = path;
    if (*name == '/') name++;

    /* 在根目录中查找文件 */
    for (int i = 0; i < count; i++) {
        if (dir_entries[i].is_dir) continue;
        if (strcmp(dir_entries[i].name, name) == 0) {
            fat32_file_t *f = (fat32_file_t*)kmalloc(sizeof(fat32_file_t));
            if (!f) return NULL;
            memcpy(f, &dir_entries[i], sizeof(fat32_file_t));
            return f;
        }
    }
    return NULL;
}

int fat32_read(fat32_file_t *f, void *buf, uint32 size) {
    if (f->cur_offset >= f->size) return 0;
    uint32 remaining = f->size - f->cur_offset;
    if (size > remaining) size = remaining;

    if (read_cluster_offset(f->fs, f->cur_cluster, f->cur_offset, buf, size) == 0) {
        f->cur_offset += size;
        return size;
    }
    return -1;
}

void fat32_close(fat32_file_t *f) {
    kfree(f);
}