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
        for(double i=0; i<segmentCount; i++) {
            cache.emplace(i/double(segmentCount),segment());
        }
    }

    bool lookup(int64_t id) {
        time++;
        highestprio = time;
        if(index.count(id)>0) {
            // hit
            index[id].accessCount++;
            index[id].lastAccess = time;
            return true;
        }
        return false;
    }

    void fullSegment(segment & curSegment) {
        if(curSegment.size()<=blocksPerSegment) {
            std::cerr << "called full, but not full\n";
            return;
        }
        // evict single block, at front
        auto & evictBlock = curSegment.front();
        // iterate over stored objects
        for(size_t i=0; i<evictBlock.ids.size(); i++) {
            const auto id = evictBlock.ids[i];
            const auto size = evictBlock.sizes[i];
            const auto lastAccess = index[id].lastAccess;
            const double prio = double(lastAccess-lowestprio) / double(highestprio-lowestprio);
            if(prio > 0.1) {
                // readmit as prio > 10%
                auto & moveToSegment = cache.lower_bound(prio)->second;
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
    }

    void checkFullBlocks() { // potentially buggy
        // from top to bottom
        for (auto rit = cache.rbegin(); rit!=cache.rend(); ++rit) {
            auto & curSegment = rit->second;
            if(curSegment.size()>blocksPerSegment) {
                fullSegment(curSegment);
            }
        }
    }
    
    void admit(int64_t id, int64_t size) {
        if(size>blockSize) {
            // don't admit too large objects
            std::cerr << "object size > block size!!\n";
            return;
        }
        auto & curSegment = cache[1.0]; // admit to highest prio segment
        auto & curBlock = curSegment.back();
        if(curBlock.bytes+size>blockSize) {
            // add a new block
            curSegment.push(block());
            curBlock = curSegment.back();
        }
        // process cache admission
        curBlock.ids.push_back(id);
        curBlock.sizes.push_back(size);
        curBlock.bytes+=size;
        writtenBytes += size;
        index[id].size = size;
        index[id].lastAccess = time;
        index[id].accessCount = 1;
        // bring cache into good state
        checkFullBlocks();
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
  
  return 0;
}
