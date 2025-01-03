#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <math.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define section ((uintptr_t)hdr->BPB_BytsPerSec)
#define clust (section * (uintptr_t)hdr->BPB_SecPerClus)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

struct fat32hdr {
    u8  BS_jmpBoot[3];
    u8  BS_OEMName[8];
    u16 BPB_BytsPerSec;
    u8  BPB_SecPerClus;
    u16 BPB_RsvdSecCnt;
    u8  BPB_NumFATs;
    u16 BPB_RootEntCnt;
    u16 BPB_TotSec16;
    u8  BPB_Media;
    u16 BPB_FATSz16;
    u16 BPB_SecPerTrk;
    u16 BPB_NumHeads;
    u32 BPB_HiddSec;
    u32 BPB_TotSec32;
    u32 BPB_FATSz32;
    u16 BPB_ExtFlags;
    u16 BPB_FSVer;
    u32 BPB_RootClus;
    u16 BPB_FSInfo;
    u16 BPB_BkBootSec;
    u8  BPB_Reserved[12];
    u8  BS_DrvNum;
    u8  BS_Reserved1;
    u8  BS_BootSig;
    u32 BS_VolID;
    u8  BS_VolLab[11];
    u8  BS_FilSysType[8];
    u8  __padding_1[420];
    u16 Signature_word;
} __attribute__((packed));

struct fat32dent {
    u8  DIR_Name[11];
    u8  DIR_Attr;
    u8  DIR_NTRes;
    u8  DIR_CrtTimeTenth;
    u16 DIR_CrtTime;
    u16 DIR_CrtDate;
    u16 DIR_LastAccDate;
    u16 DIR_FstClusHI;
    u16 DIR_WrtTime;
    u16 DIR_WrtDate;
    u16 DIR_FstClusLO;
    u32 DIR_FileSize;
} __attribute__((packed));

struct fat32long {
    u8  LDIR_Ord;       // 0
    u16 LDIR_Name1[5];  // 1
    u8  LDIR_Attr;      // 11
    u8  LDIR_Type;      // 12
    u8  LDIR_Chksum;    // 13
    u16 LDIR_Name2[6];  // 14
    u16 LDIR_FstClusLO; // 26
    u16 LDIR_Name3[2];  // 28
} __attribute__((packed));

typedef struct tagBITMAPFILEHEADER 
{
    u16 bfType;
    u32 bfSize;
    u16 bfReserved1;
    u16 bfReserved2;
    u32 bfOffBits;
    
    u32 biSize;
    u32 biWidth;
    u32 biHeight;
    u16 biPlanes;
    u16 biBitCount;
    u32 biCompression;
    u32 biSizeImage;
    u32 biXPelsPerMeter;
    u32 biYPelsPerMeter;
    u32 biClrUsed;
    u32 biClrImportant;
}__attribute__((packed)) bmpdhr;

#define CLUS_INVALID   0xffffff7

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define LAST_LONG_ENTRY 0x40
#define ATTR_LONG_NAME  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

#define N 100000
struct IMG
{
    char name[128];
    void * start;
    uintptr_t size;
    uintptr_t clust_num;
}img[N];
pthread_mutex_t blk;
pthread_cond_t cv;
int imgtot = 0;

struct fat32hdr *hdr;
void *DCIM;
uintptr_t totsize;

void *mmap_disk(const char *fname) {
    int fd = open(fname, O_RDWR);

    if (fd < 0) {
        goto release;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        goto release;
    }

    struct fat32hdr *hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    totsize = size;
    if (hdr == MAP_FAILED) {
        goto release;
    }

    close(fd);
    assert(hdr->Signature_word == 0xaa55); // this is an MBR
    assert(hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec == size);

    // printf("%s: DOS/MBR boot sector, \n", fname);
    // printf("OEM-ID \"%s\", \n", hdr->BS_OEMName);
    // printf("sectors/cluster %d, \n", hdr->BPB_SecPerClus);
    // printf("sectors %d, \n", hdr->BPB_TotSec32);
    // printf("sectors/FAT %d, \n", hdr->BPB_FATSz32);
    // printf("serial number 0x%x\n", hdr->BS_VolID);
    // fflush(stdout);
    return hdr;

release:
    perror("map disk");
    if (fd > 0) {
        close(fd);
    }
    exit(1);
}
uint32_t deocde_clust(u16 DIR_FstClusHI,u16 DIR_FstClusLO);
uintptr_t firstsecofclust(uintptr_t clustid);
uintptr_t FirstDataSector;
uintptr_t RootDirSectors;

