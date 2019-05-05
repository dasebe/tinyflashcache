#include <fstream>
#include <string>
#include <regex>
#include "caches/lru_variants.h"
#include "caches/gd_variants.h"
#include "request.h"
#include <iostream>
#include <functional>
#include <string>
#include <vector>
#include <math.h>

using namespace std;

typedef std::pair<int64_t, int64_t> IdSizePair;
typedef std::pair<long double, std::vector<IdSizePair>*> CDFPair;

std::mt19937_64 rd_gen;

bool CDFCompare(const CDFPair& a, const CDFPair& b)
{
    return a.first < b.first;
}


int main (int argc, char* argv[])
{
    // output help if insufficient params
    if(argc < 5) {
        cerr << "webcachesim CacheType ObjCount TraceLength BucketCount BucketSize [cacheParams]" << endl;
        return 1;
    }

    // trace properties
    const string cacheType = argv[1];
    const int64_t ObjCount = std::stoull(argv[2]);
    const int64_t TraceLength = std::stoull(argv[3]);
  
    // configure cache size
    const uint64_t bucket_count  = std::stoull(argv[4]);
    const uint64_t bucket_size  = std::stoull(argv[5]);
    // create buckets and their caches
    std::vector<unique_ptr<Cache> > caches;
    for(uint64_t i=0; i<bucket_count; i++) {
        caches.push_back(move(Cache::create_unique(cacheType)));
    }
    for(uint64_t i=0; i<bucket_count; i++) {  
        caches[i]->setSize(bucket_size);
    }

    // parse cache parameters
    regex opexp ("(.*)=(.*)");
    cmatch opmatch;
    string paramSummary;
    for(int i=6; i<argc; i++) {
        regex_match (argv[i],opmatch,opexp);
        if(opmatch.size()!=3) {
            cerr << "each cacheParam needs to be in form name=value" << endl;
            return 1;
        }
        for(uint64_t i=0; i<bucket_count; i++) {  
            caches[i]->setPar(opmatch[1], opmatch[2]);
        }
        paramSummary += opmatch[2];
    }
    

    // init
    rd_gen.seed(1);
    // scan files from file
    ifstream infile;
    infile.open("size.dist");
    int64_t tmp;
    vector<int64_t> base_sizes;
    while (infile >> tmp) {
        if(tmp>0 && tmp<4096) {
            base_sizes.push_back(tmp);
        }
    }
    infile.close();
    if(base_sizes.size()==0) {
        base_sizes = vector<int64_t>({16,32,64,100,128,200,256,300,100,128,200,256,300,100,128,200,256,300,100,128,200,256,300,100,128,200,256,300,400,700,900,512,1024,800,1400,1500,2048,4096});
    }
    std::uniform_int_distribution<uint64_t> size_dist(0,base_sizes.size()-1);
    std::vector<CDFPair> _popularityCdf; // CDF of popularity distribution, scaled by totalCount (i.e., range [0,totalCount])
    std::unordered_map<long double, std::vector<IdSizePair>*> data;
    int64_t sampled_size;
    long double rate = 1e10L;
    uint64_t div_factor = 1;
    cerr << "done init\n";
    // create high-level distribution datastructures
    for(int64_t i=0; i<ObjCount;) {
        rate /= 1.5;
        std::vector<IdSizePair>*& idSizes = data[roundl(rate)+1.0L];
        if (idSizes == NULL) {
            idSizes = new std::vector<IdSizePair>();
        }
        // insert multiple ones at this rate
        for(uint64_t j=0; j<div_factor; j++) {
            sampled_size=base_sizes[size_dist(rd_gen)];
            idSizes->emplace_back(i, sampled_size);
            i++;
        }
        //cerr << i << " " << round(rate)+1 << " " << sampled_size << "\n";
        div_factor *= 2;
    }
    cerr << "done dists\n";
    // calculate popularity CDF
    long double totalCount = 0;
    for (auto &it : data) {
        long double count = it.first;
        std::vector<IdSizePair>* idSizes = it.second;
        totalCount += count * idSizes->size();
        //cerr << "calc " << count << " " << (*idSizes)[0].first << " " << totalCount << "\n";
        _popularityCdf.emplace_back(totalCount, idSizes);
    }
    std::uniform_real_distribution<long double> _distribution(0, totalCount - 1);
    cerr << "done cdf\n";

    // running the simulator
    uint64_t reqs = 0, hits = 0;
    size_t hh, bucket_idx;
    std::hash<std::string> hash;
    SimpleRequest* req = new SimpleRequest(0, 0);
    cerr << "running..." << endl;
    // balls and bins counters
    std::vector<int64_t > ballsbins(bucket_count);
    std::vector<int64_t > ballsbins_misses(bucket_count);

    for(int64_t i=0; i<TraceLength; i++) {
        // sample
        long double r = _distribution(rd_gen);
        const std::vector<CDFPair>::iterator it = std::upper_bound(_popularityCdf.begin(), _popularityCdf.end(), CDFPair(r, NULL), CDFCompare);
        const std::vector<IdSizePair>& idSizes = *(it->second);
        size_t r2 = std::uniform_int_distribution<size_t>(0, idSizes.size() - 1)(rd_gen);
        const IdSizePair& idSize = idSizes[r2];
        req->reinit(idSize.first,idSize.second); // id, size
        // simulate
        reqs++;
        hh=hash(std::to_string(idSize.first));
        bucket_idx = hh % bucket_count;
        // ballsandbins
        ballsbins[bucket_idx]+=idSize.second;
        // caching
        if(caches[bucket_idx]->lookup(req)) {
            hits++;
        } else {
            caches[bucket_idx]->admit(req);
            // ballsandbins (only cache misses)
            ballsbins_misses[bucket_idx]+=idSize.second;
        }
        // output
        if(reqs % 1000000 == 0) {
            cerr << double(hits)/reqs << "\n";
        }
    }

    delete req;

    // stats gathering
    int64_t maxballs=0, minballs, sumballs=0;
    int64_t maxballs_misses=0, minballs_misses, sumballs_misses=0;
    for(uint64_t i=0; i<bucket_count; i++) {
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
    for(uint64_t i=0; i<bucket_count; i++) {
        if(ballsbins[i]<minballs) {
            minballs=ballsbins[i];
        }
        if(ballsbins_misses[i]<minballs_misses) {
            minballs_misses=ballsbins_misses[i];
        }    
    }

    cout << cacheType << " " << ObjCount << " "
         << bucket_count << " " << bucket_size << " "
         << paramSummary << " "
         << double(hits)/reqs << " " 
         << maxballs << " "
         << minballs << " "
         << double(sumballs)/caches.size() << " "
         << maxballs_misses << " "
         << minballs_misses << " "
         << double(sumballs_misses)/caches.size() << " "
         << endl;
    
    return 0;
}
