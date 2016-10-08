/**
 * pagemon.c
 * Author:xiemengyao@ict.ac.cn
 * @20160227
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/sem.h>
#include <linux/list.h>
#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/mman.h>
#include <linux/swap.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/ksm.h>
#include <linux/rmap.h>
#include <linux/module.h>
#include <linux/delayacct.h>
#include <linux/init.h>
#include <linux/writeback.h>
#include <linux/memcontrol.h>
#include <linux/mmu_notifier.h>
#include <linux/kallsyms.h>
#include <linux/swapops.h>
#include <linux/elf.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/time.h>

#define PAGE_LEN 24
#define CACHELINE_LEN 20

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

struct timespec time_start,time_end;
struct timeval time0,time1;
unsigned long long cachelinetime = 0;
unsigned long long pagehashtime = 0;
void get_random_bytes(void * buf, int nbytes);

/* get 32768 pages.128M*/
unsigned long page[32768];
static uint32_t GetPageHash(unsigned char * pagemem);
static uint32_t GetCachelineHash(unsigned char * pagemem, int number);
//static int CompareTwoPage(void);
static int CompareTwoPage2(void);
static int CopyPage(void);
static int CopyPartialPage(void);


uint32_t SuperFastHash (const char * data, int len)
{
	uint32_t hash = len, tmp;
	int rem;

    	if (len <= 0 || data == NULL) return 0;

    	rem = len & 3;
	    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += (signed char)*data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}


static int __init timer_init(void)
{
	uint32_t hash1;
	uint32_t hash2;
	int similarity;
	int i;
	for(i = 0; i < 32768; i++)
	{	
		page[i] = get_zeroed_page(GFP_KERNEL);
	}
	printk("=========================================\n");
	for(i = 0; i < 32768; i++)
	{
		hash2 = GetCachelineHash((unsigned char *)page[i],47);	
	}	
	
	for(i = 0; i < 32768; i++)
	{
		hash1 = GetPageHash((unsigned char *)page[i]);
	}
	for(i = 0; i < 32768; i++)
	{
		free_page(page[i]);
	}
	printk("get page hash:%llu\n",pagehashtime / 32768);
	printk("get cacheline hash:%llu\n",cachelinetime / 32768);
	similarity = CompareTwoPage2();
	i=CopyPage();
	i=CopyPartialPage();
	return 0;
}

static void __exit timer_exit(void)
{
	printk("Unloading pagemon module.\n");
	return;
}

/* Get page hash value.*/ 
static uint32_t GetPageHash(unsigned char * pagemem)
{
	uint32_t pagehash;
	getnstimeofday(&time_start);

	pagehash = SuperFastHash((const char *)pagemem, PAGE_LEN);

	getnstimeofday(&time_end);
//	printk("get page hash\t%u\n",(unsigned int)(time_end.tv_nsec - time_start.tv_nsec));
	pagehashtime += (unsigned long)( time_end.tv_nsec - time_start.tv_nsec);
	return pagehash;
}


/* Get cacheline hash value. pagemem denotes the page's address, number denotes 
   the NO. of the cacheline.*/
static uint32_t GetCachelineHash(unsigned char * pagemem, int number)
{
	uint32_t cachelinehash;
	unsigned char * cachelinemem;
	
	cachelinemem = pagemem + 64 * number;
	getnstimeofday(&time_start);
	cachelinehash = SuperFastHash((const char *)cachelinemem, CACHELINE_LEN);
	getnstimeofday(&time_end);
//	printk("cacheline duration\t%llu\n", time_end.tv_nsec - time_start.tv_nsec);
	cachelinetime += (unsigned long)(time_end.tv_nsec - time_start.tv_nsec);
	return cachelinehash;
}


/*
static int CompareTwoPage(void)
{
	int similarity = 0;
	int i;
	int temp[2];
	unsigned long page1,page2;
	uint32_t hash1,hash2;
	for(i = 0; i < 32768; i++)
	{	
		page[i] = get_zeroed_page(GFP_KERNEL);
	}
	get_random_bytes(&temp[0],sizeof(int));
	page1 = (temp[0] > 0) ? temp[0] % 32768 :-temp[0] % 32768;
	
	get_random_bytes(&temp[1],sizeof(int));
	page2 = (temp[1] > 0) ? temp[1] % 32768 :-temp[1] % 32768;


//	page2 = 19000;
	getnstimeofday(&time_start);
	for(i = 0; i < 64; i++)
	{
//	getnstimeofday(&time_start);
		hash1 = GetCachelineHash((unsigned char*)page[page1], i);
		hash2 = GetCachelineHash((unsigned char*)page[page2], i);
	//	if(GetCachelineHash((unsigned char *)page[page1],i) == GetCachelineHash((unsigned char *)page[page2],i))
		if(hash1 == hash2)
		{
			similarity++;
		}
//	getnstimeofday(&time_end);
	}
	getnstimeofday(&time_end);
	printk("compare two page(ns)\t%u\n",(unsigned int)(time_end.tv_nsec - time_start.tv_nsec));
	for(i = 0; i < 32768; i++)
	{
		free_page(page[i]);
	}
	return 1;
}*/

