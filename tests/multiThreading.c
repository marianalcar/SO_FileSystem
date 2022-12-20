#include <stdio.h>
#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
    char *str_ext_file ="BOAS FESTAS";
    char *path_copied_file = "/f1";
    char *path1 = "/link";
    char buffer[15];
    char *source = "tests/newfile.txt";
    int fhandle;
    ssize_t bytes;

void *fn_read() {
  fhandle = tfs_open(path_copied_file, TFS_O_CREAT);
  bytes = tfs_read(fhandle,buffer,sizeof(buffer));
  tfs_close(fhandle);
  return NULL;
}

void *fn_write() {
  fhandle = tfs_open(path_copied_file, TFS_O_CREAT);
  bytes = tfs_write(fhandle,buffer,strlen(str_ext_file));
  tfs_close(fhandle);
  return NULL;
}

void *fn_hardlink() {
  assert(tfs_link(path_copied_file,path1) != -1);
  return NULL;
}

int main() {
  pthread_t tid1[8];
  pthread_t tid2[8];
  pthread_t tid3[8];

  FILE *file = fopen(source, "r");
  fread(buffer,sizeof(char),sizeof(buffer),file);

  assert(tfs_init(NULL) != -1);

for (int aux = 0; aux < 8 ; aux++) {
  assert(pthread_create(&tid1[aux], NULL,fn_write, NULL) != -1);
  assert(pthread_create(&tid2[aux], NULL, fn_read, NULL) != -1);
  assert(pthread_create(&tid3[aux], NULL,fn_hardlink, NULL) != -1);
}

  printf("Successful test.\n");
  return 0;
}
