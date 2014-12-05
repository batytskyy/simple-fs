## Disk file system with no journaling, supports next operations:
+ create              - creates new file
+ read                 - reads bytes from file with specified name
+ ls                      - lists all files in the specified directory
+ filestat              - shows stat of a file wirh specified file descriptor fd
+ open                 - opens file (adds record that current fd is being used)
+ close                 - closes file (frees fd)
+ link                    - makes hard link
+ truncate            - changes size of a file
+ unlink                - deletes hard link
+ write                  - writes bytes in a file
+ mkdir                 - creates a dir by name
+ rmdir                 - removes dir
+ pwd                   - shows current work dir
+ cd                      - changes work dir to the specified one
+ symlink              - creates soft link