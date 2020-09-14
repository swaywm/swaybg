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
#include <time.h>
#include <libgen.h>


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
			//printf("=====ln=%s\n", ln);

			//check is it a absolute path?
			if(ln[0]=='/')
			{
				//absolute path
				*linkname=ln;
				//printf("=====%s\n", *linkname);
			}
			else
			{
				char* bname=dirname(path);
				char* newname=malloc(strlen(bname)+strlen(ln)+2);
				//printf("===== dirname=%s\n", bname);
				strcpy(newname, bname);
				strcat(newname, "/");
				strcat(newname, ln);
				free(ln);
				*linkname=newname;
				//printf("=====%s\n", *linkname);
			}
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


static int read_img_dir(const char *path, char **selected)
{
	DIR 			*pDir = NULL;
	struct dirent 	*pEnt = NULL;
	unsigned int 	cnt = 0;	
	const char		*type;
	int 			iret=0;
	char			*linkname=NULL;
	unsigned int 	maxcnt=8192;
	char			**namearray;
	unsigned int 	iSelected=0, i;

	namearray=(char**)malloc(sizeof(char*)*maxcnt);
	if(namearray==NULL)
	{
		maxcnt=4096;
		namearray=(char**)malloc(sizeof(char*)*maxcnt);
	}

	if(namearray==NULL) 
	{
		printf("malloc failed\n");
		return -1;
	}

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
				char* namedup;

				type="Regular file";
				printf("name：[%-30s]%s\n", pEnt->d_name, type);

				namedup=(char*)malloc(strlen(pEnt->d_name)+1);
				strcpy(namedup, pEnt->d_name);

				//record it
				namearray[cnt++]=namedup;
			}
			else if (pEnt->d_type == DT_DIR)
			{
				//skip sub directory.
				type="Directory";
				printf("name：[%-30s]%s, skip\n", pEnt->d_name, type);
			}
			else if (pEnt->d_type == DT_LNK)
			{
				type="Symbol link:";
				printf("name：[%-30s]%s\n", pEnt->d_name, type);
				iret=read_img_link(pEnt->d_name, &linkname);

				if(iret==0)
				{
					printf("--> Normal file, %s\n", linkname);

					//record
					namearray[cnt++]=linkname;
				}
				else if(iret==1)
				{
					printf("--> Dir , %s, skip\n", linkname);
					//skip sub directory.
					free(linkname);
				}
				else
				{
					printf("--> Unknown, iret=%d\n", iret);
				}
			}
			else
			{
				type="NOT regular file:";
				printf("name：[%-30s]%s\n", pEnt->d_name, type);
			}
		}
		else
		{
			break;
		}
	};

	if(cnt==0)
	{
		printf("Error: No file found\n");
		return -1;
	}

	//select one randomly
	srand(time(NULL));
	iSelected=rand()%cnt;
	*selected=malloc(strlen(path)+strlen(namearray[iSelected])+2);
	strcpy(*selected, path);
	strcat(*selected, "/");
	strcat(*selected, namearray[iSelected]);
	printf("Total：%d, selected: %d (%s)\n", cnt, iSelected, *selected);

	//free all
	for(i=0; i<cnt; i++)
	{
		free(namearray[i]);
	}

	return 0;
}


int read_file_or_dir(const char *name, char **selected)
{
	struct stat sb;
	int iret=0;
	char* linkname=NULL;

	//get file info
	if (lstat(name, &sb) == -1) {
		perror("lstat");
		return (-1);
	}

	switch (sb.st_mode & S_IFMT) {
		case S_IFDIR:  
			printf("directory\n");               
			read_img_dir(name, selected);
			break;

		case S_IFREG:  
			printf("regular file, return directy\n");            
			*selected=malloc(strlen(name)+1);
			strcpy(*selected, name);
			break;

		case S_IFLNK:  
			printf("symlink, size=%d\n", sb.st_size);                 
			iret=read_img_link(name, &linkname);

			if(iret==0)
			{
				printf("--> Normal file, %s\n", linkname);
				*selected=linkname;
			}
			else if(iret==1)
			{
				printf("--> Dir , %s\n", linkname);
				read_img_dir(linkname, selected);
				free(linkname);
			}
			else
			{
				printf("--> Unknown, iret=%d\n", iret);
				*selected=NULL;
			}
			break;

		default:       
			printf("unknown?\n");                
			*selected=NULL;
			break;
	}

	return iret;
}
