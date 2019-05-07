#include <math.h>
#include <iostream>
#include <functional>
#include <algorithm>
#include <string>
#include <vector>

/* sampling object id and object size from a Zipf-like distribution
 (aka the independent reference model (IRM))

 Multiple objects with similar rates are grouped together for more efficient sampling.
 Two level sampling process: first skewed sample to select the rate, then uniform sample within rate to select object.
*/


class ZipfRequests {

private:
    typedef std::pair<int64_t, int64_t> IdSizePair;
    typedef std::pair<long double, std::vector<IdSizePair>*> CDFPair;

    std::mt19937_64 rd_gen;
    std::uniform_real_distribution<long double> _distribution;
    std::vector<CDFPair> _popularityCdf; // CDF of popularity distribution, scaled by totalCount (i.e., range [0,totalCount])

    static bool CDFCompare(const CDFPair& a, const CDFPair& b)
    {
        return a.first < b.first;
    }

public:
    ZipfRequests(std::string sizeFile, int64_t objCount) {
        // init
        rd_gen.seed(1);
        // scan files from file
        std::vector<int64_t> base_sizes;
        if(sizeFile.size()>0) {
            std::ifstream infile;
            infile.open(sizeFile);
            int64_t tmp;
            while (infile >> tmp) {
                if(tmp>0 && tmp<4096) {
            base_sizes.push_back(tmp);
                }
            }
            infile.close();
        }
        if(base_sizes.size()==0) {
            base_sizes = std::vector<int64_t>({16,32,64,100,128,200,256,300,100,128,200,256,300,100,128,200,256,300,100,128,200,256,300,100,128,200,256,300,400,700,900,512,1024,800,1400,1500,2048,4096});
        }
        std::uniform_int_distribution<uint64_t> size_dist(0,base_sizes.size()-1);
        int64_t sampled_size;
        long double rate = 1e10L;
        uint64_t div_factor = 1;
        std::unordered_map<long double, std::vector<IdSizePair>*> data;

        // create high-level distribution datastructures
        for(int64_t i=0; i<objCount;) {
            std::vector<IdSizePair>*& idSizes = data[roundl(rate)+1.0L];
            if (idSizes == NULL) {
                idSizes = new std::vector<IdSizePair>();
            }
            // insert multiple objects at this rate
            for(uint64_t j=0; j<div_factor; j++) {
                sampled_size=base_sizes[size_dist(rd_gen)];
                idSizes->emplace_back(i, sampled_size);
                i++;
            }
            rate /= 1.5;  // Zipf rate (alpha <1)
            div_factor *= 2;  // increase obj count at next rate
        }

        // calculate popularity CDF
        long double totalCount = 0;
        for (auto &it : data) {
            long double count = it.first;
            std::vector<IdSizePair>* idSizes = it.second;
            totalCount += count * idSizes->size();
            _popularityCdf.emplace_back(totalCount, idSizes);
        }
        _distribution = std::uniform_real_distribution<long double>(0, totalCount - 1);
    }


    void Sample(SimpleRequest * req) {
        // sample
        long double r1 = _distribution(rd_gen);
        const std::vector<CDFPair>::iterator it = std::upper_bound(_popularityCdf.begin(), _popularityCdf.end(), CDFPair(r1, NULL), CDFCompare);
        const std::vector<IdSizePair>& idSizes = *(it->second);
        size_t r2 = std::uniform_int_distribution<size_t>(0, idSizes.size() - 1)(rd_gen);
        const IdSizePair& idSize = idSizes[r2];
        req->reinit(idSize.first,idSize.second); // id, size
    }
};
