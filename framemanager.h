#ifndef hw4_frame_manager_h_
#define hw4_frame_manager_h_

#include <vector>
#include <list>
#include <map>
#include <string>
#include <pthread.h>

namespace hw4
{
	using namespace std;
	
	class FrameManager
	{
		public:
			class FileInfo
			{
				private:
					// The mutex used for reading/writing from/to the file
					pthread_mutex_t mutex;
					// A map from the page number to the loaded frame
					map<int, int> pages;
					// The pages in the order they were most recently used (first is most recently,last is least recently)
					list<int> recency;
					
				public:
					FileInfo();
					~FileInfo();
					
					void lock();
					void unlock();
					
					const map<int, int> &getPages() const;
					int pageCount() const;
					void usePage(int page);
					int getVictim();
					bool isPageLoaded(int page) const;
					int getPageFrame(int page) const;
					void mapPage(int page, int frame);
					void unmapPage(int page);
			};
			
			class FrameInfo
			{
				private:
					bool allocated;
					pthread_mutex_t mutex;
					char *begin;
					
				public:
					FrameInfo(char *b);
					~FrameInfo();
					
					void lock();
					void unlock();
					
					bool isAllocated() const;
					void allocate();
					void deallocate();
					
					void getData(char *dest, int offset);
					void setData(char *source);
			};
		private:
			const static int nFrames = 32;
			const static int sizeFrame = 1024;
			const static int nFramesPerFile = 4;
			
			// The contiguous data for all frames
			char *frameData;
			// The information for all frames
			vector<FrameInfo> frames;
			
			// The information for each file
			map<string, FileInfo> files;
			// The files in the order they were most recently used (first is most recently, last is least recently)
			list<string> recency;
		public:
			FrameManager();
			~FrameManager();
			
			// Makes the given file the most recently used
			void usePage(const string &name, int page);
			/* Loads the given page of the given file
			 * Return values:
			 *   0: Success
			 *   1: No such file
			 *   2: Invalid byte range
			 */
			int loadPage(const string &name, int page, int lastByteQ);
			
			/* Removes the given file
			 * Return values:
			 *   0: Success
			 *   1: No such file
			 */
			int removeFile(const string &name);
			
			/* Gets the next page starting at byteStart and ending at byteEnd and fills data with 
			 * Returns:
			 *   0: Success
			 *   1: No such file
			 *   2: Invalid byte range
			 */
			int get(const string &name, int byteStart, int byteEnd, char *&data, int &length);
	};
}

#endif /* hw4_frame_manager_h_ */
