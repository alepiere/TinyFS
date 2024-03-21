#include <stdio.h>
#include <stdlib.h>
#include "libTinyFS.h"
#define MAX_FILENAME_LENGTH 8


typedef struct FileEntry {
    char filename[MAX_FILENAME_LENGTH+1];  // File name
    fileDescriptor fileDescriptor;           // File descriptor
    int file_size;                         // File size
    int inode_index;                       // Index of the inode
    int file_index;                       // Index of the first block of file
    int offset;                            // Offset of the file
    time_t creation_time;                    // Creation timestamp
    time_t modification_time;                 // Modification timestamp
    time_t access_time;                      // Access timestamp
    struct FileEntry* next;       // Pointer to the next file entry
} FileEntry;

// create a new FileEntry
FileEntry *createFileEntry(char *filename, fileDescriptor fileDescriptor, int inode_index) {
    FileEntry *newFileEntry = (FileEntry *)malloc(sizeof(FileEntry));
    if (newFileEntry == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for new FileEntry.\n");
        return NULL;
    }
    // Copy filename
    if (strcpy(newFileEntry->filename, filename) == NULL) {
        fprintf(stderr, "Error: Failed to copy filename.\n");
        free(newFileEntry);
        return NULL;
    }
    newFileEntry->filename[MAX_FILENAME_LENGTH] = '\0'; // Ensure null termination
    newFileEntry->fileDescriptor = fileDescriptor;
    newFileEntry->file_size = 0;
    newFileEntry->inode_index = inode_index;
    newFileEntry->file_index = -1;
    newFileEntry->offset = 0;
    newFileEntry->next = NULL;
    return newFileEntry;
}

// add a new FileEntry to the linked list
void insertFileEntry(FileEntry **head, FileEntry *newFileEntry) {
    if (*head == NULL) {
        *head = newFileEntry;
    } else {
        FileEntry *current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newFileEntry;
    }
}

// delete a FileEntry from the linked list
int deleteFileEntry(FileEntry *head, fileDescriptor fileDescriptor) {
    FileEntry *current = head;
    FileEntry *prev = NULL;
    while (current != NULL) {
        if (current->fileDescriptor == fileDescriptor) {
            if (prev == NULL) {
                head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return 1;
        }
        prev = current;
        current = current->next;
    }
    return -1;
}

// print the linked list of FileEntries
void printFileEntrys(FileEntry *head) {
    printf("FileEntrys:\n");
    FileEntry *current = head;
    while (current != NULL) {
        printf("Filename: %s, File size: %d\n", current->filename, current->fileDescriptor);
        current = current->next;
    }
}

// free the linked list of FileEntries
void freeTable(FileEntry *head) {
    FileEntry *current = head;
    while (current != NULL) {
        FileEntry *next = current->next;
        free(current);
        current = next;
    }
}

FileEntry* findFileEntryByFD(FileEntry *head, fileDescriptor fileDescriptor) {
    FileEntry *current = head;
    while (current != NULL) {
        if (current->fileDescriptor == fileDescriptor) {
            return current; // Return the FileEntry if found
        }
        current = current->next;
    }
    return NULL; // Return NULL if the FileEntry is not found
}
