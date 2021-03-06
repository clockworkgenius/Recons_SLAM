#include"../Include/slam_class/Pnp_Solver.h"
#include"../Include/slam_class/config_hc.h"
#include <iostream>


namespace slam_class 
{

void Pnp_Solver::Solve_Pnp (unordered_map<unsigned long, Frame*>::iterator it1, 
						unordered_map<unsigned long, Frame*>::iterator it2
)
{
   Mat img_1 = it1->second->rgbImg;
   Mat img_2 = it2->second->rgbImg;
   
   keypoints.clear();
    matches.clear();
    
    Mat descriptors_2;
    Mat map_descriptors;
    unsigned long * descriptors_id = new unsigned long[Local_Map->map_points.size()];
    int tmp_cnt = 0;
    unordered_map<unsigned long, MapPoint* >::iterator tmp_it;
    
    for( tmp_it = Local_Map->map_points.begin(); tmp_it != Local_Map->map_points.end(); tmp_it++)
    {	
	        Mat tmp_des = tmp_it->second->descriptor;
		map_descriptors.push_back(tmp_des);
		descriptors_id[tmp_cnt++] = tmp_it->second->id;	
    }
    //cout << map_descriptors << endl;
    find_feature_matches ( img_2, map_descriptors ,descriptors_2);
    
    Mat K = ( Mat_<double> ( 3,3 ) << 
		camera_Intrinsics_FX, 0, camera_Intrinsics_CX, 
	        0, camera_Intrinsics_FY, camera_Intrinsics_CY, 
	        0, 0, 1 );
    points_3d.clear();
    points_2d.clear();
    for ( DMatch m:matches )
    {
		tmp_it = Local_Map->map_points.find(descriptors_id[m.queryIdx]);
		tmp_it->second->observed_frames.push_back(it2->second);
	
		MapPoint* p1 = tmp_it->second;
		points_3d.push_back ( Point3d( p1->pos(0,0), p1->pos(1,0), p1->pos(2,0) )  );
		points_2d.push_back ( Point2d(keypoints[m.trainIdx].pt.x , keypoints[m.trainIdx].pt.y));
    }

    //cout<<"3d-2d pairs: "<<points_3d.size() <<endl;

    Mat r, t, inliers;
    solvePnPRansac ( points_3d, points_2d, K, Mat(), r, t, false,100, 4.0, 0.99, inliers ); // 调用OpenCV 的 PnP 求解
    //solvePnP( points_3d, points_2d, K, Mat(), r, t);
    
    Mat R;
    cv::Rodrigues ( r, R ); // r为旋转向量形式，用Rodrigues公式转换为矩阵

    //double threshold = Global_Map->key_frames.find(ref_frame_id)
    //if(t.at(0,0) > 
    //优化位姿
    //cout << "debug" << endl;
    //cout << points_3d.size() << endl;
    //cout << points_2d.size() << endl;
    //cout << K << endl;
    //cout << R << endl;
    //cout << t<< endl;
   //bundleAdjustment (  K, R, t );
    cout<<"R="<<endl<<R<<endl;
    cout<<"t="<<endl<<t<<endl;
    //更新局部地图
    
    //寻找没有匹配的特征点
    bool * rest_points = new bool[ keypoints.size()];
    for( int  i= 0; i < keypoints.size(); i++) 
	    rest_points[i] = true;
    for ( DMatch m:matches )
	    rest_points[m.trainIdx] = false;
    
    keypoints_1.clear();
    keypoints_2.clear();
    
    for( int  i= 0; i < keypoints.size(); i++)
    {
	    if(rest_points[i]) 
	    {
		    KeyPoint tmp = keypoints.at(i);
		    keypoints_2.push_back(tmp);
	    }
    }
    //cout << keypoints_2.size() << endl;
    delete rest_points;
    //更新cur frame 的位姿
    SE3 se3_Tcw1 = it1->second->T_c_w;
    
    MatrixXd tmp_m = se3_Tcw1.matrix();
    Mat Tcw1 = (Mat_<double> (4,4) <<
         tmp_m(0,0), tmp_m(0,1), tmp_m(0,2),tmp_m(0,3),
         tmp_m(1,0), tmp_m(1,1), tmp_m(1,2),tmp_m(1,3),
         tmp_m(2,0), tmp_m(2,1), tmp_m(2,2),tmp_m(2,3),
		0,0,0,1
    );
    Mat Tcw2 = (Mat_<double> (4,4) <<
        R.at<double>(0,0), R.at<double>(0,1), R.at<double>(0,2), t.at<double>(0,0),
        R.at<double>(1,0), R.at<double>(1,1), R.at<double>(1,2), t.at<double>(1,0),
        R.at<double>(2,0), R.at<double>(2,1), R.at<double>(2,2), t.at<double>(2,0),
		0,0,0,1
    );
    //Tcw2 = Tcw2 * Tcw1;
    
    Matrix3d rotation;
    rotation << Tcw2.at<double>(0,0), Tcw2.at<double>(0,1),  Tcw2.at<double>(0,2), 
			Tcw2.at<double>(1,0), Tcw2.at<double>(1,1),  Tcw2.at<double>(1,2),
			Tcw2.at<double>(2,0), Tcw2.at<double>(2,1),  Tcw2.at<double>(2,2);
   Vector3d translation;
   translation << Tcw2.at<double>(0,3), Tcw2.at<double>(1,3), Tcw2.at<double>(2,3);
			
    it2->second->T_c_w = SE3(rotation, translation);
    
    
    points.clear();
    descriptors_buf.clear();
    
    triangulate(img_1, img_2, 
		R,t,
		it1->second->T_c_w,
		it2->second->T_c_w
       	);
    //保存地标点
     SE3 se3_Tcw2 = it1->second->T_c_w;
    MatrixXd tmp_t = se3_Tcw2.translation();
    double cx = tmp_t(0,0);
    double cy = tmp_t(1,0);
    double cz = tmp_t(2,0);
    
    for( int i = 0; i < points.size(); i++)
    {
	   
	    if( abs(points.at(i).x) > BADPOINT_THRESHOLD ||
		    abs(points.at(i).y) > BADPOINT_THRESHOLD ||
		    abs(points.at(i).z) >BADPOINT_THRESHOLD)
		    continue;
	   double d = ((points.at(i).x-cx) *(points.at(i).x-cx) 
		     +  (points.at(i).y-cy) *(points.at(i).y-cy) 
		     +  (points.at(i).z-cz) *(points.at(i).z-cz));
	     if  ( d > BADPOINT_THRESHOLD || d < 0.05) continue;
	     
	      MapPoint * tmp = new MapPoint();
	     tmp->id =  Global_Map->map_points.size();
		    
	    tmp->pos(0,0) = points.at(i).x;
	    tmp->pos(1,0) = points.at(i).y;
	    tmp->pos(2,0) = points.at(i).z;
	    tmp->is_good = true;
	    
	    tmp->observed_frames.push_back(it1->second);
	    tmp->observed_frames.push_back(it2->second);
	    
	    tmp->descriptor = descriptors_buf.at(i);
	    tmp->matched_times = 1;
	    tmp->visible_times = 1; 
	    Global_Map->insertMapPoint(tmp);
	    Local_Map->insertMapPoint(tmp);
	    
    }
    //更新局部地图
     Local_Map->insertKeyFrame(it2->second);
}

void Pnp_Solver::triangulate(
	const Mat & img_1,
	const Mat & img_2,
	const Mat & R,
	const Mat & t,
	const SE3 & Tcw1,
	const SE3 & Tcw2
)
{
    
    Ptr<FeatureDetector> detector = ORB::create(1000);
    Ptr<DescriptorExtractor> descriptor = ORB::create(1000);
    Ptr<DescriptorMatcher> matcher  = DescriptorMatcher::create ( "BruteForce-Hamming" );
  
    match.clear();
    matches.clear();
    Mat descriptors_1, descriptors_2;
    
    //检测Orb角点位置
    detector->detect ( img_1,keypoints_1 );
    //计算 BRIEF 描述子
    descriptor->compute ( img_1, keypoints_1, descriptors_1);
    descriptor->compute ( img_2, keypoints_2, descriptors_2);
    
    matcher->match ( descriptors_1, descriptors_2, match );
 //cout << "1" << endl;   
    double min_dist=10000, max_dist=0;

    for ( int i = 0; i <descriptors_1.rows; i++ )
    {
        double dist = match[i].distance;
        if ( dist < min_dist ) min_dist = dist;
        if ( dist > max_dist ) max_dist = dist;
    }

    for ( int i = 0; i < descriptors_1.rows; i++ )
    {
        if ( match[i].distance <= max ( 2*min_dist, 30.0 ) )
        {
            matches.push_back ( match[i] );
        }
    }
    //cout << "2" << endl;  
    MatrixXd tmpT1 = Tcw1.matrix();
    MatrixXd tmpT2 = Tcw2.matrix();
    Mat T1 = (Mat_<double> (3,4) <<
        tmpT1(0,0),tmpT1(0,1),tmpT1(0,2),tmpT1(0,3),
        tmpT1(1,0),tmpT1(1,1),tmpT1(1,2),tmpT1(1,3),
        tmpT1(2,0),tmpT1(2,1),tmpT1(2,2),tmpT1(2,3)
	    
	);
    Mat T2 = (Mat_<double> (3,4) <<
          tmpT2(0,0),tmpT2(0,1),tmpT2(0,2),tmpT2(0,3),
	  tmpT2(1,0),tmpT2(1,1),tmpT2(1,2),tmpT2(1,3),
          tmpT2(2,0),tmpT2(2,1),tmpT2(2,2),tmpT2(2,3)
	);
    
    //config_hc.h
    Mat K = ( Mat_<double> ( 3,3 ) << 
		camera_Intrinsics_FX, 0, camera_Intrinsics_CX, 
	        0, camera_Intrinsics_FY, camera_Intrinsics_CY, 
	        0, 0, 1 );
    
    pts_1.clear();
    pts_2.clear();
    
	int i = 0;
    Mat t_x = ( Mat_<double> ( 3,3 ) <<
                0,                      -t.at<double> ( 2,0 ),     t.at<double> ( 1,0 ),
                t.at<double> ( 2,0 ),      0,                      -t.at<double> ( 0,0 ),
                -t.at<double> ( 1.0 ),     t.at<double> ( 0,0 ),      0 );
    
    for ( DMatch m:matches )
    {
        // 像素坐标2相机坐标
	 Point2d pt1 = pixel2cam ( keypoints_1[ m.queryIdx ].pt, K );
        Mat y1 = ( Mat_<double> ( 3,1 ) << pt1.x, pt1.y, 1 );
        Point2d pt2 = pixel2cam ( keypoints_2[ m.trainIdx ].pt, K );
        Mat y2 = ( Mat_<double> ( 3,1 ) << pt2.x, pt2.y, 1 );
        Mat d = y2.t() * t_x * R * y1;
	if(abs(d.at<double>(0,0)) <0.1)
	{
        pts_1.push_back ( pixel2cam( keypoints_1[m.queryIdx].pt, K) );
        pts_2.push_back ( pixel2cam( keypoints_2[m.trainIdx].pt, K) );
	descriptors_buf.push_back(descriptors_1.row(m.queryIdx).clone());
	}
	else cout << "Point droped" << endl;
    }
    
    Mat pts_4d;
    int N = pts_1.size();
    
    if(N == 0) return;
	
//cout << "debugtag" << endl; 
    cv::triangulatePoints( T1, T2, pts_1, pts_2, pts_4d );
//cout << "debugtag" << endl; 
    // 2非齐次坐标
    for ( int i=0; i<pts_4d.cols; i++ )
    {
        Mat x = pts_4d.col(i);
        x /= x.at<double>(3,0); // 归一化
        Point3d p (
            x.at<double>(0,0), 
            x.at<double>(1,0), 
            x.at<double>(2,0) 
        );
        points.push_back( p );
    }
}
void  Pnp_Solver::find_feature_matches ( 
			    const Mat& img,
			    Mat& map_descriptors,
			    Mat& descriptors_2
       				)
{
    
    Ptr<FeatureDetector> detector = ORB::create(1000);
    Ptr<DescriptorExtractor> descriptor = ORB::create(1000);
    Ptr<DescriptorMatcher> matcher  = DescriptorMatcher::create ( "BruteForce-Hamming" );
    
    //检测角点位置
    detector->detect ( img,keypoints );
    //计算 BRIEF 描述子
    descriptor->compute ( img, keypoints,descriptors_2);

    //对图像中的BRIEF描述子进行匹配，使用 Hamming 距离
    
    match.clear();
    matches.clear();
    // BFMatcher matcher ( NORM_HAMMING );
    matcher->match ( map_descriptors, descriptors_2, match );

    //匹配点对筛选
    double min_dist=10000, max_dist=0;

    for ( int i = 0; i < map_descriptors.rows; i++ )
    {
        double dist = match[i].distance;
        if ( dist < min_dist ) min_dist = dist;
        if ( dist > max_dist ) max_dist = dist;
    }

    for ( int i = 0; i < map_descriptors.rows; i++ )
    {
        if ( match[i].distance <= max ( 2*min_dist, 30.0 ) )
        {
            matches.push_back ( match[i] );
        }
    }
}

Point2d Pnp_Solver::pixel2cam ( const Point2d& p, const Mat& K )
{
    return Point2d
           (
               ( p.x - K.at<double> ( 0,2 ) ) / K.at<double> ( 0,0 ),
               ( p.y - K.at<double> ( 1,2 ) ) / K.at<double> ( 1,1 )
           );
}


