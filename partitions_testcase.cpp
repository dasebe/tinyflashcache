#include <iostream>
#include "partitions.h"

using namespace std;

void checkEntryIdx(Partitions & p, double val) {
    cout << "co " << val << " idx " << p.getEntryIdx(val) << "\n";
}

int main (int argc, char* argv[])
{
    cout << "**startup**\n";
    auto p = Partitions(2,1000);
    cout << "**initial**\n";
    cout << "idx 0 co " << p.getNthCutoff(0) << "\n";
    cout << "idx 1 co " << p.getNthCutoff(1) << "\n";
    checkEntryIdx(p,0);
    checkEntryIdx(p,0.49);
    checkEntryIdx(p,0.5);
    checkEntryIdx(p,0.9);
    checkEntryIdx(p,1);
    checkEntryIdx(p,1.1);
    cout << "**added 10 elems**\n";
    for(double i=40; i<140; i=i+10) {
        p.addEntry(i);
    }
    cout << "idx 0 co " << p.getNthCutoff(0) << "\n";
    cout << "idx 1 co " << p.getNthCutoff(1) << "\n";
    checkEntryIdx(p,0);
    checkEntryIdx(p,15);
    checkEntryIdx(p,39);
    checkEntryIdx(p,40);
    checkEntryIdx(p,41);
    checkEntryIdx(p,70);
    checkEntryIdx(p,90);
    checkEntryIdx(p,130);
    checkEntryIdx(p,139);
    checkEntryIdx(p,140);
    checkEntryIdx(p,141);
    checkEntryIdx(p,23095358235);
    p.reconfigure();
    cout << "idx 0 co " << p.getNthCutoff(0) << "\n";
    cout << "idx 1 co " << p.getNthCutoff(1) << "\n";
    checkEntryIdx(p,0);
    checkEntryIdx(p,15);
    checkEntryIdx(p,39);
    checkEntryIdx(p,40);
    checkEntryIdx(p,41);
    checkEntryIdx(p,70);
    checkEntryIdx(p,90);
    checkEntryIdx(p,130);
    checkEntryIdx(p,139);
    checkEntryIdx(p,140);
    checkEntryIdx(p,141);
    checkEntryIdx(p,23095358235);
    cout << "is bottom 4 " << p.isBottomPerc(4) << "\n";
    cout << "is bottom 30 " << p.isBottomPerc(30) << "\n";
    cout << "is bottom 40 " << p.isBottomPerc(40) << "\n";
    cout << "is bottom 50 " << p.isBottomPerc(50) << "\n";
    cout << "is bottom 100 " << p.isBottomPerc(100) << "\n";
    cout << "is bottom 140 " << p.isBottomPerc(140) << "\n";
    return 0;
}
