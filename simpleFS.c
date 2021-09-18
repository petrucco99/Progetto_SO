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
FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename){
	//controllo parametri e che il file non esista già 
	//e che ci siano abbastanza blocchi liberi
	if(d == NULL || filename == NULL) return NULL;
	if(SimpleFS_openFile(d, filename) != NULL) return NULL;
	if(d->sfs->disk->header->free_blocks < 1) return NULL;
	
	//creo FileHandler e inserisco info
	FileHandle* fh = malloc(sizeof(FileHandle));
	fh->sfs = d->sfs;
	FirstFileBlock* fblock = malloc(sizeof(FirstFileBlock));
	fblock->header->previous_block = -1;
	fblock->header->next_block = -1;
	fblock->header->block_in_file = 0;
	fblock->fcb->block_in_disk = DiskDriver_getFreeBlock(d->sfs->disk, 0);
	fblock->fcb->directory_block = d->dcb->fcb->block_in_disk;
	fblock->fcb->is_dir = 0;
	strcpy(fblock->fcb->name, filename);
	fblock->fcb->size_in_blocks = count_bloks(fblock->fcb->size_in_bytes);
	fblock->fcb->size_in_bytes = 0;
	fh->directory = d->dcb;
	fh->current_block = &(fblock->header);
	fh->pos_in_file = 0;
	
	//imposto mem con caratteri nulli
	memset(fblock->data, '\0', sizeof(fblock->data));
	//scrivo il blocco con le info del file sul disco
	//e nell'handle da restituire
	DiskDriver_writeBlock(d->sfs->disk, fblock, fblock->fcb->block_in_disk);
	fh->fcb = fblock;
	
	//controllo se c'è abbastanza spazio nella cartella
	if(space_in_dir(d->dcb->file_blocks, sizeof(d->dcb->file_blocks))){
		//calcolo primo sapzio libero dove memorizzare
		int i, free_space;
		for(i = 0; i<sizeof(d->dcb->file_blocks); i++){
			if(d->dcb->file_blocks[i] < 1){
				free_space = i;
				break;
			}
		}
		//passo l'indice dove è momorizzato il file al dcb
		d->dcb->file_blocks[free_space] = fblock->fcb->block_in_disk;
		d->dcb->num_entries++;
		//sovrascrivo le info della cartella
		DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb->block_in_disk);
	}
	else{
		//memorizzo il blocco dove tengo FirstDirectoryBlock
		int dir_block = d->dcb->fcb->block_in_disk;
		
		DirectoryBlock* db = malloc(sizeof(DirectoryBlock));
		int new_db_block;
		//se è presente un successivo 
		if(d->dcb->header->next_block != -1){
			//leggo e memorizo in db
			dir_block = d->dcb->header->next_block;
			db = malloc(sizeof(DirectoryBlock));
			DiskDriver_readBlock(d->sfs->disk, db, d->dcb->header->next_block);
			//continuo per tutti i swuccessivi esistenti
			while(db->header->next_block != -1){
				dir_block = db->header->next_block;
				DiskDriver_readBlock(d->sfs->disk, db, dir_block);
			}
		}
		else{
			//creo un blocco successivo se non ce ne sono
			new_db_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
			DirectoryBlock* new_db = malloc(sizeof(DirectoryBlock));
			new_db->header->next_block = -1;
			new_db->header->previous_block = dir_block;
			new_db->header->block_in_file = db->header->block_in_file + 1;
			//aggiorno next block nel fdb e sovrascrivo o aggiorno se presente
			d->dcb->header->next_block = new_db_block;
			DiskDriver_writeBlock(d->sfs->disk, db, dir_block);
			//aggiorno i puntatori
			db =new_db;
			dir_block = new_db_block;
		}
		//se l'ultimo blocco non basta ne creo uno aggintivo
		if(!space_in_dir(db->file_blocks, sizeof(db->dcb->file_blocks))){
			new_db_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
			DirectoryBlock* new_db = malloc(sizeof(DirectoryBlock));
			new_db->header->next_block = -1;
			new_db->header->previous_block = dir_block
			new_db->header->block_in_file = db->header->block_in_file + 1;
			//aggiorno il next block del vecchio directory block
			db->header->next_block = new_db_block;
			DiskDriver_writeBlock(d->sfs->disk, db, dir_block);
			//adatto i puntatori
			db = new_db;
			dir_block = new_db_block;
		}
		//memorizzo l'indice del file nel FB del nuovo DB
		db->file_blocks[d->dcb->num_entries] = fh->fcb->fcb->block_in_disk;
		d->dcb->num_entries++;
		//scrivo su un blocco libero la DB creata
		DiskDriver_writeBlock(d->sfs->disk, db, dir_block);
	}
	//flusho tutto e restituisco l'handle
	DiskDriver_flush(d->sfs->disk);
	return fh;
}

