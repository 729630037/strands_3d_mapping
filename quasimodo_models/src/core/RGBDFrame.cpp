#include "RGBDFrame.h"

#include <pcl/console/parse.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/segmentation/supervoxel_clustering.h>

//VTK include needed for drawing graph lines
#include <vtkPolyLine.h>

#include <iostream>
#include <vector>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/search/search.h>
#include <pcl/search/kdtree.h>
#include <pcl/features/normal_3d.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/filters/passthrough.h>
#include <pcl/segmentation/region_growing.h>

//#include "seeds3.cpp"
//#include "seeds2.cpp"
#include <vector>
#include <string>

//#include "seeds2.h"

#include <cv.h>
#include <highgui.h>
#include <fstream>


#include "helper.h"

#include <ctime>

namespace reglib
{
unsigned long RGBDFrame_id_counter;
RGBDFrame::RGBDFrame(){
	id = RGBDFrame_id_counter++;
	capturetime = 0;
	pose = Eigen::Matrix4d::Identity();
}

bool updated = true;
void on_trackbar( int, void* ){updated = true;}

RGBDFrame::RGBDFrame(Camera * camera_, cv::Mat rgb_, cv::Mat depth_, double capturetime_, Eigen::Matrix4d pose_, bool compute_normals){
	sweepid = -1;
	id = RGBDFrame_id_counter++;
	camera = camera_;
	rgb = rgb_;
	depth = depth_;
	capturetime = capturetime_;
	pose = pose_;

	IplImage iplimg = rgb_;
	IplImage* img = &iplimg;

	int width = img->width;
	int height = img->height;
	int sz = height*width;
	const double idepth			= camera->idepth_scale;
	const double cx				= camera->cx;
	const double cy				= camera->cy;
	const double ifx			= 1.0/camera->fx;
	const double ify			= 1.0/camera->fy;

	connections.resize(1);
	connections[0].resize(1);
	connections[0][0] = 0;

	intersections.resize(1);
	intersections[0].resize(1);
	intersections[0][0] = 0;

	nr_labels = 1;
	labels = new int[width*height];
	for(unsigned int i = 0; i < width*height; i++){labels[i] = 0;}

//	printf("Image loaded %d\n",img->nChannels);


	unsigned short * depthdata = (unsigned short *)depth.data;
	unsigned char * rgbdata = (unsigned char *)rgb.data;

	//unsigned short * oriframe = (unsigned short *)depth.data;
	//for(int i = 10000; i < 640*480; i+=10000){printf("%i depth %i\n",i,oriframe[i]);}
	//exit(0);

	depthedges.create(height,width,CV_8UC1);
	unsigned char * depthedgesdata = (unsigned char *)depthedges.data;

	double t = 0.01;
	for(unsigned int w = 0; w < width; w++){
		for(unsigned int h = 0; h < height;h++){
			int ind = h*width+w;
			depthedgesdata[ind] = 0;
			double z = idepth*double(depthdata[ind]);
			if(w > 0){
				double z2 = idepth*double(depthdata[ind-1]);
				double info = 1.0/(z*z+z2*z2);
				double diff = fabs(z2-z)*info;
				if(diff > t){depthedgesdata[ind] = 255;}
			}

			if(w < width-1){
				double z2 = idepth*double(depthdata[ind+1]);
				double info = 1.0/(z*z+z2*z2);
				double diff = fabs(z2-z)*info;
				if(diff > t){depthedgesdata[ind] = 255;}
			}

			if(h > 0){
				double z2 = idepth*double(depthdata[ind-width]);
				double info = 1.0/(z*z+z2*z2);
				double diff = fabs(z2-z)*info;
				if(diff > t){depthedgesdata[ind] = 255;}
			}

			if(h < height-1){
				double z2 = idepth*double(depthdata[ind+width]);
				double info = 1.0/(z*z+z2*z2);
				double diff = fabs(z2-z)*info;
				if(diff > t){depthedgesdata[ind] = 255;}
			}

			if(h > 0 && w > 0){
				double z2 = idepth*double(depthdata[ind-width-1]);
				double info = 1.0/(z*z+z2*z2);
				double diff = fabs(z2-z)*info;
				if(diff > t){depthedgesdata[ind] = 255;}
			}

			if(w > 0 && h < height-1){
				double z2 = idepth*double(depthdata[ind+width-1]);
				double info = 1.0/(z*z+z2*z2);
				double diff = fabs(z2-z)*info;
				if(diff > t){depthedgesdata[ind] = 255;}
			}

			if(h > 0 && w < width-1){
				double z2 = idepth*double(depthdata[ind-width+1]);
				double info = 1.0/(z*z+z2*z2);
				double diff = fabs(z2-z)*info;
				if(diff > t){depthedgesdata[ind] = 255;}
			}

			if(h < height-1 && w < width-1){
				double z2 = idepth*double(depthdata[ind+width+1]);
				double info = 1.0/(z*z+z2*z2);
				double diff = fabs(z2-z)*info;
				if(diff > t){depthedgesdata[ind] = 255;}
			}
		}
	}

//	for(unsigned int w = 0; w < width; w++){
//		for(unsigned int h = 0; h < height;h++){
//			int ind = h*width+w;
//			double z = idepth*double(depthdata[ind]);
//			if(w > 0		&& depthedgesdata[ind-1] > 0){		depthedgesdata[ind] = 255;}
//			if(h > 0		&& depthedgesdata[ind-width] > 0){	depthedgesdata[ind] = 255;}
//			if(w < width-1	&& depthedgesdata[ind+1] > 0){		depthedgesdata[ind] = 255;}
//			if(h < height-1 && depthedgesdata[ind+width] > 0){	depthedgesdata[ind] = 255;}
//		}
//	}

/*
	cv::namedWindow( "rgb", cv::WINDOW_AUTOSIZE );			cv::imshow( "rgb", rgb );
	cv::namedWindow( "normals", cv::WINDOW_AUTOSIZE );		cv::imshow( "normals", normals );
	cv::namedWindow( "depth", cv::WINDOW_AUTOSIZE );		cv::imshow( "depth", depth );
	cv::namedWindow( "depthedges", cv::WINDOW_AUTOSIZE );	cv::imshow( "depthedges", depthedges );
	cv::namedWindow( "erosion_mask", cv::WINDOW_AUTOSIZE );	cv::imshow( "erosion_mask", erosion_mask );
	cv::waitKey(0);
*/

/*
	unsigned char * erosion_maskdata = (unsigned char *)erosion_mask.data;
	for(unsigned int w = 1; w < 639; w++){
		for(unsigned int h = 1; h < 479; h++){
			if(erosion_maskdata[h*640+w] != 0){
				testw.push_back(w);
				testh.push_back(h);
			}
		}
	}
*/

	if(compute_normals){


		normals.create(height,width,CV_32FC3);
		float * normalsdata = (float *)normals.data;



		//printf("idepth: %f\n",idepth);

		pcl::PointCloud<pcl::PointXYZ>::Ptr	cloud	(new pcl::PointCloud<pcl::PointXYZ>);
		pcl::PointCloud<pcl::Normal>::Ptr	normals (new pcl::PointCloud<pcl::Normal>);
		cloud->width	= width;
		cloud->height	= height;
		cloud->points.resize(width*height);

		for(unsigned int w = 0; w < width; w++){
			for(unsigned int h = 0; h < height;h++){
				int ind = h*width+w;
				pcl::PointXYZ & p = cloud->points[ind];
				//p.b = rgbdata[3*ind+0];
				//p.g = rgbdata[3*ind+1];
				//p.r = rgbdata[3*ind+2];
				double z = idepth*double(depthdata[ind]);
				//if(w % 10 == 0 && h % 10 == 0){printf("%i %i -> %f\n",w,h,z);}

				if(z > 0){
					p.x = (double(w) - cx) * z * ifx;
					p.y = (double(h) - cy) * z * ify;
					p.z = z;
					//cloud->points[ind] = p;
				}else{
					p.x = NAN;
					p.y = NAN;
					p.z = NAN;
				}
			}
		}

		//pcl::io::savePCDFileASCII ("test_pcd.pcd", *cloud);

		pcl::IntegralImageNormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
		ne.setInputCloud(cloud);


		bool tune = false;
		unsigned char * combidata;
		cv::Mat combined;

		int NormalEstimationMethod = 0;
		int MaxDepthChangeFactor = 20;
		int NormalSmoothingSize = 7;
		int depth_dependent_smoothing = 1;

		ne.setMaxDepthChangeFactor(0.001*double(MaxDepthChangeFactor));
		ne.setNormalSmoothingSize(NormalSmoothingSize);
		ne.setDepthDependentSmoothing (depth_dependent_smoothing);
		ne.compute(*normals);

		for(unsigned int w = 0; w < width; w++){
			for(unsigned int h = 0; h < height;h++){
				int ind = h*width+w;
				pcl::Normal		p2		= normals->points[ind];
				if(!isnan(p2.normal_x)){
					normalsdata[3*ind+0]	= p2.normal_x;
					normalsdata[3*ind+1]	= p2.normal_y;
					normalsdata[3*ind+2]	= p2.normal_z;
				}else{
					normalsdata[3*ind+0]	= 2;
					normalsdata[3*ind+1]	= 2;
					normalsdata[3*ind+2]	= 2;
				}
			}
		}

		if(tune){
			combined.create(height,2*width,CV_8UC3);
			combidata = (unsigned char *)combined.data;

			cv::namedWindow( "normals", cv::WINDOW_AUTOSIZE );
			cv::namedWindow( "combined", cv::WINDOW_AUTOSIZE );

			//cv::createTrackbar( "NormalEstimationMethod", "combined", &NormalEstimationMethod, 3, on_trackbar );
			//cv::createTrackbar( "MaxDepthChangeFactor", "combined", &MaxDepthChangeFactor, 1000, on_trackbar );
			//cv::createTrackbar( "NormalSmoothingSize", "combined", &NormalSmoothingSize, 100, on_trackbar );
			//cv::createTrackbar( "depth_dependent_smoothing", "combined", &depth_dependent_smoothing, 1, on_trackbar );

			//while(true){

				if(NormalEstimationMethod == 0){ne.setNormalEstimationMethod (ne.COVARIANCE_MATRIX);}
				if(NormalEstimationMethod == 1){ne.setNormalEstimationMethod (ne.AVERAGE_3D_GRADIENT);}
				if(NormalEstimationMethod == 2){ne.setNormalEstimationMethod (ne.AVERAGE_DEPTH_CHANGE);}
				if(NormalEstimationMethod == 3){ne.setNormalEstimationMethod (ne.SIMPLE_3D_GRADIENT);}

				ne.setMaxDepthChangeFactor(0.001*double(MaxDepthChangeFactor));
				ne.setNormalSmoothingSize(NormalSmoothingSize);
				ne.setDepthDependentSmoothing (depth_dependent_smoothing);
				ne.compute(*normals);
				for(unsigned int w = 0; w < width; w++){
					for(unsigned int h = 0; h < height;h++){
						int ind = h*width+w;
						pcl::Normal		p2		= normals->points[ind];
						if(!isnan(p2.normal_x)){
							normalsdata[3*ind+0]	= p2.normal_x;
							normalsdata[3*ind+1]	= p2.normal_y;
							normalsdata[3*ind+2]	= p2.normal_z;
						}else{
							normalsdata[3*ind+0]	= 2;
							normalsdata[3*ind+1]	= 2;
							normalsdata[3*ind+2]	= 2;
						}
					}
				}


				for(unsigned int w = 0; w < width; w++){
					for(unsigned int h = 0; h < height;h++){
						int ind = h*width+w;
						int indn = h*2*width+(w+width);
						int indc = h*2*width+(w);
						combidata[3*indc+0]	= rgbdata[3*ind+0];
						combidata[3*indc+1]	= rgbdata[3*ind+1];
						combidata[3*indc+2]	= rgbdata[3*ind+2];
						pcl::Normal		p2		= normals->points[ind];
						if(!isnan(p2.normal_x)){
							combidata[3*indn+0]	= 255.0*fabs(p2.normal_x);
							combidata[3*indn+1]	= 255.0*fabs(p2.normal_y);
							combidata[3*indn+2]	= 255.0*fabs(p2.normal_z);
						}else{
							combidata[3*indn+0]	= 255;
							combidata[3*indn+1]	= 255;
							combidata[3*indn+2]	= 255;
						}
					}
				}
				char buf [1024];
				sprintf(buf,"combined%i.png",id);
				cv::imwrite( buf, combined );
				printf("saving: %s\n",buf);
				cv::imshow( "combined", combined );
				cv::waitKey(0);
				//while(!updated){cv::waitKey(50);}
				updated = false;
			//}
		}
	}
	//show(true);
}

RGBDFrame::~RGBDFrame(){}

void RGBDFrame::show(bool stop){
	cv::namedWindow( "rgb", cv::WINDOW_AUTOSIZE );			cv::imshow( "rgb", rgb );
	cv::namedWindow( "normals", cv::WINDOW_AUTOSIZE );		cv::imshow( "normals", normals );
	cv::namedWindow( "depth", cv::WINDOW_AUTOSIZE );		cv::imshow( "depth", depth );
	cv::namedWindow( "depthedges", cv::WINDOW_AUTOSIZE );	cv::imshow( "depthedges", depthedges );
	if(stop){	cv::waitKey(0);}else{					cv::waitKey(30);}
}

pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr RGBDFrame::getPCLcloud(){
	unsigned char * rgbdata = (unsigned char *)rgb.data;
	unsigned short * depthdata = (unsigned short *)depth.data;

	const unsigned int width	= camera->width; const unsigned int height	= camera->height;
	const double idepth			= camera->idepth_scale;
	const double cx				= camera->cx;		const double cy				= camera->cy;
	const double ifx			= 1.0/camera->fx;	const double ify			= 1.0/camera->fy;

	pcl::PointCloud<pcl::PointXYZRGB>::Ptr	cloud	(new pcl::PointCloud<pcl::PointXYZRGB>);
	pcl::PointCloud<pcl::Normal>::Ptr		normals (new pcl::PointCloud<pcl::Normal>);
	cloud->width	= width;
	cloud->height	= height;
	cloud->points.resize(width*height);

	for(unsigned int w = 0; w < width; w++){
		for(unsigned int h = 0; h < height;h++){
			int ind = h*width+w;
			double z = idepth*double(depthdata[ind]);
			if(z > 0){
				pcl::PointXYZRGB p;
				p.x = (double(w) - cx) * z * ifx;
				p.y = (double(h) - cy) * z * ify;
				p.z = z;
				p.b = rgbdata[3*ind+0];
				p.g = rgbdata[3*ind+1];
				p.r = rgbdata[3*ind+2];
				cloud->points[ind] = p;
			}
		}
	}

	pcl::IntegralImageNormalEstimation<pcl::PointXYZRGB, pcl::Normal> ne;
	ne.setNormalEstimationMethod (ne.AVERAGE_3D_GRADIENT);
	ne.setMaxDepthChangeFactor(0.02f);
	ne.setNormalSmoothingSize(10.0f);
	ne.setInputCloud(cloud);
	ne.compute(*normals);

	pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloud_ptr (new pcl::PointCloud<pcl::PointXYZRGBNormal>);
	cloud_ptr->width	= width;
	cloud_ptr->height	= height;
	cloud_ptr->points.resize(width*height);

	for(unsigned int w = 0; w < width; w++){
		for(unsigned int h = 0; h < height;h++){
			int ind = h*width+w;
			pcl::PointXYZRGBNormal & p0	= cloud_ptr->points[ind];
			pcl::PointXYZRGB p1			= cloud->points[ind];
			pcl::Normal p2				= normals->points[ind];
			p0.x		= p1.x;
			p0.y		= p1.y;
			p0.z		= p1.z;
			p0.r		= p1.r;
			p0.g		= p1.g;
			p0.b		= p1.b;
			p0.normal_x	= p2.normal_x;
			p0.normal_y	= p2.normal_y;
			p0.normal_z	= p2.normal_z;
		}
	}
	return cloud_ptr;
}

}
