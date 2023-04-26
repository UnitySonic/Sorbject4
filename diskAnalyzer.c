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
#include <assert.h>

/*
 * Definitions, global variables, and structures.
 */

#define MODE                            777
#define SECTOR_SIZE                     512
#define NUM_OF_SECTORS                  2880
#define MAX_LENGTH_FILE                 13
#define FREE_ENTRY_BYTE                 0x00
#define FILESIZE_OFFSET                 28
#define EXTENSION_OFFSET                8
#define ATTRIBUTES_OFFSET               11
#define DELETED_ENTRY_BYTE              0xE5
#define FIRST_CLUSTER_OFFSET            26
#define NUM_LOGICAL_CLUSTERS            2848
#define DIRECTORY_ENTRY_SIZE            32
#define FILENAME_PADDING_BYTE           0x20
#define NUMBER_OF_FAT_ENTRIES           3072
#define MAX_LENGTH_NEW_DIRECTORY        10
#define DIRECTORY_ATTRIBUTE_MASK        0x10
#define ROOT_DIRECT_START_SECTOR        19
#define DIRECTORY_ENTRIES_PER_SECTOR    16

int* fatTable;
int fileNumber;
unsigned char* disk;
char* outputDirectory;

typedef struct fat_pair {
    int Entry1;
    int Entry2;
} fat_pair_t;

typedef struct file_pair {
    char* fullFilePath;
    char* fileName;
} file_pair_t;

/*
 * Methods.
 */

fat_pair_t getFatEntry(unsigned char const* firstByte);

void loadFatTable(unsigned char* first);

void scanDirectoryEntry(char* filePath, unsigned char* directoryEntry, int directoryEntryNum, int currentLogicalCluster);

void scanFileClusters(int firstLogicalCluster, int fileSize, FILE* filePath);

unsigned char* logicalClusterToPointer(int logicalCluster);

file_pair_t appendToFilePath(char const* filePath, unsigned char const* fileName, unsigned char const* extension);

char* appendDirectoryToFilePath(char const* filePath, unsigned char const* fileName);

/*
 * Method implementations.
 */

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Incorrect number of arguments! I'm outta here");
        exit(0);
    }

    int fd = open(argv[1], O_RDONLY);
    disk = mmap(NULL, SECTOR_SIZE * NUM_OF_SECTORS, PROT_READ, MAP_PRIVATE, fd, 0);
    outputDirectory = malloc(strlen(argv[2]));

    strcpy(outputDirectory, argv[2]);
    mkdir(argv[2], MODE);
    loadFatTable(disk + SECTOR_SIZE);
    unsigned char* startOfRoot = disk + (SECTOR_SIZE * ROOT_DIRECT_START_SECTOR);
    scanDirectoryEntry("/", startOfRoot, 0, 0);
}

void outputClusterToFile(int firstLogicalCluster, int numBytesToRead, FILE* file) {
    unsigned char const* currentByteToRead = logicalClusterToPointer(firstLogicalCluster);
    fwrite(currentByteToRead, sizeof(char), numBytesToRead, file);
}

void scanFileClusters(int firstLogicalCluster, int fileSize, FILE* filePath) {
    int fatValue = fatTable[firstLogicalCluster];
    if (fatValue >= 0xff8) {
        // Cluster in use, last cluster in file
        int bytesToRead = fileSize % SECTOR_SIZE;
        outputClusterToFile(firstLogicalCluster, bytesToRead, filePath);
    } else if (fatValue >= 2 && fatValue <= 0xfef) {
        // Cluster in use specify next cluster in file
        outputClusterToFile(firstLogicalCluster, SECTOR_SIZE, filePath);
        scanFileClusters(fatValue, fileSize, filePath);
    }
}

