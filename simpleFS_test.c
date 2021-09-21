#include "simpleFS.c"
#include "bitmap.c"
#include "disk_driver.c"
#include "simpleFS.h"
#include "bitmap.h"
#include "disk_driver.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#define  TRUE 1
#define  FALSE 0

int test_global = FALSE;

int count_bloks(int n) {
	return n % BLOCK_SIZE == 0 ? n/BLOCK_SIZE : (n / BLOCK_SIZE) +1;
}
int space_in_dir(int* fblock, int dim){
		int i = 0;
		int free_s = 0;
		while(i<dim){
			if(*fblock == 0) free_s++;
			fblock++;
			i++;
		}
		return free_s;
}
int stampa(char* testo){
	int i, j;
	for(i = 0; i < strlen(testo); i++){
		char c = testo[i];
		for(j = 7; j>= 0; j--){
			printf("%d", !!((c >> j) &0x01));
		}
	}
}

int main(int agc, char** argv) {
  int num = 765;
  printf("\n Test blockToIndex(%d)", num);
  BitMapEntryKey bmap_block = BitMap_blockToIndex(num);
  printf("\n	Posizione blocco = %d; entry %d al bit %d", num, bmap_block.entry_num, bmap_block.bit_num);
  
  printf("\n Test IndexToBlock(bmap_block)");
  int pos = BitMap_indexToBlock(bmap_block.entry_num, bmap_block.bit_num);
  printf("\n Otteniamo la entry %d e il bit %d: la posizione %d", bmap_block.entry_num, bmap_block.bit_num, pos);
  
  DiskDriver d;
  BitMap bmap;
  if(test_global){
	  DiskDriver_init(&d, "test/test.txt", 50);
  }
  else{
	  char buffer[255];
	  sprintf(buffer, "test/%d.txt", time(NULL));
	  DiskDriver_init(&d, buffer, 50);
  }
  bmap.num_bits = d.header->bitmap_entries * 8;
  bmap.entries = d.bitmap_data;
  printf("\n\n Test BitMap_set()");
  printf("\n Test DiskDriver_init(d, \"test.txt\", 15)");
  printf("\n Prima ==> ");
  stampa(bmap.entries);
  printf("\n Bitmap_set(6, 1) ==> %d", BitMap_set(&bmap, 6, 1));
  printf("\n Dopo ==> ");
  stampa(bmap.entries);
  
  printf("\n Test Bitmap_get()");
  int inizio = 6, stato = 0;
  printf("\n Partiamo da %d e cerchiamo %d ==> %d", inizio, stato, BitMap_get(&bmap, inizio, stato));
  inizio = 2, stato = 1;
  printf("\n Partiamo da %d e cerchiamo %d ==> %d", inizio, stato, BitMap_get(&bmap, inizio, stato));
  inizio = 11, stato = 0;
  printf("\n Partiamo da %d e cerchiamo %d ==> %d", inizio, stato, BitMap_get(&bmap, inizio, stato));
  inizio = 13, stato = 1;
  printf("\n Partiamo da %d e cerchiamo %d ==> %d", inizio, stato, BitMap_get(&bmap, inizio, stato));
  
}
