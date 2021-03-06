#include "util.h"

double calcBadPixel(const Mat& groundtruth,const Mat& disparityImage, Mat&  mask, double th,double amp,Mat& dsterr)
{
	if(mask.empty())mask = Mat::ones(groundtruth.size(),CV_8U);
	th*=amp;
	double ret=0.0;
	int count=0;
	int c2=0;
	if(dsterr.empty())dsterr=Mat::zeros(groundtruth.size(),CV_8U);
	for(int j=0;j<disparityImage.rows;j++)
	{
		const uchar* src=disparityImage.ptr<uchar>(j);
		const uchar* dest=groundtruth.ptr<uchar>(j);
		const uchar* mk=mask.ptr<uchar>(j);
		for(int i=0;i<disparityImage.cols;i++)
		{
			if(mk[i]!=0)
			{
				int s=src[i];
				int d=dest[i];
				int diff=s-d;
				if(abs(diff)>th)
				{
					//if(!isErr)
						dsterr.at<uchar>(j,i)=255;//abs(diff);

					c2++;
				}
				count++;
			}
		}
	}
	return ((double)c2/count)*100.0;
}

StereoEval::StereoEval(char* ground_truth_, char* mask_nonocc_, char* mask_all_, char* mask_disc_, double amp_)
{
	ground_truth = imread(ground_truth_,0);
	if(ground_truth.empty())cout<<"cannot open"<<ground_truth_<<endl;

	mask_all=imread(mask_all_,0);
	if(mask_all.empty())cout<<"cannot open"<<mask_all_<<endl;

	mask_nonocc=imread(mask_nonocc_,0);
	if(mask_nonocc.empty())cout<<"cannot open"<<mask_nonocc_<<endl;
	Mat temp = imread(mask_disc_,0);
	cv::compare(temp,255,mask_disc,CMP_EQ);
	if(mask_disc.empty())cout<<"cannot open"<<mask_disc_<<endl;
	amp = amp_;	
	threshmap_init();
}
void StereoEval::threshmap_init()
{
	isInit = true;
	all_th = Mat::zeros(ground_truth.size(),CV_8U);
	nonocc_th = Mat::zeros(ground_truth.size(),CV_8U);
	disc_th = Mat::zeros(ground_truth.size(),CV_8U);
	state_all = Mat::zeros(ground_truth.size(),CV_8UC3);
	state_nonocc = Mat::zeros(ground_truth.size(),CV_8UC3);
	state_disc = Mat::zeros(ground_truth.size(),CV_8UC3);
}

void StereoEval::init(Mat& ground_truth_, Mat& mask_nonocc_, Mat& mask_all_, Mat& mask_disc_, double amp_)
{
	ground_truth = ground_truth_;
	mask_all=mask_all_;
	mask_nonocc=mask_nonocc_;
	cv::compare(mask_disc_,255,mask_disc,CMP_EQ);
	amp = amp_;	
	threshmap_init();
}
StereoEval::StereoEval()
{
	isInit = false;
}
StereoEval::StereoEval(Mat& ground_truth_, Mat& mask_nonocc_, Mat& mask_all_, Mat& mask_disc_, double amp_)
{
	init(ground_truth_,mask_nonocc_,mask_all_,mask_disc_,amp_);
}

void  StereoEval::getBadPixel(Mat& src, double threshold, bool isPrint)
{
	all_th.setTo(0);
	nonocc_th.setTo(0);
	disc_th.setTo(0);
	all = calcBadPixel(ground_truth,src,mask_all,threshold,amp,all_th);
	nonocc = calcBadPixel(ground_truth,src,mask_nonocc,threshold,amp,nonocc_th);
	disc = calcBadPixel(ground_truth,src,mask_disc,threshold,amp,disc_th);

	message = format("nonocc,all,disc: %2.2f, %2.2f, %2.2f",nonocc, all, disc);
	if(isPrint)
	{
			cout<<message<<endl;
	}
}

void  StereoEval::getMSE(Mat& src,  bool isPrint)
{
/*	all_th.setTo(0);
	nonocc_th.setTo(0);
	disc_th.setTo(0);
	allMSE = calcMSE(ground_truth,src,0,0,mask_all);
	nonoccMSE = calcMSE(ground_truth,src,0,0,mask_nonocc);
	discMSE = calcMSE(ground_truth,src,0,0,mask_disc);

	message = format("nonocc,all,disc: %2.2f, %2.2f, %2.2f",nonoccMSE, allMSE, discMSE);
	if(isPrint)
	{
		cout<<message<<endl;
	}*/
}
void StereoEval::operator() (Mat& src, double threshold, bool isPrint,int disparity_scale)
{
	if(disparity_scale==1)
	{
		getBadPixel(src,threshold,isPrint);
	}
	else
	{
		Mat cnv(src.size(),CV_8U);
		const double div = amp/(double)disparity_scale;
		uchar* dest = cnv.data;
		if(src.type()==CV_16S)
		{
			short* data = src.ptr<short>(0);
			for(int i=0;i<src.size().area();i++)
			{
				dest[i]=(uchar)(data[i]*div + 0.5);
			}
		}
		else if (src.type()==CV_8U)
		{
			uchar* data = src.ptr<uchar>(0);
			for(int i=0;i<src.size().area();i++)
			{
				dest[i]=(uchar)(data[i]*div + 0.5);
			}
		}
		else if(src.type()==CV_32F)
		{
			float* data = src.ptr<float>(0);
			for(int i=0;i<src.size().area();i++)
			{
				dest[i]=(uchar)(data[i]*div + 0.5);
			}
		}
		getBadPixel(cnv,threshold,isPrint);
	}
}

