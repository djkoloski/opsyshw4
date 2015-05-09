#ifndef hw4_frame_manager_cpp_
#define hw4_frame_manager_cpp_

#include "framemanager.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include <sys/stat.h>
#include <stdio.h>

namespace hw4
{
	FrameManager::FileInfo::FileInfo() :
		mutex(),
		pages()
	{
		pthread_mutex_init(&mutex, NULL);
	}
	FrameManager::FileInfo::~FileInfo()
	{
		pthread_mutex_destroy(&mutex);
	}
	void FrameManager::FileInfo::lock()
	{
		pthread_mutex_lock(&mutex);
	}
	void FrameManager::FileInfo::unlock()
	{
		pthread_mutex_unlock(&mutex);
	}
	int FrameManager::FileInfo::getVictim() const
	{
		if (pages.size() == 0)
			return -1;
		return pages.begin()->first;
	}
	bool FrameManager::FileInfo::isPageLoaded(int page) const
	{
		return pages.count(page) != 0;
	}
	int FrameManager::FileInfo::getPageFrame(int page) const
	{
		map<int, int>::const_iterator i = pages.find(page);
		return i->second;
	}
	void FrameManager::FileInfo::mapPage(int page, int frame)
	{
		pages[page] = frame;
	}
	void FrameManager::FileInfo::unmapPage(int page)
	{
		pages.erase(page);
	}
	
	FrameManager::FrameInfo::FrameInfo(char *b) :
		mutex(),
		allocated(false),
		begin(b)
	{
		pthread_mutex_init(&mutex, NULL);
	}
	FrameManager::FrameInfo::~FrameInfo()
	{
		pthread_mutex_destroy(&mutex);
	}
	void FrameManager::FrameInfo::lock()
	{
		pthread_mutex_lock(&mutex);
	}
	void FrameManager::FrameInfo::unlock()
	{
		pthread_mutex_unlock(&mutex);
	}
	bool FrameManager::FrameInfo::isAllocated() const
	{
		return allocated;
	}
	void FrameManager::FrameInfo::allocate()
	{
		allocated = true;
	}
	void FrameManager::FrameInfo::deallocate()
	{
		allocated = false;
	}
	void FrameManager::FrameInfo::getData(char *dest)
	{
		memcpy(dest, begin, sizeFrame);
	}
	void FrameManager::FrameInfo::setData(char *source)
	{
		memcpy(begin, source, sizeFrame);
	}
	
	FrameManager::FrameManager() :
		frameData(new char[nFrames * sizeFrame]),
		frames(),
		files(),
		recency()
	{
		for (int i = 0; i < nFrames; ++i)
			frames.push_back(FrameInfo(frameData + i * sizeFrame));
	}
	FrameManager::~FrameManager()
	{
		for (int i = 0; i < frames.size(); ++i)
		{
			if (frames[i].isAllocated())
			{
				frames[i].deallocate();
				cout << "[thread " << pthread_self() << "] Deallocated frame " << i << endl;
			}
		}
		
		delete[] frameData;
		frames.clear();
		
		files.clear();
		recency.clear();
	}
	void FrameManager::useFile(const string &name)
	{
		list<string>::iterator i = find(recency.begin(), recency.end(), name);
		if (i != recency.end())
			recency.erase(i);
		recency.push_front(name);
	}
	int FrameManager::loadPage(const string &name, int page, int lastByteQ)
	{
		if (files.count(name) && files[name].isPageLoaded(page))
			return true;
		
		int byteStart = page * sizeFrame;
		int byteEnd = byteStart + sizeFrame;
		struct stat buffer;
		int status = lstat(name.c_str(), &buffer);
		if (status != 0)
			return 1;
		
		if (buffer.st_size <= byteStart)
			return 2;
		
		if (buffer.st_size < byteEnd)
			byteEnd = buffer.st_size;
		
		if (lastByteQ > byteEnd)
			return 2;
		
		char *data = new char[sizeFrame];
		
		FileInfo &file = (files.count(name) ? files[name] : files[name] = FileInfo());
		file.lock();
		
		FILE *fd = fopen(name.c_str(), "rb");
		fseek(fd, byteStart, SEEK_SET);
		fread(data, sizeof(char), byteEnd - byteStart, fd);
		fclose(fd);
		
		file.unlock();
		
		for (int i = 0; i < frames.size(); ++i)
		{
			frames[i].lock();
			if (!frames[i].isAllocated())
			{
				frames[i].allocate();
				frames[i].setData(data);
				frames[i].unlock();
				
				delete[] data;
				
				file.mapPage(page, i);
				return i;
			}
			frames[i].unlock();
		}
		
		string victim;
		int vPage = -1;
		do
		{
			victim = recency.back();
			recency.pop_back();
			if (files.count(victim) != 0 && files[victim].getVictim() != -1)
			{
				vPage = files[victim].getVictim();
				break;
			}
		}
		while (vPage == -1);
		int vFrame = files[victim].getPageFrame(vPage);
		
		frames[vFrame].lock();
		frames[vFrame].deallocate();
		frames[vFrame].allocate();
		frames[vFrame].setData(data);
		frames[vFrame].unlock();
		cout << "[thread " << pthread_self() << "] Allocated page " << page << " to frame " << vFrame << " (replaced page " << vPage << ")" << endl;
		
		delete[] data;
		
		file.mapPage(page, vFrame);
		
		return 0;
	}
	int FrameManager::get(const string &name, int byteStart, int byteEnd, char *&data, int &length)
	{
		int page = byteStart / sizeFrame;
		int status = loadPage(name, page, byteEnd);
		if (status != 0)
			return status;
		
		int pageEnd = sizeFrame * (page + 1);
		byteEnd = (pageEnd < byteEnd ? pageEnd : byteEnd);
		length = byteEnd - byteStart;
		
		FileInfo file = files[name];
		int frame = file.getPageFrame(page);
		data = new char[sizeFrame];
		frames[frame].getData(data);
		
		return 0;
	}
}

#endif /* hw4_frame_manager_cpp */
