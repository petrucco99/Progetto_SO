#include "simpleFS.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>






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
		dh->pos_in_block = fblock->fcb.block_in_disk;
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
		dh->pos_in_block = fblock->fcb.block_in_disk;
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
	bmap.num_bits= fs->disk->header->bitmap_entries * 8;
	bmap.entries = fs->disk->bitmap_data;
	for (i = 0; i < bmap.num_bits; i++){
		BitMap_set(&bmap, i, 0);
	}
	
	//memorizzo le entries della bmap nel disk
	fs->disk->bitmap_data = bmap.entries;
	
	//creo blocco cartella base e inserisco informazioni sull'header
	FirstDirectoryBlock* fblock = malloc(sizeof(FirstDirectoryBlock));
	fblock->header.previous_block = -1;
	fblock->header.next_block = -1;
	fblock->header.block_in_file = 0;
	
	//inserisco info sul FBC
	fblock->fcb.block_in_disk = fs->disk->header->first_free_block;
	fblock->fcb.directory_block = -1;
	fblock->fcb.is_dir = 1;
	strcpy(fblock->fcb.name, "/");
	fblock->fcb.size_in_blocks = count_bloks(fblock->fcb.size_in_blocks);
	fblock->fcb.size_in_bytes= sizeof(FirstDirectoryBlock);
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
	fblock->header.previous_block = -1;
	fblock->header.next_block = -1;
	fblock->header.block_in_file = 0;
	fblock->fcb.block_in_disk = DiskDriver_getFreeBlock(d->sfs->disk, 0);
	fblock->fcb.directory_block = d->dcb->fcb.block_in_disk;
	fblock->fcb.is_dir = 0;
	strcpy(fblock->fcb.name, filename);
	fblock->fcb.size_in_blocks = count_bloks(fblock->fcb.size_in_bytes);
	fblock->fcb.size_in_bytes = 0;
	fh->directory = d->dcb;
	fh->current_block = &(fblock->header);
	fh->pos_in_file = 0;
	
	//imposto mem con caratteri nulli
	memset(fblock->data, '\0', sizeof(fblock->data));
	//scrivo il blocco con le info del file sul disco
	//e nell'handle da restituire
	DiskDriver_writeBlock(d->sfs->disk, fblock, fblock->fcb.block_in_disk);
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
		d->dcb->file_blocks[free_space] = fblock->fcb.block_in_disk;
		d->dcb->num_entries++;
		//sovrascrivo le info della cartella
		DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
	}
	else{
		//memorizzo il blocco dove tengo FirstDirectoryBlock
		int dir_block = d->dcb->fcb.block_in_disk;
		
		DirectoryBlock* db = malloc(sizeof(DirectoryBlock));
		int new_db_block;
		//se è presente un successivo 
		if(d->dcb->header.next_block != -1){
			//leggo e memorizo in db
			dir_block = d->dcb->header.next_block;
			db = malloc(sizeof(DirectoryBlock));
			DiskDriver_readBlock(d->sfs->disk, db, d->dcb->header.next_block);
			//continuo per tutti i swuccessivi esistenti
			while(db->header.next_block != -1){
				dir_block = db->header.next_block;
				DiskDriver_readBlock(d->sfs->disk, db, dir_block);
			}
		}
		else{
			//creo un blocco successivo se non ce ne sono
			new_db_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
			DirectoryBlock* new_db = malloc(sizeof(DirectoryBlock));
			new_db->header.next_block = -1;
			new_db->header.previous_block = dir_block;
			new_db->header.block_in_file = db->header.block_in_file + 1;
			//aggiorno next block nel fdb e sovrascrivo o aggiorno se presente
			d->dcb->header.next_block = new_db_block;
			DiskDriver_writeBlock(d->sfs->disk, db, dir_block);
			//aggiorno i puntatori
			db =new_db;
			dir_block = new_db_block;
		}
		//se l'ultimo blocco non basta ne creo uno aggintivo
		if(!space_in_dir(db->file_blocks, sizeof(d->dcb->file_blocks))){
			new_db_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
			DirectoryBlock* new_db = malloc(sizeof(DirectoryBlock));
			new_db->header.next_block = -1;
			new_db->header.previous_block = dir_block;
			new_db->header.block_in_file = db->header.block_in_file + 1;
			//aggiorno il next block del vecchio directory block
			db->header.next_block = new_db_block;
			DiskDriver_writeBlock(d->sfs->disk, db, dir_block);
			//adatto i puntatori
			db = new_db;
			dir_block = new_db_block;
		}
		//memorizzo l'indice del file nel FB del nuovo DB
		db->file_blocks[d->dcb->num_entries] = fh->fcb->fcb.block_in_disk;
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
	if(names == NULL || d == NULL) return -1;
	//creo FCB dove salvo info dall'handle
	FirstDirectoryBlock* fdir = malloc(sizeof(FileControlBlock));
	fdir = d->dcb;
	
	//per ogni file se è presente nel blocco memorizzo nell'array
	//se non è presente memorizzo nel blocco successivo
	int i, j = 0;
	for(i = 0; i < d->dcb->num_entries; i++, j++){
		if(j >= sizeof(fdir->file_blocks)){
			DirectoryBlock* fblock;
			j = j - sizeof(fblock->file_blocks);
			DiskDriver_readBlock(d->sfs->disk, fblock, fblock->header.next_block);
		}
		//leggo il nome del file nella posizione corrente
		FirstFileBlock* primo = malloc(sizeof(FirstFileBlock));
		DiskDriver_readBlock(d->sfs->disk, primo, fdir->file_blocks[j]);
		names[j] = primo->fcb.name;
	}
	return 0;
}

