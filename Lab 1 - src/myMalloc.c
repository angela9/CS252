
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "myMalloc.h"
#include "printing.h"

/* Due to the way assert() prints error messges we use out own assert function
 * for deteminism when testing assertions
 */


#ifdef TEST_ASSERT
  inline static void assert(int e) {
    if (!e) {
      const char * msg = "Assertion Failed!\n";
      write(2, msg, strlen(msg));
      exit(1);
    }
  }
#else
  #include <assert.h>
#endif


/*
 * Mutex to ensure thread safety for the freelist
 */


static pthread_mutex_t mutex;


/*
 * Array of sentinel nodes for the freelists
 */


header freelistSentinels[N_LISTS];


/*
 * Pointer to the second fencepost in the most recently allocated chunk from
 * the OS. Used for coalescing chunks
 */

header * lastFencePost;


/*
 * Pointer to maintian the base of the heap to allow printing based on the
 * distance from the base of the heap
 */ 
void * base;


/*
 * List of chunks allocated by  the OS for printing boundary tags
 */


header * osChunkList [MAX_OS_CHUNKS];
size_t numOsChunks = 0;


/*
 * direct the compiler to run the init function before running main
 * this allows initialization of required globals
 */


static void init (void) __attribute__ ((constructor));

// Helper functions for manipulating pointers to headers
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off);
static inline header * get_left_header(header * h);
static inline header * ptr_to_header(void * p);

// Helper functions for allocating more memory from the OS
static inline void initialize_fencepost(header * fp, size_t left_size);
static inline void insert_os_chunk(header * hdr);
static inline void insert_fenceposts(void * raw_mem, size_t size);
static header * allocate_chunk(size_t size);

// Helper functions for freeing a block
static inline void deallocate_object(void * p);

// Helper functions for allocating a block
static inline header * allocate_object(size_t raw_size);

// Helper functions for verifying that the data structures are structurally 
// valid
static inline header * detect_cycles();
static inline header * verify_pointers();
static inline bool verify_freelist();
static inline header * verify_chunk(header * chunk);
static inline bool verify_tags();

static void init();

static bool isMallocInitialized;


/**
 * @brief Helper function to retrieve a header pointer from a pointer and an 
 *        offset
 *
 * @param ptr base pointer
 * @param off number of bytes from base pointer where header is located
 *
 * @return a pointer to a header offset bytes from pointer
 */


static inline header * get_header_from_offset(void * ptr, ptrdiff_t off) {
	return (header *)((char *) ptr + off);
}


/**
 * @brief Helper function to get the header to the right of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */


header * get_right_header(header * h) {
	return get_header_from_offset(h, get_block_size(h));
}


/**
 * @brief Helper function to get the header to the left of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */


inline static header * get_left_header(header * h) {
  return get_header_from_offset(h, -h->left_size);
}


/**
 * @brief Fenceposts are marked as always allocated and may need to have
 * a left object size to ensure coalescing happens properly
 *
 * @param fp a pointer to the header being used as a fencepost
 * @param left_size the size of the object to the left of the fencepost
 */


inline static void initialize_fencepost(header * fp, size_t left_size) {
	set_block_state(fp,FENCEPOST);
	set_block_size(fp, ALLOC_HEADER_SIZE);
	fp->left_size = left_size;
}


/**
 * @brief Helper function to maintain list of chunks from the OS for debugging
 *
 * @param hdr the first fencepost in the chunk allocated by the OS
 */


inline static void insert_os_chunk(header * hdr) {
  if (numOsChunks < MAX_OS_CHUNKS) {
    osChunkList[numOsChunks++] = hdr;
  }
}


/**
 * @brief given a chunk of memory insert fenceposts at the left and 
 * right boundaries of the block to prevent coalescing outside of the
 * block
 *
 * @param raw_mem a void pointer to the memory chunk to initialize
 * @param size the size of the allocated chunk
 */


inline static void insert_fenceposts(void * raw_mem, size_t size) {
  // Convert to char * before performing operations
  char * mem = (char *) raw_mem;

  // Insert a fencepost at the left edge of the block
  header * leftFencePost = (header *) mem;
  initialize_fencepost(leftFencePost, ALLOC_HEADER_SIZE);

  // Insert a fencepost at the right edge of the block
  header * rightFencePost = get_header_from_offset(mem, size - ALLOC_HEADER_SIZE);
  initialize_fencepost(rightFencePost, size - 2 * ALLOC_HEADER_SIZE);
}


