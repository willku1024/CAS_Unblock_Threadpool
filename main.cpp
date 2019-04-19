#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <pthread.h>
#include <future>
#include <iostream>

#ifndef USESTLQ
#include "Threadpool.casQueue.hpp" 
#pragma message "!!!Use casQueue ThreadPool!!!"
#else
#include "Threadpool.stlQueue.hpp" 
#pragma message "!!!Use stlQueue ThreadPool!!!"
#endif

using namespace std;

int run(int i, const char *p)
{
    printf("callback thread[%lu] : (%d, %s)\n", pthread_self(), i, (char *)p);
    return i*i;
}

int main()
{
    const int threadNums = 100;
    const int ringQsize  = 15;
    ThreadPool threadPool(threadNums,ringQsize);
#if 10
        for (int i = 0; i < threadNums; i++)
    {
        threadPool.addTask(run, i, "helloworld");
        
    }
#else
    std::vector<std::future<int>> v;
    for (int i = 0; i < threadNums; i++)
    {
        auto ans = threadPool.addTask(run, i, "helloworld");
        v.push_back(std::move(ans));
    }

    for (size_t i = 0; i < v.size(); ++i)
    {
        printf("The return value is %d\n", v[i].get());
    
    }
#endif
    while (1)
    {
        //printf("there are still %d tasks need to process\n", threadPool.size());
        if (threadPool.size() == 0)
        {
            printf("Now, main thread will exit.\n");
            exit(0);
        }
        sleep(1);
    }
    return 0;
}