#include <stdio.h>
#include <string.h>
//#include <curses.h>
/*

[0x8000 + 140`143] LpXe[u

156-189:[gfBNgR[h

0  :LENGTH
1  :zoku-length
2-9:extent pos
33- file name
*/


int fix_filename(unsigned char *buf,int size)
{
	int i;
	int fixed = 0;
	char *ptr;
	char fname_buf[256];

	for(i=0;i<size-36;)
	{
		int len , flen;

		len    = buf[i];
		if(len==0) break;

		flen   = buf[i+32];
		memset(fname_buf,0,256);
		memcpy(fname_buf,&buf[i+33],flen);
		printf("file name '%s'\n",fname_buf);

		ptr = strchr(fname_buf,';');
		if(ptr)
		{
			*ptr = 0x00;
			flen = strlen(fname_buf);
			memcpy(&buf[i+33],fname_buf,flen+1);
			buf[i+32] = flen;
			fixed++;
			printf("FIX name\n");
		}
		i += len;
	}
	return fixed;
}

int search_filename(unsigned char *buf,int size,char *name)
{
	int i;
	int fixed = 0;
	int file_extnt;

	for(i=0;i<size-36;)
	{
		int len , flen;

		len    = buf[i];
		if(len==0) break;

		file_extnt = buf[i+2]+(buf[i+3]<<8)+(buf[i+4]<<16)+(buf[i+5]<<24);
		flen       = buf[i+32];

		if(memcmp(name, &buf[i+33],strlen(name))==0)
		{
			printf("file '%s' found %08x\n",name,file_extnt);
			return file_extnt;
		}
		i += len;
	}
	return 0;
}

unsigned char pvd_buf[0x800];
unsigned char root_buf[0x10000];
unsigned char game_buf[0x10000];
char sig_buf[32+1];


int main(int argc,char **argv)
{
	FILE *fp;
	int i;
	int fixed;
	int key;
	int root_lba,game_lba,umdbin_lba;

	if(argc<2)
	{
		printf("umdsign [iso_filename]\n");
		return 0;
	}

	fp =fopen(argv[1],"r+b");
	if(fp==0)
	{
		printf("Can't open %s\n",argv[1]);
		return 0;
	}
	// get PVD
	fseek(fp,0x8000,SEEK_SET);
	fread(pvd_buf,1,0x800,fp);

	// root entent
	i = 156 + 2;
	root_lba = pvd_buf[i+0]+(pvd_buf[i+1]<<8)+(pvd_buf[i+2]<<16)+(pvd_buf[i+3]<<24);
	printf("ROOT DIRECTRY LBA %08X\n",root_lba);

	// read root directry
	fseek(fp,root_lba * 0x800,SEEK_SET);
	fread(root_buf,1,0x1000,fp);

	// seatch & fix UMD_DATA.BIN
	fixed      = fix_filename(root_buf,0x1000);
	umdbin_lba = search_filename(root_buf,0x1000,"UMD_DATA.BIN");
	game_lba   = search_filename(root_buf,0x1000,"PSP_GAME");

	if(game_lba==0)
		printf("PSP_GAME not found\n");
	else
	{
		// read
		fseek(fp,game_lba * 0x800,SEEK_SET);
		fread(game_buf,1,0x1000,fp);
		fixed += fix_filename(game_buf,0x1000);
	}

	if(umdbin_lba==0)
		printf("UMD_DATA.BIN not found\n");
	else
	{
		// get signature data
		fseek(fp,umdbin_lba * 0x800,SEEK_SET);
		fread(sig_buf,1,32,fp);
		sig_buf[33] = 0;
		printf("signature data = '%s'\n",sig_buf);
	}

	if(game_lba && umdbin_lba)
	{
		if(
			pvd_buf[0x370]==0x00 && 
			pvd_buf[0x371]==0x01 && 
			pvd_buf[0x372]==0x00 && 
			(memcmp(&pvd_buf[0x373],sig_buf,32)==0) )
			{
				printf("Already Signed\n");
			}
			else
			{
				printf("Need Sign\n");
				pvd_buf[0x370] = 0x00;
				pvd_buf[0x371] = 0x01; // media mark ?
				pvd_buf[0x372] = 0x00;
				memcpy(&pvd_buf[0x373],sig_buf,32);
				fixed++;
			}

			if(fixed==0)
			{
				printf("Already Okay\n");
			}
			else
			{
				//printf("FIX & SIGNATURE UMD ISO, are you sure[y/n]?");
				//key = getch()|0x20;
				//printf("%c\n",key);
				//if(key=='y')
				{
					printf("write root directry\n");
					fseek(fp,root_lba * 0x800,SEEK_SET);
					fwrite(root_buf,1,0x1000,fp);

					printf("write PSP_GAME directry\n");
					fseek(fp,game_lba * 0x800,SEEK_SET);
					fwrite(game_buf,1,0x1000,fp);

					printf("write PVD\n");
					fseek(fp,0x8000,SEEK_SET);
					fwrite(pvd_buf,1,0x800,fp);

					printf("Finish!");
			}
		}
	}
	fclose(fp);
}
