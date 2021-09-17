#include "bitmap.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>


/*Con in ingresso il parametro "num" che prende il valore del 
 * blocco nellq memoria; lo convertiamo nei valori che rappresentano 
 * l'indice dell'entry e il suo spiazzamento */
BitMapEntryKey BitMap_blockToIndex(int num){
	BitMapEntryKey box;
	box.entry_num = num/8;
	box.bit_num = num%8;
	return box;
}

/*Convertiamo l'indice dell'entry e il suo spiazzamento in un
 * intero che rappresenta la sua posizione in memoria*/
int BitMap_indexToBlock(int entry, uint8_t bit_num){
	return (entry*8)+bit_num;
}

/* inizio a cercare da start nella bitmap "bmap" fino a trovare
 * l'indice del primo bit con stato "status"*/
int BitMap_get(BitMap* bmap, int start, int status){
	if(start>bmap->num_bits) return -1;
	
	int p, i, res;
	
	for(i=start; i<=bmap->num_bits; i++){
		BitMapEntryKey chiave = BitMap_blockToIndex(i);
		res = (bmap->entries[chiave.entry_num] &(1<<(7-chiave.bit_num));
		
		if(status==1){
			if(res>0) return i;
		}
		else{
			if(res==0) return i;
		}
	}
}

//impostiamo lo start della bitmap con "pos" con lo stato 
//appropriato con "status"

int BitMap_set(BitMap* bmap, int pos, int status){
	if(pos>bmap->num_bits) return -1;
	
	BitMapEntryKey chiave = BitMap_blockToIndex(pos);
	uint8_t maschera = 1<<(7-chiave.bit_num);
	
	if(status) bmap->entries[chiave.entry_num] |=maschera;
	else bmap->entries[chiave.entry_num] &= ~(maschera);
	return status;
}


