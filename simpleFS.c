#include "simplefs.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fctnl.h>
#include <unistd.h>
#include <stdlib.h>

int count_bloks(int n) {
	return n % BLOCK_SIZE == 0 ? n/BLOCK_SIZE : (n / BLOCK_SIZE) +1;
}
int space_in_dir(int* fblock, int dim){
		int i = 0;
		int free_s = =;
		while(i<dim){
			if(*fblock == 0) free_s++;
			fblock++;
			i++;
		}
		return free_s;
}




//inizializzo il FS su un disco già fatto e ritorno un handle top level della directory
DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk){
	//primo controllo sui parametri
	if(fs == NULL || disk == NULL) return NULL;
	
	//disk sarà il principale del file sistem
	fs->disk = disk;
	DirectoryHandle* dh = malloc(sizeof(DirectoryHandle));
	dh->sfs = fs;
	
	//inseriamo la radice al primo posto di bitmap o la leggo se è presente
	if(fs->disk->header->first_free_block != 0){
		FirstDirectoryBlock* fblock = malloc(sizeof(FirstDirectoryBlock));
		DiskDriver_readBlock(disk, fblock, 0);
		dh->dcb = fblock;
		dh->directory = NULL;
		dh->current_block = &(dh->dcb->header);
		dh->pos_in_dir = 0;
		dh->pos_in_block = fblock->fcb->block_in_disk;
	}
	else{
		//formatto FS se non esiste
		SimpleFS_format(fs);
		
		//collego il FS attuale in un nuovo dir handle
		dh->sfs = fs;
		//copio le info in memoria della FDirBlock
		FirstDirectoryBlock* fblock = malloc(sizeof(FirstDirectoryBlock));
		DiskDriver_readBlock(disk, fblock, 0);
		dh->dcb = fblock;
		dh->directory = NULL;
		dh->current_block = &(dh->dcb->header);
		dh->pos_in_dir = 0;
		dh->pos_in_block = fblock->fcb->block_in_disk;
	}
	return dh;
}

//creo struttura iniziale : top level dir = "/"; creo bitmap,
//tengo current_directory_block nella struct SimpleFS a top level
void SimpleFS_format(SimpleFS* fs){
	//controllo fs non sia nullo
	if(fs ==NULL) return;
	
	//azzero la bitmap e setto ogni elemento a 0
	int i;
	BitMap bmap;
	bmap->num_bits= fs->disk->header->bitmap_entries * 8;
	bmap->entries = fs->disk->bitmap_data;
	for (i=0; i<bmap->num_blocks; i++){
		BitMap_set(*bmap, i, 0)
	}
	
	//memorizzo le entries della bmap nel disk
	fs->disk->bitmap_data = bitmap.entries;
	
	//creo blocco cartella base e inserisco informazioni sull'header
	FirstDirectoryBlock* fblock = malloc(sizeof(FirstDirectoryBlock));
	fblock->header->previous_block = -1;
	fblock->header->next_block = -1;
	fblock->header->block_in_file = 0;
	
	//inserisco info sul FBC
	fblock->fcb->block_in_disk = fs->disk->header->first_free_block;
	fblock->fcb->directory_block = -1;
	fblock->fcb->is_dir = 1;
	strcpy(fblock->fcb->name, "/");
	fblock->fcb->size_in_blocks = count_bloks(fblock->fcb->size_in_blocks);
	fblock->fcb->size_in_bytes= sizeof(FirstDirectoryBlock);
	fblock->num_entries = 0;
	
	//imposto file block a 0 e memorizzo la FDB in disco
	memset(fblock->file_blocks, 0, sizeof(fblock->file_blocks));
	DiskDriver_writeBlock(fs->disk, fblock, fs->disk->header->first_free_block);
	DiskDriver_flush(fs->disk);
	
	return;
}

//crea un file vuoto nella directory d
FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename);

int SimpleFS_readDir(char** names, DirectoryHandle* d);

FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename);

//chiudo l'handle
int SimpleFS_close(FileHandle* f){
	if(f==NULL) return -1;
	free(f);
	return 0;
}

int SimpleFS_write(FileHandle* f, void* data, int size);

int SimpleFS_read(FileHandle* f, void* data, int size);

int SimpleFS_seek(FileHandle* f, int pos);

int SimpleFS_changeDir(DirectoryHandle* d, char* dirname);

int SimpleFS_mkDir(DirectoryHandle* d, char* dirname);

int SimpleFS_remove(SimpleFS* fs, char* filename);
