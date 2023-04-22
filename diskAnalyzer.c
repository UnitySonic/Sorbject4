#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/user.h>
#define _GNU_SOURCE



#define SECTOR_SIZE  512
#define DIRECTORY_ENTRY_SIZE 32
#define NUMBER_OF_FAT_ENTRIES 3072
#define MAX_LENGTH_NEW_DIRECTORY 10
#define MAX_LENGTH_FILE 13
#define NUM_OF_SECTORS 2880
#define DIRECTORY_ENTRIES_PER_SECTOR 16

//offsets

#define EXTENSION_OFFSET 8
#define ATTRIBUTES_OFFSET 11
#define FIRST_CLUSTER_OFFSET 26
#define FILESIZE_OFFSET 28

//masks
#define DELETED_ENTRY_BYTE 0xE5
#define FILENAME_PADDING_BYTE 0x20
#define FREE_ENTRY_BYTE  0x00

#define DIRECTORY_ATTRIBUTE_MASK 0x10
#define ROOT_DIRECT_START_SECTOR 19



unsigned char * disk;

int * FAT_TABLE;
typedef struct FATPair{
    int Entry1;
    int Entry2;

}FATPair_t;


FATPair_t  getFatEntry(unsigned char* firstByte, int i);
void loadFatTable(unsigned char * first);
void scanDirectoryEntry(char * filePath, unsigned char * DirectoryEntry, int directoryEntryNum);
int scanFileClusters(int FirstLogicalCluster, int FileSize, char * filePath);





/**
 * TODOLIST
 * 1. Go to scanDirectoryEntry and flesh out the elseif block that deals with deleted directory entries
 * 2. implement scanFileClusters(or your own function iguess). This function needs to read and write file cluster data
 *    into files in the specified output directory. i.e. if in directoryscan you find a file named A.txt, you're going
 *    to create a file called A.txt in the output directory. Then you'll copy over all the file cluster bytes into A.txt. Basically
 *    you're reconstructing the files from the FAT12 image into an actual usable file on the computer.
 *
 * Notes. I am really not proud of how I did scanDirectoryEntry. Feel free to rewrite it if you've got the time and you've done the above.
*/




int main(int argc, char * argv[])
{
    int fd;

    if(argc != 3)
    {
        fprintf(stderr, "Incorrect number of arguments! I'm outta here");
        exit(0);
    }


    fd = open(argv[1], O_RDONLY);

    disk = mmap ( NULL , SECTOR_SIZE * NUM_OF_SECTORS ,
    PROT_READ ,
    MAP_PRIVATE  , fd , 0) ;

    loadFatTable(disk + SECTOR_SIZE);
    unsigned char * StartOfRoot = disk +(SECTOR_SIZE * ROOT_DIRECT_START_SECTOR);
    scanDirectoryEntry("/", StartOfRoot,0);

}




void loadFatTable(unsigned char * first)
{
    FAT_TABLE = malloc(sizeof(int) * NUMBER_OF_FAT_ENTRIES);

    unsigned char * currentByteInFat = first;

    for(int i = 0; i < NUMBER_OF_FAT_ENTRIES; i = i+2)
    {
        FATPair_t currentPair = getFatEntry(currentByteInFat, i);
        FAT_TABLE[i] = currentPair.Entry1;
        FAT_TABLE[i+1] = currentPair.Entry2;
        currentByteInFat = currentByteInFat + 3;
    }

}

FATPair_t  getFatEntry(unsigned char * first, int i)
{
    unsigned char * second = first + 1;
    unsigned char * third = first + 2;

    int firstByte = (int) (*first);
    int secondByte = (int)(*second);
    int thirdByte = (int)(*third);

    int firstEntry = firstByte | ( (secondByte & 0x0F) << 8);
    int secondEntry = (thirdByte << 8) | ((secondByte & 0xF0) >> 4);

    FATPair_t pair;
    pair.Entry1 = firstEntry;
    pair.Entry2 = secondEntry;

    return pair;
}




