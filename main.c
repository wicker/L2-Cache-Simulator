/* main.c
 * Jen Hanni
 * 
 * The main reads the file 'test.txt' and 
 * pass on the command and addresses to the appropriate functions.
 * 
 * test.txt contains lines of the form:
 * 2 10019d94
 * where the first single digit is 'n' 
 * and the second string is the address in hex
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

// only change these lines when changing total size of the cache! 
// LINES is the total number of lines (16384 for a 16K line cache)
// WAYS is the total number of ways (columns) in that cache

#define LINES 16384
#define WAYS 4

// do not touch these global variables!

#define MAXLINE (LINES - 1)
#define MAXWAY (WAYS - 1)

#define M 0
#define E 1
#define S 2 
#define I 3

typedef struct  
{
  int MESIbits;
  int LRUbits;
  int tag;
  int address;
} cacheLine;

// initialize a multi-dimensional array [row][col]
// creates the single global cache
cacheLine L2cache[MAXLINE][MAXWAY];

FILE *ifp,*ofpD,*ofp;

// step through the cache and set initial values
// LRU bit will be the same value as the way
// MESI bit begins invalid (empty)

void setLRUbitsToWay()
{
  int index;
  for (index = 0; index <= MAXLINE; index++)
  {
    int way;
    int LRUBIT = MAXWAY;
    for (way = LRUBIT; way >= 0; way--)
    {
       L2cache[index][way].MESIbits = I;
       L2cache[index][way].LRUbits = way;
       LRUBIT--;
    }
  }
}

// check every way in the given index to see if the given tag exists
// tag in each way of the appropriate index
int checkTag(int index, int tag)
{
  for (way = 0; way <= MAXWAY; way++)
  {
    if (L2cache[index][way].tag == tag)
       return way;
  } 
  return WAYS;
}

// given an index, this function returns the least recently used way
// LRU bits of 0 give the least recently used way
// LRU bits of MAXWAY give most recently used way
int checkLRU(int index)
{
  for (way = 0; way <= MAXWAY; way++) 
  {
    if (L2cache[index][way].LRUbits == 0)
       return way;
  }
  return WAYS; 
}

// this function updates the LRU bits to reflect a new most recently used way
void updateLRU(int index, int ourway)
{
  // if the LRU bits of our way are already the most recently used, we do nothing
  if (L2cache[index][ourway].LRUbits == MAXWAY)
     break;
  else 
  {
     int ourbits = L2cache[index][ourway].LRUbits++;
     for (ourbits; ourbits <= MAXWAY; ourbits++)
     {
         for (way = 0; way <= MAXWAY; way++)
         {
            if (L2cache[index][ourway].LRUbits == ourbits)
               break;
         }
     L2cache[index][ourway].LRUbits = ourbits--;
     }
  }
}

int main()
{
  memset(L2cache, 0, sizeof (cacheLine));

  // initialize the counters
  int refCount = 0;
  int readCount = 0;
  int writeCount = 0;
  int hitCount = 0;
  int missCount = 0;
  int hitM = 0;
  int hit = 0;
  int way;

  // initialize the input from the tracefile
  int n;
  int addr;

  // initialize the LRU bits to the way
  setLRUbitsToWay();
  
  // open the tracefile, make it available to 'r' read
  // open the output file to make it available to append each iteration's result
  ifp = fopen("testfile.din", "r");
  ofp = fopen("testout.txt", "a");

  fprintf(ofp,"-----------------------------------------------------------------------------------------------------\n");
  fflush(ofp);

  // set it up to read line by line
  // set n and addr accordingly
  while (fscanf(ifp, "%d %x\n", &n, &addr) != EOF) 
  {
    // parse 32-bit hex address
    int tag = addr >> 20;
    int index = (addr >> 6) & 16383;
    // get 1 bit from the fifth position to check later whether 1 or 0 
    int byteSelect = (addr >> 5) & ~(~0 << 1);
    refCount++;

    fprintf(ofp,"Reference: %d\n",refCount);
    fflush(ofp);

    switch (n) 
    {
      // the L1 cache makes a read request 
      // n = 0 read data request from L1 cache
      // n = 2 instruction fetch (treated as a read request from L1 cache)
      case 0:
      case 2:
	readCount++;
	// determine whether this tag exists in the cache and, if so, which way
        way = checkTag(index, tag);
        // if the tag exists
	if (way == 0 || way == 1 || way == 2 || way == 3)
	{
	   // this tag exists and it's valid as per its MESI bits
	   int MESI = L2cache[index][way].MESIbits;
	   if (MESI == M || MESI == E || MESI == S)
	   {
	      hitCount++;
	      // update the LRU to set 'way' to least recently used
	      updateLRU(index, way);
	      dataToL1(byteSelect,index,tag,way);
              // MESI remains unchanged
              // copy the addr to address in the cacheLine struct for case 9 display
              L2cache[index][way].address = addr;
  	   }
           // this tag exists but it's been invalidated and can't be used
	   else 
           {
	      missCount++;
	      dataFromDRAM(addr,index,way,refCount);
              dataToL1(byteSelect,index,tag,way);
              // update LRU
              updateLRU(index, way);
	      L2cache[index][way].MESIbits = E;
              L2cache[index][way].address = addr;
	   }
	}
        // this tag simply doesn't exist in the cache in any form
        else
  	{
	   missCount++;
	   // use the LRU bit to determine which way to evict
	   way = checkLRU(index);
           // update LRU
           updateLRU(index, way);
           dataFromDRAM(addr,index,way,refCount);
           L2cache[index][way].tag = tag;
           dataToL1(byteSelect,index,tag,way);
	   L2cache[index][way].MESIbits = E;
           L2cache[index][way].address = addr;
        }
	break;
      // 1 write data request from L1 cache
      case 1:
	writeCount++; 
	// determine whether this tag exists in the cache and, if so, which way
        way = checkTag(index, tag);
        // if the tag exists
	if (way == 0 || way == 1 || way == 2 || way == 3)
	{
	   // this tag exists so check if it's valid as per its MESI bits
	   int MESI = L2cache[index][way].MESIbits;
	   if (MESI == M || MESI == E || MESI == S)
	   {
	      hitCount++;
	      // update the LRU to set 'way' to least recently used
	      updateLRU(index, way);
              // set MESI to modified because only we have it now
	      L2cache[index][way].MESIbits = E;
	      // because L2 has everything in L1, we send contents of our L2
              // from our copy of that index/way to the DRAM
	      dataToDRAM(addr,index,way,refCount);
              L2cache[index][way].MESIbits = M;
              L2cache[index][way].address = addr;
           }
           // this tag exists but its MESI bits say invalid, needs to be updated
           // in this case we have to get the data from L1 and expand it to 64 bytes
           else
           {
              missCount++;
              // update LRU
              updateLRU(index, way);
              // set MESI to Modified
	      L2cache[index][way].MESIbits = E;
	      dataFromL1(index, way, addr, byteSelect);
	      dataToDRAM(addr,index,way,refCount);
	      // MESI = Exclusive after memory write
	      L2cache[index][way].MESIbits = M;
              L2cache[index][way].address = addr;
           }
        }
        // this tag simply doesn't exist in the cache in any form
        // this covers the very unlikely odd case where L1 has what L2 doesn't
        else
  	{
	   missCount++;
	   // use the LRU bit to determine which way to evict
	   way = checkLRU(index);
	   L2cache[index][way].MESIbits = E;
	   dataFromL1(index, way, addr, byteSelect);
           L2cache[index][way].tag = tag;
	   dataToDRAM(addr,index,way,refCount);
           // update LRU
           updateLRU(index, way);
           // MESI = Exclusive after memory write
	   L2cache[index][way].MESIbits = M;
           L2cache[index][way].address = addr;
        }
      
      break;
      // 4 snooped a read request from another processor
      case 4:
	readCount++;
	// determine whether this tag exists in the cache and, if so, which way
        way = checkTag(index, tag);
        // if the tag exists
	if (way == 0 || way == 1 || way == 2 || way == 3)
	{
	   // this tag exists so check if it's valid as per its MESI bits
	   int MESI = L2cache[index][way].MESIbits;
           // if the tag exists but it's modified
	   if (MESI == M)
	   {
	      hitCount++;
	      hitM++;
	      // first response is to read out our modified copy to other cache
              dataToSnooped(addr);
              // then we write to our L1, just in case, and on to DRAM
	      dataToL1(byteSelect,index,tag,way);
              dataToDRAM(addr,index,way,refCount); 
              // set MESI to shared after mem write to other processor completed
	      L2cache[index][way].MESIbits = S;
              L2cache[index][way].address = addr;
           }
           // if the tag exists but it's exclusive
           else if (MESI == E || MESI == S)
           {
              hitCount++;
              hit++;
	      dataToSnooped(addr);
	      dataToL1(byteSelect,index,tag,way);
              // set MESI to shared after mem write to other processor completed
	      L2cache[index][way].MESIbits = S;
              L2cache[index][way].address = addr;
           }
	   // if the tag exists but it's invalid -- we don't have it
           else
           {
              missCount++;
           }
        }
      break;
      // 3 invalidate command or snooped write request
      case 3:
        way = checkTag(index, tag);
	// if the tag exists
	if (way == 0 || way == 1 || way == 2 || way == 3)
	{
          // this tag exists so check if it's valid as per its MESI bits
          int MESI = L2cache[index][way].MESIbits;
	  if (MESI == I)
  	  {
	     missCount++;
	  }
	  else if (MESI == M || MESI == E || MESI == S)
	  {
	     L2cache[index][way].MESIbits = I;
	     hitCount++;
             L2cache[index][way].address = addr;
	  }
        // if we don't have it, we do nothing
        }
      break;
      // snooped write request from another processor		
      case 5:
        writeCount++;
        way = checkTag(index, tag);
        // if the tag exists
	if (way == 0 || way == 1 || way == 2 || way == 3)
	{
	   // this tag exists so check if it's valid as per its MESI bits
	   int MESI = L2cache[index][way].MESIbits;
           // if the tag exists but it's modified
	   if (MESI == M || MESI == E || MESI == S)
              {
                 hitCount++;
	         L2cache[index][way].MESIbits = I;
                 L2cache[index][way].address = addr;
              }
	   else
           {
              missCount++;
           }
        // if we don't have it, we ignore entirely
        }
        else
           missCount++;
      break;
      // 6 snooped read for ownership request
      case 6:
	readCount++;
        way = checkTag(index, tag);
        // if the tag exists
	if (way == 0 || way == 1 || way == 2 || way == 3)
	{
   	   // this tag exists so check if it's valid as per its MESI bits
	   int MESI = L2cache[index][way].MESIbits;
           // if the tag exists but it's modified, we can serve this request
           // before setting our MESI bits to invalid
	   if (MESI == M || MESI == E || MESI == S)
           {
              hitCount++;
              if (MESI == M) 
              {
                 hitM++;
              }
              dataToSnooped(addr);
	      invalidateInL1(addr);
	      L2cache[index][way].MESIbits = I; 
              // LRU update not necessary here because our algorithm checks for 
              // empty (invalid MESI bit) lines first before checking LRU bits
              L2cache[index][way].address = addr;
           }
           else
           {
              missCount++;
           }
        // if we don't have it, we do nothing
        }
      break;
      // 8 clear the cache and refresh all states
      case 8:
        // Use the algorithm for n = 3||5 and step through all indices and ways.
        // No MESI, LRU, or counter updates here, and no display.
        way = 0;
        
	for (index = 0; index < 16384; index++)
        {
           L2cache[index][0].MESIbits = 3;
           L2cache[index][0].LRUbits = 0;
           L2cache[index][1].MESIbits = 3;
           L2cache[index][1].LRUbits = 1;
           L2cache[index][2].MESIbits = 3;
           L2cache[index][2].LRUbits = 2;
           L2cache[index][3].MESIbits = 3;
           L2cache[index][3].LRUbits = 3;
           L2cache[index][way].address = addr;
	}         

      break;
      // 9 print everything, destroy nothing
      case 9:
	// Print contents of L2 cache in a pretty, pretty box to an output file

        ofpD = fopen("display.txt", "a");

	fprintf(ofpD,"------------------------------------------------------\n");
	fprintf(ofpD,"At reference number %d, the L2 cache looked like this.\n\n",refCount);
   fflush(ofpD);

        for (index = 0; index < 16384; index++)
        {
           for (way = 0; way < 4; way++)
           {
               if (L2cache[index][way].MESIbits != I)
               {
                  fprintf(ofpD,"Index %d way %d tag %d LRUbits %d MESIbits %d\n\n",index,way,tag,L2cache[index][way].LRUbits,L2cache[index][way].MESIbits);
   fflush(ofpD);
               }
           }
        }

        break;
    // end switch statement
    }
  // print the counters in a neat form here at the end of each loop for testing

  float hitRatio;
  int total = hitCount + missCount;
  hitRatio = (float) hitCount / total;

  if ( n >= 0 && n < 8)
  {
  fprintf(ofp, "\ntag %d stored in index %d at way %d\n",tag,index,way);
   fflush(ofp);
  fprintf(ofp, "n address    hitCount missCount LRU: way0  way1 way2 way3 MESI  readCount writeCount hitRatio\n");
   fflush(ofp);

  fprintf(ofp, "%-1d 0x%-8x %-8d %-15d %-5d %-4d %-4d %-4d %-4d %-9d %-11d %-4f\n",
           n, addr, hitCount, missCount, L2cache[index][0].LRUbits, L2cache[index][1].LRUbits, L2cache[index][2].LRUbits, L2cache[index][3].LRUbits, L2cache[index][way].MESIbits, readCount, writeCount, hitRatio);
   fflush(ofp);

  }
  else if (n == 8)
  {
  fprintf(ofp, "the MESI bits of every line in this cache were set to invalid\n");
   fflush(ofp);
  }
  else if (n == 9)
  {
  fprintf(ofp, "all valid lines with contents, LRU bits, and MESI bits in display.txt\n");
   fflush(ofp);
  }
  fprintf(ofp,"--------------------------------------------------------------------------------------------------\n");
   fflush(ofp);

  // end while loop
  }

  float hitRatio = (float) hitCount / refCount;
  
  printf(" Total References: %d\n Reads: %d\n Writes: %d\n Hits: %d\n Misses %d\n Hit ratio: %f\n",refCount,readCount,writeCount,hitCount,missCount,hitRatio);

  return 0; 
// end of main() function
}
