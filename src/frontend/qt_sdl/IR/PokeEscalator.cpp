#include "PokeEscalator.h"

#include <stdint.h>
#include <stdio.h>
#include <cstring>


#define SKIP_HEADER 8


int pw_decompress_data(uint8_t *data, uint8_t *buf, size_t dlen);
void writeByte(FILE * fh, uint16_t offset, uint16_t byte, size_t count);
void writeEeprom(FILE * fh, uint16_t offset, void * dat, size_t len);
uint8_t eeprom_check(uint8_t * buf, size_t len);
uint16_t pw_ir_checksum_seeded(uint8_t *data, size_t len, uint16_t seed);
void eeprom_reliable_write(FILE * fh, uint16_t off1, uint16_t off2, uint8_t * buf, size_t len);
void readEeprom(FILE * fh, uint16_t offset, void * dat, size_t len);

uint8_t dataBuf[0xb8];
uint8_t decompBuf[0xb8];
uint8_t writeBuf[0xb8];



uint8_t eeprom_check(uint8_t * buf, size_t len){
    uint8_t chk = 1;
    for (size_t i = 0; i < len; i++) chk+= buf[i];
    return chk;
}



void eeprom_reliable_write(FILE * fh, uint16_t off1, uint16_t off2, uint8_t * buf, size_t len){
        uint8_t chk = eeprom_check(buf, len);
        writeEeprom(fh, off1, buf, len);
        writeByte(fh, off1+len, chk, 1); //checksum is always 1 byte immediately following the data
        writeEeprom(fh, off2, buf, len);
        writeByte(fh, off2+len, chk, 1); //checksum is always 1 byte immediately following the data
}


//mamba
uint16_t pw_ir_checksum_seeded(uint8_t *data, size_t len, uint16_t seed) {
    // Dmitry's palm
    uint32_t crc = seed;
    for(size_t i = 0; i < len; i++) {
        uint16_t v = data[i];
        if(!(i&1)) v <<= 8;
        crc += v;
    }
    while(crc>>16) crc = (uint16_t)crc + (crc>>16);
    return crc;
}


void readEeprom(FILE * fh, uint16_t offset, void * dat, size_t len){
    fseek(fh, offset, SEEK_SET);
    fread(dat, 1, len, fh);
}
//https://github.com/mamba2410/picowalker-core/blob/core/src/ir/compression.c

/*
 *  data is packet minus 8-byte header
 *  Assume buf can hold decompressed data
 */

//MODIFY to accpet a whole packet
void writeEeprom(FILE * fh, uint16_t offset, void * dat, size_t len){
    if (fseek(fh, offset, SEEK_SET) !=0) perror("fseek failed");
    if (fwrite(dat, 1, len, fh) != len) perror("fwrite failed");
    //printf("%04hx ", offset);
    //fflush(fh);
    //printf("ftell end %04lx\n", ftell(fh));
}


void writeByte(FILE * fh, uint16_t offset, uint16_t byte, size_t count){

    if (fseek(fh, offset, SEEK_SET) !=0) perror("fseek failed");

    uint8_t dat[1];
    dat[0] = byte;  //

    for (size_t i = 0; i < count; i++) fwrite(dat, 1, 1, fh); //this is so me rn ->    https://lkml.org/lkml/2012/7/6/495


}


