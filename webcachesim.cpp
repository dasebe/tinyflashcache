#include <fstream>
#include <string>
#include <regex>
#include "caches/lru_variants.h"
#include "caches/gd_variants.h"
#include "request.h"
#include "ripq.h"

using namespace std;

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
