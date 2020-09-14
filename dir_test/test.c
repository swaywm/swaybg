#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../dir_sel.h"


int main(int argc, char **argv)
 
{
	int iret;
	char* selected=NULL;

	if (argc != 2)
	{
		printf("usage: %s dirname\n", argv[0]);
		return -1;
	}

	iret=read_file_or_dir(argv[1], &selected); 

	if(selected != NULL)
	{
		printf(">>>> Final selected: %s\n", selected);
		free(selected);
	}

	return 0;
}