//mamba
int pw_decompress_data(uint8_t *data, uint8_t *buf, size_t dlen) {
    if(data == 0 || buf == 0) return -1;
    size_t c = 0;
    size_t oc = 0;
    uint8_t decomp_type = data[c++];
    if(decomp_type != 0x10)
        printf("BAD");
        return -1;
    // LE size
    uint32_t decomp_size = data[c] | data[c+1] << 8 | data[c+2] << 16;
    if(decomp_size != 128)
        return -1;
    c += 3;
    while(c < dlen) {
        // loop through header
        uint8_t header = data[c++];
        for(uint8_t chunk_idx = (1<<7); chunk_idx > 0 && oc < decomp_size; chunk_idx >>= 1) {
            uint8_t cmd = header & chunk_idx;

            if(cmd != 0) {
                // 2-byte backreference
                uint8_t sz = (data[c]>>4) + 3;
                uint16_t backref = (( (data[c]&0x0f) << 8) | (data[c+1] )) + 1;
                c += 2;

                if(backref > oc) return -1;

                size_t start = oc-backref;
                size_t end   = start + sz;

                //if( end > decomp_size) return -1;

                for(size_t i = start; i < end; i++, oc++)
                    buf[oc] = buf[i];

            } else {
                // 1-byte raw data
                buf[oc++] = data[c++];
            }
        }
    }

    printf("decomp size: %02zx\n", oc);
    if(oc != decomp_size) return -1;
    return 0;
}


//TO DO complete reliables
void walk_start(FILE * fh){
        writeByte(fh, 0x016f, 0xa5, 1);
        writeByte(fh, 0x026f, 0xa5, 1);

        for (size_t i = 0; i < 0x2900; i+= 128){
            readEeprom(fh, 0xd700 + i, dataBuf, 128);
            writeEeprom(fh, 0x8f00 + i, dataBuf, 128);

        }
         for (size_t i = 0; i < 0x280; i+= 128){
            readEeprom(fh, 0xd480 + i, dataBuf, 128);
            writeEeprom(fh, 0xcc00 + i, dataBuf, 128);
        }
        writeByte(fh, 0x016f, 0x00, 1);
        writeByte(fh, 0x026f, 0x00, 1);



        //Healt hData: 0x0156 and 0x0256

        writeByte(fh, 0x0156 + 0x10, 0x00, 1);
        writeByte(fh, 0x0156 + 0x13, 0x00, 1);
        writeByte(fh, 0x0156 + 0x0e, 0x00, 1);

        writeByte(fh, 0x0256 + 0x10, 0x00, 1);
        writeByte(fh, 0x0256 + 0x13, 0x00, 1);
        writeByte(fh, 0x0256 + 0x0e, 0x00, 1);


        //Zero some things
        writeByte(fh, 0xcf0c, 0x00, 3264);
        writeByte(fh, 0xde24, 0x00, 0x1568);
        writeByte(fh, 0xce8c, 0x00, 0x64);


        //cc00 is teamData



       //Read Struct TeamData
        readEeprom(fh, 0xcc00, dataBuf, 128);


        /* !!!OLD WORKING */
        //Copy to proper UniqueIdentityData and IdentityData Fields
        writeEeprom(fh, 0x083, dataBuf + 8, 0x28);
        writeEeprom(fh, 0x183, dataBuf + 8, 0x28);
        writeEeprom(fh, 0x00ed + 0x10, dataBuf + 8, 0x28);
        writeEeprom(fh, 0x01ed + 0x10, dataBuf + 8, 0x28);


        //Copy in trainerID, sID

        writeEeprom(fh, 0x0ed + 0x0c, dataBuf + 8 + 0x28, 2); //tid
        writeEeprom(fh, 0x01ed + 0x0c, dataBuf + 8 + 0x28, 2); //tid


        writeEeprom(fh, 0x0ed + 0x0e, dataBuf + 8 + 0x28 + 2, 2); //sid
        writeEeprom(fh, 0x1ed + 0x0e, dataBuf + 8 + 0x28 + 2, 2); //sid

        //Copy in trainer name
        writeEeprom(fh, 0x0ed + 0x48, dataBuf + 8 + 0x28 + 2 + 2 + 4, 16); //trainer name




        writeEeprom(fh, 0x1ed + 0x48, dataBuf + 8 + 0x28 + 2 + 2 + 4, 16); //trainer name
        //??? flag
        writeByte(fh, 0x00ed, 0x00, 4);
        writeByte(fh, 0x00ed + 0x00, 0x01, 1); //unk0
        writeByte(fh, 0x00ed + 0x04, 0x01, 1); //unk1
        writeByte(fh, 0x00ed + 0x08, 0x08, 1); //unk2
        writeByte(fh, 0x00ed + 0x0A, 0x08, 1); //unk3

        writeByte(fh, 0x00ed + 0x5b, 0x03, 1); //flags
        writeByte(fh, 0x00ed + 0x5c, 0x00, 1); //protover
        writeByte(fh, 0x00ed + 0x5f, 0x02, 1); //unk8


        writeByte(fh, 0x01ed, 0x00, 4);
        writeByte(fh, 0x01ed + 0x00, 0x01, 1); //unk0
        writeByte(fh, 0x01ed + 0x04, 0x01, 1); //unk1
        writeByte(fh, 0x01ed + 0x08, 0x08, 1); //unk2
        writeByte(fh, 0x01ed + 0x0A, 0x08, 1); //unk3

        writeByte(fh, 0x01ed + 0x5b, 0x03, 1); //flags
        writeByte(fh, 0x01ed + 0x5c, 0x00, 1); //protover
        writeByte(fh, 0x01ed + 0x5f, 0x02, 1); //unk8


        //Checks
        readEeprom(fh, 0x00ed, dataBuf, 128);
        writeByte(fh, 0x00ed + 0x68, eeprom_check(dataBuf, 0x68), 1);
        readEeprom(fh, 0x01ed, dataBuf, 128);
        writeByte(fh, 0x01ed + 0x68, eeprom_check(dataBuf, 0x68), 1);


}


