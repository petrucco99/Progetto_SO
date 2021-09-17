#define  _GNU_SOURCE
#include "disk_driver.h"
#include "bitmap.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
//se non funge o da errori sostituire il return nelle sue chiamate
int count_bloks(int n) {
	return n % BLOCK_SIZE == 0 ? n/BLOCK_SIZE : (n / BLOCK_SIZE) +1;
}


/* Apre il file, o lo crea se serve, allocando lo spazio sul disco,
 * calcolando la dimensione della bitmap per il file appena creato.
 * Compila un Disk header e riempie la Bitmap con 0*/
void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks){
	int bmap_entry = num_blocks;
	int f;
	if(!access(filename, F_OK)){
		f = open(filename, O_RDWR, 0666);
		if(!f){
			printf("Errore nell'apertura del file.\n");
			return;
		}
		int mem = posix_fallocate(f, 0, sizeof(DiskHeader) + bmap_entry + num_blocks * BLOCK_SIZE);
		disk->fd = f;
		disk->header = (DiskHeader*) mmap((0,sizeof(DiskHeader) + bmap_entry + (num_blocks * BLOCK_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED, f, 0);		
	}
	else{
		f = open(filename, O_CREAT | O_RDWR, 0666);
		if(!f){
			printf("Errore nell'apertura del file.\n");
			return;
		}
		disk->fd = f;
		int mem = posix_fallocate(f, 0, sizeof(DiskHeader) + bmap_entry + num_blocks * BLOCK_SIZE);
		
		//creiamo l'header disk
		
		disk->header = (DiskHeader*) mmap((0,sizeof(DiskHeader) + bmap_entry + (num_blocks * BLOCK_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED, f, 0);
		disk->header->num_blocks = num_blocks;
		disk->header->bitmap_blocks = count_bloks(bmap_entry);
		disk->header->bitmap_entries = bmap_entry;
		disk->header->free_blocks = num_bloks;
	}
	
	lseek(f, 0, SEEK_SET);
	//memorizzo nella bitmap il * alla mmap saltando il spazio lascaito per il DiskHeader
	disk->bitmap_data = (char*) disk->header + sizeof(DiskHeader);
	//lascio nel driver un dato che porta al primo blocco libero
	disk->header->first_free_block = DiskDriver_getFreeBlock(disk, 0);
	return;
}

int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num){
	if(block_num > disk->header->num_blocks) return -1;
	
	BitMap bmap;
	bmap->num_bits = disk->header->bitmap_entries*8;
	bmap->entries = disk->bitmap_data;
	
	if(BitMap_get(&bmap, block_num, 0) == block_num) return -1;
	memcpy(dest, disk->bitmap_data + disk->header->bitmap_entries +(block_num * BLOCK_SIZE), BLOCK_SIZE);
	return 0;
}

int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num){
	if(block_num > dirk->header->num_blocks) return -1;
	
	BitMap bmap;
	bmap->num_bits = disk->header->bitmap_entries*8;
	bmap->entries = disk->bitmap_data;
	
	//decremento freeblock se è libero
	if(BitMap_get(&bmap, block_num, 0) == block_num) disk->header->free_blocks--;
	if((strlen(src) * 8) > BLOCK_SIZE) return -1;
	
	//aggiorno lo stato del blocco ad occupato
	BitMap_set(&bmap, block_num, 1);
	
	memcpy(disk->bitmap_data + disk->header->bitmap_entries + (block_num * BLOCK_SIZE), src, BLOCK_SIZE);
	
	//controllo che si sia memorizzato su disco
	if(DiskDriver_flush(disk) == -1) return -1;
	disk->header->first_free_block = DiskDriver_getFreeBlock(disk, 0);
	return 0;
}

int DiskDriver_freeBlock(DiskDriver* disk, int block_num){
	if(block_num > disk->header->num_blocks) return -1;
	
	//incremento blocchi liberi nel disk header
	if(DiskDriver_getFreeBlock(disk, block_num-1) != block_num) disk->header->free_blocks++;
	
	BitMap bmap;
	bmap->num_bits = disk->header->bitmap_entries*8;
	bmap->entries = disk->bitmap_data;
	
	BitMap_set(&bamp, block_num, 0);
	disk->bitmap_data = bamp->entries;
	DiskDriver_flush(disk);
	//se il blocco è prima di quello salvato in diskheader lo cambio 
	if(block_num < disk->header->first_free_block) disk->header->first_free_block = block_num;
	return 0;
}

int DiskDriver_getFreeBlock(DiskDriver* disk, int start){
	BitMap bmap;
	bmap->num_bits = disk->header->bitmap_entries*8;
	bmap->entries = disk->bitmap_data;
	
	if(start > disk->header->num_blocks) return -1;
	if(disk->header->num_blocks <= 0) return -1;
	return BitMap_get(&bamp, start, 0);
}

int DiskDriver_flush(DiskDriver* disk){
	int dim = sizeof(DiskHeader) + disk->header->bitmap_entries + (disk->header->num_blocks*BLOCK_SIZE);
	return msync(disk->header, size, MS_SYNC);
}
