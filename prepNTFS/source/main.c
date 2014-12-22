#include <stdio.h>
#include <string.h>
#include "ntfs.h"
#include "cobra.h"
#include "scsi.h"
#include <lv2/sysfs.h>
#include <sys/file.h>
#include <dirent.h>
#include <unistd.h>

#include <io/pad.h>

#define SUFIX(a)	((a==1)? "_1" :(a==2)? "_2" :(a==3)? "_3" :(a==4)? "_4" :"")
#define SUFIX2(a)	((a==1)?" (1)":(a==2)?" (2)":(a==3)?" (3)":(a==4)?" (4)":"")

#define USB_MASS_STORAGE_1(n)	(0x10300000000000AULL+(n)) /* For 0-5 */
#define USB_MASS_STORAGE_2(n)	(0x10300000000001FULL+((n)-6)) /* For 6-127 */
#define USB_MASS_STORAGE(n)	(((n) < 6) ? USB_MASS_STORAGE_1(n) : USB_MASS_STORAGE_2(n))

#define MAX_SECTIONS	((0x10000-sizeof(rawseciso_args))/8)

typedef struct
{
	uint64_t device;
	uint32_t emu_mode;
	uint32_t num_sections;
	uint32_t num_tracks;
} __attribute__((packed)) rawseciso_args;


uint8_t plugin_args[0x10000];
uint8_t cue_buf[0x10000];
char path[0x420];
//char extension[10];

uint32_t sections[MAX_SECTIONS], sections_size[MAX_SECTIONS];
ntfs_md *mounts;
int mountCount;