uint32_t deocde_clust(u16 DIR_FstClusHI,u16 DIR_FstClusLO)
{
    return ((u32)DIR_FstClusHI << 16) + DIR_FstClusLO;
}    
uintptr_t firstsecofclust(uintptr_t clustid)
{
    uintptr_t FirstSectorofCluster = ((clustid - 2) * hdr->BPB_SecPerClus) + FirstDataSector;
    return FirstSectorofCluster;
}
void * getsecaddr(uintptr_t secid)
{
    return (void *)((uintptr_t)hdr + secid * section);
}
// void *get_root_sec(void * start)
// {
//     return (void *)((uintptr_t)start + firstsecofclust(hdr->BPB_RootClus));
// }
void* T_produce()
{
    char tmp_name[93];
    int len = 0, last = 0;
    struct fat32dent *tmpshort = (struct fat32dent *)DCIM;
    struct fat32long *tmplong = (struct fat32long *)DCIM;
    for(;(uintptr_t)tmpshort < (uintptr_t)hdr + (uintptr_t)totsize; tmpshort++, tmplong = (struct fat32long *)(void *)tmpshort)
    { 
        if(((tmpshort->DIR_Attr & ATTR_ARCHIVE) == ATTR_ARCHIVE) && ((tmplong->LDIR_Attr & ATTR_LONG_NAME) != ATTR_LONG_NAME) &&
                tmpshort->DIR_Name[8] == 'B' && tmpshort->DIR_Name[9] == 'M' && tmpshort->DIR_Name[10] == 'P') //short
        {
            memset(tmp_name,0,sizeof(tmp_name));
            bool skip = 0;len = 0;
            if(tmpshort->DIR_Name[6] == '~') // && number
            {
                last = 0;
                tmplong = (struct fat32long *)(void*)(tmpshort - 1);
                while((uintptr_t)tmplong >= (uintptr_t)DCIM)
                {
                    if((tmplong->LDIR_Attr & ATTR_LONG_NAME) != ATTR_LONG_NAME)
                    {
                        skip = 1;
                        break;
                    }

                    if((tmplong->LDIR_Ord & LAST_LONG_ENTRY) == LAST_LONG_ENTRY)
                    {
                        if((tmplong->LDIR_Ord ^ LAST_LONG_ENTRY) != last + 1)
                        {
                            skip = 1;
                            break;
                        }
                        bool isend = 0;
                        for(int i = 0; i < 5; ++i)
                        {
                            if(tmplong->LDIR_Name1[i] == 0x00)
                            {
                                isend = 1;
                                break;
                            }
                            else if(tmplong->LDIR_Name1[i] > 0xFF || len > 63)
                            {
                                skip = 1;
                                break;
                            }
                            tmp_name[len++] = tmplong->LDIR_Name1[i];
                        }
                        if(skip || isend) break;
                        for(int i = 0; i < 6 ; ++i)
                        {
                            if(tmplong->LDIR_Name2[i] == 0xFFFF)
                            {
                                isend = 1;
                                break;
                            }
                            else if(tmplong->LDIR_Name2[i] > 0xFF || len > 63)
                            {
                                skip = 1;
                                break;
                            }
                            tmp_name[len++] = tmplong->LDIR_Name2[i];
                        }
                        if(skip || isend) break;
                        for(int i = 0; i < 2; ++i)
                        {
                            if(tmplong->LDIR_Name3[i] == 0xFFFF)
                            {
                                isend = 1;
                                break;
                            }
                            else if(tmplong->LDIR_Name3[i] > 0xFF || len > 63)
                            {
                                skip = 1;
                                break;
                            }
                            tmp_name[len++] = tmplong->LDIR_Name3[i];
                        }
                        isend = 1;
                        break;
                    }
                    else
                    {
                        if(tmplong->LDIR_Ord != last + 1)
                        {
                            skip = 1;
                            break;
                        }
                        for(int i = 0; i < 5; ++i)
                        {
                            if(tmplong->LDIR_Name1[i] == 0x00 || tmplong->LDIR_Name1[i] > 0xFF || len > 63)
                            {
                                skip = 1;
                                break;
                            }
                            tmp_name[len++] = tmplong->LDIR_Name1[i];
                        }
                        if(skip) break;
                        for(int i = 0; i < 6 ; ++i)
                        {
                            if(tmplong->LDIR_Name2[i] == 0x00 || tmplong->LDIR_Name2[i] > 0xFF || len > 63)
                            {
                                skip = 1;
                                break;
                            }
                            tmp_name[len++] = tmplong->LDIR_Name2[i];
                        }
                        if(skip) break;
                        for(int i = 0; i < 2; ++i)
                        {
                            if(tmplong->LDIR_Name3[i] == 0x00 || tmplong->LDIR_Name3[i] > 0xFF || len > 63)
                            { 
                                skip = 1;
                                break;
                            }
                            tmp_name[len++] = tmplong->LDIR_Name3[i];
                        }
                        if(skip) break;
                        tmplong--; last++;
                    }
                }
            }
            else //no long
            {
                for(int i = 0; i < 8; ++i)
                {
                    if(tmpshort->DIR_Name[i] == 0x20 || tmpshort->DIR_Name[i] == 0x00 || tmpshort->DIR_Name[i] > 0xFF)
                    {
                        break;
                    }
                    else if(tmpshort->DIR_Name[i] > 0xFF)
                    {
                        skip = 1;
                        break;
                    }
                    tmp_name[i] = tmpshort->DIR_Name[i];
                }
                strcat(tmp_name, ".bmp");
                len = strlen(tmp_name);
            }

            if(!skip)
            {
                // pthread_mutex_lock(&blk);
                // while(!(imgtot < N - 1))
                // {
                //     pthread_cond_wait(&cv, &blk);
                // }
                imgtot++;
                memcpy(img[imgtot].name, tmp_name, len);
                img[imgtot].name[len] = '\0';
                //printf("%s\n",img[imgtot].name);
                //fflush(stdout);
                img[imgtot].size = tmpshort->DIR_FileSize;
                img[imgtot].start = getsecaddr(firstsecofclust(deocde_clust(tmpshort->DIR_FstClusHI, tmpshort->DIR_FstClusLO)));
                img[imgtot].clust_num = deocde_clust(tmpshort->DIR_FstClusHI, tmpshort->DIR_FstClusLO);
                // pthread_cond_broadcast(&cv);
                // pthread_mutex_unlock(&blk);
            }
        }
        else
        {
            continue;
        }
    }
//?
    return NULL;
}

