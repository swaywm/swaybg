/*

   Read background directory and select one image file randomly.

   by Lihui Zhang <swordhuihui@gmail.com>
   2020.09.13

*/

#ifndef __HEADER_IMG_SELECTOR_
#define __HEADER_IMG_SELECTOR_


//Select one file randomly from path "name", if "name" is a directory
// or return "name" directly, if "name" is a file.
// symbol link is supported.
// DO NOT FORGET free *selected if it not NULL!!!!

int read_file_or_dir(const char *name, char **selected);


#endif

