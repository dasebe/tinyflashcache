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
#include <cmath>

using namespace std;

typedef std::pair<int64_t, int64_t> IdSizePair;
typedef std::pair<uint64_t, std::vector<IdSizePair>*> CDFPair;

std::mt19937_64 rd_gen;
std::random_device rd;

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
    rd_gen.seed(rd());
    vector<int64_t> base_sizes{ 10, 100, 1000 };
    std::uniform_int_distribution<uint64_t> size_dist(0,base_sizes.size()-1);
    std::vector<CDFPair> _popularityCdf; // CDF of popularity distribution, scaled by totalCount (i.e., range [0,totalCount])
    std::uniform_int_distribution<uint64_t> _distribution;
    std::unordered_map<int64_t, std::vector<IdSizePair>*> data;
    int64_t sampled_size;
    long double rate = 2e9;
    cerr << "done init\n";
    // create high-level distribution datastructures
    for(int64_t i=0; i<ObjCount; i++) {
        rate /= 1.74;
        sampled_size=base_sizes[size_dist(rd_gen)];
        std::vector<IdSizePair>*& idSizes = data[round(rate)];
        if (idSizes == NULL) {
            idSizes = new std::vector<IdSizePair>();
        }
        idSizes->emplace_back(i, sampled_size);
    }
    cerr << "done dists\n";
    // calculate popularity CDF
    uint64_t totalCount = 0;
    for (auto &it : data) {
        int64_t count = it.first;
        std::vector<IdSizePair>* idSizes = it.second;
        totalCount += count * idSizes->size();
        _popularityCdf.emplace_back(totalCount, idSizes);
    }
    _distribution = std::uniform_int_distribution<uint64_t>(0, totalCount - 1);
    cerr << "done cdf\n";

    // running the simulator
    uint64_t reqs = 0, hits = 0;
    size_t hh, bucket_idx;
    std::hash<std::string> hash;
    SimpleRequest* req = new SimpleRequest(0, 0);
    cerr << "running..." << endl;

    for(int64_t i=0; i<TraceLength; i++) {
        // sample
        uint64_t r = _distribution(rd_gen);
        const std::vector<CDFPair>::iterator it = std::upper_bound(_popularityCdf.begin(), _popularityCdf.end(), CDFPair(r, NULL), CDFCompare);
        const std::vector<IdSizePair>& idSizes = *(it->second);
        size_t r2 = std::uniform_int_distribution<size_t>(0, idSizes.size() - 1)(rd_gen);
        const IdSizePair& idSize = idSizes[r2];
        req->reinit(idSize.first,idSize.second);

        // simulate
        reqs++;
        hh=hash(std::to_string(idSize.first));
        bucket_idx = hh % bucket_count;
        //cerr << id << " " << hh << " " << hh % bucket_count <<  "\n";
        if(caches[bucket_idx]->lookup(req)) {
            hits++;
        } else {
            caches[bucket_idx]->admit(req);
        }
    }

    delete req;

    cout << cacheType << " " << bucket_count << " " << bucket_size << " " << paramSummary << " "
         << double(hits)/reqs << endl;

    return 0;
}
