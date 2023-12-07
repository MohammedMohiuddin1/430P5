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

  //make a temporary bio buffer
  i8 bio_buffer[BYTESPERBLOCK]; 

  //variable to keep track of how many bytes of data we have left to read. 
  i32 remaining_bytes = numb;

  //get inum of the file to be read
  i32 Inum = bfsFdToInum(fd);

  //get cursor position of file
  i32 cursor_position = fsTell(fd);

  //get current block. This will hold the the index of the current block
  i32 current_block = cursor_position / BYTESPERBLOCK;

  //offset within the current block to account for the cursor
  i32 remainder = cursor_position % BYTESPERBLOCK;
  
  //offset to traverse the buffer
  i32 byte_offset = 0; 
  
  //used for end of file case
  i32 start = cursor_position;

  while(remaining_bytes > 0) //while there are still bytes of data continue reading blocks
  {
     bfsRead(Inum, current_block, bio_buffer); //read contents of block into buffer
  
     //if there is a remainder
     if(remainder > 0) 
     {
        //get the bytes that we have left 
        i32 bytes_left = BYTESPERBLOCK - remainder; 

        //determine how much we have to read
        if(remaining_bytes > bytes_left) //case for if there are more bytes to read than bytes left in the current block
        {
            remaining_bytes -= bytes_left;
        }
        else //case for if the current bytes left in this block can satisfy remaining bytes. 
        {
            bytes_left = remaining_bytes;
            remaining_bytes = 0;
        }
        //copy data from bio_buffer to actual buffer (buf)
        memcpy(buf + byte_offset, bio_buffer + remainder, bytes_left);
        
        //update the byte offset, set remainder to 0, and move the file cursor. 
        byte_offset += bytes_left;
        remainder = 0;
        fsSeek(fd, bytes_left, SEEK_CUR);

      }
      else //case for if there is no remainder (process entire block)
      {  
         if(remaining_bytes >= BYTESPERBLOCK) 
         {
           memcpy(buf + byte_offset, bio_buffer, BYTESPERBLOCK);
           remaining_bytes -= BYTESPERBLOCK;
           byte_offset += BYTESPERBLOCK;
           fsSeek(fd, BYTESPERBLOCK, SEEK_CUR);
         }
         else //final case for if remaining bytes left in block are less than the usual size (512)
         {
          memcpy(buf + byte_offset, bio_buffer, remaining_bytes);
          fsSeek(fd, remaining_bytes, SEEK_CUR);
          remaining_bytes = 0;

         }

      }
      //if there are bytes left to process then adjust index to the next block
      if(remaining_bytes != 0)
      {
        current_block++;
      }
  }
      //test case for end of file
      if(start + numb > fsSize(fd))
      {
        fsSeek(fd, fsSize(fd), SEEK_SET); //move cursor back if we have passed End of file

        i32 total_read = fsSize(fd) - start;

        return total_read; //return how much we were able to read
      }
      
      return numb; //return numb for the cases where we don't deal with end of file
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

    //make a temporary bio buffer
    i8 bio_buffer[BYTESPERBLOCK];

    //variable to keep track of how many bytes of data we have left to read. 
    i32 remaining_bytes = numb;

    //get inum of the file to be read
    i32 Inum = bfsFdToInum(fd);
    
    //get cursor position of file
    i32 cursor_position = fsTell(fd);

    //get current block starting from the first FBN going all the way until the last. This will hold the the index of the current block
    i32 current_block = cursor_position / BYTESPERBLOCK;

    // Find where the cursor is
    i32 remainder = cursor_position % BYTESPERBLOCK;

    //offset to traverse the buffer
    i32 byte_offset = 0;

    //get total file size (size after writing the numb bytes)
    i32 total_size = cursor_position + numb;
    
    //case for determining whether we need to allocate more blocks (allocation has exceeded file size)
    if (fsSize(fd) < total_size) 
    {

      //determine current number of blocks that file has 
      i32 num_of_current_blocks = 0;

      //file has enough space fills up perfectly into whole blocks
      if (fsSize(fd) % BYTESPERBLOCK == 0) 
      {
         num_of_current_blocks = fsSize(fd) / BYTESPERBLOCK;
      }
      //doesn't fit evenly we have some space in the last block (so we add a + 1 to account for this)
      else if(fsSize(fd) % BYTESPERBLOCK != 0) 
      {
         num_of_current_blocks = fsSize(fd) / BYTESPERBLOCK + 1;
      }

    //variable for current space
    i32 current_space = num_of_current_blocks * BYTESPERBLOCK;

      
      if (current_space < total_size) 
      {

        //determine how many blocks we need to allocated based on memory needed
        i32 needed_memory = total_size - current_space;
        i32 needed_blocks = 0;

        //needed memory can fit into whole blocks evenly
        if (needed_memory % BYTESPERBLOCK == 0) 
        {
          needed_blocks = needed_memory / BYTESPERBLOCK;
        }
        //needed memory doesn't fit into whole blocks evenly and has some partial space (we add a + 1 to account for that)
        else if (needed_blocks % BYTESPERBLOCK != 0) 
        {
          needed_blocks = needed_memory / BYTESPERBLOCK + 1;
        }
    
        //new size with added blocks
        i32 new_size = num_of_current_blocks + needed_blocks;

        //extend with new size
        bfsExtend(Inum, new_size);
      }

      //update the size of the file
      bfsSetSize(Inum, total_size);
    
    }
    
    remaining_bytes = numb;

    while (remaining_bytes > 0) //as long as we have bytes to write
    {
        if (remainder > 0) //if there is a remainder (we don't start at the beginning of a block)
        { 
            //get the bytes that we have left
            i32 bytes_left = BYTESPERBLOCK - remainder; 

            //determine how many bytes we have to read
            if (remaining_bytes > bytes_left) //case for if there are more bytes to write than bytes left in the current block
            {
                remaining_bytes -= bytes_left;
            } 
            else //case for if the current bytes left in this block can satisfy remaining bytes. 
            { 
                bytes_left = remaining_bytes;
                remaining_bytes = 0;
            }

            //perform a bioRead into the bio_buffer
            i32 dbn = bfsFbnToDbn(Inum, current_block);
            bioRead(dbn, bio_buffer);

            //write to the bio buffer starting at remainder
            memcpy(bio_buffer + remainder, buf + byte_offset, bytes_left);
            byte_offset += bytes_left;
            remainder = 0;

            //write the updated buffer back to the block using bioWrite
            bioWrite(dbn, bio_buffer);

            //move the cursor
            fsSeek(fd, bytes_left, SEEK_CUR);
        } 
        else 
        {   

            // bioRead into bio buffer
            i32 dbn = bfsFbnToDbn(Inum, current_block);
            bioRead(dbn, bio_buffer);

            
            if (remaining_bytes >= BYTESPERBLOCK) //as long as remaining bytes are >= 512, write the entire block
            { 

                //copy a full block of bytes to bio buffer
                memcpy(bio_buffer, buf + byte_offset, BYTESPERBLOCK);

                // Update remaining bytes and byte offset
                remaining_bytes -= BYTESPERBLOCK;
                byte_offset += BYTESPERBLOCK;

                //Write the updated bio buffer back to the block using bioWrite
                bioWrite(dbn, bio_buffer);

                //move the cursor
                fsSeek(fd, BYTESPERBLOCK, SEEK_CUR);
            } 
            else //final case for if remaining bytes left in block are less than the usual size (512) 
            { 
                // copy the remaining bytes from buf to bio buffer
                memcpy(bio_buffer, buf + byte_offset, remaining_bytes);

                //write the updated bio buffer back to that block using bioWrite
                bioWrite(dbn, bio_buffer);

                //move the cursor
                fsSeek(fd, remaining_bytes, SEEK_CUR);

                //set remaining bytes to 0 since we have nothing left
                remaining_bytes = 0;
            }
        }

        //if there are bytes left to process then adjust index to the next block
        if (remaining_bytes != 0) 
        {
            current_block++;
        }
    }

    return 0;
}
