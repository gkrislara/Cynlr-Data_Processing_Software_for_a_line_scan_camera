#pragma once

#include <stdint.h>
#include <mutex>
#include <memory>
#include <random>
#ifdef  _WIN32	
#include <windows.h>              // _kbhit, _getwch
#include <tlhelp32.h>
#endif

// Cache line size for alignment
constexpr size_t CACHELINE_SIZE = 64; 

#define thread_safety 0

/* Apps like DGB, DFB DLB and TCB can extend the circular buffer with their algorithms*/

template <typename T>
class CircularBufferFilterW {
#ifdef _WIN32
	alignas(CACHELINE_SIZE) T* buffer; 
#elif
	T* buffer;
#endif
	size_t head;
	size_t tail;
	size_t count;
	size_t bufferSize;
#if thread_safety
	std::mutex mtx;
#endif

public:
	
	CircularBufferFilterW(size_t size, size_t headPos)
	{
		bufferSize = size;
		buffer = new T[bufferSize];
		memset(buffer, 0, sizeof(T) * bufferSize);
		head = headPos;
		tail = 0; 
		count = head - tail;
	}

	~CircularBufferFilterW() 
	{
		clear(); // Clear the buffer on destruction
		delete[] buffer;
	}

	void push(T value) 
	{
#if thread_safety
		std::lock_guard<std::mutex> lock(mtx);
#endif
			
		buffer[head] = value;
		head = (head + 1) % bufferSize;
		if (head == tail)//isFull()) 
		{ // If head catches up with tail, we overwrite the oldest item
			tail = (tail + 1) % bufferSize;
		}
		count++;
	}


	void pop(T* value) 
	{
#if thread_safety
		std::lock_guard<std::mutex> lock(mutex);
#endif
		*value = buffer[tail];
		tail = (tail + 1) % bufferSize;
		if (tail == head) { // If tail catches up with head, we reset the buffer
			clear();
		}
		count--;

	}
	
	size_t level() const { return count;  }

	size_t capacity() const { return bufferSize; }
	
	bool isFull() const { return (((head + 1) % bufferSize) == tail) ; }
	
	bool isEmpty() const { return ((head == 0) && (tail == 0)); }
	
	void clear() {
#if thread_safety
		std::lock_guard<std::mutex> lock(mtx);
#endif
		head = 0;
		tail = 0;
		count = 0;
	}

	size_t getHead()
	{
		return head;
	}

	size_t getTail()
	{
		return tail;
	}

	T getBufferAt(size_t i)
	{
		return buffer[i];
	}
	void setBufferAt(size_t i,T val)
	{
		buffer[i] = val;
	}

	size_t bufCount(T val)
	{
		size_t cnt = 0;
		for (size_t i = tail ; i != head  ; i = (i + 1) % bufferSize)
		{
			if (buffer[i] == val)
				cnt++;
		}
		return cnt;
	}


};


#ifdef _WIN32

/* extend this to more general xorshift*/
class xorshiftW {
	//HSD ~PRNG
	__m128i s;
	uint64_t initial_state[2];
	uint64_t s0;
	uint64_t s1;

	void update()
	{
		// Perform the shifts and XORs using _mm_xor_si128
		// Create new __m128i values for intermediate steps
		__m128i temp_s0 = _mm_cvtsi64_si128(s0); // Convert 64-bit to 128-bit for SIMD ops
		__m128i temp_s1 = _mm_cvtsi64_si128(s1);

		// s1 ^= s1 << 23; 
		temp_s1 = _mm_xor_si128(temp_s1, _mm_slli_epi64(temp_s1, 23));

		// s = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
		temp_s1 = _mm_xor_si128(temp_s1, temp_s0);
		temp_s1 = _mm_xor_si128(temp_s1, _mm_srli_epi64(temp_s1, 18));
		temp_s1 = _mm_xor_si128(temp_s1, _mm_srli_epi64(temp_s0, 5));

		// Update the state with the modified values
		s = _mm_set_epi64x(_mm_cvtsi128_si64(temp_s1), _mm_cvtsi128_si64(temp_s0)); // Set higher 64 bits with temp_s1, lower with temp_s0
	}

public:
	void setup(uint64_t init0, uint64_t init1)
	{
		s0 = init0;
		initial_state[0] = s0;
		s1 = init1;
		initial_state[1] = s1;
		/* initial s*/
		s = _mm_loadu_si128((__m128i*)initial_state);
	}

	uint64_t next() {
		// Extract the two 64-bit components from 's'
		// _mm_cvtsi128_si64 extracts the lower 64 bits
		s0 = _mm_cvtsi128_si64(s); // lower 64bit
		s1 = _mm_cvtsi128_si64(_mm_srli_si128(s, 8)); //higher 64bit 
		uint64_t result = s0 + s1;

		//update s
		update();
		
		return result;
	}
};


template <typename T>
class KModCounterW : public CircularBufferFilterW <T>{
	T label;
	size_t sz;


public:

	KModCounterW(size_t size) : CircularBufferFilterW<T>(size, 0)
	{
		label = 0;
		sz = size;
	}

	void increment(size_t h, T offset) {
		label = CircularBufferFilterW<T>::getBufferAt(h);
		label = (label + 1 +  offset) % UINT16_MAX ;
		CircularBufferFilterW<T>::setBufferAt(h, label);
	}

	T get(size_t h) {
		return  CircularBufferFilterW<T>::getBufferAt(h);
	}

	void decrement(size_t h) {
		label = CircularBufferFilterW<T>::getBufferAt(h);
		label = (label - 1) % UINT16_MAX;
		CircularBufferFilterW<T>::setBufferAt(h, label);
	}

	void reset(size_t h) {
		CircularBufferFilterW<T>::setBufferAt(h, 0);
	}

	void set(size_t h, T val) {
		CircularBufferFilterW<T>::setBufferAt(h, val);
	}
};

#endif// _WIN32


//add other platforms if needed