void walker_erase(FILE * fh){
    //I should probably clear more than this
    writeByte(fh, 0x00ed + 0x5b, 0x00, 1);


    writeByte(fh, 0x00ed, 0x00, 0x10);
    //Not unique IDENTITY data rn. picowalker-core/src/eeprom.c ~ line 140
    writeByte(fh, 0x00ed + 0x38, 0x00, 0x30);
    writeByte(fh, 0x00ed + 0x5f, 0x02, 1); //unk8 THIS NEEDS TO STILL BE 0x02




    //Game does NOT CLEAR UNIQUE IDENTITY DATA
    writeByte(fh, 0x01ed, 0x00, 0x10);
    writeByte(fh, 0x01ed + 0x38, 0x00, 0x30);
    writeByte(fh, 0x01ed + 0x5f, 0x02, 1); //unk8. THIS NEEDS TO STILL BE 0x02

    //Populate checksums
    readEeprom(fh, 0x00ed, dataBuf, 128);
    writeByte(fh, 0x00ed + 0x68, eeprom_check(dataBuf, 0x68), 1);

    readEeprom(fh, 0x01ed, dataBuf, 128);
    writeByte(fh, 0x01ed + 0x68, eeprom_check(dataBuf, 0x68), 1);




    //if clear steps flag
    writeByte(fh, 0xce80, 0x00, 0xd4c);
    //else
    writeByte(fh, 0xce8c, 0x00, 0x64); //Caught summary
    writeByte(fh, 0xcf0c, 0x00, 3264); //Event log
    writeByte(fh, 0xcef0, 0x00, 28); //Historic step count
    //endif



    //if clear events flag
    writeByte(fh, 0xb800, 0x00, 0x6c8); //recieved something
    //endif


    writeByte(fh, 0xde24, 0x00, 0x1568); //peers


        //Seems to be that the walker actually reads the team data at 0xcc00? NOT TRUE
        //writeByte(fh, 0xcc00, 0x00, 0x224);



        /*
        for (int i = 0; i < 0x68; i++) writeBuf[i] = 0xff;
        writeEeprom(fh, 0x0083, writeBuf, 0x28);
        writeEeprom(fh, 0x0183, writeBuf, 0x28);
        for (int i = 0; i < 0x68; i++) writeBuf[i] = 0x00;
        writeEeprom(fh, 0x00ED, writeBuf, 0x68);
        writeEeprom(fh, 0x01ED, writeBuf, 0x68);
        for (int i = 0; i < 0x68; i++) writeBuf[i] = 0xff;
        writeEeprom(fh, 0x00ED+ 0x10, writeBuf, 0x28);
        */
}