int main(int argc, const char* argv[])
{
	int i, parts;
	unsigned int num_tracks;
	u8 cue=0;
	int emu_mode;
	TrackDef tracks[100];
	ScsiTrackDescriptor *scsi_tracks;

	rawseciso_args *p_args;

	sysFSDirent dir;
    DIR_ITER *pdir = NULL;
    struct stat st;
	char c_path[4][8]={"PS3ISO", "BDISO", "DVDISO", "PSXISO"};
	char extensions[4][8]={".iso", ".ISO", ".bin", ".BIN"};

	snprintf(path, sizeof(path), "/dev_hdd0/tmp/wmtmp");
	mkdir(path, S_IRWXO | S_IRWXU | S_IRWXG | S_IFDIR);
	sysFsChmod(path, S_IFDIR | 0777);

	sysLv2FsUnlink((char*)"/dev_hdd0/tmp/wmtmp/games.html");

	int fd=-1;
	u64 read = 0;
	char path0[512];
	char direntry[1024];
	char filename[512];
	bool is_iso = false;

//--- hold CROSS to keep previous cached files
//--- hold CROSS

    unsigned button = 0;

    padInfo padinfo;
    padData paddata;

    ioPadInit(7);

    int n, r;
    for(r=0; r<10; r++)
    {
        ioPadGetInfo(&padinfo);
        for(n = 0; n < 7; n++)
        {
            if(padinfo.status[n])
            {
                ioPadGetData(n, &paddata);
                button = (paddata.button[2] << 8) | (paddata.button[3] & 0xff);
                break;
            }
        }
        if(button) break; else usleep(20000);
    }
    ioPadEnd();

    if(button) ;
    else
//---

    {
		sysLv2FsOpenDir(path, &fd);
		if(fd>=0)
		{
			while(!sysLv2FsReadDir(fd, &dir, &read) && read)
				if(strstr(dir.d_name, ".ntfs[")) {sprintf(path0, "%s/%s", path, dir.d_name); sysLv2FsUnlink(path0);}
			sysLv2FsCloseDir(fd);
		}
    }

	mountCount = ntfsMountAll(&mounts, NTFS_DEFAULT | NTFS_RECOVER | NTFS_READ_ONLY);
	if (mountCount <= 0) return 0;

	for (int profile = 0; profile < 5; profile++)
	{
		for (i = 0; i < mountCount; i++)
		{
			if (strncmp(mounts[i].name, "ntfs", 4) == 0)
			{
				for(u8 m=0;m<4;m++) //0="PS3ISO", 1="BDISO", 2="DVDISO", 3="PSXISO"
				{
					bool has_dirs = false; u8 d0 = 0;

		read_folder_ntfs:

					if(d0==0)
						snprintf(path, sizeof(path), "%s:/%s%s", mounts[i].name, c_path[m], SUFIX(profile));
					else
						snprintf(path, sizeof(path), "%s:/%s%s/%c", mounts[i].name, c_path[m], SUFIX(profile), d0);

					pdir = ps3ntfs_diropen(path);
					if(pdir!=NULL)
					{
						while(ps3ntfs_dirnext(pdir, dir.d_name, &st) == 0)
						{
							is_iso = ((strcasestr(dir.d_name, ".iso")) && (dir.d_name[strlen(dir.d_name)-1]=='O' || dir.d_name[strlen(dir.d_name)-1]=='o')) ||
							 (m==3 && (strcasestr(dir.d_name, ".bin")) && (dir.d_name[strlen(dir.d_name)-1]=='N' || dir.d_name[strlen(dir.d_name)-1]=='n'));

							if(is_iso)
                            {
								sprintf(filename, "%s", dir.d_name);
								//sprintf(extension, "%s", filename + strlen(filename) - 4);

								filename[strlen(filename)-4]=0;
								//extension[4]=0;

								sprintf(direntry, "%s", dir.d_name);
                            }
							else if(!strstr(dir.d_name, "."))
							{
								for(u8 e=0;e<4;e++)
								{
									if(e>1 && m<3) break;
									sprintf(filename, "%s%s", dir.d_name, extensions[e]);
									sprintf(direntry, "%s/%s", dir.d_name, filename);

									sprintf(filename, "%s", dir.d_name);
									//sprintf(extension, "%s", extensions[e]);

									snprintf(path, sizeof(path), "%s:/%s%s/%s", mounts[i].name, c_path[m], SUFIX(profile), direntry);
									parts = ps3ntfs_file_to_sectors(path, sections, sections_size, MAX_SECTIONS, 1);
									if(parts>0 && parts<MAX_SECTIONS) {is_iso = true; break;}
								}
								if(!has_dirs) has_dirs = (strlen(dir.d_name)==1);
							}

							if( is_iso )
							{
								if(d0==0)
									snprintf(path, sizeof(path), "%s:/%s%s/%s", mounts[i].name, c_path[m], SUFIX(profile), direntry);
								else
									snprintf(path, sizeof(path), "%s:/%s%s/%c/%s", mounts[i].name, c_path[m], SUFIX(profile), d0, direntry);

								parts = ps3ntfs_file_to_sectors(path, sections, sections_size, MAX_SECTIONS, 1);

								if (parts == MAX_SECTIONS)
								{
									continue;
								}
								else if (parts > 0)
								{
									num_tracks = 1;
									if(m==0) emu_mode = EMU_PS3; else
									if(m==1) emu_mode = EMU_BD;  else
									if(m==2) emu_mode = EMU_DVD; else
									if(m==3)
									{
										emu_mode = EMU_PSX;
										cue=0;
										int fd;
										path[strlen(path)-3]='C'; path[strlen(path)-2]='U'; path[strlen(path)-1]='E';
										fd = ps3ntfs_open(path, O_RDONLY, 0);
										if(fd<0)
										{
											path[strlen(path)-3]='c'; path[strlen(path)-2]='u'; path[strlen(path)-1]='e';
											fd = ps3ntfs_open(path, O_RDONLY, 0);
										}

										if (fd >= 0)
										{
											int r = ps3ntfs_read(fd, (char *)cue_buf, sizeof(cue_buf));
											ps3ntfs_close(fd);

											if (r > 0)
											{
												char dummy[64];

												if (cobra_parse_cue(cue_buf, r, tracks, 100, &num_tracks, dummy, sizeof(dummy)-1) != 0)
												{
													num_tracks=1;
													cue=0;
												}
												else
													cue=1;
											}
										}
									}

									p_args = (rawseciso_args *)plugin_args; memset(p_args, 0, 0x10000);
									p_args->device = USB_MASS_STORAGE((mounts[i].interface->ioType & 0xff) - '0');
									p_args->emu_mode = emu_mode;
									p_args->num_sections = parts;

									memcpy(plugin_args+sizeof(rawseciso_args), sections, parts*sizeof(uint32_t));
									memcpy(plugin_args+sizeof(rawseciso_args)+(parts*sizeof(uint32_t)), sections_size, parts*sizeof(uint32_t));

									if (emu_mode == EMU_PSX)
									{
										int max = MAX_SECTIONS - ((num_tracks*sizeof(ScsiTrackDescriptor)) / 8);

										if (parts == max)
										{
											continue;
										}

										p_args->num_tracks = num_tracks;
										scsi_tracks = (ScsiTrackDescriptor *)(plugin_args+sizeof(rawseciso_args)+(2*parts*sizeof(uint32_t)));

										if (!cue)
										{
											scsi_tracks[0].adr_control = 0x14;
											scsi_tracks[0].track_number = 1;
											scsi_tracks[0].track_start_addr = 0;
										}
										else
										{
											for (u8 j = 0; j < num_tracks; j++)
											{
												scsi_tracks[j].adr_control = (tracks[j].is_audio) ? 0x10 : 0x14;
												scsi_tracks[j].track_number = j+1;
												scsi_tracks[j].track_start_addr = tracks[j].lba;
											}
										}
									}

									FILE *flistW;
									snprintf(path, sizeof(path), "/dev_hdd0/tmp/wmtmp/%s%s.ntfs[%s]", filename, SUFIX2(profile), c_path[m]);
									flistW = fopen(path, "wb");
									if(flistW!=NULL)
									{
										fwrite(plugin_args, sizeof(plugin_args), 1, flistW);
										fclose(flistW);
										sysFsChmod(path, 0666);
									}
								}
							}
						}
						ps3ntfs_dirclose(pdir);
					}
					if(has_dirs && (d0<'Z'))
					{
						if(d0==0) d0='0'; else if(d0=='9') d0='A'; else d0++;
						goto read_folder_ntfs;
					}
				}
			}
		}
    }
	for (u8 u = 0; u < mountCount; u++) ntfsUnmount(mounts[u].name, 1);
	return 0;
}