void recoverDeletedClusters(int firstLogicalCluster, int fileSize, FILE* filePath, int cluster) {
    int fatValue = fatTable[firstLogicalCluster];

    if (fatValue != 0)
        return;

    if (SECTOR_SIZE * cluster >= fileSize) {
        int bytesToRead = fileSize % SECTOR_SIZE;
        outputClusterToFile(firstLogicalCluster, bytesToRead, filePath);
    } else {
        outputClusterToFile(firstLogicalCluster, SECTOR_SIZE, filePath);
        if (firstLogicalCluster < NUM_LOGICAL_CLUSTERS)
            recoverDeletedClusters(firstLogicalCluster + 1, fileSize, filePath, cluster + 1);
    }
}

void loadFatTable(unsigned char* first) {
    fatTable = malloc(sizeof(int) * NUMBER_OF_FAT_ENTRIES);

    unsigned char* currentByteInFat = first;

    for (int i = 0; i < NUMBER_OF_FAT_ENTRIES; i = i + 2) {
        fat_pair_t currentPair = getFatEntry(currentByteInFat);
        fatTable[i] = currentPair.Entry1;
        fatTable[i + 1] = currentPair.Entry2;
        currentByteInFat = currentByteInFat + 3;
    }
}

fat_pair_t getFatEntry(unsigned char const* first) {
    unsigned char const* second = first + 1;
    unsigned char const* third = first + 2;

    int firstByte = (int) (*first);
    int secondByte = (int) (*second);
    int thirdByte = (int) (*third);

    int firstEntry = firstByte | ((secondByte & 0x0F) << 8);
    int secondEntry = (thirdByte << 4) | ((secondByte & 0xF0) >> 4);

    fat_pair_t pair;
    pair.Entry1 = firstEntry;
    pair.Entry2 = secondEntry;

    return pair;
}


unsigned char* logicalClusterToPointer(int firstLogicalCluster) {
    int sectorNumber = 31 + firstLogicalCluster;
    return disk + (SECTOR_SIZE * sectorNumber);
}

void scanDirectoryEntry(char* filePath, unsigned char* directoryEntry, int directoryEntryNum, int currentLogicalCluster) {
    //Here we get Information About the Current directoryEntry
    unsigned char const* fileName = directoryEntry;
    unsigned char const* extension = directoryEntry + EXTENSION_OFFSET;

    unsigned char attributes = *(directoryEntry + ATTRIBUTES_OFFSET);
    short firstLogicalCluster = *((short*) (directoryEntry + FIRST_CLUSTER_OFFSET));
    int fileSize = *((int*) (directoryEntry + FILESIZE_OFFSET));

    if ((int) (*fileName) == FREE_ENTRY_BYTE) {
        //Free Entry and everything else is free. Do nothing
        return;
    } else if ((((int) attributes) & DIRECTORY_ATTRIBUTE_MASK) == DIRECTORY_ATTRIBUTE_MASK) {
        char* newFilePath = appendDirectoryToFilePath(filePath, fileName);
        scanDirectoryEntry(newFilePath,
                           logicalClusterToPointer(firstLogicalCluster) + (DIRECTORY_ENTRY_SIZE * 2),
                           2, firstLogicalCluster);
    } else {
        file_pair_t pair = appendToFilePath(filePath, fileName, extension);
        char* newFile = pair.fileName;
        char* newFilePath = pair.fullFilePath;

        char* outputDirectString = malloc(strlen(newFile) + strlen(outputDirectory) + 2);
        strcpy(outputDirectString, outputDirectory);
        outputDirectString[strlen(outputDirectory)] = '/';
        outputDirectString[strlen(outputDirectory) + 1] = '\0';
        strcat(outputDirectString, newFile);
        outputDirectString[strlen(newFile) + strlen(outputDirectory) + 1] = '\0';
        FILE* file = fopen(outputDirectString, "wb");
        assert(file != NULL);

        if (((int) (*fileName)) == DELETED_ENTRY_BYTE) {
            recoverDeletedClusters(firstLogicalCluster, fileSize, file, 1);
            printf("FILE\tDELETED\t%s\t%d\n", newFilePath, fileSize);
        } else {
            scanFileClusters(firstLogicalCluster, fileSize, file);
            printf("FILE\tNORMAL\t%s\t%d\n", newFilePath, fileSize);
        }
        fclose(file);
        free(outputDirectString);
        free(newFile);
        free(newFilePath);
    }

    if (directoryEntryNum != DIRECTORY_ENTRIES_PER_SECTOR - 1)
        scanDirectoryEntry(filePath, directoryEntry + 32, directoryEntryNum + 1, currentLogicalCluster);
    else {
        int FATValue = fatTable[firstLogicalCluster];
        if (FATValue >= 0x002 && FATValue <= 0xfef)
            scanDirectoryEntry(filePath, logicalClusterToPointer(FATValue), 0, FATValue);
    }
}