void walk_end(FILE * fh){

    writeByte(fh, 0x00ED + 0x04, 0x00, 1);
    writeByte(fh, 0x00ED + 0x0a, 0x00, 1);

    uint8_t flagBuf[1];
    readEeprom(fh, 0x00ED + 0x5b, flagBuf, 1);
    writeByte(fh, 0x00ED + 0x5b, flagBuf[0] & ~(1<<1), 1); //Clear poke flag

    writeByte(fh, 0x01ED + 0x04, 0x00, 1);
    writeByte(fh, 0x01ED + 0x0a, 0x00, 1);

    readEeprom(fh, 0x01ED + 0x5b, flagBuf, 1);
    writeByte(fh, 0x01ED + 0x5b, flagBuf[0] & ~(1<<1), 1); //Clear poke flag


    //Checks
    readEeprom(fh, 0x00ed, dataBuf, 128);
    writeByte(fh, 0x00ed + 0x68, eeprom_check(dataBuf, 0x68), 1);
    readEeprom(fh, 0x01ed, dataBuf, 128);
    writeByte(fh, 0x01ed + 0x68, eeprom_check(dataBuf, 0x68), 1);


    writeByte(fh, 0xce8c, 0x00, 0x64); //Caught summary
    writeByte(fh, 0xcf0c, 0x00, 3264); //Event log
    writeByte(fh, 0xb800, 0x00, 0x6c8); //recieved something
    writeByte(fh, 0xde24, 0x00, 0x1568); //peers
    writeByte(fh, 0x8f00, 0x00, 0x10); //route info
}




