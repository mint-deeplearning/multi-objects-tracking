#include "kcftracker.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

static cv::Point clickPos;
static cv::Point movePos;

std::atomic_bool InputSignal;
bool addNewTrack{false};

cv::Mat frameCopy;

typedef struct TrackLists{
	Tracker* track;
	int obj_number;//编号
}TrackLists;

typedef struct TrackResLists{
	cv::Rect res;
	bool trackState;
	int obj_number;
}TrackResLists;

#if 0
typedef struct TThreadLists{
	std::thread sthread{NULL};
	int obj_number;
}TThreadLists;
#endif
std::vector<TrackResLists> trackResLists;

void onMouse(int event, int x, int y, int flags, void* param)
{
	switch(event)
	{
		case CV_EVENT_LBUTTONDOWN:
			clickPos.x = x;
			clickPos.y = y;
			addNewTrack = true;//当初始化完成后，需要置为false
		break;
		case CV_EVENT_MOUSEMOVE:
			movePos.x = x;
			movePos.y = y;
		break;
	}
}

void free_track_lists(std::vector<TrackLists>& T)
{
	//int memoryNums = 0;
	for(auto iter = T.begin(); iter != T.end(); iter++)
	{
		if(iter->track){
			//memoryNums++;
			delete iter->track;
		}
	}
	//printf("memoryNums = %d\n",memoryNums);
	T.erase(T.begin(), T.end());
	T.clear();
}

void TrackUpdatethread(void* tracker, int obj_number)
{
	bool isLoss = false;

	int TrackState = -2;
	while(1){
		if(!InputSignal) continue;
		//sem_wait();
		cv::Rect res = ((Tracker*)tracker)->update(frameCopy, &TrackState);

		for(auto iter = trackResLists.begin(); iter != trackResLists.end(); iter++)
		{
			if(iter->obj_number == obj_number){
				iter->res = res;
				iter->trackState = TrackState == -1 ? false : true;
				break;
			}
		}

		if(TrackState == -1){
			break;
		}
	}

}

void drawbox(cv::Mat src, std::vector<TrackResLists> res)
{
	int font_face = cv::FONT_HERSHEY_COMPLEX; 

	char osd_number[128];
	for(auto iter = res.begin(); iter != res.end(); iter++)
	{
		cv::Rect roi = iter->res;
		int obj_number = iter->obj_number;
		sprintf(osd_number, "obj:%d", obj_number);
		cv::rectangle(src, roi, cv::Scalar(0,0,255), 2, 8);
		cv::putText(src, osd_number, cv::Point(roi.x+roi.width*0.5-16, roi.y-10), font_face,
					0.6, cv::Scalar(0,0,255),1,8);
	}
}
void trackVideos(std::string videoPath)
{
	std::string windownName = "tracking.jpg";
	cv::VideoCapture cap(videoPath.c_str());
	if(!cap.isOpened()){
		printf("video open error!\n");
		return;
	}
	
	cv::Mat frame, frameOSD;

	std::vector<TrackLists> trackLists;

	int obj_numbers = 0;

	//std::vector<TThreadLists> threadLists;
	std::map<int, std::thread> threadMaps;
	

	//TrackLists temptTrack;
	while(cap.read(frame))
	{
		cv::setMouseCallback(windownName, onMouse);
		cv::Rect roi = cv::Rect(movePos.x-16, movePos.y-16, 32, 32);
		roi &= cv::Rect(0,0,frame.cols,frame.rows);
		frameCopy = frame.clone();
		//sem_post();
		InputSignal = true;
		if(addNewTrack){
			/*创建一个跟踪器*/
			TrackLists temptTrack;
			temptTrack.track = new KCFTracker(true, false, true, false);
			temptTrack.obj_number = obj_numbers++;

			cv::Rect trackInitRoi = cv::Rect(clickPos.x-16, clickPos.y-16, 32, 32);
			trackInitRoi &= cv::Rect(0,0,frame.cols,frame.rows);
			temptTrack.track->init(trackInitRoi, frame);
			addNewTrack = false;
			//跟踪器的vector
			trackLists.push_back(temptTrack);

			//跟踪线程的vector
			#if 0
			TThreadLists tempThreadList;
			tempThreadList.sthread = std::thread(TrackUpdatethread, temptTrack.track, temptTrack.obj_number);
			tempThreadList.obj_number = temptTrack.obj_number;
			threadLists.push_back(tempThreadList);
			#endif
			threadMaps[temptTrack.obj_number] = std::thread(TrackUpdatethread, temptTrack.track, temptTrack.obj_number);

			//跟踪结果的vector
			TrackResLists tempTrackRes;
			tempTrackRes.res = trackInitRoi;
			tempTrackRes.trackState = true;
			tempTrackRes.obj_number = temptTrack.obj_number;
			trackResLists.push_back(tempTrackRes);
		}
		//printf("error 0!\n");

		std::vector<int> delNumber;
		for(auto iter = trackResLists.begin(); iter != trackResLists.end(); iter++)
		{
			//目标丢失了，删除线程，删除跟踪器
			if(!iter->trackState)
			{
				//删除线程
				for(auto thriter = threadMaps.begin(); thriter != threadMaps.end(); thriter++)
				{
					if(thriter->first == iter->obj_number){
						if(thriter->second.joinable()){
							printf("release obj_number[%d] thread!\n", thriter->first);
							thriter->second.join();
						}
						threadMaps.erase(thriter);
						break;
					}
				}
				//删除跟踪器
				for(auto trackiter = trackLists.begin(); trackiter != trackLists.end(); trackiter++)
				{
					if(trackiter->obj_number == iter->obj_number)
					{
						//printf("error 6!\n");
						delete trackiter->track;
						trackLists.erase(trackiter);
						break;
					}
				}
				delNumber.push_back(iter-trackResLists.begin());
				//输出跟踪结果
				//trackResLists.erase(iter);
				//iter++;
			}
		}

		for(auto iter = delNumber.begin(); iter != delNumber.end(); iter++)
		{
			trackResLists.erase(trackResLists.begin()+(*iter));
		}

		drawbox(frame, trackResLists);
		cv::imshow(windownName, frame);
		cv::waitKey(20);
		InputSignal = false;
	}

	int trackNums = (int)trackLists.size();
	//printf("add track nums = %d\n", trackNums);
	free_track_lists(trackLists);
	cap.release();
}

int main()
{
	InputSignal = false;
	clickPos = cv::Point(0,0);
	movePos = cv::Point(0,0);
	addNewTrack = false;
	trackVideos("../中午-晴_output.mp4");
	return 0;
}