/* Return the similarity of two page.*/
static int CompareTwoPage2(void)
{
	int similarity = 0;
	int i;
	int j;
	int temp[2];
	unsigned long page1,page2;
	int isequal = 0;
	const char * frompage;
	const char * topage;
	for(i = 0; i < 32768; i++)
	{	
		page[i] = get_zeroed_page(GFP_KERNEL);
	}
	get_random_bytes(&temp[0],sizeof(int));
	page1 = (temp[0] > 0) ? temp[0] % 32768 :-temp[0] % 32768;
	
	get_random_bytes(&temp[1],sizeof(int));
	page2 = (temp[1] > 0) ? temp[1] % 32768 :-temp[1] % 32768;

	frompage = (const char *)page[page1];
	topage = (const char *)page[page2];

	getnstimeofday(&time_start);
	for(i = 0; i < 64; i++)
	{
		isequal = 0;
		for(j = 0; j < 64; j++)
		{
			if(frompage[i * 64 + j] == topage[i * 64 + j])
				isequal++;
		}
		if(isequal == 64)//cacheline i :equal
			similarity++;
	}
	getnstimeofday(&time_end);
	printk("compare two page(ns)\t%u\n",(unsigned int)(time_end.tv_nsec - time_start.tv_nsec));
	for(i = 0; i < 32768; i++)
	{
		free_page(page[i]);
	}
	printk("similarity:%d\n",similarity);
	return similarity;
}


/* Copy the whole page to another page.*/
static int CopyPage(void)
{
	int i;
	int temp[2];
	int page1,page2;
	unsigned long sumtime = 0;

	for(i = 0; i < 32768; i++)
	{	
		page[i] = get_zeroed_page(GFP_KERNEL);
	}
/* Method 1*/
	do_gettimeofday(&time0);
	
	for(i = 0;i < 16348; i++)
	{
		copy_page((unsigned char *)page[i], (unsigned char *)page[i + 16384]);
	}

	do_gettimeofday(&time1);
	printk("Method 1:sequence copy(ns)\t%lu\n",((unsigned long)(time1.tv_usec - time0.tv_usec)) * 1000 / 16384);

/* Method 2*/
	getnstimeofday(&time_start);
	copy_page((unsigned char*)page[0],(unsigned char *)page[17000]);	
	getnstimeofday(&time_end);
	printk("Method 2:signal copy(ns)\t%lu\n",(unsigned long)(time_end.tv_nsec - time_start.tv_nsec));

/* Method 3*/
	for(i = 0;i < 16348; i++)
	{
	
		get_random_bytes(&temp[0],sizeof(int));
		page1 = (temp[0] > 0) ? temp[0] % 32768 :-temp[0] % 32768;
		
		get_random_bytes(&temp[1],sizeof(int));
		page2 = (temp[1] > 0) ? temp[1] % 32768 :-temp[1] % 32768;

		getnstimeofday(&time_start);
	
		copy_page((unsigned char *)page[page1], (unsigned char *)page[page2]);
	
		getnstimeofday(&time_end);
		sumtime += (unsigned long)(time_end.tv_nsec - time_start.tv_nsec);
	}
	printk("Method 3:random copy(ns)\t%lu\n", sumtime / 16384);
	
	for(i = 0; i < 32768; i++)
	{
		free_page(page[i]);
	}
	return 1;
}
    
/* Copy partial page to another.*/
static int CopyPartialPage(void)
{
	int cache[32];
	int i;
	int j;
	int k;	
	int temp[1];
	
	for(i = 0; i < 32768; i++)
	{
		page[i] = get_zeroed_page(GFP_KERNEL);
	}
	for(i = 1; i < 32; i++)
	{
		for(j = 0; j < i;j++)
		{
			get_random_bytes(&temp[0],sizeof(int));
			cache[j] = (temp[0] > 0) ? temp[0] % 64 :-temp[0] % 64;
		}
		getnstimeofday(&time_start);
		for(k = 0; k < i; k++)
		{
			memcpy((unsigned char *)page[0] + cache[k] * 64, (unsigned char *)page[10] + cache[k] * 64, 64);
		}
		getnstimeofday(&time_end);	
		printk("%d cachelines(ns)\t%u\n",i,(unsigned int)( time_end.tv_nsec - time_start.tv_nsec));
	}
/*	i = 1;	
	getnstimeofday(&time_start);
	memcpy((unsigned char *)page[2] + 3 * 64,(unsigned char *)page[20] + 3 * 64, 64);
	getnstimeofday(&time_end);	
	printk("%d cachelines(ns)\t%u\n",i,(unsigned int)( time_end.tv_nsec - time_start.tv_nsec));
	
	i = 2;
	getnstimeofday(&time_start);
	memcpy((unsigned char *)page[2] + 3 * 64,(unsigned char *)page[20] + 3 * 64, 64);
	memcpy((unsigned char *)page[2] + 45 * 64,(unsigned char *)page[20] + 45 * 64, 64);
	getnstimeofday(&time_end);	
	printk("%d cachelines(ns)\t%u\n",i,(unsigned int)( time_end.tv_nsec - time_start.tv_nsec));
	
	i = 4;
	getnstimeofday(&time_start);
	memcpy((unsigned char *)page[2] + 3 * 64,(unsigned char *)page[20] + 3 * 64, 64);
	memcpy((unsigned char *)page[2] + 45 * 64,(unsigned char *)page[20] + 45 * 64, 64);
	memcpy((unsigned char *)page[2] + 38 * 64,(unsigned char *)page[20] + 38 * 64, 64);
	memcpy((unsigned char *)page[2] + 60 * 64,(unsigned char *)page[20] + 60 * 64, 64);
	getnstimeofday(&time_end);	
	printk("%d cachelines(ns)\t%u\n",i,(unsigned int)( time_end.tv_nsec - time_start.tv_nsec));
	for(i = 0; i < 32768; i++)
	{
		free_page(page[i]);
	}*/
	return 1;
}

module_init(timer_init);
module_exit(timer_exit);
MODULE_AUTHOR("mengyao xie");
MODULE_LICENSE("GPL");
