#include <random>
#include "MainThread.h"

#define DATA_TRANSFER_COUNT 10
#define DATA_SIZE 1e20

Data * generateData()
{
    Data * d = new Data();
    d->size = DATA_SIZE + rand() % 1000;
    d->buffer = new char[d->size];
    for(uint i = 0; i < d->size; i++)
        d->buffer[i] = (char) rand() % 127;
    return d;
}

int main()
{
    uint threadCount = 5;
    MainThread mt;
    WorkerCallbackImpl impl;
    mt.init(threadCount, &impl); 

    for(uint i=0; i < DATA_TRANSFER_COUNT; i++)
    {
        for(uint j =0; j < threadCount;j ++)
        {
            mt.sendTo(j, generateData());
        } 
    }
}
