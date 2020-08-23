# DSW

A utility named "dsw" was included with 1st Edition Research Unix each succeeding edition through the 6th. The utility was coded in assembly language and was migrated to C language by Ross Nealon of the University of Wollongong as part of the Interdata 7/32 port that the university did, (a port to the Interdata 8/32 was done by Bell Labs).

The man page for the utility makes its purpose clear: it's basically an interactive file delete. About the name, though, the man page says:

> "The name dsw is a carryover from the ancient past. Its etymology is amusing."

In ["A Brief History of the 'rm' and 'rmdir' commands"](http://tldp.org/LDP/LG/issue49/fischer.html), Eric Fischer says dmr posted on USENET in 1981 about the name:

> "In 1981 dmr posted on USENET saying that the name had originally meant "delete from switches." This was a reference to the toggle switches on the front panel of the PDP-7 computer that the earliest version of Unix ran on."

Later, Doug McIlroy wrote about dsw in, ["A Research UNIX Reader: Annotated Excerpts from the Programmer's Manual, 1971-1986"](https://archive.org/details/a_research_unix_reader):

> "DSW (v1 page 73)
The nostalgically named dsw was a desperation tool designed to clean up files with unutterable names. This had been done on the PDP-7 by a program with console switch input; hence the sobriquet "delete from switches." It survived from v1 until it was displaced by rm -i (Ritchie, v7)."

And the penultimate sentence in the Introduction of the 7th Edition Programmer's Manual says:

> "Dsw. This little-known, but indispensable facility has been taken over by rm -ri."

## Deja Vu All Over Again

As Doug notes, the purpose behind the dsw utility was to allow files to be deleted whose names could not be typed. There is a similar problem on nt with the added problem of files created using names reserved by the win32 subsystem. Deleting un-typeable names can be handled by "rm -i" on nt, and reserved names can be dealt with by using a fully-qualified name and the "no-parse" prefix. But, the rm utility I use here is based on the plan9 version and doesn't include the "-i" switch, plus I'd rather not type fully-qualified names. So, this is a version of dsw for nt.

## Process

The utility opens a handle to the specified directory, or the current directory. In the past, I used NtQueryDirectoryFile to get the entries, but that executive API now has a documented interface, [GetFileInformationByHandleEx](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex), and that's what's used in this code. For each directory entry returned by the API, its name is printed and the user is prompted for disposition: "y" to delete the file, <enter> to skip, or "x" to exit.

If "y" is entered, a fully-qualified name is built that includes "\\\\.\\" no-parse prefix. This name is then used to reset read-only on the file attributes, (if needed), and is then passed to the DeleteFile API.

If the delete fails for access, an attempt is made to zap the dacl on the file's security descriptor. If that fails, an attempt is made to assign ownership of the file to the administrator.

## Build

Open a "vc tools" command prompt, either 32-bit or 64-bit, change to the directory containing the dsw.c file and then:
```
# cl -W3 -O2 dsw.c
```
The security descriptor code requires the advapi32.lib library. There is a comment pragma in the code to automatically include that library. If you prefer to comment or delete that pragma, then the library will need to be included on the compile command line:
```
# cl -W3 -O2 dsw.c -link advapi32.lib
```
## Files

The following files are included:
```
DSW
|   dsw.c                       dsw for nt.
|   README.md                   this.
|
\---docs
        dmr-message.txt         msg from dmr with 1st Edition source.
        dsw-ed2.s               dsw 2nd Edition.
        dsw-ed5.s               dsw 5th Edition.
        dsw-ed6-wollongong.c    dsw 6th Edition Interdata 7/32 port.
        dsw-ed6.s               dsw 6th Edition.
        dsw.1                   man page, 1972-03-15.
        dsw.1.pdf                 man->ps2pdf
        dsw.1.txt                 man->file
        dsw.pwb.1               man page, 1977-05-21 (PWB).
```
That is all.
```
  SR  15,15
  BR  14
```