char* appendDirectoryToFilePath(char const* filePath, unsigned char const* fileName) {
    //Appending the subdirectory to the filePath parameter
    int len = 0;
    char* newDirectory = malloc(sizeof(char) * MAX_LENGTH_NEW_DIRECTORY);
    for (int i = 0; i < 8; i++) {
        if (((int) (*(fileName + i))) == FILENAME_PADDING_BYTE) {
            newDirectory[i] = '/';
            newDirectory[i + 1] = '\0';
            newDirectory = realloc(newDirectory, sizeof(char) * (i + 2));
            break;
        }

        if (((int) (*(fileName + i))) == DELETED_ENTRY_BYTE)
            newDirectory[i] = '_';
        else
            newDirectory[i] = *((char const*) (fileName + i));
        if (i == 7) {
            newDirectory[8] = '/';
            newDirectory[9] = '\0';
        }
        len++;
    }
    size_t lengthOfFilePath = strlen(filePath);
    char* newFilePath = malloc(sizeof(char) * (lengthOfFilePath + len));
    strcpy(newFilePath, filePath);
    strcat(newFilePath, newDirectory);

    return newFilePath;
}

file_pair_t appendToFilePath(char const* filePath, unsigned char const* fileName, unsigned char const* extension) {
    // yeah so this is just
    char* newFile = malloc(sizeof(char) * MAX_LENGTH_FILE);
    int nextPosInString = 0;
    for (int i = 0; i < 8; i++) {

        if (((int) (*(fileName + i))) == FILENAME_PADDING_BYTE)
            break;
        else if (((int) (*(fileName + i))) == DELETED_ENTRY_BYTE)
            newFile[i] = '_';
        else
            newFile[i] = *((char const*) (fileName + i));
        nextPosInString++;
    }
    newFile[nextPosInString] = '.';
    int extensionStart = nextPosInString;
    nextPosInString++;

    for (int i = 0; i < 3; i++) {
        if (((int) (*(extension + i))) == FILENAME_PADDING_BYTE)
            break;
        newFile[nextPosInString] = *((char const*) (extension + i));
        nextPosInString++;
    }
    newFile[nextPosInString] = '\0';
    newFile = realloc(newFile, (sizeof(char) * (nextPosInString + 1)));

    assert(newFile != NULL);

    size_t lengthOfFilePath = strlen(filePath);
    char* newFilePath = malloc(sizeof(char) * (lengthOfFilePath + strlen(newFile) + 1));
    strcpy(newFilePath, filePath);
    strcat(newFilePath, newFile);

    file_pair_t filePair;
    int fileLength;

    if (fileNumber == 0)
        fileLength = 2;
    else
        fileLength = (int) (log(fileNumber) + 2);
    char* outDirectFileString = malloc(fileLength + strlen("file") + strlen(newFile + extensionStart));

    sprintf(outDirectFileString, "file%d%s", fileNumber, newFile + extensionStart);
    ++fileNumber;

    filePair.fileName = outDirectFileString;
    filePair.fullFilePath = newFilePath;
    return filePair;
}