/**
 * @brief Allocate another chunk from the OS and prepare to insert it
 * into the free list
 *
 * @param size The size to allocate from the OS
 *
 * @return A pointer to the allocable block in the chunk (just after the 
 * first fencpost)
 */


static header * allocate_chunk(size_t size) {
  void * mem = sbrk(size);
  
  insert_fenceposts(mem, size);
  header * hdr = (header *) ((char *)mem + ALLOC_HEADER_SIZE);
  set_block_state(hdr, UNALLOCATED);
  set_block_size(hdr, size - 2 * ALLOC_HEADER_SIZE);
  hdr->left_size = ALLOC_HEADER_SIZE;
  return hdr;
}


/**
 * @brief Helper allocate an object given a raw request size from the user
 *
 * @param raw_size number of bytes the user needs
 *
 * @return A block satisfying the user's request
 */


static inline header * allocate_object(size_t raw_size) {
  // TODO implement allocation
  if(raw_size == 0){
	return NULL;
  }
  size_t size_needed;
  size_t _size = raw_size;
  if (raw_size % sizeof(size_t) == 0) {
	size_needed = raw_size;
  }
  else {
	size_needed = raw_size - (raw_size % sizeof(size_t)) + sizeof(size_t);
  }
  if(size_needed < (sizeof(header)-ALLOC_HEADER_SIZE)){
	  size_needed = sizeof(header) - ALLOC_HEADER_SIZE;
  }
  size_t k = size_needed;
  size_needed += ALLOC_HEADER_SIZE;

  //K IS DIVIDED BY SIZE OF HEADER TO GET TO THE REQ INDEX IN THE FREELIST SINCE THE FREELIST IS INDEXED BY MULTIPLES OF 8
  if ((k/sizeof(header*)-1) >58) { 
    for(int i = (k/sizeof(header*))-1; i < N_LISTS-1; i++) {
	header * box = &freelistSentinels[i];
	if(box->next==&freelistSentinels[i]) {
		continue;
	}
  	if(box->next!=&freelistSentinels[i]) {
		header * current = &freelistSentinels[i];
		current = current->next;
		size_t siz = get_block_size(current);
		current->prev->next = current->next;
		current->next->prev = current->prev;
		if(sizeof(header) + size_needed <= get_block_size(current)) {
			header * _new = current;
			_new = get_header_from_offset(current,siz - size_needed);
			_new->left_size = siz - size_needed;
			set_block_size_and_state(_new,size_needed,ALLOCATED);
			set_block_size_and_state(current,siz - size_needed,UNALLOCATED);
			size_t tm = get_block_size(current);
			tm = tm - sizeof(header*)*2;
			tm = (tm/sizeof(header*)) - 1;
			header * m = &freelistSentinels[tm];
			current->next = m->next;
			current->prev = m;
			m->next->prev = current;
			m->next = current;
			header * temp = get_header_from_offset(_new,get_block_size(_new));
			temp->left_size = get_block_size(_new);
			_new = get_header_from_offset(_new,ALLOC_HEADER_SIZE);
			return _new;
		}
  		else {
			set_block_state(current,ALLOCATED);
			current = get_header_from_offset(current,ALLOC_HEADER_SIZE);
			return current;
		}
	}
    }
  }
  //WHEN THE FREELIST INDEX IS BEYOND 58
  header * q = &freelistSentinels[N_LISTS-1];
  q = q->next;
  while(&freelistSentinels[N_LISTS-1]!= q) {
	  if(get_block_size(q) >= size_needed) { 
		  if(sizeof(size_t)*2+ALLOC_HEADER_SIZE + size_needed <= get_block_size(q)) {
			  size_t siz = get_block_size(q);
			  header * _new = get_header_from_offset(q,siz-size_needed);
			  set_block_size_and_state(q,siz - size_needed,UNALLOCATED);
			  _new->left_size =  siz - size_needed;
			  set_block_size_and_state(_new,size_needed,ALLOCATED);
			  size_t tm = get_block_size(q);
			  tm = tm - ALLOC_HEADER_SIZE;
			  tm = (tm/sizeof(size_t))-1;
			  if(tm>=N_LISTS-1) {
				  tm = N_LISTS-1;
			  }
			  siz-= ALLOC_HEADER_SIZE;
			  siz = (siz/sizeof(size_t))-1;
			  if(siz >= N_LISTS-1) {
				  siz = N_LISTS-1;
			  }
			  if (siz != tm) {
			   q->prev->next = q->next;
			   q->next->prev = q->prev;
			   header * m = &freelistSentinels[tm];
			   q->next = m->next;
			   q->prev = m;
			   m->next->prev = q;
			   m->next = q;
			  }
		  header * temp = get_header_from_offset(_new,get_block_size(_new));
		  temp->left_size = get_block_size(_new);
		  _new = get_header_from_offset(_new,ALLOC_HEADER_SIZE);
		  return _new;
		  }
                  q->prev->next = q->next;
		  q->next->prev = q->prev;
		  set_block_state(q,ALLOCATED);
		  q = get_header_from_offset(q,ALLOC_HEADER_SIZE);
		  return q;
	  }
	  else {
		  q = q->next;
	  }
  }
  return allocate_object(raw_size);  
}