//leggo nei blocchi preallocati i nomi dei file nella directory
int SimpleFS_readDir(char** names, DirectoryHandle* d){
	//controllo i parametri
	if(names == NULL || d = NULL) return -1;
	//creo FCB dove salvo info dall'handle
	FileControlBlock* fblock = malloc(sizeof(FileControlBlock));
	fblock = d->fcb;
	
	//per ogni file se è presente nel blocco memorizzo nell'array
	//se non è presente memorizzo nel blocco successivo
	int i, j = 0;
	for(i=0; i<d->dcb->num_entries; i++, j++){
		if(j>=sizeof(fblock->file_blocks)){
			DirectoryBlock* fblock;
			j=j-sizeof(fblock->file_blocks);
			DiskDriver_readBlock(d->sfs->disk, fblock, fblock->header->next_block);
		}
		//leggo il nome del file nella posizione corrente
		FirstFileBlock* primo = malloc(sizeof(FirstFileBlock));
		DiskDriver_readBlock(d->sfs->disk, primo, fblock->file_blocks[j]);
		names[j] = primo->fcb->name;
	}
	return 0;
}

//apre il file della directory d se esiste
FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename){
	//controllo parametri
	if(d ==NULL || filename ==NULL) return NULL;
	
	//memorizzo info della cartella
	FileHandle* fh = malloc(sizeof(FileHandle));
	FirstDirectoryBlock* db = malloc(sizeof(FirstDirectoryBlock));
	db->dcb;
	
	int i, j=0;
	
	//per ogni elem nella cartella
	for(i = 0; i<d->dcb->num_entries; i++, j++){
		//se l'indice del blocco è maggiore della dim
		if(j >= sizeof(db->file_blocks)){
			DirectoryBlock* db;
			j = j-sizeof(db->file_blocks);
			DiskDriver_readBlock(d->sfs->disk, db, db->header->next_block);
		}
		//leggo il primo blocco
		FirstFileBlock* fblock = malloc(sizeof(FirstFileBlock));
		DiskDriver_readBlock(d->sfs->disk, fblock, db->file_blocks[j]);
		//se il nome preso è del file
		if(strcmp(fblock->fcb->name, filename) == 0 && fblock->fcb->is_dir == 0){
			//salvo nell'handle
			fh->sfs = d->sfs;
			fh->fcb = fblock;
			fh->directory = d->dcb;
			fh->current_block = &(fblock->header);
			fh->pos_in_file = 0;
			return fh;
		}
	}
	return NULL;
}

//chiudo l'handle
int SimpleFS_close(FileHandle* f){
	if(f==NULL) return -1;
	free(f);
	return 0;
}

///int SimpleFS_write(FileHandle* f, void* data, int size);

int SimpleFS_read(FileHandle* f, void* data, int size){
	//controllo parametri
	if(f = NULL || data = NULL || size < 0) return -1;
	
	//memorizzo il primo blocco da leggere
	FirstFileBlock* block = malloc(sizeof(FirstFileBlock));
	DiskDriver_readBlock(f->sfs->disk, block, f->fcb->fcb->block_in_disk);
	int next_block = f->fcb->header->next_block;
	
	//formatto la stringa
	memset(data, '\0', size);
	
	//controllo la dim da leggere e se è minore lo leggo tutto
	if(size <= strlen(block->data)){
		strcpy(data, block->data);
	}
	else{
		//se è maggiore leggo il primo blocco
		strcpy(data, block->data);
		FileBlock* file = malloc(sizeof(FileBlock));
		
		//continuo la lettura aggiungendo in coda finchè
		//esistono blocchi successivi e il num dei caratteri
		//da leggere è minore della stringa da returnare
		while(strlen(data) < size && next_block != -1){
			DiskDriver_readBlock(f->sfs->disk, file, next_block);
			sprintf(data, "%s%s", data, file->data);
			next_block = file->header->next_block;
		}
	}
	return strlen(data);
}

//ritorna il numero di bytes letti e ritrorna pos
int SimpleFS_seek(FileHandle* f, int pos){
	//controllo parametri
	if(f = NULL || pos < 0) return -1;
	//controllo numero caratteri del file
	int dim = 0;
	FirstFileBlock* block = malloc(sizeof(FirstFileBlock));
	DiskDriver_readBlock(f->sfs->disk, block, f->fcb->fcb->block_in_disk);
	dim += sizeof(block->data);
	if(block->header->next_block != -1){
		FileBlock* file = malloc(sizeof(FileBlock));
		DiskDriver_readBlock(f->sfs->disk, file, file->header->next_block);
		dim += sizeof(file->data);
		while(file->header->next_block != -1){
			DiskDriver_readBlock(f->sfs->disk, file, file->header->next_block);
			dim+= sizeof(file->data);
		}
	}
	//restituisco errore se la posizione del cursore è 
	//maggiore di quella del file
	if(pos > dim) return -1;
	//altrimenti aggiorno la posizione del cursore
	else{
		f->pos_in_file = pos;
		return pos;
	}
}

//cerca una dir in d; se è ".." sale di livello (0=successo)
int SimpleFS_changeDir(DirectoryHandle* d, char* dirname);

int SimpleFS_mkDir(DirectoryHandle* d, char* dirname);

int SimpleFS_remove(SimpleFS* fs, char* filename);
