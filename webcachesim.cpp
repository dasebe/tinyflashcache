#include <fstream>
#include <string>
#include "caches/lru_variants.h"
#include "caches/gd_variants.h"
#include "request.h"
#include "zipf.h"

//using namespace std;

int main (int argc, char* argv[])
{
    // output help if insufficient params
    if(argc != 6) {
        std::cerr << "webcachesim CacheType ObjCount TraceLength BucketCount BucketSize\n";
        return 1;
    }

    // trace properties
    const std::string cacheType = argv[1];
    const int64_t objCount = std::stoull(argv[2]);
    const int64_t traceLength = std::stoull(argv[3]);
  
    // configure cache size
    const uint64_t bucketCount  = std::stoull(argv[4]);
    const uint64_t bucketSize  = std::stoull(argv[5]);

    // create buckets and their caches
    std::vector<std::unique_ptr<Cache> > caches;
    for(uint64_t i=0; i<bucketCount; i++) {
        caches.push_back(move(Cache::create_unique(cacheType)));
        caches[i]->setSize(bucketSize);
    }

    // init zipf distribution
    auto zr = ZipfRequests("size.dist",objCount);
    
    // running the simulator
    uint64_t reqs = 0, hits = 0;
    size_t hh, bucket_idx;
    std::hash<std::string> hash;
    SimpleRequest* req = new SimpleRequest(0, 0);

    // balls and bins counters
    std::vector<int64_t > ballsbins(bucketCount);
    std::vector<int64_t > ballsbins_misses(bucketCount);

    for(int64_t i=0; i<traceLength; i++) {
        zr.Sample(req);
        reqs++;

        // hash into bin
        hh=hash(std::to_string(req->getId()));
        bucket_idx = hh % bucketCount;
        // ballsandbins stats
        ballsbins[bucket_idx]+=req->getSize();
        // caching
        if(caches[bucket_idx]->lookup(req)) {
            hits++;
        } else {
            caches[bucket_idx]->admit(req);
            // ballsandbins stats (for only cache misses)
            ballsbins_misses[bucket_idx]+=req->getSize();
        }
        // progress output
        // if(reqs % 1000000 == 0) {
        //     std::cerr << double(hits)/reqs << "\n";
        // }
    }

    delete req;

    // stats calculation and output
    int64_t maxballs=0, minballs, sumballs=0;
    int64_t maxballs_misses=0, minballs_misses, sumballs_misses=0;
    for(uint64_t i=0; i<bucketCount; i++) {
        if(ballsbins[i]>maxballs) {
            maxballs=ballsbins[i];
        }
        if(ballsbins_misses[i]>maxballs_misses) {
            maxballs_misses=ballsbins_misses[i];
        }
        sumballs+=ballsbins[i];
        sumballs_misses+=ballsbins_misses[i];
    }
    minballs=maxballs;
    minballs_misses=maxballs_misses;
    for(uint64_t i=0; i<bucketCount; i++) {
        if(ballsbins[i]<minballs) {
            minballs=ballsbins[i];
        }
        if(ballsbins_misses[i]<minballs_misses) {
            minballs_misses=ballsbins_misses[i];
        }    
    }

    std::cout << cacheType << " " << objCount << " "
         << bucketCount << " " << bucketSize << " "
         << " " // old/unused output
         << double(hits)/reqs << " " 
         << maxballs << " "
         << minballs << " "
         << double(sumballs)/caches.size() << " "
         << maxballs_misses << " "
         << minballs_misses << " "
         << double(sumballs_misses)/caches.size() << " "
         << "\n";

    zr.Summarize();
    
    return 0;
}
