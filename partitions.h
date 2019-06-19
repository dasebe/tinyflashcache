#include <unordered_map>
#include <vector>
#include <map>
#include <algorithm>
#include <cassert>

class Partitions {
    std::vector<double> queue;
    std::unordered_map<size_t, double> idxToCutoff;
    std::map<double, size_t> cutoffToIdx;
    double maxSeenElement;
    double maxPartitionedElement;

public:
    Partitions(size_t partCount) {
        for(size_t idx=0; idx<partCount-1; idx++) {
            const double cutoff = double(idx+1)/double(partCount);
            idxToCutoff[idx] = cutoff;
            cutoffToIdx[cutoff] = idx;
        }
        // terminal node: just above 1
        idxToCutoff[partCount-1] = 1.01;
        cutoffToIdx[1.01] = partCount-1;
        maxPartitionedElement = 1.01;
        maxSeenElement = 1.01;
    }

    double getNthCutoff(size_t idx) {
        assert(idx <= idxToCutoff.size());
        return idxToCutoff[idx];
    }

    size_t getEntryIdx(double e) {
        if(e>=maxPartitionedElement) {
            // large element -> might iterate above bounary
            maxSeenElement = e;
            return (idxToCutoff.size() - 1);
        }
        // smaller element
        return cutoffToIdx.upper_bound(e)->second;
    }

    void addEntry(double e) {
        queue.push_back(e);
        if(e>maxSeenElement) {
            maxSeenElement = e;
        }
    }
    
    void reconfigure() {
        for(auto & c: idxToCutoff) {
            size_t element_offset = c.first * queue.size();
            if(element_offset < 1) {
                // element within normal range
                std::nth_element (queue.begin(), queue.begin()+element_offset, queue.end());
                c.second = *(queue.begin()+element_offset);
            } else {
                c.second = maxSeenElement;
                maxPartitionedElement = maxSeenElement;
            }
        }
        // refresh reverse mapping
        cutoffToIdx.clear();
        for(auto & c: idxToCutoff) {
            cutoffToIdx[c.second] = c.first;
        }
        queue.clear();
    }
};