uintptr_t clust_of_addr(void * addr)
{
    uintptr_t cluster_num = ((((uintptr_t)addr - (uintptr_t)hdr) / section) - FirstDataSector) / hdr->BPB_SecPerClus + 2;
    return cluster_num;
}
uintptr_t addr_of_clust(uintptr_t clust_num)
{
    return (uintptr_t)getsecaddr(firstsecofclust(clust_num));
}
int tail = 0;
uintptr_t compare_clust(uintptr_t c1, uintptr_t c2)
{
    uintptr_t res = 0;
    u8 * p1 = (void *)addr_of_clust(c1);
    u8 * p2 = (void *)addr_of_clust(c2);
    for(int i = 0; i < clust; ++i)
    {
        res += abs(*p1 - *p2);
    }
    return res;
}
#define INF 0x7fffffff
uintptr_t find_cluster(uintptr_t tmpclust)
{
    uintptr_t res = tmpclust + 1;uintptr_t sum = INF;
    for(int clust_num = tmpclust + 1; clust_num - tmpclust < 10 && addr_of_clust(clust_num) < (uintptr_t)hdr + (uintptr_t)totsize; clust_num ++)
    {
        uintptr_t tmpsum = compare_clust(tmpclust, clust_num);
        if(tmpsum < sum)
        {
            res = clust_num;
            sum = tmpclust;
        }
    }
    return res;
}

