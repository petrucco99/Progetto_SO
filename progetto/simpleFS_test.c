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

int test;
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
	printf("Cosa vuoi testare?   1=Bitmap	2=DiskDriver	3=SimpleFS\n");
	scanf("%d", &test);
	if(test == 1){
		//TEST BITMAP
		int num = 765;
		printf("\nTest blockToIndex(%d)", num);
		BitMapEntryKey bmap_block = BitMap_blockToIndex(num);
		printf("\nPosizione blocco = %d; entry %d al bit %d", num, bmap_block.entry_num, bmap_block.bit_num);

		printf("\n Test IndexToBlock");
		//printf("\n entrynum = %d, bitmun = %d", bmap_block.entry_num, bmap_block.bit_num);
		int pos = BitMap_indexToBlock(bmap_block.entry_num, bmap_block.bit_num);
		printf("\n Otteniamo la entry %d e il bit %d: la posizione %d", bmap_block.entry_num, bmap_block.bit_num, pos);
		//fflush(stdout);
		DiskDriver d; 
		memset(&d, 0, sizeof(DiskDriver));  
		//printf("prova\n");
		//fflush(stdout);
		BitMap bmap;
		//printf("prova\n");
		//fflush(stdout);   
		if(test_globale){
			//printf("prova2\n");
			//fflush(stdout);
			DiskDriver_init(&d, "test/test.txt", 50);
			//printf("prova2\n");
			//fflush(stdout);
		}
		else{
			//printf("prova1\n");
			//fflush(stdout);
			char buffer[255];
			//printf("prova1\n");
			//fflush(stdout);
			sprintf(buffer, "test/%ld.txt", time(NULL));
			//printf("prova1\n");
			//puts(buffer);
			//fflush(stdout);
			DiskDriver_init(&d, buffer, 50);
			//printf("prova1\n");
			//fflush(stdout);
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
	else if(test == 2){
		//TEST DISKDRIVER
		printf("\n Test DiskDriver_init(), DiskDriver_getFreeBlock(), BitMap_get()\n");
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
		stampa(bmap.entries);
	}
	else if(test == 3){
		//TEST SIMPLEFS
		// Test SimpleFS_init
		printf("\n+++ Test SimpleFS_init()");
		printf("\n+++ Test SimpleFS_format()");
		SimpleFS fs;  
		DiskDriver disk;
		if(test_globale) {
			DiskDriver_init(&disk, "test/test.txt", 50); 
		}else{
			char disk_filename[255];
			sprintf(disk_filename, "test/%d.txt", time(NULL));
			DiskDriver_init(&disk, disk_filename, 50); 
		}
		DirectoryHandle * directory_handle = SimpleFS_init(&fs, &disk);
		if(directory_handle != NULL) {
			printf("\n    File System creato e inizializzato correttamente");
		}else{
			printf("\n    Errore nella creazione del file system\n");
			return;
		}
		printf("\n    BitMap => ");
		stampa(disk.bitmap_data);

		// Test SimpleFS_createFile
		printf("\n\n+++ Test SimpleFS_createFile()");
		int i, num_file = 4;
		for(i = 0; i < num_file; i++) {
			char filename[255];
			sprintf(filename, "prova_%d.txt", directory_handle->dcb->num_entries);
			if(SimpleFS_createFile(directory_handle,filename) != NULL) {
				printf("\n    File %s creato correttamente", filename);
			}else{
				printf("\n    Errore nella creazione di %s", filename);
			}
		} 
		printf("\n    BitMap => ");
		stampa(disk.bitmap_data);

	 	// Test SimpleFS_mkDir
		printf("\n\n+++ Test SimpleFS_mkDir()");
		int ret = SimpleFS_mkDir(directory_handle, "pluto");
		printf("\n    SimpleFS_mkDir(dh, \"pluto\") => %d", ret);
		if(ret == 0) {
			printf("\n    Cartella creata correttamente");
		}else{
			printf("\n    Errore nella creazione della cartella\n");
			return;
		}
		printf("\n    BitMap => ");
		stampa(disk.bitmap_data);


	 	// Test SimpleFS_readDir
		printf("\n\n+++ Test SimpleFS_readDir()");
		printf("\n    Nella cartella ci sono %d elementi:", directory_handle->dcb->num_entries);
		char ** elenco2 = malloc(directory_handle->dcb->num_entries * 255);
		SimpleFS_readDir(elenco2, directory_handle);
		for(i = 0; i < directory_handle->dcb->num_entries; i++) {
			printf("\n    > %s", elenco2[i]);
		}

	 	// Test SimpleFS_openFile
		printf("\n\n+++ Test SimpleFS_openFile()");
		char nome_file[255] = "prova_1.txt";
		FileHandle * file_handle = malloc(sizeof(FileHandle));
		file_handle = SimpleFS_openFile(directory_handle, nome_file);
		ret = file_handle == NULL ? -1 : 0;
		printf("\n    SimpleFS_openFile(directory_handle, \"%s\") => %d", nome_file, ret);
		if(file_handle != NULL) {
			printf("\n    File aperto correttamente");
		}else{
			printf("\n    Errore nell'apertura del file\n");
			return;
		}
 
	 	// Test SimpleFS_write
		printf("\n\n+++ Test SimpleFS_write()");
		FILE * file_test = fopen("prova.txt", "r");
		struct stat fileStat;
		stat("prova.txt", &fileStat);
		char stringa[fileStat.st_size];
		if(test_file) {
			fread(stringa, sizeof(char), fileStat.st_size, file_test);
		}else{
			strcpy(stringa, "Nel mezzo del cammin di nostra vita mi ritrovai per una selva oscura ché la diritta via era smarrita. Ahi quanto a dir qual era è cosa dura esta selva selvaggia e aspra e forte che nel pensier rinova la paura! Tant'è amara che poco è più morte; ma per trattar del ben ch'i' vi trovai, dirò de l'altre cose ch'i' v'ho scorte. Io non so ben ridir com'i' v'intrai, tant'era pien di sonno a quel punto che la verace via abbandonai. Ma poi ch'i' fui al piè d'un colle giunto, là dove terminava quella valle che m'avea di paura il cor compunto, guardai in alto, e vidi le sue spalle vestite già deNel mezzo del cammin di nostra vita mi ritrovai per una selva oscura ché la diritta via era smarrita. Ahi quanto a dir qual era è cosa dura esta selva selvaggia e aspra e forte che nel pensier rinova la paura! Tant'è amara che poco è più morte; ma per trattar del ben ch'i' vi trovai, dirò de l'altre cose ch'i' v'ho scorte. Io non so ben ridir com'i' v'intrai, tant'era pien di sonno a quel punto che la verace via abbandonai. Ma poi ch'i' fui al piè d'un colle giunto, là dove terminava quella valle che m'avea di paura il cor compunto, guardai in alto, e vidi le sue spalle vestite già d");
			//strcpy(stringa, "Nel mezzo del cammin");
		}
		ret = SimpleFS_write(file_handle, stringa, strlen(stringa));
		printf("\n    SimpleFS_write(file_handle, stringa, %zu) => %d", strlen(stringa), ret);
		if(ret == strlen(stringa)) {
			printf("\n    Scrittura avvenuta correttamente");
		}else{
			printf("\n    Errore nella scrittura del file\n");
		}
		printf("\n    BitMap => ");
		stampa(disk.bitmap_data);  

		// Test SimpleFS_write() con pos != 0
		printf("\n\n+++ Test SimpleFS_write()");
		SimpleFS_seek(file_handle, 12);
		ret = SimpleFS_write(file_handle, "i viaggi", 8);
		printf("\n    SimpleFS_seek(file_handle, 12) => %d", SimpleFS_seek(file_handle, 12));
		printf("\n    SimpleFS_write(file_handle, stringa, %d) => %d", 8, ret);
		if(ret == 8) {
			printf("\n    Scrittura avvenuta correttamente");
		}else{
			printf("\n    Errore nella scrittura del file\n");
		}
		printf("\n    BitMap => ");
		stampa(disk.bitmap_data);

	 	// Test SimpleFS_read
		printf("\n\n+++ Test SimpleFS_read()");
		int size = file_handle->fcb->fcb.size_in_bytes;
		char data[size];
		printf("\n    SimpleFS_read(file_handle, data, %d) ha restituito: %d", size, SimpleFS_read(file_handle, data, size));
		printf("\n    Adesso \"data\" contiene: %s", data);

		// Test SimpleFS_changeDir
		printf("\n\n+++ Test SimpleFS_changeDir()");
		printf("\n    SimpleFS_changeDir(directory_handle, \"pluto\") => %d", SimpleFS_changeDir(directory_handle, "pluto"));
		printf("\n    SimpleFS_changeDir(directory_handle, \"..\")    => %d", SimpleFS_changeDir(directory_handle, ".."));
		printf("\n    SimpleFS_changeDir(directory_handle, \"..\")    => %d", SimpleFS_changeDir(directory_handle, ".."));

		// Test SimpleFS_seek
		printf("\n\n+++ Test SimpleFS_seek()");
		int pos = 10;
		ret = SimpleFS_seek(file_handle, pos);
		printf("\n    SimpleFS_seek(file_handle, %d) => %d", pos, ret);
		if(ret == pos) {
			printf("\n    Spostamento del cursore avvenuto correttamente");
		}else{
			printf("\n    Errore nello spostamento del cursore\n");
			return;
		}
  
		// Test SimpleFS_close
		printf("\n\n+++ Test SimpleFS_close()");
		ret = SimpleFS_close(file_handle);
		printf("\n    SimpleFS_close(file_handle) => %d", ret);
		if(ret >= 0) {
			printf("\n    Chiusura del file avvenuta correttamente");
		}else{
			printf("\n    Errore nella chiusura del file\n");
			return;
		}
		
		SimpleFS_changeDir(directory_handle, "pluto"); 
		for(i=0; i<500; i++) {
			char nomello[255];
			sprintf(nomello, "%s%d%s", "pluto_", i, ".txt");
			SimpleFS_createFile(directory_handle, nomello);
		}
		printf("\n\n    Ho aggiunto %d files in \"pluto\"", directory_handle->dcb->num_entries);
		SimpleFS_changeDir(directory_handle, "..");
		printf("\n    BitMap => "); 
		stampa(disk.bitmap_data); 
   
		// Test SimpleFS_remove 
		printf("\n\n+++ Test SimpleFS_remove() [cartella]");
		strcpy(nome_file, "pluto"); 
		//printf("\n\n\nINIZIO");
		//fflush(stdout);
		ret = SimpleFS_remove(&fs, nome_file);
		//printf("\n\n\nFINE"); 
		//fflush(stdout); 
		printf("\n    SimpleFS_remove(file_handle, \"%s\") => %d", nome_file, ret);
		if(ret >= 0) {
			printf("\n    Cancellazione del file avvenuta correttamente");
		}else{
			printf("\n    Errore nella cancellazione del file\n");
			return;
		}
		printf("\n    BitMap => ");
		stampa(disk.bitmap_data);
	}
	printf("\n\n");
}
