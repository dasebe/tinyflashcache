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

    // automatic periodic reconfiguration
    size_t reconfiguration_interval;
    size_t reqs_to_next_reconfiguration;

    // additionally track bottom percent cutoff
    double first_percentile;

public:
    Partitions(size_t partCount, size_t rec_int)
        : reconfiguration_interval(rec_int),
          reqs_to_next_reconfiguration(1)
    {
        std::cerr << "Partitions " << partCount << " " << rec_int << "\n";
        assert(partCount>0);
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
        first_percentile = 0.01;
    }

    double getNthCutoff(size_t idx) {
        if(reqs_to_next_reconfiguration<1) {
            reconfigure();
        }
        assert(idx <= idxToCutoff.size());
        return idxToCutoff[idx];
    }

    size_t getEntryIdx(double e) {
        if(reqs_to_next_reconfiguration<1) {
            reconfigure();
        }
        if(e>=maxPartitionedElement) {
            // large element -> might iterate above bounary
            maxSeenElement = e;
            return (idxToCutoff.size() - 1);
        }
        // smaller element
        return cutoffToIdx.upper_bound(e)->second;
    }

    bool isBottomPerc(double e) {
        if(reqs_to_next_reconfiguration<1) {
            reconfigure();
        }
        return(e<first_percentile);
    }

    void addEntry(double e) {
        queue.push_back(e);
        if(e>maxSeenElement) {
            maxSeenElement = e;
        }
        reqs_to_next_reconfiguration--;
        if(reqs_to_next_reconfiguration<1) {
            reconfigure();
        }
    }
    
    void reconfigure() {
        if(queue.size()<idxToCutoff.size()) {
            // don't reconfigure if too few datapoints
            return;
        }
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
        // reconfigure bottom percentile
        size_t element_offset = 0.1 * queue.size();
        std::nth_element (queue.begin(), queue.begin()+element_offset, queue.end());
        first_percentile = *(queue.begin()+element_offset);
        // refresh reverse mapping
        cutoffToIdx.clear();
        for(auto & c: idxToCutoff) {
            cutoffToIdx[c.second] = c.first;
            std::cerr << "histreconf " << c.first << " " << c.second << "\n";
        }
        queue.clear();
        reqs_to_next_reconfiguration = reconfiguration_interval;
    }
};
