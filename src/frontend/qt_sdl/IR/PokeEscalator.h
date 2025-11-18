
#ifndef POKEESCALATOR_H
#define POKEESCALATOR_H

#include <stdio.h>
#include <stdint.h>
//returns length of return WtG data
//fh -> filehandle to eeprom
// TxLen - > length of packet sent to walker
// TxData -> pointer to data sent to walker
// RxData -> buffer that the walker will fill
uint16_t txToWalker(FILE * fh, char TxLen, char * TxData, char * RxData);


//Some structs from mambas picowalker


#endif