/**
 * @brief Helper to get the header from a pointer allocated with malloc
 *
 * @param p pointer to the data region of the block
 *
 * @return A pointer to the header of the block
 */


static inline header * ptr_to_header(void * p) {
  return (header *)((char *) p - ALLOC_HEADER_SIZE); //sizeof(header));
}


/**
 * @brief Helper to manage deallocation of a pointer returned by the user
 *
 * @param p The pointer returned to the user by a call to malloc
 */
static inline void deallocate_object(void * p) {
  // TODO implement deallocation
  if(p == NULL) {
	  return;
  }
  header * objHeader = ptr_to_header(p);
  if (get_block_state(objHeader) == ALLOCATED) {
	set_block_state(objHeader, UNALLOCATED);
  }
  //DOUBLE FREE DETECTION
  else {
	printf("Double Free Detected\n");  
	assert(false);
	return;
  }
  if (get_block_size(objHeader) == 0) {
	return;
  }
  header * left = get_left_header(objHeader);
  int boolean = false;
  header * right = get_right_header(objHeader);

  //IF RIGHT IS FREE
  if(get_block_state(right) == UNALLOCATED) {
	 size_t rb = get_block_size(right);
	 rb = rb - ALLOC_HEADER_SIZE;
	 rb = (rb/sizeof(size_t))-1;
	 if(rb >= N_LISTS-1){
		 rb = N_LISTS - 1;
	 }
	 set_block_size_and_state(objHeader, get_block_size(right) + get_block_size(objHeader),UNALLOCATED);
	 size_t siz = get_block_size(objHeader);
	 siz -=  ALLOC_HEADER_SIZE;
	 siz = (siz/sizeof(size_t))-1;
	 if(siz >= N_LISTS-1){
		 siz = N_LISTS - 1;
	 }
	 right->prev->next = right->next;
	 right->next->prev = right->prev;
	 header * l = &freelistSentinels[siz];
	 objHeader->next = l->next;
	 objHeader->prev = l;
	 l->next->prev = objHeader;
	 l->next = objHeader;
	 header * temp = get_header_from_offset(objHeader, get_block_size(objHeader));
	 temp->left_size = get_block_size(objHeader);
	 boolean = true;
 }
 //IF LEFT IS FREE
  if(get_block_state(left) == UNALLOCATED){
	size_t lb = get_block_size(left);
	lb = lb - ALLOC_HEADER_SIZE;
	lb = (lb/sizeof(size_t)) - 1;
	if(lb >= N_LISTS - 1){
		lb = N_LISTS - 1;
	}
	set_block_size_and_state(left,get_block_size(left) + get_block_size(objHeader),UNALLOCATED);
	header * x = objHeader;
	objHeader = left;
	size_t siz = get_block_size(objHeader) - ALLOC_HEADER_SIZE;
	siz = (siz/sizeof(size_t)) - 1;
	if(siz >= N_LISTS-1) {
		siz = N_LISTS - 1;
	}
	left->prev->next = left->next;
	left->next->prev = left->prev;
	header * l = &freelistSentinels[siz];
	left->next = l->next;
	left->prev = l;
	l->next->prev = left;
	l->next = left;
	if(boolean == true){
		x->next->prev = x->prev;
		x->prev->next = x->next;
	}
	header * temp = get_header_from_offset(objHeader, get_block_size(objHeader));
	temp->left_size = get_block_size(objHeader);
	boolean = true;
  }
  size_t siz = get_block_size(objHeader);
  size_t temp = siz - ALLOC_HEADER_SIZE;
  temp = (temp/sizeof(size_t))-1;
  if(temp >= N_LISTS) {
	  temp = N_LISTS - 1;
  }
  if(boolean == true) {
	  return;
 }
 header * l = &freelistSentinels[temp];
 objHeader->next = l->next;
 objHeader->prev = l;
 l->next->prev = objHeader;
 l->next = objHeader;
}



