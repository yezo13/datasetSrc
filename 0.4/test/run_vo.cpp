// -------------- test the visual odometry -------------
#include <fstream>
#include <boost/timer.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/viz.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "myslam/config.h"
#include "myslam/visual_odometry.h"

int main ( int argc, char** argv )
{
    if ( argc != 2 )
    {
        cout<<"usage: run_vo parameter_file"<<endl;
        return 1;
    }

    myslam::Config::setParameterFile ( argv[1] );
    myslam::VisualOdometry::Ptr vo ( new myslam::VisualOdometry );

    string dataset_dir = myslam::Config::get<string> ( "dataset_dir" );
    cout<<"dataset: "<<dataset_dir<<endl;
    ifstream fin ( dataset_dir+"/associate.txt" );
    if ( !fin )
    {
        cout<<"please generate the associate file called associate.txt!"<<endl;
        return 1;
    }

    vector<string> rgb_files, depth_files;
    vector<double> rgb_times, depth_times;
    while ( !fin.eof() )
    {
        string rgb_time, rgb_file, depth_time, depth_file;
        fin>>rgb_time>>rgb_file>>depth_time>>depth_file;
        rgb_times.push_back ( atof ( rgb_time.c_str() ) );
        depth_times.push_back ( atof ( depth_time.c_str() ) );
        rgb_files.push_back ( dataset_dir+"/"+rgb_file );
        depth_files.push_back ( dataset_dir+"/"+depth_file );

        if ( fin.good() == false )
            break;
    }

    myslam::Camera::Ptr camera ( new myslam::Camera );

    // visualization
    cv::viz::Viz3d vis ( "Visual Odometry" );
    cv::viz::WCoordinateSystem world_coor ( 1.0 ), camera_coor ( 0.5 );
    cv::Point3d cam_pos ( 0, -1.0, -1.0 ), cam_focal_point ( 0,0,0 ), cam_y_dir ( 0,1,0 );
    cv::Affine3d cam_pose = cv::viz::makeCameraPose ( cam_pos, cam_focal_point, cam_y_dir );
    vis.setViewerPose ( cam_pose );

    world_coor.setRenderingProperty ( cv::viz::LINE_WIDTH, 2.0 );
    camera_coor.setRenderingProperty ( cv::viz::LINE_WIDTH, 1.0 );
    vis.showWidget ( "World", world_coor );
    vis.showWidget ( "Camera", camera_coor );

    cout<<"read total "<<rgb_files.size() <<" entries"<<endl;
    
    double *frame_time = new double [rgb_files.size()];  //each time of img
    
    vector<Eigen::Vector3d> euler_angles; //eular-angle of each img
    
   for ( int i=0; i<rgb_files.size(); i++ )
   //for(int i = 0; i < 20;i ++ )
    {
        cout<<"****** loop "<<i<<" ******"<<endl;
        Mat color = cv::imread ( rgb_files[i] );
        Mat depth = cv::imread ( depth_files[i], -1 );
        if ( color.data==nullptr || depth.data==nullptr )
            break;
        myslam::Frame::Ptr pFrame = myslam::Frame::createFrame();
        pFrame->camera_ = camera;
        pFrame->color_ = color;
        pFrame->depth_ = depth;
        pFrame->time_stamp_ = rgb_times[i];

        boost::timer timer;
        vo->addFrame ( pFrame );
        cout<<"VO costs time: "<<timer.elapsed() <<endl;
	//cout<<"this frame's time_stamp is " << pFrame->time_stamp_ << endl;
	frame_time[i] = rgb_times[i] - rgb_times[0];  //count the time
	

        if ( vo->state_ == myslam::VisualOdometry::LOST )
            break;
        SE3 Twc = pFrame->T_c_w_.inverse();

        // show the map and the camera pose
        cv::Affine3d M (
            cv::Affine3d::Mat3 (
                Twc.rotation_matrix() ( 0,0 ), Twc.rotation_matrix() ( 0,1 ), Twc.rotation_matrix() ( 0,2 ),
                Twc.rotation_matrix() ( 1,0 ), Twc.rotation_matrix() ( 1,1 ), Twc.rotation_matrix() ( 1,2 ),
                Twc.rotation_matrix() ( 2,0 ), Twc.rotation_matrix() ( 2,1 ), Twc.rotation_matrix() ( 2,2 )
            ),
            cv::Affine3d::Vec3 (
                Twc.translation() ( 0,0 ), Twc.translation() ( 1,0 ), Twc.translation() ( 2,0 )
            )
        );
	euler_angles.push_back(Twc.rotation_matrix().eulerAngles(2,1,0));; //calculate the euler_angle
	

        Mat img_show = color.clone();
        for ( auto& pt:vo->map_->map_points_ )
        {
            myslam::MapPoint::Ptr p = pt.second;
            Vector2d pixel = pFrame->camera_->world2pixel ( p->pos_, pFrame->T_c_w_ );
            cv::circle ( img_show, cv::Point2f ( pixel ( 0,0 ),pixel ( 1,0 ) ), 5, cv::Scalar ( 0,255,0 ), 2 );
        }

        cv::imshow ( "image", img_show );
        cv::waitKey ( 1 );
        vis.setWidgetPose ( "Camera", M );
        vis.spinOnce ( 1, false );
        cout<<endl;
    }
    
    cout << "----------------------" << endl ;
    
    vector<Eigen::Vector3d>::iterator it;
    int count = 0;
    /*
    for(it = euler_angles.begin();it != euler_angles.end();it++){
      cout << "The time is " << frame_time[count++] << "and the angle is" << (*it).transpose() << endl;
    }
    */
    
    ofstream out;
    out.open("result.csv",ios::out);
    out << "time" << ',' << "z" << ',' << "y" << ',' << "x" << endl;
    it = euler_angles.begin();
    for (int q = 0; q < rgb_files.size();q++){
	Eigen::Vector3d last_angle = (*it).transpose();
	it++;
	Eigen::Vector3d curr_angle = (*it).transpose();
	Eigen::Vector3d div_angle = curr_angle - last_angle;
	double div_time = frame_time[q] - frame_time[q-1];
	//cout << "and the angle speed is " << div_angle / div_time << endl;
	//judge if the speed is reliable or not.
	if(((div_angle(0)/div_time < 2) && (div_angle(1)/div_time < 2) && (div_angle(2)/div_time < 2))
	  && ((div_angle(0)/div_time > -2) && (div_angle(1)/div_time > -2) && (div_angle(2)/div_time > -2))
	)
	  out<< frame_time[q] << ',' << div_angle(0)/div_time << ',' << div_angle(1)/div_time << ','<< div_angle(2)/div_time << endl;

    }
    
    

    return 0;
}
