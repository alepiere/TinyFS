int openDisk(char *filename, int nBytes);

int closeDisk(int disk); /* self explanatory */

int readBlock(int disk, int bNum, void *block);

int writeBlock(int disk, int bNum, void *block);