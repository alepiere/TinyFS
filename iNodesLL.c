#include <stdio.h>
#include <stdlib.h>
#define MAX_FILENAME_LENGTH 8

typedef struct INode {
    char filename[MAX_FILENAME_LENGTH + 1]; // File name (including null terminator)
    int fileSize;                           // Size of the file
    struct Inode *next;                     // Index of the next inode in the linked list
    // Add any other necessary fields for the inode block
} Inode;

// create a new inode
Inode *createInode(char *filename, int fileSize) {
    Inode *newInode = (Inode *)malloc(sizeof(Inode));
    if (newInode == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for new inode.\n");
        return NULL;
    }
    // Copy filename
    strncpy(newInode->filename, filename, 8);
    newInode->filename[8] = '\0'; // Ensure null termination
    newInode->fileSize = fileSize;
    newInode->next = NULL;
    return newInode;
}

// add a new inode to the linked list
void insertInode(Inode **head, Inode *newInode) {
    if (*head == NULL) {
        *head = newInode;
    } else {
        Inode *current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newInode;
    }
}

// print the linked list of inodes
void printInodes(Inode *head) {
    Inode *current = head;
    while (current != NULL) {
        printf("Filename: %s, File size: %d\n", current->filename, current->fileSize);
        current = current->next;
    }
}

// free the linked list of inodes
void freeInodes(Inode *head) {
    Inode *current = head;
    Inode *next;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
}

