#include <stdio.h>
#include <stdlib.h>
#define MAX_FILENAME_LENGTH 8

typedef struct FileEntry {
    char filename[MAX_FILENAME_LENGTH];  // File name
    int fileDescriptor;           // File descriptor
    int filesize;                 // File size
    struct FileEntry* next;       // Pointer to the next file entry
} FileEntry;

// create a new FileEntry
FileEntry *createFileEntry(char *filename, int fileDescriptor) {
    FileEntry *newFileEntry = (FileEntry *)malloc(sizeof(FileEntry));
    if (newFileEntry == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for new FileEntry.\n");
        return NULL;
    }
    // Copy filename
    if (strcpy(newFileEntry->filename, filename) == NULL) {
    fprintf(stderr, "Error: Failed to copy filename.\n");
    free(newFileEntry);
    return -1;
    }
    newFileEntry->filename[MAX_FILENAME_LENGTH] = '\0'; // Ensure null termination
    newFileEntry->fileDescriptor = fileDescriptor;
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
void deleteFileEntry(FileEntry **head, int fileDescriptor) {
    FileEntry *current = *head;
    FileEntry *prev = NULL;
    while (current != NULL) {
        if (current->fileDescriptor == fileDescriptor) {
            if (prev == NULL) {
                *head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// print the linked list of FileEntrys
void printFileEntrys(FileEntry *head) {
    printf("FileEntrys:\n");
    FileEntry *current = head;
    while (current != NULL) {
        printf("Filename: %s, File size: %d\n", current->filename, current->fileDescriptor);
        current = current->next;
    }
}

// free the linked list of FileEntrys
void freeTable(FileEntry *head) {
    FileEntry *current = head;
    while (current != NULL) {
        FileEntry *next = current->next;
        free(current);
        current = next;
    }
}

