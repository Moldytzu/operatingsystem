#include <mm/slab.h>
#include <mm/pmm.h>
#include <misc/logger.h>

#define BITMAP_OF(x) ((void *)((uint64_t)x + sizeof(slab_cache_t)))

// creates a new slab cache for required object size, returns a pointer to it
slab_cache_t *slabCreate(size_t objectSize)
{
    objectSize = align(objectSize, 8); // make sure objectSize is a multiple of 8

    // allocate an initial cache
    slab_cache_t *cache = pmmPage();

    // determine bitmap bytes
    // with formula (availableBytes - bitmapBytes / (bitmapBytes * 8)) we determine minimum number of bytes an object can have
    // applying that, we can brute force the bitmapBytes variable in a few iterations

    uint16_t bitmapAndContentsSize = PMM_PAGE - sizeof(slab_cache_t); // page minus header
    uint8_t bitmapBytes = 1;
    for (; bitmapBytes <= 64; bitmapBytes++) // we set an upper limit of 64 bytes because that would be the minimum number of bitmap bytes to store metadata for 8 byte objects
    {
        if ((bitmapAndContentsSize - bitmapBytes) / (bitmapBytes * 8) <= objectSize)
            break;
    }

    uint16_t contentSize = bitmapAndContentsSize - bitmapBytes;
    uint16_t objects = contentSize / objectSize;

    logInfo("slab: best bitmap size for %d bytes object is %d bytes and we can hold %d objects", objectSize, bitmapBytes, objects);

    // set required metadata
    cache->busy = false;
    cache->objects = objects;
    cache->objectSize = objectSize;
    cache->bitmapBytes = bitmapBytes;
    cache->next = NULL;

    return cache;
}

// allocates a new object in a slab cache
void *slabAllocate(slab_cache_t *cache)
{
    lock(cache->lock, {
        void *bmp = BITMAP_OF(cache);
        for (int i = 0; i < cache->objects; i++) // iterate over all indices
        {
            if (bmpGet(bmp, i)) // find first bitmap index
                continue;

            bmpSet(bmp, i);                                                                                       // mark it as busy
            release(cache->lock);                                                                                 // release the lock
            return (void *)((uint64_t)cache + sizeof(slab_cache_t) + cache->bitmapBytes + cache->objectSize * i); // return the slab
        }
    });

    // todo: try to allocate a new cache and link them together

    cache->busy = true;

    return NULL; // didn't found
}

// deallocate a slab from a cache
void slabDeallocate(slab_cache_t *cache, void *ptr)
{
    do
    {
        lock(cache->lock, {
            if ((uint64_t)cache >> 12 == (uint64_t)ptr >> 12) // check if page index match
            {
                // it means the pointer is part of the cache
                uint64_t index = ((uint64_t)ptr - (uint64_t)cache - sizeof(slab_cache_t) - cache->bitmapBytes) / cache->objectSize; // reverse of the formula used in slabAllocate

                if (!bmpGet(cache, index)) // check if we can deallocate
                {
                    logInfo("slab: trying to deallocate free slab %p from %p", ptr, cache);
                    return;
                }

                bmpUnset(cache, index); // mark as free

                cache->busy = false;

                release(cache->lock);
                return; // return
            }
        });

        cache = cache->next;
    } while (cache);

    logInfo("slab: failed to deallocate %p from %p", ptr, cache);
}

void slabInit()
{
    slab_cache_t *cache = slabCreate(32);
    logInfo("slab: allocated cache at %p", cache);

    for (int i = 0; i < 5; i++)
        logInfo("%p", slabAllocate(cache));

    slabDeallocate(cache, slabAllocate(cache));

    while (1)
        ;
}