void scanDirectoryEntry(char * filePath, unsigned char * DirectoryEntry, int DirectoryEntryNum)
{
    //Here we get Information About the Current DirectoryEntry
    unsigned char * FILENAME = DirectoryEntry;
    unsigned char * EXTENSION = DirectoryEntry + EXTENSION_OFFSET;


    unsigned char ATTRIBUTES = *((unsigned char*)(DirectoryEntry + ATTRIBUTES_OFFSET));
    short FIRST_LOGICAL_CLUSTER = *((short*)(DirectoryEntry + FIRST_CLUSTER_OFFSET));
    int FILESIZE = *((int*)(DirectoryEntry + FILESIZE_OFFSET));


    if((int)(*FILENAME) == FREE_ENTRY_BYTE)
    {
        //Free Entry and everything else is free. Do nothing
        return;
    }

    else if(((int)(*FILENAME)) == DELETED_ENTRY_BYTE)
    {
        //Put your Deleted Logic Here
    }


    else if((((int) ATTRIBUTES) & DIRECTORY_ATTRIBUTE_MASK) == DIRECTORY_ATTRIBUTE_MASK)
    {
        //Appending the subdirectory to the filePath parameter
        int len = 0;
        char  * newDirectory = malloc(sizeof(char) * MAX_LENGTH_NEW_DIRECTORY);
        for(int i = 0; i < 8; i++)
        {
            if(((int)(*FILENAME)) == FILENAME_PADDING_BYTE)
            {
                newDirectory[i] = '/';
                newDirectory[i+1] = '\0';
                newDirectory = realloc(newDirectory, sizeof(char) *(i+2));
                break;
            }
            newDirectory[i] = *( (char*) (FILENAME));
            FILENAME++;
            if(i == 7)
            {
                newDirectory[8] = '/';
                newDirectory[9] = '\0';
            }
            len++;
        }


        int lengthOfFilePath = strlen(filePath);
        char * newFilePath = malloc(sizeof(char) * (lengthOfFilePath + len));
        strcpy(newFilePath,filePath);
        strcat(newFilePath,newDirectory);

        //Looks like magic but this is the formula to find the sector of a logical cluster
        int sectorNumber = 33 + FIRST_LOGICAL_CLUSTER - 2;
        unsigned char * sectorToScan = disk + ((SECTOR_SIZE*sectorNumber) + (DIRECTORY_ENTRY_SIZE *2)) ;

        scanDirectoryEntry(newFilePath, sectorToScan,0);
    }
    else
    {
        //yeah so this is just
        char  * newDirectory = malloc(sizeof(char) * MAX_LENGTH_FILE);
        int nextPosInString = 0;
        for(int i = 0; i < 8; i++)
        {

            if(((int)(*(FILENAME + i))) == FILENAME_PADDING_BYTE)
                break;
            newDirectory[i] = *( (char*) (FILENAME+i));
            nextPosInString++;
        }
        newDirectory[nextPosInString] = '.';
        nextPosInString++;

        for (int i = 0; i < 3; i++)
        {
            if(((int)(*(EXTENSION + i))) == FILENAME_PADDING_BYTE)
                break;
            newDirectory[nextPosInString] = *( (char*) (EXTENSION+i));
            nextPosInString++;
        }
        newDirectory[nextPosInString] = '\0';
        newDirectory = realloc(newDirectory, (sizeof(char) * (nextPosInString + 1)));



        int lengthOfFilePath = strlen(filePath);
        char * newFilePath = malloc(sizeof(char) * (lengthOfFilePath + nextPosInString+1));
        strcpy(newFilePath,filePath);
        strcat(newFilePath,newDirectory);
        //scanFileClusters(FIRST_LOGICAL_CLUSTER, FILESIZE, newFilePath);

        printf("FILE\tNORMAL\t%s\t%d\n", newFilePath, FILESIZE);


    }

    if(DirectoryEntryNum != DIRECTORY_ENTRIES_PER_SECTOR -1)
        scanDirectoryEntry(filePath, DirectoryEntry+32, DirectoryEntryNum++);
}








