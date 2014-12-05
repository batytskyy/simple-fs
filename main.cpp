#include "fstream"
#include "fs.h"
#include <cstring>

using namespace fs;
using namespace std;

void formatDevice(const char * fName);

const char* FILE_NAME = "fs";                   // name of the device
const int DEVICE_CAPACITY_MB = 50;              // it's capacity (formatting arg)

int main() {
    formatDevice(FILE_NAME);
    mount(FILE_NAME);

    filestat(25);
    mkdir("dir1");
    symlink("dir1", "symlink");
    mkdir("symlink/dir2");
    cd("dir1");
    ls();
    mkdir("dir2");
    cd("dir2");
    pwd();
    cd("..");
    pwd();
    ls("dir1");
    pwd();
    mkdir("/dir1/../");
    mkdir("/dir1/dir4");
    mkdir("/dir1/dir4/dir5");
    mkdir("/dir1/dir5/dir6");
    create("/dir1/name");
    ls("/dir1/dir4");
    create("/dir1/name2");
    rmdir("/");

    umount();
    return 0;
}

// allocates bytes on device
void formatDevice(const char *fName) {
    remove(fName);

    std::ofstream outfile (fName);
    outfile.close();

    std::fstream fio;
    fio.open(fName, fstream::in | fstream::binary | fstream::out);

    if (fio.fail()) {
        exit(-1);
    }

    fio.seekp((DEVICE_CAPACITY_MB << 20) - 1);
    fio.write("", 1);

    fio.seekg(0, fio.end);
    fio.close();
}