void StereoEval::compare(Mat& before, Mat& after,double threshold, bool isPrint)
{
	if(isPrint)cout<<"before: ";
	getBadPixel(before,threshold,isPrint);
	Mat pall=all_th.clone();
	Mat pnonocc=nonocc_th.clone();
	Mat pdisc=disc_th.clone();
	if(isPrint)cout<<"after : ";
	getBadPixel(after,threshold,isPrint);

	Mat mask;
	Mat v;
	vector<Mat> sa;
	//improve: green
	//degrade: red
	//nochange and bad pixel: blue
	//no change: black
	state_nonocc.setTo(Scalar(0,0,255),~pnonocc);
	state_nonocc.setTo(Scalar(0,255,0),~nonocc_th);
	cv::compare(pnonocc,nonocc_th,mask,cv::CMP_EQ);
	state_nonocc.setTo(0,mask);
	nonocc_th.copyTo(v,mask);
	state_nonocc.setTo(Scalar(255,0,0),v);
	split(state_nonocc,sa);
	{
	
	int tcount = countNonZero(mask_nonocc);
	cv::compare(sa[2],255,mask,cv::CMP_EQ);
	int degrade = countNonZero(mask);
	cv::compare(sa[1],255,mask,cv::CMP_EQ);
	int  improve = countNonZero(mask);
	cv::compare(sa[0],255,mask,cv::CMP_EQ);
	int  remainbad = countNonZero(mask);
	cout<<format("nonocc: deg: -%2.2f, imp: %2.2f, rmn: %2.2f, rmn+deg: %2.2f \n",(100.0*degrade/(double)tcount),(100.0*improve/(double)tcount),(100.0*remainbad/(double)tcount),100.0*(remainbad+degrade)/(double)tcount);
	}

	state_all.setTo(Scalar(0,0,255),~pall);
	state_all.setTo(Scalar(0,255,0),~all_th);
	cv::compare(pall,all_th,mask,cv::CMP_EQ);
	state_all.setTo(0,mask);
	all_th.copyTo(v,mask);
	state_all.setTo(Scalar(255,0,0),v);
	split(state_all,sa);
	{
	
	int tcount = countNonZero(mask_all);
	cv::compare(sa[2],255,mask,cv::CMP_EQ);
	int degrade = countNonZero(mask);
	cv::compare(sa[1],255,mask,cv::CMP_EQ);
	int  improve = countNonZero(mask);
	cv::compare(sa[0],255,mask,cv::CMP_EQ);
	int  remainbad = countNonZero(mask);
	cout<<format("all   : deg: -%2.2f, imp: %2.2f, rmn: %2.2f, rmn+deg: %2.2f \n",(100.0*degrade/(double)tcount),(100.0*improve/(double)tcount),(100.0*remainbad/(double)tcount),100.0*(remainbad+degrade)/(double)tcount);
	}

	state_disc.setTo(Scalar(0,0,255),~pdisc);
	state_disc.setTo(Scalar(0,255,0),~disc_th);
	cv::compare(pdisc,disc_th,mask,cv::CMP_EQ);
	state_disc.setTo(0,mask);
	disc_th.copyTo(v,mask);
	state_disc.setTo(Scalar(255,0,0),v);
	split(state_disc,sa);
	{
	
	int tcount = countNonZero(mask_disc);
	cv::compare(sa[2],255,mask,cv::CMP_EQ);
	int degrade = countNonZero(mask);
	cv::compare(sa[1],255,mask,cv::CMP_EQ);
	int  improve = countNonZero(mask);
	cv::compare(sa[0],255,mask,cv::CMP_EQ);
	int  remainbad = countNonZero(mask);
	cout<<format("disc  : deg: -%2.2f, imp: %2.2f, rmn: %2.2f, rmn+deg: %2.2f \n",(100.0*degrade/(double)tcount),(100.0*improve/(double)tcount),(100.0*remainbad/(double)tcount),100.0*(remainbad+degrade)/(double)tcount);
	}
	//
}