//Returns W2G_len
uint16_t txToWalker(FILE * fh, char G2W_len, char * G2W_data, char * w2g){

    //This is a fake packet which means the game is requesting an advertisement
    if (G2W_len == 0){
        w2g[0] = 0xFC ^ 0xAA;
        return 0x01;
    }

    //Decrypt
    for (int i = 0; i < (uint8_t) G2W_len ; i++) G2W_data[i] = (uint8_t) G2W_data[i] ^ 0xaa;


    uint8_t CMD = (uint8_t) G2W_data[0];
    uint8_t * wp = (uint8_t *) w2g + 8;


    //Prepare basics of the next packet.
    //Clear Checksum field

    uint16_t len = 8;
    w2g[1] = 0x02; //usualli
    w2g[2] = 0x00;
    w2g[3] = 0x00;
    //Set SessionID.
    if (CMD == 0xfa){ w2g[4] = 0xDE; w2g[5] = 0xAD; w2g[6] = 0xBE; w2g[7] = 0xEF;}
    else{ w2g[4] = G2W_data[4]; w2g[5] = G2W_data[5]; w2g[6] = G2W_data[6]; w2g[7] = G2W_data[7];}


    //START HANDLING PACKETS


    //Now we can simply handle packets
    if (CMD == 0xfa){
        //Just send back zeros as sessionid
        w2g[0] = 0xf8;
    }
    //Identity Data Request
    else if (CMD == 0x20){
        w2g[0] = 0x22;
        readEeprom(fh, 0x00ed, dataBuf, 0x68);

        for (int i = 0; i < 0x68; i++) w2g[i+8] = (uint8_t) dataBuf[i];

        //for (int i = 0; i < 0x28; i++) w2g[i+8+0x10] = 0xff;

        len = 0x68 + 8;
    }


    //Sends Identity Data. Responds with no data. TODO write
    else if (CMD == 0x32 || CMD == 0x40){ //40 is an alias for 32
        w2g[0] = 0x34;
        if (CMD == 0x40) w2g[0] = 0x42;
        //id data is stored @ 0x0ed and 0x1ed realiably. Which to use?

        //writeEeprom(fh, 0x00ed, G2W_data + 8, 0x68);
        //writeEeprom(fh, 0x01ed, G2W_data + 8, 0x68);
    }

    //Eeprom read req
    else if (CMD == 0x0c){
        w2g[0] = 0x0e;
        //Game sends 16 bit start addy, then a length
        uint16_t addy = (uint16_t) G2W_data[8] * 256 - G2W_data[9]; //I spend 5 hours figuring out that the address here is the end address of the read not the start
        uint8_t len_tmp = G2W_data[10];
        readEeprom(fh, addy, dataBuf, 0xb8);
        for (int i = 0; i < len_tmp; i++) w2g[i+8] = dataBuf[i];
        len = len_tmp + 8;
    }


    //Direct Writes
     else if (CMD == 0x02 || CMD == 0x82){
        //Respond
        uint8_t addr = G2W_data[1];
        w2g[0] = 0x04;
        w2g[1] = addr;

        uint16_t offset = addr << 8 | (CMD & 0x80);
//        if (offset > 0x8e80) offset -= 0x4800; undoing this will avoid the staging area


        writeEeprom(fh, offset, G2W_data + 8, 128); //No need to copy, the data is already here
    }

    //Compressed Writes.
    else if (CMD == 0x00 || CMD == 0x80){
        //Respond
        uint8_t addr = G2W_data[1];
        w2g[0] = 0x04;

        uint16_t offset = addr << 8 | (CMD & 0x80);
//        if (offset > 0x8e80) offset -= 0x4800; undoing this will avoid the staging area


        int res = pw_decompress_data((uint8_t * ) G2W_data + 8, decompBuf, len - 8);
        writeEeprom(fh, offset, decompBuf, 128);
    }


    //Ping. Respond with PONG
    else if (CMD == 0x24){
        w2g[0] = 0x26;
    }


    //Walk Start. Why does it respond with walk start. I think this packet is ignored DS side


    /*
        needs to clear byte at 0xb800. write 0xa5 to 0x016F.
        0x2900 bytes are copied from 0xd700 to 0x8400

        0x280 bytes are copied from 0xd480 to 0xcc00
        write 0x00 to 0x016f

        erase 0x1568 bytes at 0xde24

        ??more flags are shuffled in preparation for walk and a log entry is added??

        printf("1\n");
        printf("1\n");

       */


    //Walk start that occurs after pairing
    else if (CMD == 0x38){
        w2g[0] = 0x38;
        walk_start(fh);
   }



    //Immediate Disconnect, no reply.
    else if (CMD == 0xf4){
        if (fclose(fh) != 0) perror("fclose failed");
        else printf("CLOSED EEPROM\n");
    }

    //ERASE WALKER
    else if (CMD == 0x2a){
        w2g[0] = 0x2a;
        len = 0x28;
        walker_erase(fh);
   }


    //This is walk start after the game has already been paired
    else if (CMD == 0x5a){
        w2g[0] = 0x5a;
        walk_start(fh);

    }


    //Identity data sent G2W. Part of 'recieve a gift'
    else if (CMD == 0x60){
        w2g[0] = 0x62;

    }

    //Part of connection end for 'recieve a gift'. Unkown if I have to do anything else. Probably clear watts or steps
    else if (CMD == 0x66){
        w2g[0] = 0x68;

    }


    else if (CMD == 0x52){
        w2g[0] = 0x54;


        //writeEeprom(fh, 0x0ed, G2W_data + 8, 0x68);
    }


    //END WALK
    else if (CMD == 0x4e){
        w2g[0] = 0x50;
        walk_end(fh);
    }

    //----------------------



    uint16_t crc = pw_ir_checksum_seeded((uint8_t *) w2g, len, 0x02);
    w2g[2] = crc & 0xff;
    w2g[3] = (crc & 0xff00) >> 8;



    //Re-encrypt
    for (int i = 0; i < len; i++) w2g[i] = w2g[i] ^ 0xaa;
    return len;

}
