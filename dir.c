/*

   Read background directory and select one image file randomly.

   by Lihui Zhang <swordhuihui@gmail.com>
   2020.09.13

*/



#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


static int read_img_link_raw(const char * path, char** linkname)
{
	struct stat sb;
	char* ln;
	int r;

	//-1, error. 0: points to normal file. 1: -> directory. 2: -> symbol link, 3: others 
	int iRet=-1;
	


	if (lstat(path, &sb) == -1) {
		perror("lstat");
		return (-1);
	}

	switch (sb.st_mode & S_IFMT) {
		case S_IFDIR:  
			iRet=1;
			break;

		case S_IFLNK:  
			iRet=2;
			printf("symlink, size=%d\n", sb.st_size);                 
			ln=malloc(sb.st_size+1);
			if (ln == NULL) 
			{
				fprintf(stderr, "insufficient memory\n");
				return -1;
			}

			r = readlink(path, ln, sb.st_size + 1);
			if (r < 0) 
			{
				perror("lstat");
				return -1;
			}

			if (r > sb.st_size) 
			{
				fprintf(stderr, "symlink increased in size "
						"between lstat() and readlink()\n");
				return -1;
			}

			ln[sb.st_size] = '\0';
			*linkname=ln;
			break;

		case S_IFREG:  
			iRet=0;
			printf("regular file\n");            
			break;

		default:       
			iRet=3;
			printf("unknown?\n");                
			break;
	}

	return iRet;
}


static int read_img_link(const char * f, char** linkname)
{
	int iRet=-1;
	int depth=64;
	char *ln=NULL;

	
	char *path=malloc(strlen(f)+1);
	strcpy(path, f);

	while(depth>0)
	{
		iRet=read_img_link_raw(path, &ln);

		if(iRet==2)
		{
			//points to another symbol link
			depth--;
			free(path);
			path=ln;
			continue;
		}
		else
		{
			//not symbol link
			*linkname=path;
			break;
		}
	}

	return iRet;
				
}


int read_img_dir(const char *path)
{
	DIR *pDir = NULL;
	struct dirent * pEnt = NULL;
	unsigned int cnt = 0;	
	const char* type;
	int iret=0;
	char* linkname=NULL;

	pDir = opendir(path);
	if (NULL == pDir)
	{
		perror("opendir");
		return -1;

	}	
	while (1)
	{
		pEnt = readdir(pDir);
		if(pEnt != NULL)
		{
			if (pEnt->d_type == DT_REG )
			{
				type="是普通文件:";
			}
			else if (pEnt->d_type == DT_DIR)
			{
				type="是Directory:";
			}
			else if (pEnt->d_type == DT_LNK)
			{
				type="是Symbol link:";
				iret=read_img_link(pEnt->d_name, &linkname);

				if(iret==0)
				{
					printf("--> Normal file, %s\n", linkname);
					free(linkname);
				}
				else if(iret==1)
				{
					printf("--> Dir , %s\n", linkname);
					free(linkname);
				}
				else
				{
					printf("--> Unknown, iret=%d\n", iret);
				}
			}
			else
			{
				type="不是普通文件:";
			}
			printf("name：[%-30s]%s\n", pEnt->d_name, type);
			cnt++;
		}
		else
		{
			break;
		}
	};
	printf("总文件数为：%d\n", cnt);

}


int read_file_or_dir(const char *name, char **selected)
 
{
	struct stat sb;
	int iret;
	char* linkname=NULL;

	//get file info
	if (lstat(name, &sb) == -1) {
		perror("lstat");
		return (-1);
	}

	switch (sb.st_mode & S_IFMT) {
		case S_IFDIR:  
			printf("directory\n");               
			read_img_dir(name);
			break;

		case S_IFREG:  
			printf("regular file, read directly\n");            
			break;

		case S_IFLNK:  
			printf("symlink, size=%d\n", sb.st_size);                 
			iret=read_img_link(name, &linkname);

			if(iret==0)
			{
				printf("--> Normal file, %s\n", linkname);
				free(linkname);
			}
			else if(iret==1)
			{
				printf("--> Dir , %s\n", linkname);
				read_img_dir(linkname);
				free(linkname);
			}
			else
			{
				printf("--> Unknown, iret=%d\n", iret);
			}
			break;

		default:       
			printf("unknown?\n");                
			break;
	}

	return 0;
}