//apre il file della directory d se esiste
FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename){
	//controllo parametri
	if(d == NULL || filename == NULL) return NULL;
	
	//memorizzo info della cartella
	FileHandle* fh = malloc(sizeof(FileHandle));
	FirstDirectoryBlock* db = malloc(sizeof(FirstDirectoryBlock));
	db = d->dcb;
	
	int i, j=0;
	
	//per ogni elem nella cartella
	for(i = 0; i<d->dcb->num_entries; i++, j++){
		//se l'indice del blocco è maggiore della dim
		if(j >= sizeof(db->file_blocks)){
			DirectoryBlock* db;
			j = j-sizeof(db->file_blocks);
			DiskDriver_readBlock(d->sfs->disk, db, db->header.next_block);
		}
		//leggo il primo blocco
		FirstFileBlock* fblock = malloc(sizeof(FirstFileBlock));
		DiskDriver_readBlock(d->sfs->disk, fblock, db->file_blocks[j]);
		//se il nome preso è del file
		if(strcmp(fblock->fcb.name, filename) == 0 && fblock->fcb.is_dir == 0){
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
//scrivi sukl file
int SimpleFS_write(FileHandle* f, void* data, int size){
	//controllo input
	if(f == NULL || data == NULL || size<0 ) return -1;
	
	//inizializzo variabili e aggiorno cursore
	int wbyte = 0, wblock = 0;
	int p = f->pos_in_file;
	f->pos_in_file = (int) (f->pos_in_file + strlen(data));
	
	//se il cursore adesso si trova ancora nel blocco
	if(size + p == sizeof(f->fcb->data)){
		//copio la stringa nel blocco da "p"
		memcpy(f->fcb->data + p, data, strlen(data));
		DiskDriver_writeBlock(f->sfs->disk, f->fcb, f->fcb->fcb.block_in_disk);
		wbyte = strlen(data);
		wblock++;
	}
	else{
		//inizializo variabili per i byte da scrivere
		int dim = 0, block;
		//copio la stringa da scrivere per poterla modificare 
		char* copia = malloc(strlen(data));
		strcpy(copia, data);
		
		//mi tengo gli indici dei blocchi precedenti,
		//successivi e l'indice precedente
		int pblock = f->fcb->fcb.block_in_disk;
		int nblock = f->fcb->header.next_block;
		int pindex = f->fcb->header.block_in_file;
		FileBlock* file = malloc(sizeof(FileBlock));
		
		//controllo la posizione dove scrivere
		//se non rientra nel blocco
		if(p > sizeof(f->fcb->data)){
			p = p - sizeof(f->fcb->data);
			if(nblock != -1){
				//vado avanti e decremento finchè esiste un 
				//blocco successivo e la posizione è maggiore
				while(nblock != -1 && p > sizeof(file->data)){
					p -= sizeof(file->data);
					pindex++;
					pblock = nblock;
					nblock = file->header.next_block;
				}
			}
			//se non sono presenti blocchi successivi
			else{
				//finche la posizione è maggiore del blocco
				while(p > sizeof(file->data)){
					nblock = DiskDriver_getFreeBlock(f->sfs->disk, 0);
					file->header.next_block = -1;
					file->header.previous_block = pblock;
					file->header.block_in_file = pindex;
					
					//decremento posizione
					p -= sizeof(file->data);
					pindex ++;
					
					//aggiorno blocco attuale e precedente
					pblock = nblock;
					DiskDriver_writeBlock(f->sfs->disk, file, nblock);
					nblock = -1;
				}
			}
		}
		//se la posizione è nel blocco setto i dati da scrivere
		else{
			dim = sizeof(f->fcb->data)- p;
			//copio nel blocco attuale i primi caratteri "dim"
			memcpy(f->fcb->data, copia, dim);
			DiskDriver_writeBlock(f->sfs->disk, f->fcb, f->fcb->fcb.block_in_disk);
			//aumento copia per farlo puntare al prossimo carattere
			copia += dim;
			wbyte += dim;
			wblock++;
		}
		//se la posizione del puntatore è > dello spazio
		dim = strlen(copia) + p ? sizeof(file->data) - p : strlen(copia);
		
		//finche non ci sono più caratteri da scrivere
		while(dim != 0){
			//se esiste un blocco successivo
			if(nblock != -1){
				//leggo il successivo e lo inizializzo con 
				//caratteri nulli e scrivo dim caratgteri da p
				block = nblock;
				DiskDriver_readBlock(f->sfs->disk, file, nblock);
				nblock = file->header.next_block;
				memset(file->data + p, '\0', sizeof(file->data) - p);
				memcpy(file->data + p, copia, dim);
				
				//aggiorno copia
				copia += dim;
				wbyte += dim;
				DiskDriver_writeBlock(f->sfs->disk, file, block);
				//aggiorno blocco precedente con attuale
				nblock = block;
			}
			else{
				//trovo un blocco dove memorizzare i dati
				block = DiskDriver_getFreeBlock(f->sfs->disk, 0);
				//se precedente èera FirsFileBlock aggiorno successivo 
				if(pindex == 0){
					f->fcb->header.next_block = block;
					DiskDriver_writeBlock(f->sfs->disk, f->fcb, f->fcb->fcb.block_in_disk);
				}
				else{
					//leggo blocco precedente e aggiorno l'indice del successivo
					FileBlock* fblock = malloc(sizeof(FileBlock));
					DiskDriver_readBlock(f->sfs->disk, fblock, pblock);
					fblock->header.next_block = block;
					DiskDriver_writeBlock(f->sfs->disk, fblock, pblock);
				}
				//memorizzo info del blocco attuale
				file->header.previous_block = pblock;
				nblock = -1;
				file->header.next_block = nblock;
				pblock += 1;
				file->header.block_in_file = pblock;
				//inizializzo con char nulli il blocco attuale
				//scrivo dim caratteri
				memset(file->data, '\0', sizeof(file->data));
				memcpy(file->data, copia, dim);
				//aggiorno copia
				copia += dim;
				wbyte += dim;
				DiskDriver_writeBlock(f->sfs->disk, file, block);
				pblock = block;
			}
			//se la dim finale del puntatore > dello spazio
			dim = strlen(copia) + p >sizeof(file->data) ? sizeof(file->data) - p : strlen(copia);
			wblock++;
		}
	}
	//aggiorno byte e blocchi file
	f->fcb->fcb.size_in_bytes = p + strlen(data) > f->fcb->fcb.size_in_bytes ? p + strlen(data) : f->fcb->fcb.size_in_bytes;
	f->fcb->fcb.size_in_blocks = wblock;
	DiskDriver_writeBlock(f->sfs->disk, f->fcb, f->fcb->fcb.block_in_disk);
	return wbyte;
}

//legge nel file nella posizione size i bytes in data
int SimpleFS_read(FileHandle* f, void* data, int size){
	//controllo parametri
	if(f == NULL || data == NULL || size < 0) return -1;
	
	//memorizzo il primo blocco da leggere
	FirstFileBlock* block = malloc(sizeof(FirstFileBlock));
	DiskDriver_readBlock(f->sfs->disk, block, f->fcb->fcb.block_in_disk);
	int next_block = f->fcb->header.next_block;
	
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
			sprintf(data, "%s%s", (char*) data, file->data);
			next_block = file->header.next_block;
		}
	}
	return strlen(data);
}

//ritorna il numero di bytes letti e ritrorna pos
int SimpleFS_seek(FileHandle* f, int pos){
	//controllo parametri
	if(f == NULL || pos < 0) return -1;
	//controllo numero caratteri del file
	int dim = 0;
	FirstFileBlock* block = malloc(sizeof(FirstFileBlock));
	DiskDriver_readBlock(f->sfs->disk, block, f->fcb->fcb.block_in_disk);
	dim += sizeof(block->data);
	if(block->header.next_block != -1){
		FileBlock* file = malloc(sizeof(FileBlock));
		DiskDriver_readBlock(f->sfs->disk, file, file->header.next_block);
		dim += sizeof(file->data);
		while(file->header.next_block != -1){
			DiskDriver_readBlock(f->sfs->disk, file, file->header.next_block);
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
int SimpleFS_changeDir(DirectoryHandle* d, char* dirname){
	//controllo su input
	if(d == NULL || dirname == NULL) return -1;
	
	//caso ".."
	if(strcmp(dirname, "..") == 0){
		//se siamo nella radice errore
		if(strcmp(d->dcb->fcb.name, "/") == 0) return -1;
		else{
			//leggo le info sulla cartella genitore
			FirstDirectoryBlock* parent = malloc(sizeof(FirstDirectoryBlock));
			DiskDriver_readBlock(d->sfs->disk, parent, d->directory->fcb.block_in_disk);
			d->dcb = d->directory;
			d->current_block = &(d->dcb->header);
			d->pos_in_block = d->dcb->fcb.block_in_disk;
			d->pos_in_dir = 0;
			d->directory = parent;
			return 0;
		}
	}
	//caso diverso da ".."
	else{
		//controllo se esiste la dir e mi ci sposto se esiste
		int block = DirectoryExist(d, dirname);
		if(block != -1){
			FirstDirectoryBlock* child = malloc(sizeof(FirstDirectoryBlock));
			DiskDriver_readBlock(d->sfs->disk, child, block);
			//modifica directory handle
			d->directory = d->dcb;
			d->current_block = &(d->dcb->header);
			d->pos_in_block = block;
			d->pos_in_dir = 0;
			d->dcb = child;
			return 0;			
		}
		else return -1;
	}
}

//funzione ausiliaria che controlla se la dir esiste
int DirectoryExist(DirectoryHandle* d, char* dname){
	//controllo parametri
	if(d == NULL || dname == NULL) return -1;
	
	int i, j = 0;
	FirstDirectoryBlock* dir = malloc(sizeof(FirstDirectoryBlock));
	DiskDriver_readBlock(d->sfs->disk, dir, d->dcb->fcb.block_in_disk);
	
	for(i = 0; i<d->dcb->num_entries; i++, j++){
		//controllo indice non sia maggiore della dim del blocco
		if(j >= sizeof(dir->file_blocks)){
			DirectoryBlock* dir;
			j = j- sizeof(dir->file_blocks);
			DiskDriver_readBlock(d->sfs->disk, dir, dir->header.next_block);
		}
		//leggo primo blocco dell'elem attuale
		FirstDirectoryBlock* fblock = malloc(sizeof(FirstDirectoryBlock));
		DiskDriver_readBlock(d->sfs->disk, fblock, dir->file_blocks[j]);
		
		//controllo directory
		if(strcmp(fblock->fcb.name, dname) == 0 && fblock->fcb.is_dir == 1){
			return fblock->fcb.block_in_disk;
		}
	}
	return -1;
}

//creo cartella dirname il directory d
int SimpleFS_mkDir(DirectoryHandle* d, char* dirname){
	//controllo input
	if(d == NULL || dirname == NULL) return -1;
	if(d->sfs->disk->header->free_blocks < 1) return -1;
	if(DirectoryExist(d, dirname) != -1) return -1;
	
	//creo il primo blocco della cartella
	FirstDirectoryBlock* fdblock = malloc(sizeof(FirstDirectoryBlock));
	BlockHeader h;
	h.previous_block = -1;
	h.next_block = -1;
	h.block_in_file = 0;
	fdblock->header = h;
	fdblock->fcb.block_in_disk = DiskDriver_getFreeBlock(d->sfs->disk, 0);
	fdblock->fcb.directory_block = d->dcb->fcb.block_in_disk;
	fdblock->fcb.is_dir = 1;
	strcpy(fdblock->fcb.name, dirname);
	fdblock->fcb.size_in_blocks = 0;
	fdblock->fcb.size_in_bytes = 0;
	fdblock->num_entries = 0;
	memset(fdblock->file_blocks, 0, sizeof(fdblock->file_blocks));
	DiskDriver_writeBlock(d->sfs->disk, fdblock, fdblock->fcb.block_in_disk);
	int i;
	//se è presente abbastanza spazio nel blocco attuale
	if(space_in_dir(d->dcb->file_blocks, sizeof(d->dcb->file_blocks))){
		//calcolo primo spazio libero nel FB della cartella
		int first;
		for(i = 0; i < sizeof(d->dcb->file_blocks); i++){
			if(d->dcb->file_blocks[i] < 1){
				first = i;
				break;
			}
		}
		//memorizzo il blocco dove è salvato la cartella
		d->dcb->file_blocks[first] = fdblock->fcb.block_in_disk;
		d->dcb->num_entries++;
		DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
	}
	else{
		//se non è presente lo spazio necessario
		int block = d->dcb->fcb.block_in_disk;
		DirectoryBlock* dir;
		int new_block;
		
		//se è presente un blocco successivo memorizzo in dir l'ultimo
		if(d->dcb->header.next_block != -1){
			block = d->dcb->header.next_block;
			dir = malloc(sizeof(DirectoryBlock));
			DiskDriver_readBlock(d->sfs->disk, dir, d->dcb->header.next_block);
			while(dir->header.next_block != -1){
				block = dir->header.next_block;
				DiskDriver_readBlock(d->sfs->disk, dir, block);
			}
		}
		//adesso in dir abbiamo l'ultimo blocco della cartella
		//controllo se è presente abbastanza spazio
		if(!space_in_dir(dir->file_blocks, sizeof(d->dcb->file_blocks))){
			//se negativo creo nuovo blocco per le info
			new_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
			DirectoryBlock* dblock = malloc(sizeof(DirectoryBlock));
			dblock->header.next_block = -1;
			dblock->header.previous_block = block;
			dblock->header.block_in_file = dir->header.block_in_file + 1;
			
			//aggiorno next block e puntatori
			dir->header.next_block = new_block;
			DiskDriver_writeBlock(d->sfs->disk, dir, block);
			dir = dblock;
			block = new_block;
		}
		//trovo il primo spazio libero nei fileblock della cartella
		int first;
		for(i = 0; i < sizeof(dir->file_blocks); i++){
			if(dir->file_blocks[i] < 1){
				first = i;
				break;
			}
		}
		//scrivo l'indicedel blocco dove è memorizzata 
		//la nuova nella cartella genitore
		dir->file_blocks[first] = fdblock->fcb.block_in_disk;
		d->dcb->num_entries++;
		//scrivo su blocco libero la nuova directory
		DiskDriver_writeBlock(d->sfs->disk, dir, block);
	}
	DiskDriver_flush(d->sfs->disk);
	return 0;
}

//rimuovo il file
int SimpleFS_remove(SimpleFS* fs, char* filename){
	if(fs == NULL || filename == NULL){
		printf("\nProva FS");
		return -1;
		printf("\nNON È IL FS");
	}
	DirectoryHandle* d = SimpleFS_init(fs, fs->disk);
	//printf("\n%d", d);
	//fflush(stdout);
	int res = SimpleFS_remove_aux(d, filename);
	return res;
}

//funzione ausiliaria per la remove
int SimpleFS_remove_aux(DirectoryHandle* d, char* filename){
	if(d == NULL || filename == NULL){
		printf("\nProva DH");
		return -1;
		printf("\nNON È IL DH");
	}
	int current_block, next_block, i, j;

	// Se la cartella un blocco successivo
	if(d->dcb->header.next_block != -1) {
		//printf("\nprova 1");
		//fflush(stdout);
		int next_dir_block = d->dcb->header.next_block;
		DirectoryBlock * db = malloc(sizeof(DirectoryBlock));
		//printf("\n NEXT_DIR_BLOCK = %d", d->dcb->header.next_block);
		//fflush(stdout);

		// Finché ci sono blocchi successivi
		while(next_dir_block != -1) {
			//printf("\n next_dir_block = %d", next_dir_block);
			//fflush(stdout);

			// Leggo il blocco successivo
			DiskDriver_readBlock(d->sfs->disk, db, next_dir_block);

			// Per ogni elemento contenuto nel blocco
			for(i = 0; i < sizeof(db->file_blocks); i++) {

				// Se c'è un elemento non nullo
				if(db->file_blocks[i] > 0) {

					// Memorizzo le informazioni del primo blocco del file contenuto in questo elemento
					FirstFileBlock * file = malloc(sizeof(FirstFileBlock));
					DiskDriver_readBlock(d->sfs->disk, file, db->file_blocks[i]);
					current_block = file->fcb.block_in_disk;

					// Se il nome del file appena letto corrisponde al nome che sto cercando
					if(strcmp(file->fcb.name, filename) == 0){

						// Se è un file, cancello tutti i blocchi del file stesso
						if(file->fcb.is_dir == 0) {
							if(file->header.next_block != -1) {
								do {
									next_block = file->header.next_block;
									DiskDriver_freeBlock(d->sfs->disk, current_block);
									db->file_blocks[i] = 0;
									if(next_block != -1) DiskDriver_readBlock(d->sfs->disk, file, next_block);
									current_block = next_block;
								} while(next_block != -1);
								return 0;
							}else{

								// Se è formato da un solo blocco, cancello il blocco stesso
								DiskDriver_freeBlock(d->sfs->disk, current_block);
							}
						}else{

							// Altrimenti, se è una cartella, leggo il primo blocco della cartella
							FirstDirectoryBlock * fdb2 = malloc(sizeof(FirstDirectoryBlock));
							DiskDriver_readBlock(d->sfs->disk, fdb2, db->file_blocks[i]);

							// Creo un DirectoryHandle per la cartella appena creata
							DirectoryHandle * dh = malloc(sizeof(DirectoryHandle));
							dh->sfs = d->sfs;
							dh->dcb = fdb2;
							dh->directory = d->dcb;
							dh->current_block = &fdb2->header;
							dh->pos_in_dir = 0;
							dh->pos_in_block = 0;

							// Se la cartella è formata da più di un blocco
							if(fdb2->header.next_block != -1) {

								// Per ogni elemento del primo blocco
								for(j = 0; j < sizeof(fdb2->file_blocks); j++) {

									// Se l'elemento contiene qualcosa, chiamo la funzione ricorsivamente
									if(db->file_blocks[j] > 0) {
										FirstFileBlock * file_to_delete = malloc(sizeof(FirstFileBlock));
										DiskDriver_readBlock(d->sfs->disk, file_to_delete, fdb2->file_blocks[j]);
										SimpleFS_remove_aux(dh, file_to_delete->fcb.name);
										db->file_blocks[j] = 0;
									}
								}

								// Leggo il blocco successivo
								DirectoryBlock * db2 = malloc(sizeof(DirectoryBlock));
								int next_db2_block = fdb2->header.next_block;

								// Finché ci sono blocchi successivi
								while(next_db2_block != -1) {

									// Leggo il blocco successivo
									DiskDriver_readBlock(d->sfs->disk, db2, next_db2_block);

									// Per ogni elemento del blocco, se contiene qualcosa, richiamo ricorsivamente la funzione
									for(j = 0; j < sizeof(db2->file_blocks); j++) {
										if(db->file_blocks[i] > 0) {
											FirstFileBlock * file_to_delete = malloc(sizeof(FirstFileBlock));
											DiskDriver_readBlock(d->sfs->disk, file_to_delete, fdb2->file_blocks[j]);
											SimpleFS_remove_aux(dh, file_to_delete->fcb.name);
											db->file_blocks[i] = 0;
										}
									}
								}
							}else{

								// Altrimenti, se la cartella non ha blocchi successivi
								// Per ogni elemento del primo blocco
								for(j = 0; j < sizeof(fdb2->file_blocks); j++) {

									// Se l'elemento contiene qualcosa, richiamo ricorsivamente la funzione
									if(fdb2->file_blocks[i] > 0) {
										FirstFileBlock * file_to_delete = malloc(sizeof(FirstFileBlock));
										DiskDriver_readBlock(d->sfs->disk, file_to_delete, fdb2->file_blocks[j]);
										SimpleFS_remove_aux(dh, file_to_delete->fcb.name);
										fdb2->file_blocks[i] = 0;
									}
								}
							}
						}
					}
				}
			}
			next_dir_block = db->header.next_block;
		}
	}else{

		// Altrimenti, se la cartella è formata da un solo blocco
		// Per ogni elemento contenuto nel primo blocco
		int remaining_entries;
		for(i = 0, remaining_entries = d->dcb->num_entries; i < sizeof(d->dcb->file_blocks); i++) {

			// Se c'è un file memorizzato
			if(d->dcb->file_blocks[i] > 0 && remaining_entries) {
				remaining_entries--;

				// Memorizzo le informazioni del file
				FirstFileBlock * file_to_delete = malloc(sizeof(FirstFileBlock));
				DiskDriver_readBlock(d->sfs->disk, file_to_delete, d->dcb->file_blocks[i]);
				current_block = d->dcb->file_blocks[i];

				// Se è il file che sto cercando
				if(strcmp(file_to_delete->fcb.name, filename) == 0){

					// Se non si tratta di una cartella
					if(file_to_delete->fcb.is_dir == 0) {

						// Se il file ha più di un blocco
						if(file_to_delete->header.next_block != -1) {
							do {

								// Memorizzo il prossimo blocco
								next_block = file_to_delete->header.next_block;

								// Cancello il blocco attuale
								DiskDriver_freeBlock(d->sfs->disk, current_block);
								d->dcb->file_blocks[i] = 0;

								// Se esiste un blocco successivo, lo leggo
								if(next_block != -1) DiskDriver_readBlock(d->sfs->disk, file_to_delete, next_block);

								// Imposto il blocco successivo come attuale
								current_block = next_block;
							} while(next_block != -1);
						}else{

							// Se ha un solo blocco, lo cancello
							DiskDriver_freeBlock(d->sfs->disk, current_block);
						}
						d->dcb->file_blocks[i] = 0;

						// Dopo aver canncellato tutti i blocchi del file, restituisco 0
						return 0;
					}else{

						// Se è una cartella, memorizzo le informazioni del suo primo blocco
						FirstDirectoryBlock * fdb2 = malloc(sizeof(FirstDirectoryBlock));
						DiskDriver_readBlock(d->sfs->disk, fdb2, d->dcb->file_blocks[i]);

						// Creo un DirectoryHandle da passare alle chiamate ricorsive
						DirectoryHandle * dh = malloc(sizeof(DirectoryHandle));
						dh->sfs = d->sfs;
						dh->dcb = fdb2;
						FirstDirectoryBlock * parent_folder = malloc(sizeof(FirstDirectoryBlock));
						DiskDriver_readBlock(d->sfs->disk, parent_folder, fdb2->fcb.directory_block);
						dh->directory = parent_folder;
						dh->current_block = &(fdb2->header);
						dh->pos_in_dir = 0;
						dh->pos_in_block = 0;

						// Se la cartella è formata da più blocchi
						if(fdb2->header.next_block != -1) {
							for(j = 0; j < sizeof(fdb2->file_blocks); j++) {
								if(d->dcb->file_blocks[j] > 0) {
									FirstFileBlock * file_to_delete = malloc(sizeof(FirstFileBlock));
									DiskDriver_readBlock(d->sfs->disk, file_to_delete, fdb2->file_blocks[j]);
									SimpleFS_remove_aux(dh, file_to_delete->fcb.name);
									fdb2->file_blocks[j] = 0;
								}
							}
							DirectoryBlock * db2 = malloc(sizeof(DirectoryBlock));
							int next_db2_block = fdb2->header.next_block;
							while(next_db2_block != -1) {
								DiskDriver_readBlock(d->sfs->disk, db2, next_db2_block);
								for(j = 0; j < sizeof(db2->file_blocks); j++) {
									if(d->dcb->file_blocks[j] > 0) {
										FirstFileBlock * file_to_delete = malloc(sizeof(FirstFileBlock));
										DiskDriver_readBlock(d->sfs->disk, file_to_delete, fdb2->file_blocks[j]);
										SimpleFS_remove_aux(dh, file_to_delete->fcb.name);
										fdb2->file_blocks[j] = 0;
									}
								}
							}
						}else{
							int remaining_entries2;

							// Se invece è formata da un solo blocco, leggo tutti i suoi elementi
							for(j = 0, remaining_entries2 = fdb2->num_entries; j < sizeof(fdb2->file_blocks); j++) {

								// Se l'elemento contiene un blocco figlio
								if(fdb2->file_blocks[j] > 0 && remaining_entries2) {

									// Memorizzo le informazioni del suo figlio
									FirstFileBlock * file_to_delete = malloc(sizeof(FirstFileBlock));
									DiskDriver_readBlock(d->sfs->disk, file_to_delete, fdb2->file_blocks[j]);

									// Chiamo ricorsivamente la funzione con il nome del figlio appena letto
									SimpleFS_remove_aux(dh, file_to_delete->fcb.name);
									fdb2->file_blocks[j] = 0;
									remaining_entries2--;
								}
							}
							DiskDriver_freeBlock(d->sfs->disk, fdb2->fcb.block_in_disk);
							return 0;
						}
					}
					d->dcb->file_blocks[i] = 0;
				}
			}
		}
	}
	return -1;
	
}
