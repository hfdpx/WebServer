#ifndef _CACHE_H_
#define _CACHE_H_

#include <map>
#include <string>
#include "csapp.h"
#include "noncopyable.h"
#include <boost/shared_ptr.hpp>
#include<vector>
#include<bits/stdc++.h>
using namespace std;
/*-
* 采用智能指针,本质就是利用RAll机制
  方便资源回收
*/

//自己实现的文件结构体
class FileInfo : noncopyable
{
public:

    //构造函数中进行映射
	FileInfo(std::string& fileName, int fileSize) {
		int srcfd = Open(fileName.c_str(), O_RDONLY, 0); /* 打开文件 */
		size_ = fileSize;
		addr_ = Mmap(0, fileSize, PROT_READ, MAP_PRIVATE, srcfd, 0);//将文件映射进入内存
		Close(srcfd); /* 关闭文件 */
	}

	//析构函数中解除映射
	~FileInfo() {
		Munmap(addr_, size_); /* 解除映射 */
	}
public:
	void *addr_; /* 地址信息 */
	int size_; /* 文件大小 */
};

//在Cache中缓存多个文件
class Cache : noncopyable
{
	typedef std::map<std::string, boost::shared_ptr<FileInfo>>::iterator it;
private:
	std::map<std::string, boost::shared_ptr<FileInfo>> cache_; /* 实现文件名称到地址的一个映射 */

	static const size_t MAX_CACHE_SIZE = 100; /* 最多缓存100个文件 */

	std::vector<std::string> vv;

public:

    //在Cache中寻找文件,没有找到需要加载到Cache中
	boost::shared_ptr<FileInfo> getFileAddr(std::string fileName, int fileSize) {

	    /*采用LRU最近最久未使用算法实现页面置换*/

	    /*如果在Cache中找到了*/
		if (cache_.end() != cache_.find(fileName)) {

            //将该文件名放到最前面
            vv.erase(find(vv.begin(),vv.end(),fileName));
            vv.insert(vv.begin(),fileName);

            cout<<fileName<<"files have finded!!!"<<endl;

			return cache_[fileName];
		}

		/* 文件数目过多,需要删除一个元素 */
		if (cache_.size() >= MAX_CACHE_SIZE) {

            std:string str=vv.back();
            vv.pop_back();

            cache_.erase(str);

            cout<<str<<"files have delete!!!"<<endl;
		}

		/* 没有找到的话,我们需要加载文件 */
		boost::shared_ptr<FileInfo> fileInfo(new FileInfo(fileName, fileSize));

		cache_[fileName] = fileInfo;

		vv.insert(vv.begin(),fileName);

		cout<<fileName<<"files have load!!!"<<endl;
		//返回文件
		return fileInfo;
	}

};

#endif /* _CACHE_H_ */
