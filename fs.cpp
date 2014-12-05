#include "fs.h"

#include <fstream>
#include <iostream>
#include <cstdio>
#include <set>
#include <vector>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

using namespace std;
using namespace fs;

namespace fs {

void getFileName(char fileName[FNAME_LEN]);
bool addDirRecord(int inodeId, const char* fileName, int dirId);
int getFreeBlockId();
int align_size(int size);
Link *getLinks(int dirId, int &linksNumber);
bool dirContainsFile(int fileId);
int getFileId(const char* absFileName, int &parentDirId, std::string &path);
int getFileId(const char* absFileName, int &parentDirId);
int getFileId(const char* absFileName);
bool removeDirRecord(int dirId, int recordId);
void clearBlock(int blockId, int size = BLOCK_SIZE, int shift = 0);
bool setInodeBlockByIndex(Inode &inode, int index);
char* getAbsPath(const char* path);
std::string simplifyPath(std::vector<std::string> parts);
std::vector<std::string> splitPath(const char* path);

void readBlock(int block_id, char* data, int size = BLOCK_SIZE, int shift = 0);
void readBlock(int block_id, Inode* inode);
void writeBlock(int block_id, const char* data, int size = BLOCK_SIZE, int shift = 0);
void writeBlock(int block_id, const Inode* inode);
int divCeil(int a, int b);
int divFloor(int a, int b);
bool isBlockUsed(int block_id);
void setBlockUsed(int block_id);
void setBlockUnused(int block_id);



int device_capacity = -1;
int bitmask_blocks = -1;
int data_blocks = -1;               // number of blocks, which bitmask occupies
int root_inode_id = -1;             // root fd
set<int> openedDescriptors;         // list of opened descriptors
fstream fio;                        // device
string wd;                          // current work dir

bool mount(const char *fileName) {
    umount();
    wd = "/";

    fio.open(fileName, fstream::in | fstream::out | fstream::binary);
    if (fio.fail()) {
        return false;
    }

    // measure device capacity
    fio.seekg(0, fio.end);
    int device_capacity = fio.tellg();


    // how many blocks for bitmask
    bitmask_blocks = divCeil(device_capacity, BLOCK_SIZE * BLOCK_SIZE * 8);
    // how many blocks are used for data
    data_blocks = divCeil(device_capacity, BLOCK_SIZE);

    root_inode_id = bitmask_blocks;

    // if no inode for root is created
    if (!isBlockUsed(root_inode_id)) {
        setBlockUsed(root_inode_id);

        Inode root;
        root.links = 1;
        root.size = 0;
        root.type = 1;                              // is the directory
        writeBlock(root_inode_id, &root);

        addDirRecord(root_inode_id, ".", root_inode_id);
        addDirRecord(root_inode_id, "..", root_inode_id);
    }

    return true;
}

void umount() {
    device_capacity = -1;
    bitmask_blocks = -1;
    data_blocks = -1;
    root_inode_id = -1;
    openedDescriptors.clear();
    wd = "";

    fio.close();
}

/// reimplement4
int create(const char* fileName, int type, char *linkTo) {
    char* absFileName = getAbsPath(fileName);

    int parentDirId = -1;
    int fileId = getFileId(absFileName, parentDirId);

    if (fileId != -1) {
        cout << "Error: object \"" << absFileName <<  "\" already exists" << endl;
        delete absFileName;
        return -1;
    }

    delete absFileName;

    if (parentDirId == -1) {
        cout << "Error: bad path" << endl;
        return -1;
    }

    // find space for the new Inode
    int inodeId = getFreeBlockId();

    if (inodeId == -1) {
        cout << "Error: no free space available" << endl;
        return -1;
    }

    vector<string> names = splitPath(fileName);
    if (names[names.size() - 1].size() > FNAME_LEN - 1) {
        cout << "Error: object name " << names[names.size() - 1] << " is too long" << endl;
        return -1;
    }

    setBlockUsed(inodeId);

    // add file inode to corresponding derectory link
    if (!addDirRecord(inodeId, names[names.size() - 1].c_str(), parentDirId)) {
        setBlockUnused(inodeId);           // no block was used
        return -1;
    }

    // create empty inode for new file
    Inode newFileInode;
    newFileInode.links = 1;
    newFileInode.size = 0;
    newFileInode.type = type;                    // is a file
    writeBlock(inodeId, &newFileInode);


    if (type == 1) {                  // is a dir
        addDirRecord(parentDirId, "..", inodeId);
        addDirRecord(inodeId, ".", inodeId);
    }

    if (type == 2) {
        write(inodeId, strlen(linkTo) + 1, linkTo);
    }

    return inodeId;
}

char* read(int inodeId, int size, int shift) {
    Inode inode;
    readBlock(inodeId, &inode);

    // size is too big
    if (size > BLOCKS_PER_INODE * BLOCK_SIZE) {
        cout << "Error: size " << size << " out of " <<
                BLOCKS_PER_INODE * BLOCK_SIZE << " is too big" <<  endl;
        return NULL;
    }

    if (size + shift > inode.size) {
        cout << "Error: access " << size + shift << " byte out of " << inode.size <<
                " bytes file size" << endl;
        return NULL;
    }

    // return buffer
    char* buff = new char[size];

    // index of the first block in inode
    int firstBlockIndex = shift / BLOCK_SIZE;

    // shift in the first block
    int firstBlockShift = shift % BLOCK_SIZE;

    // buffer for the file blocks
    char fileBlock[BLOCK_SIZE];

    // copy only a part of the first block
    if (BLOCK_SIZE - firstBlockShift >= size) {
        readBlock(inode.blocks[firstBlockIndex], fileBlock);
        memcpy(&buff[0], &fileBlock[firstBlockShift], size);
    } else {
        // index of the last block in inode
        int lastBlockIndex = divFloor(size + shift, BLOCK_SIZE);
        int curBuffSize = 0;

        readBlock(inode.blocks[firstBlockIndex], fileBlock);

        memcpy(&buff[curBuffSize], &fileBlock[firstBlockShift], BLOCK_SIZE - firstBlockShift);
        curBuffSize = curBuffSize + (BLOCK_SIZE - firstBlockShift);

        int i;
        for (i = firstBlockIndex + 1; i < lastBlockIndex; i++) {
            readBlock(inode.blocks[i], fileBlock);

            memcpy(&buff[curBuffSize], fileBlock, BLOCK_SIZE);
            curBuffSize += BLOCK_SIZE;
        }

        readBlock(inode.blocks[lastBlockIndex], fileBlock);
        // copy left part of the last block
        int lastBlockPart = size - curBuffSize;

        memcpy(&buff[curBuffSize], fileBlock, lastBlockPart);
    }

    return buff;
}

void ls(const char* path) {
    int linksNumber = 0;

    char* absPath = getAbsPath(path);

    int dirId = getFileId(absPath);

    if (dirId == -1) {
        cout << "Error: can't access \"" << absPath << "\" : no such object" << endl;
        delete absPath;
        return;
    }

    Link* links = getLinks(dirId, linksNumber);
    for (int i = 0;  i < linksNumber; i++) {
        cout << links[i].fileName << " ";
        cout << links[i].inodeId << endl;
    }

    delete absPath;
    delete links;
}

void ls() {
    ls(wd.c_str());
}

void filestat(int inodeId) {
    bool isFileInDir = true;

    Inode inode;
    readBlock(inodeId, &inode);

    if (inode.type == 1) {                        // dir
        if (inode.size < 2 * sizeof(Link)) isFileInDir = false;
    } else {
        if (inode.size == 0) isFileInDir = false;
    }

    // if root dir
    if (inodeId == root_inode_id) isFileInDir = true;

    if (!isFileInDir) {
        cout << "Error: incorrect id" << endl;
        return;
    }

    if (inode.type == 0) {
        cout << "type: file" << endl;
    } else if (inode.type == 1) {
        cout << "type: directory" << endl;
    } else {
        cout << "type: symlink" << endl;
    }

    cout << "object size: " << inode.size << endl;
    cout << "number of links: " << inode.links << endl;
}

int open(const char *fileName) {
    int fileId = getFileId(fileName);

    Inode inode;
    readBlock(fileId, &inode);

    if (inode.type != 0) {
        cout << "Error: object isn't a file, can't open" << endl;
        return -1;
    }

    if (fileId > 0) openedDescriptors.insert(fileId);
    return getFileId(fileName);
}

void close(int inodeId) {
    openedDescriptors.erase(inodeId);
}

void link(const char *existFileName, const char *linkName) {
    char* absExistFileName = getAbsPath(existFileName);
    int existFileId = getFileId(getAbsPath(absExistFileName));
    if (existFileId == -1) {
        cout << "Error: no such file \"" << absExistFileName <<  "\" exists" << endl;
        delete absExistFileName;
        return;
    }

    char* absLinkName = getAbsPath(linkName);
    int linkFileId = getFileId(getAbsPath(absLinkName));
    if (linkFileId != -1) {
        cout << "Error: file \"" << linkName <<  "\" already exists" << endl;
        delete absLinkName;
        return;
    }

    addDirRecord(existFileId, linkName, root_inode_id);

    Inode inode;
    readBlock(existFileId, &inode);

    // increase number of links
    inode.links += 1;

    writeBlock(existFileId, &inode);
}

void unlink(const char* linkName) {
    int dirId;
    int existLinkId;

    char* absLinkName = getAbsPath(linkName);
    existLinkId = getFileId(absLinkName, dirId);
    delete absLinkName;

    // is not a link or a file
    if (existLinkId == -1) {
        cout << "Error: no such link \"" << linkName <<  "\" exist" << endl;
        return;
    }

    Inode inode;
    readBlock(existLinkId, &inode);

    set<int>::iterator it = openedDescriptors.find(existLinkId);

    // check whether the file is closed
    if (it != openedDescriptors.end()) {
        cout << "Error: close file first" << endl;
        return;
    }

    if (inode.links > 1) {                      // file has other links
        inode.links -= 1;
        removeDirRecord(dirId, existLinkId);
        writeBlock(existLinkId, &inode);
    } else {                                    // file has no other links, delete it
        truncate(existLinkId, 0);
        removeDirRecord(dirId, existLinkId);
        setBlockUnused(existLinkId);
    }
}

void write(int inodeId, int size, char* data, int shift) {
    if (size > BLOCKS_PER_INODE * BLOCK_SIZE) {
        cout << "Error: size " << size << " out of " <<
                BLOCKS_PER_INODE * BLOCK_SIZE << " is too big" <<  endl;
        cout << "in write" << endl;
        return;
    }

    if (size <= 0) return;

    Inode inode;
    readBlock(inodeId, &inode);

    // truncate first (there is no enough space to write)
    if (size + shift > inode.size) {
        truncate(inodeId, size + shift);
        readBlock(inodeId, &inode);
    }

    int firstBlockShift = shift % BLOCK_SIZE;
    int firstBlockIndex = shift / BLOCK_SIZE;
    int curBlockIndex = firstBlockIndex;
    int lastBlockIndex = divFloor(inode.size, BLOCK_SIZE);
    int bytesWritten = 0;

    // shift exist
    if (firstBlockShift != 0) {
        // write to the first block
        if (!setInodeBlockByIndex(inode, curBlockIndex)) return;

        setBlockUsed(inode.blocks[curBlockIndex]);

        // write only necessary part of file
        bytesWritten = min(BLOCK_SIZE - firstBlockShift, size);
        writeBlock(inode.blocks[curBlockIndex], data, bytesWritten, firstBlockShift);

        curBlockIndex++;
    }

    for (int i = curBlockIndex; i < lastBlockIndex; i++) {
        if (!setInodeBlockByIndex(inode, i)) {
            for (int j = firstBlockIndex; j <= i; j++) {
                setBlockUnused(inode.blocks[j]);
            }
            return;
        }

        setBlockUsed(inode.blocks[i]);
        writeBlock(inode.blocks[i], &data[bytesWritten], BLOCK_SIZE);
        bytesWritten += BLOCK_SIZE;
    }

    // copy leftover part of the last block, if not copied previously
    if (curBlockIndex <= lastBlockIndex) {
        int lastBlockPart = inode.size - bytesWritten;

        if (!setInodeBlockByIndex(inode, lastBlockIndex)) return;
        setBlockUsed(inode.blocks[lastBlockIndex]);
        writeBlock(inode.blocks[lastBlockIndex], &data[bytesWritten], lastBlockPart);
    }

    writeBlock(inodeId, &inode);
}

void truncate(const char *fileName, int newSize) {
    int inodeId = getFileId(fileName);
    if (inodeId == -1) {
        cout << "Error: no such file found" << endl;
        return;
    }

    // size is too big
    if (newSize > BLOCKS_PER_INODE * BLOCK_SIZE) {
        cout << "Error: size " << newSize << " out of " <<
                BLOCKS_PER_INODE * BLOCK_SIZE << " is too big" <<  endl;
        cout << "in truncate" << endl;
        return;
    } else if (newSize < 0) {
        cout << "Error: negative size is not allowed" << endl;
        return;
    }

    truncate(inodeId, newSize);
}

void mkdir(const char* dirName) {
    create(dirName, 1);
}

void cd(const char* path) {
    int dirId;                  // parent dir
    int fileId;                 // file inode id
    string newPath;             // new path

    char* absPath = getAbsPath(path);

    fileId = getFileId(absPath, dirId, newPath);

    if (fileId == -1) {
        cout << "Error: no such object exists" << endl;
        return;
    }

    Inode inode;
    readBlock(fileId, &inode);

    if (inode.type != 1) {
        cout << "Error: not a directory" << endl;
        return;
    }

    newPath = simplifyPath(splitPath(newPath.c_str()));
    if (newPath.back() != '/') newPath.append("/");
    wd = newPath;
}

void symlink(char* from, const char* name) {
    create(name, 2, from);
}

void rmdir(const char* dirName) {
    int dirId;
    int parentDirId;

    char* absDirName = getAbsPath(dirName);

    dirId = getFileId(absDirName, parentDirId);

    if (dirId == -1) {
        cout << "Error: no such dir exists" << endl;
        delete absDirName;
        return;
    }

    Inode inode;
    readBlock(dirId, &inode);

    if (inode.type != 1) {
        cout << "Error: not a directory" << endl;
        delete absDirName;
        return;
    }

    if (inode.size > 2 * sizeof(Link)) {
        cout << "Error: this directory is not empty" << endl;
        delete absDirName;
        return;
    }

    unlink(absDirName);
    delete absDirName;
}

void pwd() {
    cout << wd << endl;
}

int getFileId(const char* absFileName, int &parentDirId, string &path) {
    int filesInDir;
    Link* links;

    parentDirId = -1;                              // id of the parent dir
    vector<string> names = splitPath(absFileName); // path names, including symlinks
    string curName = names[1];                     // names[0] is "/"
    int curNameInd = 1;                            // index in names
    int curFileId = root_inode_id;                 // current file looked up
    path.append("/");

    vector<string> symNames;                       // current symlink path
    vector<string>::iterator linkName;             // current link in symlinks

    int pathLen = names.size() - 1;                // names[0] is "/"

    for (int j = 0; j < pathLen; j++) {
        if (curFileId == -1) {
            parentDirId = -1;
            break;
        }

        links = getLinks(curFileId, filesInDir);

        // use name from path
        if (linkName == symNames.end()) {
            curName = names[curNameInd++];
        // use name of a symlink
        } else {
            curName = *linkName;
            linkName++;
        }

        path.append(curName);
        path.append("/");

        for (int i = 0; i < filesInDir; i++) {
            if (!curName.compare(links[i].fileName)) {         // name matches
                int tempFileId = parentDirId;
                parentDirId = curFileId;
                curFileId = links[i].inodeId;

                Inode inode;
                readBlock(curFileId, &inode);

                if (inode.type == 2) {                        // is a symlink
                    char* symLink = read(curFileId, inode.size);
                    symNames = splitPath(reinterpret_cast<const char*>(symLink));

                    delete symLink;

                    curFileId = parentDirId;
                    parentDirId = tempFileId;

                    if (symNames[0] == "/") {
                        curFileId = root_inode_id;
                        symNames.erase(symNames.begin(), symNames.begin() + 1);
                        pathLen--;
                    }

                    pathLen += symNames.size();
                    linkName = symNames.begin();
                }
                break;
            }

            if (i == filesInDir - 1) {
                parentDirId = curFileId;
                curFileId = -1;
            }
        }

        delete links;
    }

    return curFileId;
}

int getFileId(const char* absFileName, int &parentDirId) {
    string path;
    return getFileId(absFileName, parentDirId, path);
}

int getFileId(const char* absFileName) {
    int dir = 0;
    return getFileId(absFileName, dir);
}

vector<string> splitPath(const char* path) {
    vector<string> names;
    if (strlen(path) == 0) return names;

    using namespace boost::algorithm;

    string str = string(path);
    vector<string> tokens;

    if(str[0] == '/') {
        names.push_back("/");
        str.erase(0, 1);
    }

    split(tokens, str, is_any_of("/"));
    for(auto& s: tokens) names.push_back(s);

    if (names[names.size() - 1] == "") names.pop_back();

    return names;
}

void truncate(int inodeId, int newSize) {
    Inode inode;
    readBlock(inodeId, &inode);

    int newBlocksNumber = divCeil(newSize, BLOCK_SIZE);
    int blocksNumber;                           // number of blocks before truncate()

    blocksNumber = divCeil(inode.size, BLOCK_SIZE);

    if (newSize > inode.size) { // add new blocks if needed
        // create imaginary blocks, that are not presented in memory (with all zeros)

        if (inode.size > 0) {
            clearBlock(inode.blocks[blocksNumber], BLOCK_SIZE - inode.size, inode.size);
            blocksNumber++;
        }

        for (int i = blocksNumber; i < newBlocksNumber; i++) inode.blocks[i] = -1;

    } else {                // just free some blocks if needed
        // mark freed blocks as unused
        for (int i = newBlocksNumber; i < blocksNumber; i++) {
            if (inode.blocks[i] != -1) setBlockUnused(inode.blocks[i]);
            inode.blocks[i] = 0;
        }
    }

    inode.size = newSize;

    writeBlock(inodeId, &inode);
}

bool removeDirRecord(int dirId, int recordId) {
    Inode dirInode;
    readBlock(dirId, &dirInode);

    Link* links;
    int linksNumber;
    links = getLinks(dirId, linksNumber);

    int recordIndex = 0;
    // look for record index
    for (recordIndex = 0; recordIndex < linksNumber; recordIndex++) {
        if (links[recordIndex].inodeId == recordId) break;
    }

    // no dir record found
    if (recordIndex == linksNumber) return false;

    // shift all links by one (delete old link)
    for (int i = recordIndex; i < linksNumber - 1; i++) links[i] = links[i + 1];

    // check whether it is possible to free a block
    if (dirInode.size % BLOCK_SIZE == sizeof(Link)) {
        // delete last block, it will be free after links shift
        setBlockUnused(dirInode.blocks[linksNumber - 1]);
        dirInode.blocks[linksNumber - 1] = 0;
    }

    dirInode.size -= sizeof(Link);

    writeBlock(dirId, &dirInode);

    // write new data to dir
    write(dirId, dirInode.size, reinterpret_cast<char*>(links));

    delete links;
    return true;
}

bool dirContainsFile(int fileId) {
    int filesInDir = 0;            // how many files exist in dir
    Link* links = getLinks(root_inode_id, filesInDir);

    // check whether dir contains the name
    bool isFileInDir = false;

    for (int i = 0;  i < filesInDir; i++) {
        if (links[i].inodeId == fileId) isFileInDir = true;
    }

    delete links;
    return isFileInDir;
}

Link* getLinks(int dirId, int &linksNumber) {
    Inode dirInode;
    readBlock(dirId, &dirInode);

    Link* links;
    linksNumber = dirInode.size / sizeof(Link);
    links = (Link*)read(dirId, dirInode.size);

    return links;
}

bool addDirRecord(int inodeId, const char *fileName, int dirId) {
    // new record (link to file from dir)
    Link link;
    strcpy(link.fileName, fileName);
    link.inodeId = inodeId;

    Inode dirInode;
    readBlock(dirId, &dirInode);

    // new block needed
    if ((dirInode.size % BLOCK_SIZE) == 0) {
        // allocate new block
        int freeBlockId = getFreeBlockId();

        if (freeBlockId == -1) return false;            // no disk space
        setBlockUsed(freeBlockId);

        int inodeBlockNumber = dirInode.size / BLOCK_SIZE;
        dirInode.blocks[inodeBlockNumber] = freeBlockId;
    }

    int inodeBlockNumber = dirInode.size / BLOCK_SIZE;

    // id of the first non-full block
    int blockId = dirInode.blocks[inodeBlockNumber];
    int shift = dirInode.size % BLOCK_SIZE;

    writeBlock(blockId, reinterpret_cast<const char*>(&link), sizeof(Link), shift);

    dirInode.size += sizeof(Link);
    writeBlock(dirId, &dirInode);

    return true;
}

int getFreeBlockId() {
    int freeBlockId;
    for (freeBlockId = root_inode_id; freeBlockId <= data_blocks; freeBlockId++) {
        if (!isBlockUsed(freeBlockId)) break;
    }

    if (freeBlockId > data_blocks) return -1;
    return freeBlockId;
}

void setBlockUsed(int block_id) {
    fio.seekg((block_id - bitmask_blocks) / 8, fio.beg);
    int old_mask = fio.get();
    fio.seekp((block_id - bitmask_blocks) / 8, fio.beg);
    fio.put(static_cast<char>(old_mask | (1 << ((block_id - bitmask_blocks) % 8))));
}

void setBlockUnused(int block_id) {
    fio.seekg((block_id - bitmask_blocks) / 8, fio.beg);
    int old_mask = fio.get();
    fio.seekp((block_id - bitmask_blocks) / 8, fio.beg);
    fio.put(static_cast<char>(old_mask & ~(1 << ((block_id - bitmask_blocks) % 8))));
}

bool isBlockUsed(int block_id) {
    fio.seekg((block_id - bitmask_blocks) / 8, fio.beg);
    return (fio.get() & (1 << ((block_id - bitmask_blocks) % 8))) != 0;
}

void readBlock(int block_id, char* data, int size, int shift) {
    if (block_id == 0) {
        cout << "Error: attempt to read corrupted data (fd = 0)" << endl;
        return;
    }

    if (block_id > 0) {
        fio.seekg(block_id * BLOCK_SIZE + shift, fio.beg);
        fio.read(data, size);
    } else {
        // read all zeros, if fd = -1
        for (int i = 0; i < size; i++) data[i] = 0;
    }
}

void readBlock(int block_id, Inode* inode) {
    readBlock(block_id, reinterpret_cast<char*>(inode));
}

void writeBlock(int block_id, const char* data, int size, int shift) {
    fio.seekp(block_id * BLOCK_SIZE + shift, fio.beg);
    fio.write(data, size);
}

void writeBlock(int block_id, const Inode* inode) {
    writeBlock(block_id, reinterpret_cast<const char*>(inode));
}

int divCeil(int a, int b) {
    return a == 0 ? 0 : (a - 1) / b + 1;
}

int divFloor(int a, int b) {
    return a == 0 ? 0 : (a - 1) / b;
}

void getFileName(char fileName[FNAME_LEN]) {
    fgets(fileName, FNAME_LEN, stdin);

    fflush(stdin);
    if (fileName[strlen(fileName) - 1] == '\n') fileName[strlen(fileName) - 1] = '\0';
}

int align_size(int size) {
    int min_size = 8;
    return size + (min_size - size % min_size) % min_size;
}

void clearBlock(int blockId, int size, int shift) {
    char* clearedBlock = new char[size];

    for (int i = 0; i < size; i++) clearedBlock[i] = 0;

    writeBlock(blockId, clearedBlock, size, shift);

    delete clearedBlock;
}

bool setInodeBlockByIndex(Inode &inode, int index) {
    if (inode.blocks[index] < 0) {
        inode.blocks[index] = getFreeBlockId();

        // no block found
        if (inode.blocks[index] == -1) {
            cout << "Error: not enough disk space, impossible to write " << endl;
            return false;
        }

    }

    return true;
}

char* getAbsPath(const char* path) {
    char* absPath = new char[100];                   // path, adding the pwd

    if (path[0] != '/') {
        strcpy(absPath, wd.c_str());
        strcat(absPath, path);
    } else {
        strcpy(absPath, path);
    }

    if (absPath[strlen(absPath) - 1] != '/') {
        strcat(absPath, "/");
    }

    return absPath;
}

string simplifyPath(vector<string> parts) {
    vector<string> newParts;
    string newPath;

    for (int i = 0; i < parts.size(); i++) {
        if (!parts[i].compare("..")) newParts.pop_back();
        else if (!parts[i].compare(".")) {}
        else newParts.push_back(parts[i]);
    }

    newPath.append("/");
    for (int i = 1; i < newParts.size(); i++) {
        newPath.append(newParts[i]);
        newPath.append("/");
    }

    return newPath;
}
}       // fs::namespace end