/**
 * @brief Helper to detect cycles in the free list
 * https://en.wikipedia.org/wiki/Cycle_detection#Floyd's_Tortoise_and_Hare
 *
 * @return One of the nodes in the cycle or NULL if no cycle is present
 */
static inline header * detect_cycles() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * slow = freelist->next, * fast = freelist->next->next; 
         fast != freelist; 
         slow = slow->next, fast = fast->next->next) {
      if (slow == fast) {
        return slow;
      }
    }
  }
  return NULL;
}


/**
 * @brief Helper to verify that there are no unlinked previous or next pointers
 *        in the free list
 *
 * @return A node whose previous and next pointers are incorrect or NULL if no
 *         such node exists
 */
static inline header * verify_pointers() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * cur = freelist->next; cur != freelist; cur = cur->next) {
      if (cur->next->prev != cur || cur->prev->next != cur) {
        return cur;
      }
    }
  }
  return NULL;
}


/**
 * @brief Verify the structure of the free list is correct by checkin for 
 *        cycles and misdirected pointers
 *
 * @return true if the list is valid
 */
static inline bool verify_freelist() {
  header * cycle = detect_cycles();
  if (cycle != NULL) {
    fprintf(stderr, "Cycle Detected\n");
    print_sublist(print_object, cycle->next, cycle);
    return false;
  }

  header * invalid = verify_pointers();
  if (invalid != NULL) {
    fprintf(stderr, "Invalid pointers\n");
    print_object(invalid);
    return false;
  }

  return true;
}


/**
 * @brief Helper to verify that the sizes in a chunk from the OS are correct
 *        and that allocated node's canary values are correct
 *
 * @param chunk AREA_SIZE chunk allocated from the OS
 *
 * @return a pointer to an invalid header or NULL if all header's are valid
 */


static inline header * verify_chunk(header * chunk) {
	if (get_block_state(chunk) != FENCEPOST) {
		fprintf(stderr, "Invalid fencepost\n");
		print_object(chunk);
		return chunk;
	}
	
	for (; get_block_state(chunk) != FENCEPOST; chunk = get_right_header(chunk)) {
		if (get_block_size(chunk)  != get_right_header(chunk)->left_size) {
			fprintf(stderr, "Invalid sizes\n");
			print_object(chunk);
			return chunk;
		}
	}
	
	return NULL;
}



/**
 * @brief For each chunk allocated by the OS verify that the boundary tags
 *        are consistent
 *
 * @return true if the boundary tags are valid
 */


static inline bool verify_tags() {
  for (size_t i = 0; i < numOsChunks; i++) {
    header * invalid = verify_chunk(osChunkList[i]);
    if (invalid != NULL) {
      return invalid;
    }
  }

  return NULL;
}


/**
 * @brief Initialize mutex lock and prepare an initial chunk of memory for allocation
 */


static void init() {
  // Initialize mutex for thread safety
  pthread_mutex_init(&mutex, NULL);

#ifdef DEBUG
  // Manually set printf buffer so it won't call malloc when debugging the allocator
  setvbuf(stdout, NULL, _IONBF, 0);
#endif // DEBUG

  // Allocate the first chunk from the OS
  header * block = allocate_chunk(ARENA_SIZE);

  header * prevFencePost = get_header_from_offset(block, -ALLOC_HEADER_SIZE);
  insert_os_chunk(prevFencePost);
  lastFencePost = get_header_from_offset(block, get_block_size(block));

  // Set the base pointer to the beginning of the first fencepost in the first
  // chunk from the OS
  base = ((char *) block) - ALLOC_HEADER_SIZE; //sizeof(header);

  // Initialize freelist sentinels
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    freelist->next = freelist;
    freelist->prev = freelist;
  }

  // Insert first chunk into the free list
  header * freelist = &freelistSentinels[N_LISTS - 1];
  freelist->next = block;
  freelist->prev = block;
  block->next = freelist;
  block->prev = freelist;
}


/* 
 * External interface
 */


void * my_malloc(size_t size) {
  pthread_mutex_lock(&mutex);
  header * hdr = allocate_object(size); 
  pthread_mutex_unlock(&mutex);
  return hdr;
}

void * my_calloc(size_t nmemb, size_t size) {
  return memset(my_malloc(size * nmemb), 0, size * nmemb);
}

void * my_realloc(void * ptr, size_t size) {
  void * mem = my_malloc(size);
  memcpy(mem, ptr, size);
  my_free(ptr);
  return mem; 
}

void my_free(void * p) {
  pthread_mutex_lock(&mutex);
  deallocate_object(p);
  pthread_mutex_unlock(&mutex);
}

bool verify() {
  return verify_freelist() && verify_tags();
}
