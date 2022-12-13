# README

### Homework 6

Chun-Wei Shaw

Mohsin Rizvi

Sai Satwik Vaddi

### Hours spent and difficulty rankings

cs4213: 16hrs, hmwk1 < hmwk2 < hmwk6 < hmwk4 < hmwk3 < hmwk5

mkr2151: 22hrs, hmwk1 < hmwk2 < hmwk6 < hmwk4 < hmwk5 < hmwk3

sv2665: 35hrs, hmwk1 < hmwk2 < hmwk6 < hmwk4 < hmwk3 < hmwk5

### Main files used

Our implementation consists mostly of the following files:

ezfs.h : This was provided to us and defines many structures used in the assignment.

myez.c : This includes the source code implementation of our filesystem.

format_disk_as_ezfs.c : This file implements a utility for formatting a block device
into an EZFS file system.

### Our solution and assumptions made

We do not believe our solution significantly differs from the assignment, and we have
not made many significant assumptions.

### Testing

We used the following applications, among others, to test our file system:

vi : we were able to successfully edit and save files with vi.

emacs : we were unable to successfully edit and save files with emacs.

gcc : we were able to successfully compile source code with gcc, and then run the resulting program.

ln : we were unable to successfully use ln to create symbolic links.

Some file system functions implemented in the default file system that myezfs does not
support include support for symbolic links, extended file attributes, and forcing data to write
from a buffer to a file with fsync.

The lack of symbolic link support in myezfs caused the ln program to not work.

I suspect that the lack of fsync support caused emacs to be unable to save files.

### Parts worked on

TODO
