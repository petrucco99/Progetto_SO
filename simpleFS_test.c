#include "simpleFS.c"
#include "bitmap.c"
#include "disk_driver.c"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#define  TRUE 1
#define  FALSE 0

int test_globale = FALSE;
int test_file = 0;

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
	/*TEST BITMAP
	int num = 765;
	printf("\n Test blockToIndex(%d)", num);
	BitMapEntryKey bmap_block = BitMap_blockToIndex(num);
	printf("\n	Posizione blocco = %d; entry %d al bit %d", num, bmap_block.entry_num, bmap_block.bit_num);

	printf("\n Test IndexToBlock(bmap_block)");
	int pos = BitMap_indexToBlock(bmap_block.entry_num, bmap_block.bit_num);
	printf("\n Otteniamo la entry %d e il bit %d: la posizione %d", bmap_block.entry_num, bmap_block.bit_num, pos);

	DiskDriver d;
	BitMap bmap;
	if(test_globale){
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
	printf("\n Partiamo da %d e cerchiamo %d ==> %d", inizio, stato, BitMap_get(&bmap, inizio, stato));*/

	/*TEST DISKDRIVER
	printf("\n Test DiskDriver_init(), DiskDriver_getFreeBlock(), BitMap_get()");
	DiskDriver d;
	if(test_globale){
		DiskDriver_init(&d, "test/test.txt", 50);
	}
	else{
		char buffer[255];
		sprintf(buffer, "test/%d.txt", time(NULL));
		DiskDriver_init(&d, buffer, 50);
	}
	BitMap bmap;
	bmap.num_bits = d.header->bitmap_entries * 8;
	bmap.entries = d.bitmap_data;
	printf("BitMap creata e inizializzata correttamente");
	printf("\n Primo blocco libero ==> %d", d.header->first_free_block);
	
	printf("\n Test DiskDriver_writeBlock(), DiskDriver_flush()");
	printf("\n Prima ==> ");
	stampa(bmap.entries);
	printf("\n Risultato writeBlock (\"Ciao\", 4) è %d", DiskDriver_writeBlock(&d, "Ciao", 4));
	printf("\n Dopo ==> ");
	stampa(bmap.entries);
	
	printf("\n TestDiskDriver_readBlock()");
	void* ris = malloc(BLOCK_SIZE);
	printf("\n Usiamo readBlock(ris, 4) per controllare ==> %d", DiskDriver_readBlock(&d, ris, 4));
	printf("\n Dopo ReadBlock ris contiene ==> %s", (char*) ris);
	
	printf("\n Test DiskDriver_freeBlock()");
	printf("\n Prima ==> ");
	stampa(bmap.entries);
	printf("\n Libero blocco %d, con ritorno %d", 4, DiskDriver_freeBlock(&d, 4));
	printf("\n Dopo ==> ");
	stampa(bmap.entries);*/
	
	//TEST SIMPLEFS
	printf("\n Test SimpleFS_init(), SimpleFS_format()");
	SimpleFS fs;
	DiskDriver d;
	if(test_globale) {
		DiskDriver_init(&d, "test/test.txt", 50);
	}
	else{
		char buffer[255];
		sprintf(buffer, "test/%d.txt", time(NULL));
		DiskDriver_init(&d, buffer, 50);
	}
	DirectoryHandle* dh = SimpleFS_init(&fs, &d);
	if(dh != NULL){
		printf("\n File System creato e inizializzato");
	}
	else{
		printf("\n errore nella crezione del FS");
		return -1;
	}
	printf("\n BitMap ==> ");
	stampa(d.bitmap_data);
	
	printf("\n Test SimpleFS_createFile()");
	int i, nfile = 4;
	for(i = 0; i<nfile; i++){
		char buffer[255];
		sprintf(buffer, "prova_%d.txt", dh->dcb->num_entries);
		if(SimpleFS_createFile(dh, buffer) != NULL) {
			printf("\n File %s creato", buffer);
		}
		else{
			printf("\n Errore nella creazione di %s", buffer);
		}
	}
	printf("\n BitMap ==> ");
	stampa(d.bitmap_data);
	
	printf("\n Test SimpleFS_mkDir()");
	int res = SimpleFS_mkDir(dh, "pippo");
	printf("\n SimpleFS_mkDir(dh, \"pippo\") ==> %d", res);
	if(res == 0){
		printf("\n Cartella creata");
	}
	else{
		printf("\n Errore creazione cartella");
		return -1;
	}
	printf("\n BitMap ==> ");
	stampa(d.bitmap_data);
	
	printf("\n Test SimpleFS_readDir()");
	printf("\n Sono presenti %d elementi", dh->dcb->num_entries);
	char** elenco = malloc(dh->dcb->num_entries * 255);
	SimpleFS_readDir(elenco, dh);
	for(i = 0; i < dh->dcb->num_entries; i++){
		printf("\n    > %s", elenco[i]);
	}
	
	printf("\n Test SimpleFS_openFile()");
	char nome[255] = "prova.txt";
	FileHandle* fh = malloc(sizeof(FileHandle));
	fh = SimpleFS_openFile(dh, nome);
	res = fh == NULL ? -1 : 0;
	printf("\n SimpleFS_openFile(dh, \"%s\") ==> %d", nome, res);
	if(fh != NULL){
		printf("\n File aperto");
	}
	else{
		printf("\n Errore nell'apertura");
		return -1;
	}
	
	printf("\n Test SimpleFS_write()");
	FILE* ftest = fopen("prova.txt", "r");
	struct stat fileStat;
	stat("prova.txt", &fileStat);
	char stringa[fileStat.st_size];
	if(test_file){
		fread(stringa, sizeof(char), fileStat.st_size, test_file);
	}
	else{
		strcpy(stringa, "Prova di scrittura su file e adesso comincio a scrivere cose a caso per vedere se riesco ad aprire il file e a scriverlo e a salvarlo e a fare cose varie tramite le operazioni che ho fatto.");
	}
	res = SimpleFS_write(fh, stringa, strlen(stringa));
	printf("\n SimpleFS_write(fh, stringa, %zu) ==> %d", strlen(stringa), res);
	if(res == strlen(stringa)){
		printf("\n scrittura avvenuta con successo");
	}
	else{
		printf("\n Errore nella scrittura");
	}
	printf("\n BitMap ==> ");
	stampa(d.bitmap_data);
	
	// test write con pos != 0
	printf("\n Test SimpleFS_write()");
	SimpleFS_seek(fh, 10);
	res = SimpleFS_write(fh, "pippo pluto", 11);
	printf("\n SimpleFS_seek(fh, 10) ==> %d", SimpleFS_seek(fh, 10));
	printf("\n SimpleFS_write(fh, stringa, %d) ==> %d", 8, res);
	if(res == 11){
		printf("\n scrittura avvenuta con successo");
	}
	else{
		printf("\n Errore nella scrittura");
	}
	printf("\n BitMap ==> ");
	stampa(d.bitmap_data);
	
	printf("\n Test SimpleFS_read()");
	int size = fh->fcb->fcb.size_in_bytes;
	char dati[size];
	printf("\n SimpleFS_read(fh, dati, %d) restituisce: %d", size, SimpleFS_read(fh, dati, size));
	printf("\n Adesso \"dati\" contiene : %s", dati);
	
	printf("\n Test SimpleFS_changeDir()");
	printf("\n SimpleFs_changeDir(dh, \"pippo\") ==> %d", SimpleFS_changeDir(dh, "pippo"));
	printf("\n SimpleFs_changeDir(dh, \"..\") ==> %d", SimpleFS_changeDir(dh, ".."));
	printf("\n SimpleFs_changeDir(dh, \"..\") ==> %d", SimpleFS_changeDir(dh, ".."));
	
	printf("\n Test SimpleFS_close()");
	res = SimpleFS_close(fh);
	printf("\n SimpleFS_close(fh) ==> %d", res);
	if(res >= 0){
		printf("\n Chiusura effettuata");
	}
	else{
		printf("\n Errore nella chiusura del file");
		return -1;
	}
	SimpleFS_changeDir(dh, "pippo");
	for(i = 0; i < 500; i++){
		char nome[255];
		sprintf(nome, "%s%d%s", "pippo-", i, ".txt");
		SimpleFS_createFile(dh, nome);
	}
	printf("\n Aggiungo %d file in \"pippo\"", dh->dcb->num_entries);
	SimpleFS_changeDir(dh, "..");
	printf("\n BitMap ==> ");
	stampa(d.bitmap_data);
	
	printf("\n Test SimpleFS_remove() [cartella]");
	strcpy(nome, "pippo");
	res = SimpleFS_remove(&fs, nome);
	printf("\n SimpleFs_remove(fh, \"%s\") ==> %d", nome, res);
	if(res >= 0){
		printf("\n Cancellazione effettuata");
	}
	else{
		printf("\n Errore nella cancellazione");
		return -1;
	}
	printf("\n BitMap ==> ");
	stampa(d.bitmap_data);
	
	printf("\n Test SimpleFS_remove() [file]");
	strcpy(nome, "prova_2.txt");
	res = SimpleFS_remove(&fs, nome);
	printf("\n SimpleFS_remove(fh, \"%s\") ==> %d", nome, res);
	if(res >= 0){
		printf("\n Cancellazione effettuata");
	}
	else{
		printf("\n Errore nella cancellazione");
		return -1;
	}
	printf("\n BitMap ==> ");
	stampa(d.bitmap_data);
	printf("\n\n");
	
}
