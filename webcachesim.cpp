#include <fstream>
#include <string>
#include <regex>
#include <queue>
#include "caches/lru_variants.h"
#include "caches/gd_variants.h"
#include "request.h"
#include "partitions.h"

using namespace std;

class flash {
    struct block {
        std::vector<int64_t> ids;
        int64_t bytes;
    };

    typedef std::queue<block> segment;

    std::vector<segment> cache;
    Partitions parts; // maps priority values to partitions
    int64_t currentTime; // updated every request
    int64_t evictedTime; // updated on eviction
    int64_t time;
    
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

    int64_t writtenBytes;
    int64_t amplifiedBytes;

public:
    flash(uint64_t ssegmentCount, int64_t sblockSize, uint64_t sblockCount)
        : parts(ssegmentCount, 100000),
          currentTime(0), evictedTime(0),
          segmentCount(ssegmentCount), blockSize(sblockSize),
          blockCount(sblockCount), blocksPerSegment(blockCount / segmentCount),
          writtenBytes(0), amplifiedBytes(0)
    {
        LOG("flash init 1",segmentCount,blockSize,blockCount);
        for(size_t i=0; i<segmentCount; i++) {
            cache.push_back(segment());
            cache[i].push(block());
        }
        LOG("flash init 2",segmentCount,blockSize,blockCount);
    }

    double calcPrio(size_t id) {
        assert(index.count(id)>0);
        const auto lastAccess = index[id].lastAccess;
        return double(lastAccess-evictedTime) / double(currentTime-evictedTime);
    }

    bool lookup(int64_t id) {
        currentTime++; // update time
        if(index.count(id)>0) {
            // hit
            index[id].accessCount++;
            index[id].lastAccess = currentTime;
            parts.addEntry(calcPrio(id));
            LOG("hit",id,currentTime,index[id].accessCount);
            return true;
        }
        LOG("miss",id,currentTime,0);
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
        // store evicted prio so that evictedTime is constant while evicting
        int64_t tmpEvictedPrio = evictedTime;
        // iterate over stored objects
        for(size_t i=0; i<evictBlock.ids.size(); i++) {
            const auto id = evictBlock.ids[i];
            const auto size = index[id].size;
            auto prio = calcPrio(id);
            //            LOG("fullSegment 3a",i,id,prio);
            //            LOG("fullSegment 3b",lastAccess,evictedTime,currentTime);
            if(parts.isBottomPerc(prio)) {
                // evict this object as very low priority
                // update lowest prior end
                if(index[id].lastAccess > tmpEvictedPrio) {
                    tmpEvictedPrio = index[id].lastAccess; // TODO: fixme
                }
                index.erase(id);
            } else {                
                // readmit as large enough priority
                auto segmentIdx = parts.getEntryIdx(prio);
                auto & moveToSegment = cache[segmentIdx];
                appendToSegment(moveToSegment, id, size);
                amplifiedBytes += size;
            }
        }
        // update evictedTime
        evictedTime = tmpEvictedPrio;
        curSegment.pop();
        LOG("fullSegment 2",0,0,0);
    }

    void checkRebalance() {
        LOG("checkRebalance 1",0,0,0);
        bool anyFullSegment;
        do { // iterate until no full segments
            anyFullSegment = false; // reset and beginning of round
            // from top to bottom
            for (auto rit = cache.rbegin(); rit!=cache.rend(); ++rit) {
                auto & curSegment = (*rit);
                if(curSegment.size()>blocksPerSegment) {
                    fullSegment(curSegment);
                    anyFullSegment = true;
                }
            }
            LOG("checkRebalance i",anyFullSegment,0,0);
        } while(anyFullSegment);
        LOG("checkRebalance 2",0,0,0);
    }

    void appendToSegment(segment & curSegment,int64_t id, int64_t size) {
        LOG("appendToSegment 1",0,0,0);
        block * curBlock = NULL;
        if(curSegment.size()==0) {
            std::cerr << "error: initialization failed\n";
        } else {
            // current block exists
            LOG("appendToSegment 2e",0,0,0);
            curBlock = &curSegment.back();
        }
        if(curBlock->bytes + size > blockSize) {
            // object does not fit into current block
            // -> add a new block
            LOG("appendToSegment 3n",0,0,0);
            curSegment.push(block());
            curBlock = &curSegment.back();
        }
        LOG("appendToSegment 3",0,0,0);
        // process cache admission
        curBlock->ids.push_back(id);
        curBlock->bytes+=size;
    }
    
    void admit(int64_t id, int64_t size) {
        LOG("admit 1",id,size,0);
        if(size>blockSize) {
            // don't admit too large objects
            std::cerr << "object size > block size!!\n";
            return;
        }
        segment & curSegment = *(--cache.end()); // admit to highest prio segment
        LOG("admit 2",0,0,curSegment.size());
        appendToSegment(curSegment, id, size);
        writtenBytes += size;
        index[id].size = size;
        index[id].lastAccess = currentTime;
        index[id].accessCount = 1;
        // bring cache into good state
        LOG("admit 5",0,0,0);
        checkRebalance();
        LOG("admit 6",id,size,0);
        // record this new entries priority in histogram
        parts.addEntry(calcPrio(id));
    }

    int64_t getWrittenBytes() {
        return writtenBytes;
    }

    int64_t getAmplifiedBytes() {
        return amplifiedBytes;
    }

  void printSegmentStats() {
      for(size_t i; i<cache.size(); i++) {
          std::cerr << i << " " << parts.getNthCutoff(i) << " " << cache[i].size() << "\n";
    }
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

  cerr << "Flash segmentCount " << segmentCount << " blockSize " << blockSize
       << " blockCount " << cacheSize / blockSize << "\n";

  auto f = flash(segmentCount, blockSize,
                 cacheSize / blockSize
                 );
  
  ifstream infile;
  uint64_t reqs = 0, hits = 0;
  int64_t t, id, size, type;

  cerr << "running..." << endl;

  infile.open(path);
  while (infile >> t >> id >> size >> type)
  {
      if(f.lookup(id)) {
          // hit
          hits++;
      } else {
          f.admit(id, size);
      }
      reqs++;

      if(reqs % 10000 == 0) {
          std::cout << reqs << " "
                    << hits/double(reqs) << " "
                    << f.getAmplifiedBytes()/double(f.getWrittenBytes())
                    << "\n";
      }
  }

  infile.close();

  std::cout << path << " "
            << cacheSize << " "
            << segmentCount << " "
            << blockSize << " "
            << f.getWrittenBytes() << " "
            << f.getAmplifiedBytes() << " "
            << double(hits)/reqs << "\n";
  f.printSegmentStats();
  
  return 0;
}