 void Pnp_Solver::bundleAdjustment (
    const Mat& K,
    Mat& R, Mat& t )
{
    // 初始化g2o
    /*typedef g2o::BlockSolver< g2o::BlockSolverTraits<6,3> > Block;  // pose 维度为 6, landmark 维度为 3
    Block::LinearSolverType* linearSolver = new g2o::LinearSolverCSparse<Block::PoseMatrixType>(); // 线性方程求解器
    Block* solver_ptr = new Block ( linearSolver );     // 矩阵块求解器
    g2o::OptimizationAlgorithmLevenberg* solver = new g2o::OptimizationAlgorithmLevenberg ( solver_ptr );
    g2o::SparseOptimizer optimizer;
    optimizer.setAlgorithm ( solver );*/
    
    
    typedef g2o::BlockSolver< g2o::BlockSolverTraits<3,1> > Block;
	Block::LinearSolverType* linearSolver = new g2o::LinearSolverDense<Block::PoseMatrixType>();
	Block* solver_ptr = new Block( std::unique_ptr<Block::LinearSolverType>(linearSolver) );
	g2o::OptimizationAlgorithmLevenberg* solver = new g2o::OptimizationAlgorithmLevenberg(std::unique_ptr<Block>(solver_ptr) );
	g2o::SparseOptimizer optimizer;   
	optimizer.setAlgorithm( solver );   
	optimizer.setVerbose( true ); 
	
    // vertex
    g2o::VertexSE3Expmap* pose = new g2o::VertexSE3Expmap(); // camera pose
   Matrix3d R_mat;
    R_mat <<
          R.at<double> ( 0,0 ), R.at<double> ( 0,1 ), R.at<double> ( 0,2 ),
               R.at<double> ( 1,0 ), R.at<double> ( 1,1 ), R.at<double> ( 1,2 ),
               R.at<double> ( 2,0 ), R.at<double> ( 2,1 ), R.at<double> ( 2,2 );
    pose->setId ( 0 );
    pose->setEstimate ( g2o::SE3Quat (
                            R_mat,
                            Vector3d ( t.at<double> ( 0,0 ), t.at<double> ( 1,0 ), t.at<double> ( 2,0 ) )
                        ) );
    optimizer.addVertex ( pose );

    int index = 1;
    for ( const Point3d p:points_3d )   // landmarks
    {
        g2o::VertexSBAPointXYZ* point = new g2o::VertexSBAPointXYZ();
        point->setId ( index++ );
        point->setEstimate (Vector3d ( p.x, p.y, p.z ) );
        point->setMarginalized ( true ); 
        optimizer.addVertex ( point );
    }

    // parameter: camera intrinsics
    g2o::CameraParameters* camera = new g2o::CameraParameters (
        K.at<double> ( 0,0 ), Vector2d ( K.at<double> ( 0,2 ), K.at<double> ( 1,2 ) ), 0
    );
    camera->setId ( 0 );
    optimizer.addParameter ( camera );

    // edges
    index = 1;
    for ( const Point2d p:points_2d )
    {
        g2o::EdgeProjectXYZ2UV* edge = new g2o::EdgeProjectXYZ2UV();
        edge->setId ( index );
        edge->setVertex ( 0, dynamic_cast<g2o::VertexSBAPointXYZ*> ( optimizer.vertex ( index ) ) );
        edge->setVertex ( 1, pose );
        edge->setMeasurement ( Vector2d ( p.x, p.y ) );
        edge->setParameterId ( 0,0 );
	
        edge->setInformation ( Matrix2d::Identity() );
        optimizer.addEdge ( edge );
        index++;
    }

    //chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
    optimizer.setVerbose ( true );
    optimizer.initializeOptimization();
    optimizer.optimize ( 100 );
    //chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
    //chrono::duration<double> time_used = chrono::duration_cast<chrono::duration<double>> ( t2-t1 );
   // cout<<"optimization costs time: "<<time_used.count() <<" seconds."<<endl;

    //cout<<endl<<"after optimization:"<<endl;
    //cout<<"T="<<endl<<Isometry3d ( pose->estimate() ).matrix() <<endl;
}
		
	
void Pnp_Solver::update_local_map()
{
	bool flag;
	delete_list.clear();
	
	//找到需要删除的点
	unordered_map<unsigned long, MapPoint* >::iterator tmp_it;
    
	for( tmp_it = Local_Map->map_points.begin(); tmp_it != Local_Map->map_points.end(); tmp_it++)
	//for( int i = 0; i < Local_Map->map_points.size(); i++)
	{
		flag = false;
		list< Frame*>::iterator iter;
		//for( iter = Local_Map->map_points.at(i)->observed_frames.begin(); iter != Local_Map->map_points.at(i)->observed_frames.end(); iter++) 
		//for(int j = 0; j < Local_Map->map_points.at(i)->observed_frames.size();j++)
		for( iter = tmp_it->second->observed_frames.begin(); iter != tmp_it->second->observed_frames.end(); iter++) 
		{
			unsigned long tmpid =  (*iter)->id;
			if(
				((tmpid >= (cur_frame_id - LOCALMAP_FRAMES))|| (cur_frame_id <= LOCALMAP_FRAMES)) 
				&& (tmpid <= cur_frame_id)
			)
			{
				flag = true;
				break;
			}
		}
		if(!flag)
		{
			delete_list.push_back(tmp_it->second->id);
		}
	}
	unordered_map<unsigned long, MapPoint*>::iterator it;
	for( int i = 0; i < delete_list.size(); i++)
	{
		unsigned long id = delete_list.at(i);
		it = Local_Map->map_points.find(id);
		Local_Map->map_points.erase(it);
	}
	cout << "局部地图点数量：" << Local_Map->map_points.size() << endl;
}

void Pnp_Solver::run()
{
	int i = 2;
	cout << "开始构建地图..." << endl;
	ref_frame_id = 0;
	for(i; i < Global_Map->key_frames.size(); i++)
	//for(i; i < 10; i++)
	{
		cout << "重建点数量：" << Global_Map->map_points.size() << endl;
		ref_frame_id = i-1;
		cur_frame_id = i;
		update_local_map();
		unordered_map<unsigned long, Frame*>::iterator it1, it2;
		//it1 = Global_Map->key_frames.find(cur_frame_id-1);
		it1 = Global_Map->key_frames.find(ref_frame_id);
		it2 = Global_Map->key_frames.find(cur_frame_id);
		Solve_Pnp(it1, it2);
		cout << "第" << it2->second->id << "帧处理完毕 " << endl;
		//Frame* cur_frame = (Global_Map->key_frames.find(cur_frame_id))->second;
	}
	return;
};
	
void Pnp_Solver::pose_estimation_2d2d ( std::vector<KeyPoint> keypoints_1,
                            std::vector<KeyPoint> keypoints_2,
                            std::vector< DMatch > matches,
                            Mat& R, Mat& t )
{
    // 相机内参，写在congif_hc.h
    Mat K = ( Mat_<double> ( 3,3 ) << 
		camera_Intrinsics_FX, 0, camera_Intrinsics_CX, 
	        0, camera_Intrinsics_FY, camera_Intrinsics_CY, 
	        0, 0, 1 );
    
    //匹配点转换为vector<Point2f>的形式
    vector<Point2f> points1;
    vector<Point2f> points2;

    for ( int i = 0; i < ( int ) matches.size(); i++ )
    {
        points1.push_back ( keypoints_1[matches[i].queryIdx].pt );
        points2.push_back ( keypoints_2[matches[i].trainIdx].pt );
    }

    //基础矩阵
   // Mat fundamental_matrix;
    //fundamental_matrix = findFundamentalMat ( points1, points2, CV_FM_8POINT );
    //cout<<"fundamental_matrix is "<<endl<< fundamental_matrix<<endl;

    //本质矩阵
    Point2d principal_point ( K.at<double>(0,2), K.at<double>(1,2) );	//相机光心
    double focal_length = K.at<double>(1,1);	//相机焦距
    
    Mat essential_matrix;
    essential_matrix = findEssentialMat ( points1, points2, focal_length, principal_point );
    cout<<"essential_matrix is "<<endl<< essential_matrix<<endl;

    //单应矩阵
    /*
    Mat homography_matrix;
    homography_matrix = findHomography ( points1, points2, RANSAC, 3 );
    cout<<"homography_matrix is "<<endl<<homography_matrix<<endl;
    */
    
    //本质矩阵 恢复旋转和平移
    recoverPose ( essential_matrix, points1, points2, R, t, focal_length, principal_point );
   // cout<<"R ： "<<endl<<R<<endl;
   // cout<<"t ： "<<endl<<t<<endl;
    
}	
	
}