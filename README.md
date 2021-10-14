# ssname
Generate and set 8.3 short name automatically for NTFS under Windows, if short name is missing.
```
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * SSNAME: Set short name automatically under windows NT (8.3 DOS name)  *
 * Tested under Windows XP and Windows 7.                                *
 * Copyright 2021 Raymond GILLIBERT                                      *
 * I wrote this program to re-generate shortnames automatically after    *
 * copying some files under Linux using NTFS-3G.                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - *
 * Usage: ssname.exe "long file name.extension"                          *
 * If a valid short-name already exists it does nothing, otherwise it    *
 * will find an available shortname and set it.                          *
 * It relyes on the SetFileShortNameW(), Windows XP/2003+ and later      *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - *
 * Note that it will start at ~0 and go up to ~ZZ (Base 36)              *
 * This is bad code I wrote in one night with some good drink.....       *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - *
 * This file is under the DWTFPL.                                        *
 * DO WHAT THE FUCK YOU WANT WITH THIS SOFTWARE                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
```
exemple:
```
> ssname.exe "Ùn fiçhié kì è trop.léong"
SSHORT-NAME SET TO: NFIHIK~0.lon
```
This is usefull to be used in a for loop to regenerate all short file names:

`> for %i in (*.*) do ssname "%i"`

If you want to regenerate short names for all files in current directorry.

Use: `> for /R [/D] %i in (*.*) do ssname.exe "\\.\%i"`
To make a recursive loop to include all files in sub-dirs.
Add the `/D` flag to selec folders (dirs).

The point compared to `fsutil file setshortname <long path> <shortname>` is that short-name is generated automatically.
