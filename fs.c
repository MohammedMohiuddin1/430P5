// ============================================================================
// fs.c - user FileSytem API
// ============================================================================

#include "bfs.h"
#include "fs.h"

// ============================================================================
// Close the file currently open on file descriptor 'fd'.
// ============================================================================
i32 fsClose(i32 fd) { 
  i32 inum = bfsFdToInum(fd);
  bfsDerefOFT(inum);
  return 0; 
}



// ============================================================================
// Create the file called 'fname'.  Overwrite, if it already exsists.
// On success, return its file descriptor.  On failure, EFNF
// ============================================================================
i32 fsCreate(str fname) {
  i32 inum = bfsCreateFile(fname);
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Format the BFS disk by initializing the SuperBlock, Inodes, Directory and 
// Freelist.  On succes, return 0.  On failure, abort
// ============================================================================
i32 fsFormat() {
  FILE* fp = fopen(BFSDISK, "w+b");
  if (fp == NULL) FATAL(EDISKCREATE);

  i32 ret = bfsInitSuper(fp);               // initialize Super block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitInodes(fp);                  // initialize Inodes block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitDir(fp);                     // initialize Dir block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitFreeList();                  // initialize Freelist
  if (ret != 0) { fclose(fp); FATAL(ret); }

  fclose(fp);
  return 0;
}


// ============================================================================
// Mount the BFS disk.  It must already exist
// ============================================================================
i32 fsMount() {
  FILE* fp = fopen(BFSDISK, "rb");
  if (fp == NULL) FATAL(ENODISK);           // BFSDISK not found
  fclose(fp);
  return 0;
}



// ============================================================================
// Open the existing file called 'fname'.  On success, return its file 
// descriptor.  On failure, return EFNF
// ============================================================================
i32 fsOpen(str fname) {
  i32 inum = bfsLookupFile(fname);        // lookup 'fname' in Directory
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Read 'numb' bytes of data from the cursor in the file currently fsOpen'd on
// File Descriptor 'fd' into 'buf'.  On success, return actual number of bytes
// read (may be less than 'numb' if we hit EOF).  On failure, abort
// ============================================================================
i32 fsRead(i32 fd, i32 numb, void* buf) {

  // ++++++++++++++++++++++++
  // Insert your code here
  // ++++++++++++++++++++++++ //test

  i32 Inum = bfsFdToInum(fd);

  i32 cursor_position = bfsTell(fd);

  i32 first = cursor_position / BYTESPERBLOCK; 

  i32 numb_of_bytes = cursor_position + numb;

  i32 last = numb_of_bytes / BYTESPERBLOCK; 

  i32 offset = 0; 

  for(int i = first; i <= last; i++)
  {
     i8* bio_buffer = malloc(BYTESPERBLOCK);

     if(i == first)
     {
        bfsRead(Inum, i, bio_buffer);

        i32 block_offset = cursor_position % BYTESPERBLOCK;

        i32 num_of_allocations = BYTESPERBLOCK - block_offset;

        memcpy(buf, bio_buffer + block_offset, num_of_allocations);

        offset += num_of_allocations;
     } 

     else 
     { 
       bfsRead(Inum, i, bio_buffer);

       memcpy(buf + offset, bio_buffer, BYTESPERBLOCK);

       offset += BYTESPERBLOCK;
     }

     free(bio_buffer);
  }

  bfsSetCursor(Inum, cursor_position + numb);
                                                      // Not Yet Implemented!
  return numb;
}


// ============================================================================
// Move the cursor for the file currently open on File Descriptor 'fd' to the
// byte-offset 'offset'.  'whence' can be any of:
//
//  SEEK_SET : set cursor to 'offset'
//  SEEK_CUR : add 'offset' to the current cursor
//  SEEK_END : add 'offset' to the size of the file
//
// On success, return 0.  On failure, abort
// ============================================================================
i32 fsSeek(i32 fd, i32 offset, i32 whence) {

  if (offset < 0) FATAL(EBADCURS);
 
  i32 inum = bfsFdToInum(fd);
  i32 ofte = bfsFindOFTE(inum);
  
  switch(whence) {
    case SEEK_SET:
      g_oft[ofte].curs = offset;
      break;
    case SEEK_CUR:
      g_oft[ofte].curs += offset;
      break;
    case SEEK_END: {
        i32 end = fsSize(fd);
        g_oft[ofte].curs = end + offset;
        break;
      }
    default:
        FATAL(EBADWHENCE);
  }
  return 0;
}



// ============================================================================
// Return the cursor position for the file open on File Descriptor 'fd'
// ============================================================================
i32 fsTell(i32 fd) {
  return bfsTell(fd);
}



// ============================================================================
// Retrieve the current file size in bytes.  This depends on the highest offset
// written to the file, or the highest offset set with the fsSeek function.  On
// success, return the file size.  On failure, abort
// ============================================================================
i32 fsSize(i32 fd) {
  i32 inum = bfsFdToInum(fd);
  return bfsGetSize(inum);
}



// ============================================================================
// Write 'numb' bytes of data from 'buf' into the file currently fsOpen'd on
// filedescriptor 'fd'.  The write starts at the current file offset for the
// destination file.  On success, return 0.  On failure, abort
// ============================================================================
i32 fsWrite(i32 fd, i32 numb, void* buf) {

  // ++++++++++++++++++++++++
  // Insert your code here
  // ++++++++++++++++++++++++
  
  i32 Inum = bfsFdToInum(fd);
  i32 cursor_position = bfsTell(fd);
  i32 size = fsSize(fd);
  i32 new_size = cursor_position + numb; 

  if(size < new_size)
  {

    i32 num_of_current_blocks = 0; 

    if(size % BYTESPERBLOCK == 0)
    {
       num_of_current_blocks = size / BYTESPERBLOCK;
    }
    else if(size % BYTESPERBLOCK != 0)
    {
       num_of_current_blocks = size / BYTESPERBLOCK + 1;
    }
    else 
    {
       printf("Error: could not calculate number of current blocks in file");
    }

    i32 current_space = num_of_current_blocks * BYTESPERBLOCK;

    if(current_space < new_size)
    {
      i32 needed_memory = new_size - current_space;
      i32 needed_blocks = 0;

      if(needed_memory % BYTESPERBLOCK == 0)
      {
         needed_blocks = needed_memory / BYTESPERBLOCK;
      }
      else if(needed_memory % BYTESPERBLOCK != 0)
      {
         needed_blocks = needed_memory / BYTESPERBLOCK + 1;
      }
      else
      {
        printf("Error: could not calculate number of current blocks needed"); 
      }

      i32 fbn_size = num_of_current_blocks + needed_blocks;
      bfsExtend(Inum, fbn_size);
    }

    bfsSetSize(Inum, new_size);
  }

    i32 first_fbn = cursor_position / BYTESPERBLOCK; 

    i32 last_fbn = (cursor_position + numb) / BYTESPERBLOCK;

    i32 offset = 0;

    for(int i = first_fbn; i <= last_fbn; i++)
    {
       i8* bio_buffer = malloc(BYTESPERBLOCK);
       if(i == first_fbn)
       {
         bfsRead(Inum, i, bio_buffer);

         i32 first_offset = cursor_position % BYTESPERBLOCK;

         i32 fbytes = -1; 

         if(first_fbn == last_fbn)
         {
           fbytes = numb;
         }
         else if(last_fbn > first_fbn)
         {
           fbytes = BYTESPERBLOCK - first_offset;
         }

         memcpy(bio_buffer + first_offset, buf, fbytes);

         i32 first_dbn = bfsFbnToDbn(Inum, i);

         bioWrite(first_dbn, bio_buffer);

         offset += fbytes;
       }
       else if(i == last_fbn)
       {
          bfsRead(Inum, i, bio_buffer);

          i32 last_block_start = BYTESPERBLOCK * last_fbn;
          i32 lbytes = (cursor_position + numb) - last_block_start;

          memcpy(bio_buffer, buf + (numb - lbytes), lbytes);

          i32 last_dbn = bfsFbnToDbn(Inum, i);

          bioWrite(last_dbn, bio_buffer);

          offset += lbytes;
       }
       else
       {
         memcpy(bio_buffer, buf + offset, BYTESPERBLOCK);

         i32 middle_dbn = bfsFbnToDbn(Inum, i);

         bioWrite(middle_dbn, bio_buffer);

         offset += BYTESPERBLOCK; 

       }

       free(bio_buffer);
    }
  

  bfsSetCursor(Inum, cursor_position + numb);                            // Not Yet Implemented!
  return 0; 
}
