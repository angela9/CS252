Part 2:

The output of thr1 is infinite printing of A, B, and C. (after modifying for step 4 there is also D and E printed). The printing is also random.
This is because there are multiple threads calling printC and after a very short time each thread will yield and let the next thread execute.
In thr2, printC("C") is called before creating any threads. And as this is called without a thread, it wont yield and will get stuck in an infinite loop of printing C.


Part 4:

						System (Kernel) Time 	User Time 		Real Time
pthread_mutex(count)				0.63s			1.325s			1.086s
spint lock (count_spin with thr_yield)		0.17s			0.60s			0.396s
spin lock (count spin without thr_yield)	0.00s			3.65s			1.864s

1) There is a huge difference in user time in with thr_yield and without thr_yield because only 1 thread executes at a time. Since there is a sequential execution and not a parallel one, the time required to carry out the task is not halved, as it would have been with the yielding of the thread.
2) Spin lock with yield is faster than mutex lock. This is because count runs entirely in user space and provides each thread with mutually exclusive locks on the portion of the code being executed. When thr_yield is used, the threads wait in system space instead of user space.


Part 6:

1) 4 bytes
2) ./buffer `printf "abcabcabcabcxyz\xef\xbe\xad\xde"`
3) This will exploit buffer overflow because the first part of the string fills up the buffer (15 characters) and the set_me gets overwritten.
