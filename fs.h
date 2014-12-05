#ifndef FS_H
#define FS_H

namespace fs {
const int BLOCK_SIZE = 512;
const int BLOCKS_PER_INODE= ((BLOCK_SIZE - sizeof(char) - 2 *
                              sizeof(int)) / sizeof(int));
const int FNAME_LEN = 12;                                // actual size is 11


/**
 * @brief The Inode struct describes structure of file descriptor on a disk
 */
struct Inode {
    char type;                              // 0 - file; 1 - dir; 2 -symlink
    int links;                              // quantity of links per per file
    int size;                               // current file size;
    int blocks[BLOCKS_PER_INODE];           // array of the file block numbers
};

/**
 * @brief The Link struct desribes single directory entry
 */
struct Link {
    char fileName[FNAME_LEN];         // name of a file
    int inodeId;                      // number of the Inode (corresponds to Inode block number)
};

bool mount(const char* fileName);
void umount();

// 0 - file, 1 - dir, 2 - symlink
int create(const char *fileName, int type = 0, char* linkTo = "");
char* read(int inodeId, int size, int shift = 0);
void ls(const char *path);
void ls();
void filestat(int inodeId);
int open(const char* fileName);
void close(int inodeId);
void link(const char* existFileName, const char* linkName);
void truncate(const char* fileName, int newSize);
void unlink(const char* linkName);
void write(int inodeId, int size, char* data, int shift = 0);
void truncate(int inodeId, int newSize);

void mkdir(const char* dirName);
void rmdir(const char* dirName);
void pwd();
void cd(const char* path);
void symlink(char *to, const char *name);

}       // fs::namespace end

#endif // FS_H
