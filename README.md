# Lab9



## Target 

- To enlarge the size of a single file
- Implement the symbolic links



## Explanation

### Larger file

Originally, the number of data blocks a inode can cover is in the range of one direct block and one block of index. On this basis, we sacrifice one direct block address and add a secondary index. Mimics the hybrid index in Linux's file system.

Implementation:

One entry of the direct block will be used to place the blockno of the first-level index of the secondary index. In the first-level index block, a total of 256 blockno are placed, and these 256 blockno point to the second-level blockno respectively. The 256 blockno of the second-level index block are the real data block.

Therefore, the address space of a single file is expanded by 256*256 - 1 block.

### Symbolic link (Soft link)

Recalling the implementation of xv6's link (hard link): add the inum of the original file to the directory entry, and give it a name. So obviously in a hard link, the linked object must exist (because we store the inum, it must refer to a existing file).

The core of the soft link is to save the **string of the path**. We can see the string of the path is regarded as a file(or, the only thing in the file is the string). Then the inum(of its inode) of this special file is added to the directory entry.

In hard link, the directory save the original file''s inum. But in soft link, directory save the inum of the file which stores the path of another file.

Therefore, creating a soft link does not necessarily satisfy that the linked file exist, because when declaring a soft link, we only create a file containing a path string, it can always work well. And we will define this file as the "soft link" types.

When opening a file, use the string(the path) passed in by the user and use namei() to find the inode of the corresponding file. If the opening file is a soft link, we need to read the path from the soft link file, and then continue to run namei() with that path to find the actual file we are interested in.

If the found file is still a soft link, we need to do the same recursively . So, a threshold should be set. After the threshold is exceeded, it is considered that there is a loop in the soft link parsing, and the search is stopped and the we return failure.

Of course, if a valid file cannot be found from the path in the soft link file, it also returns a failure.

In the test of this lab, there is a special omode(open mode) NOFOLLOW. It means that only open the soft link file itself instead of following and searching the path in file. In fact, this is also a hint to us:  **a soft link is an independent file, and it only saves the string of the file path to which it is linked**.