void* T_consume()
{
    while(1)
    {
        pthread_mutex_lock(&blk);
        while(!(imgtot > 0))
        {
            pthread_cond_wait(&cv, &blk);
        }
        struct IMG tmpimg;
        memcpy(&tmpimg, &img[imgtot], sizeof(struct IMG));
        imgtot--;
        pthread_cond_broadcast(&cv);
        pthread_mutex_unlock(&blk);

    //     bmpdhr *tmp = (void *)getsecaddr(firstsecofclust(tmpimg.clust_num));
    //     assert(clust_of_addr(tmp) == tmpimg.clust_num);
    //     uintptr_t size = tmpimg.size;
    //     if(tmp->bfType != 0x4d42)
    //     {
    //         continue;
    //     }

    //     char tmp_addr[150];
    //     snprintf(tmp_addr, sizeof(tmp_addr), "/tmp/%s",tmpimg.name);
    //     int fd = open(tmp_addr, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    //     if(fd < 0)
    //     {
    //         continue;
    //     }
        
    //     uintptr_t RowSize = ((tmp->biWidth * 3 + 31) / 32) * 4;
    //     uintptr_t skipbytes = 4 - ((tmp->biWidth * 3) / 8) % 4;
    //     uintptr_t need_clust = (size % clust) == 0 ? size / clust: size / clust + 1;
    //     write(fd, tmp, clust);
    //     uintptr_t remain_bytes = size - clust;
    //     uintptr_t last_clust = tmpimg.clust_num;
    //     uintptr_t need_bytes = RowSize - ((clust - tmp->bfOffBits) % RowSize);
    //     int success = 1; 
    //     printf("needclust:%p\n",(void *)need_clust);
    //     for(int i = 2; i <= need_clust; ++i)
    //     {
    //         uintptr_t next_clust = find_cluster(last_clust, RowSize, need_bytes, skipbytes);
    //         printf("%p %p\n", (void *)next_clust, (void *)last_clust);
    //         if(i == need_clust)
    //         {
    //             success = write(fd, (void *)addr_of_clust(next_clust), remain_bytes);
    //             if(success == -1) break;
    //         }
    //         else
    //         {
    //             success = write(fd, (void *)addr_of_clust(next_clust), clust);
    //             if(success == -1) break;
    //             remain_bytes -= clust;
    //             last_clust = next_clust;
    //             need_bytes = RowSize - ((clust - need_bytes) % RowSize);
    //             assert(0 <= need_bytes && need_bytes < RowSize);
    //         }
    //     }
    //     if(success == -1) continue;
    //     char command[256];
    //     snprintf(command, sizeof(command), "sha1sum %s", tmp_addr);

    //     FILE *pipe = popen(command, "r");
    //     if (!pipe) 
    //     {
    //         continue;
    //     }

    //     char output[256];
    //     if (fgets(output, sizeof(output), pipe) != NULL) 
    //     {
    //         char sha1sum[41];
    //         sscanf(output, "%40s", sha1sum);
    //         printf("%s %s\n", sha1sum, tmpimg.name);
    //         fflush(stdout);
    //     } 
    //     else 
    //     {
    //         continue;
    //     }
     }
    
    return NULL;
}

