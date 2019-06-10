#include <fstream>
#include <string>
#include <regex>
#include <unordered_map>
#include <queue>
#include <vector>
#include <map>
#include "caches/lru_variants.h"
#include "caches/gd_variants.h"
#include "request.h"

using namespace std;


class flash {
    struct block {
        std::vector<int64_t> ids;
        std::vector<int64_t> sizes;
        int64_t bytes;
    };

    typedef std::queue<block> segment;

    std::map<double, segment > cache;
    const uint64_t segmentCount;
    const int64_t blockSize;
    const uint64_t blockCount;
    const uint64_t blocksPerSegment;
  
    struct object{
        int64_t size;
        int64_t lastAccess;
        int64_t accessCount;
    };
    std::unordered_map<int64_t, object> index;
    int64_t highestprio; // updated every request
    int64_t lowestprio; // updated on eviction

    int64_t time;
    int64_t writtenBytes;
    int64_t amplifiedBytes;

public:
    flash(uint64_t ssegmentCount, int64_t sblockSize, uint64_t sblockCount)
        : segmentCount(ssegmentCount), blockSize(sblockSize),
          blockCount(sblockCount), blocksPerSegment(blockCount / segmentCount),
          highestprio(0), lowestprio(0), time(0),
          writtenBytes(0), amplifiedBytes(0)
    {
        // mapping of priority to segment using upper bound on cache
        for(double i=0; i<segmentCount-1; i++) {
            cache.emplace(
                          (i+1)/double(segmentCount), // initial priority steps
                          segment({block()}) // single empty block in each segment
                          );
        }
        // terminal node: just above 1
        cache.emplace(
                      1.1,
                      segment({block()})
                      );
        LOG("flash init",segmentCount,blockSize,blockCount);
    }

    bool lookup(int64_t id) {
        time++;
        highestprio = time;
        if(index.count(id)>0) {
            // hit
            index[id].accessCount++;
            index[id].lastAccess = time;
            LOG("hit",id,time,index[id].accessCount);
            return true;
        }
        LOG("miss",id,time,0);
        return false;
    }

    void fullSegment(segment & curSegment) {
        LOG("fullSegment 1",curSegment.size(),blocksPerSegment,0);
        if(curSegment.size()<=blocksPerSegment) {
            std::cerr << "called full, but not full\n";
            return;
        }
        // evict single block, at front
        auto & evictBlock = curSegment.front();
        LOG("fullSegment 2",evictBlock.ids.size(),curSegment.size(),0);
        // iterate over stored objects
        for(size_t i=0; i<evictBlock.ids.size(); i++) {
            const auto id = evictBlock.ids[i];
            const auto size = evictBlock.sizes[i];
            const auto lastAccess = index[id].lastAccess;
            const double prio = double(lastAccess-lowestprio) / double(highestprio-lowestprio);
            //            LOG("fullSegment 3a",i,id,prio);
            //            LOG("fullSegment 3b",lastAccess,lowestprio,highestprio);
            if(prio > 0.1) {
                // readmit as prio > 10%
                auto & moveToSegment = cache.upper_bound(prio)->second;
                auto & moveToBlock = moveToSegment.back();
                moveToBlock.ids.push_back(id);
                moveToBlock.sizes.push_back(size);
                moveToBlock.bytes+=size;
                amplifiedBytes += size;
            } else {
                // update lowest prior end
                lowestprio = index[id].lastAccess; // TODO: fixme
                index.erase(id);
            }
        }
        curSegment.pop();
        LOG("fullSegment 2",0,0,0);
    }

    void checkFullBlocks() { // potentially buggy
        LOG("checkFullBlocks 1",0,0,0);
        // from top to bottom
        for (auto rit = cache.rbegin(); rit!=cache.rend(); ++rit) {
            auto & curSegment = rit->second;
            if(curSegment.size()>blocksPerSegment) {
                fullSegment(curSegment);
            }
        }
        LOG("checkFullBlocks 2",0,0,0);
    }
    
    void admit(int64_t id, int64_t size) {
        LOG("admit 1",id,size,0);
        if(size>blockSize) {
            // don't admit too large objects
            std::cerr << "object size > block size!!\n";
            return;
        }
        segment & curSegment = cache.upper_bound(1.0)->second; // admit to highest prio segment
        LOG("admit 2",0,0,curSegment.size());
        block * curBlock = NULL;
        if(curSegment.size()==0) {
            std::cerr << "error: initialization failed\n";
        } else {
            // current block exists
            LOG("admit 3e",0,0,0);
            curBlock = &curSegment.back();
        }
        if(curBlock->bytes + size > blockSize) {
            // object does not fit into current block
            // -> add a new block
            LOG("admit 3n",0,0,0);
            curSegment.push(block());
            curBlock = &curSegment.back();
        }
        LOG("admit 4",0,0,0);
        // process cache admission
        curBlock->ids.push_back(id);
        curBlock->sizes.push_back(size);
        curBlock->bytes+=size;
        writtenBytes += size;
        index[id].size = size;
        index[id].lastAccess = time;
        index[id].accessCount = 1;
        // bring cache into good state
        LOG("admit 5",0,0,0);
        checkFullBlocks();
        LOG("admit 6",id,size,0);
    }

    int64_t getWrittenBytes() {
        return writtenBytes;
    }

    int64_t getAmplifiedBytes() {
        return amplifiedBytes;
    }
    
};


int main (int argc, char* argv[])
{

  // output help if insufficient params
  if(argc < 4) {
    cerr << "webcachesim traceFile cacheSizeBytes segmentCount blockSize [cacheParams]" << endl;
    return 1;
  }
  const char* path = argv[1];
  const uint64_t cacheSize  = std::stoull(argv[2]);
  const uint64_t segmentCount  = std::stoull(argv[3]);
  const int64_t blockSize  = std::stoull(argv[4]);

  auto f = flash(segmentCount, blockSize,
                 cacheSize / blockSize
                 );
  
  ifstream infile;
  uint64_t reqs = 0, hits = 0;
  int64_t t, id, size;

  cerr << "running..." << endl;

  infile.open(path);
  while (infile >> t >> id >> size)
  {
      if(f.lookup(id)) {
          // hit
          hits++;
      } else {
          f.admit(id, size);
      }
      reqs++;
  }

  infile.close();

  std::cout << path << " "
            << cacheSize << " "
            << segmentCount << " "
            << blockSize << " "
            << f.getWrittenBytes() << " "
            << f.getAmplifiedBytes() << " "
            << double(hits)/reqs << "\n";

  
  return 0;
}
