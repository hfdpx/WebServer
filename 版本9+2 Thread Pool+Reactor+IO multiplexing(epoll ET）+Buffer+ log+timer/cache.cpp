#include "cache.h"
#include <utility>
#include <algorithm>

#include "rlog.h"

/* 线程安全版本的getFileAddr */
void Cache::getFileAddr(std::string fileName, int fileSize, pInfo& ptr)
{
    /*-
    * shared_ptr并不是线程安全的,对其的读写都需要加锁.
    */
    MutexLockGuard lock(mutex_);

    /*采用LRU最近最久未使用算法实现页面置换*/

    if (cache_.end() != cache_.find(fileName))   /* 如果在cache中找到了 */
    {
        //将该文件名放到最前面
        vv.erase(find(vv.begin(),vv.end(),fileName));
        vv.insert(vv.begin(),fileName);

         //mylog("%s files have finded!!!\n",fileName);

        ptr = cache_[fileName];
        return;
    }
    if (cache_.size() >= MAX_CACHE_SIZE)   /* 文件数目过多,需要删除一个元素 */
    {
        std::string str=vv.back();
        vv.pop_back();

        cache_.erase(str);

        // mylog("%s files have delete!!!\n",str);
    }

    /* 没有找到的话,我们需要加载文件 */
    ptr = std::make_shared<FileInfo>(fileName, fileSize);

    vv.insert(vv.begin(),fileName);

    cache_[fileName] = ptr;
}

/* 下面的版本线程不安全 */
pInfo Cache::getFileAddr(std::string fileName, int fileSize)
{
    // no use LRU

    if (cache_.end() != cache_.find(fileName))   /* 如果在cache中找到了 */
    {
        return cache_[fileName];
    }
    if (cache_.size() >= MAX_CACHE_SIZE)   /* 文件数目过多,需要删除一个元素 */
    {
        cache_.erase(cache_.begin()); /* 直接移除掉最前一个元素 */
    }
    /* 没有找到的话,我们需要加载文件 */
    std::shared_ptr<FileInfo> fileInfo = std::make_shared<FileInfo>(fileName, fileSize);

    cache_[fileName] = fileInfo;
    return fileInfo;
}


FileInfo::FileInfo(std::string& fileName, int fileSize)
{
    int srcfd = Open(fileName.c_str(), O_RDONLY, 0); /* 打开文件 */
    size_ = fileSize;
    addr_ = Mmap(0, fileSize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd); /* 关闭文件 */
}