bool check_line(uintptr_t skip_bytes, uintptr_t RowSize, uint8_t line[])
{
    bool check = 1;
    for(int i = 1; i <= skip_bytes; ++i)
    {
        if(line[RowSize - i] != 0x00)
        {
            check = 0;
            break;
        }
    }
    return check;
}
void test()
{
    struct IMG tmpimg;
    for(int i = 1; i <= imgtot; ++i)
    {
        memcpy(&tmpimg, &img[i], sizeof(struct IMG));

        
        bmpdhr *tmp = (void *)getsecaddr(firstsecofclust(tmpimg.clust_num));
        //assert(clust_of_addr(tmp) == tmpimg.clust_num);
        uintptr_t size = tmpimg.size;
        if(tmp->bfType != 0x4d42)
        {
            continue;
        }

        char tmp_addr[150];
        snprintf(tmp_addr, sizeof(tmp_addr), "/tmp/%s",tmpimg.name);
        int fd = open(tmp_addr, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(fd < 0)
        {
            continue;
        }

        int success = 1;
        uintptr_t need_clust = (size % clust) == 0 ? size / clust: size / clust + 1;
        // write(fd, tmp, clust);
        if(need_clust == 1)
        {
            if((uintptr_t)tmp + (uintptr_t)size <= (uintptr_t)((uintptr_t)hdr + (uintptr_t)totsize))
            {
                write(fd, tmp, size);
            }
            else
            {
                success = -1;
            }
        }
        else
        {
            uintptr_t last_clust = tmpimg.clust_num;
            uintptr_t RowSize = (((tmp->biWidth * 24) + 31) >> 5) << 2;
            uint8_t line[RowSize * 2];
            uintptr_t skip_bytes = (4 - (((tmp->biWidth * 24) >> 3) & 3)) % 4;
            uintptr_t need_bytes = RowSize - (clust - tmp->bfOffBits) % RowSize;
            write(fd, tmp, clust - (RowSize - need_bytes));
            uintptr_t remain_bytes = size - (clust - (RowSize - need_bytes));
            
            for(int i = 2; i <= need_clust; ++i)
            {
                uintptr_t next_clust = last_clust + 1;//find_cluster(last_clust, RowSize, need_bytes, skipbytes);
                if((uintptr_t)(addr_of_clust(next_clust)) >= (uintptr_t)((uintptr_t)hdr + (uintptr_t)totsize))
                {
                    success = -1;
                    break;
                }
                if(i == need_clust)
                {
                    memcpy((void *)line, (void *)(addr_of_clust(next_clust) - (RowSize - need_bytes)), RowSize - need_bytes);
                    memcpy((void *)(line + (RowSize - need_bytes)), (void *)addr_of_clust(next_clust), need_bytes);
                    line[RowSize] = '\0';
                    write(fd, (void *)(line), RowSize);
                    //write(fd, (void *)addr_of_clust(next_clust) + need_bytes, remain_bytes - RowSize);
                    write(fd, (void *)addr_of_clust(next_clust) + need_bytes, size - (need_clust - 1) * clust - need_bytes);
                }
                else
                {
                    memcpy((void *)line, (void *)(addr_of_clust(next_clust) - (RowSize - need_bytes)), RowSize - need_bytes);
                    memcpy((void *)(line + (RowSize - need_bytes)), (void *)addr_of_clust(next_clust), need_bytes);
                    line[RowSize] = '\0';

                    if(!check_line(skip_bytes, RowSize, line))
                    {
                        for(int clust_num = next_clust;addr_of_clust(clust_num) < (uintptr_t)hdr + (uintptr_t)totsize; clust_num ++)
                        {
                            memcpy((void *)(line + (RowSize - need_bytes)), (void *)addr_of_clust(clust_num), need_bytes);
                            line[RowSize] = '\0';
                            if(check_line(skip_bytes, RowSize, line))
                            {
                                next_clust = clust_num;
                                break;
                            }
                        }
                        // for(int clust_num = last_clust - 1; addr_of_clust(clust_num) > (uintptr_t)hdr; clust_num --)
                        // {
                        //     memcpy((void *)(line + (RowSize - need_bytes)), (void *)addr_of_clust(clust_num), need_bytes);
                        //     line[RowSize] = '\0';
                        //     if(check_line(skip_bytes, RowSize))
                        //     {
                        //         next_clust = clust_num;
                        //         break;
                        //     }
                        // }
                        // success = -1;
                        // break;
                    }
                    if(addr_of_clust(next_clust) >= (uintptr_t)hdr + (uintptr_t)totsize)
                    {
                        next_clust = last_clust + 1;
                    }
                    
                    write(fd, (void *)(line), RowSize);
                    remain_bytes -= RowSize;
                    uintptr_t last_need_bytes = need_bytes;
                    need_bytes = RowSize - ((clust - need_bytes) % RowSize);
                    write(fd, (void *)(addr_of_clust(next_clust) + last_need_bytes), clust - (RowSize - need_bytes) - last_need_bytes);
                    remain_bytes -= clust - (RowSize - need_bytes) - last_need_bytes;

                    last_clust = next_clust;
                }
            }
        }
        
        if(success == -1) continue;
        char command[256];
        snprintf(command, sizeof(command), "sha1sum %s", tmp_addr);

        FILE *pipe = popen(command, "r");
        if (!pipe) 
        {
            continue;
        }

        char output[256];
        if (fgets(output, sizeof(output), pipe) != NULL) 
        {
            char sha1sum[41];
            sscanf(output, "%40s", sha1sum);
            printf("%40s %s\n", sha1sum, tmpimg.name);
            fflush(stdout);
        } 
        else 
        {
            continue;
        }
    }
}
int main(int argc, char *argv[]) {
    hdr = mmap_disk(argv[1]);

    RootDirSectors = ((hdr->BPB_RootEntCnt * 32) + (hdr->BPB_BytsPerSec - 1)) / hdr->BPB_BytsPerSec;
    assert(RootDirSectors == 0);
    FirstDataSector = hdr->BPB_RsvdSecCnt + (hdr->BPB_NumFATs * hdr->BPB_FATSz32) + RootDirSectors;
    
    DCIM = (void*)(uintptr_t)((hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32 
                        + (hdr->BPB_RootClus - 2) * hdr->BPB_SecPerClus) * hdr->BPB_BytsPerSec);
    DCIM = (void*)((uintptr_t)DCIM + (uintptr_t)hdr);
    pthread_mutex_init(&blk, NULL);
    pthread_cond_init(&cv, NULL);
    T_produce();
    test();
    // pthread_t threadP;
    // pthread_create(
    //     &threadP,
    //     NULL,
    //     T_produce,
    //     "produce"
    //     );
    // pthread_t threadV[4];
    // for(int i = 0; i < 4; ++i)
    // {
    //     pthread_create(
    //     &threadV[i],
    //     NULL,
    //     T_consume,
    //     "consume"
    //     );
    // }
    // void *thread_result = NULL;
    // pthread_join(threadP ,thread_result);
    // for(int i = 0; i < 4; ++i)
    // {
    //     pthread_join(threadV[i] ,thread_result);
    // }
    return 0;
}
