
#include "../Include/slam_class/Camera.h"

namespace slam_class
{
    

 void Camera::setIntrinsics(double _fx, double _fy, double _cx, double _cy)
 {
	intrinsics.setValue ( _fx,  _fy, _cx,  _cy);
	
 }
 
Vector3d Camera::world2camera ( const Vector3d& p_w, const SE3& T_c_w )
{
    return T_c_w*p_w;
}

Vector3d Camera::camera2world ( const Vector3d& p_c, const SE3& T_c_w )
{
    return T_c_w.inverse() *p_c;
}

Vector2d Camera::camera2pixel ( const Vector3d& p_c )
{
    return Vector2d(
               intrinsics.fx * p_c ( 0,0 ) / p_c ( 2,0 ) + intrinsics.cx,
               intrinsics.fy * p_c ( 1,0 ) / p_c ( 2,0 ) + intrinsics.cy
           );
}

Vector3d Camera::pixel2camera ( const Vector2d& p_p, double depth )
{
    return Vector3d (
               ( p_p ( 0,0 )-intrinsics.cx ) *depth/intrinsics.fx,
               ( p_p ( 1,0 )-intrinsics.cy ) *depth/intrinsics.fy,
               depth
           );
}

Vector2d Camera::world2pixel ( const Vector3d& p_w, const SE3& T_c_w )
{
    return camera2pixel ( world2camera(p_w, T_c_w) );
}

Vector3d Camera::pixel2world ( const Vector2d& p_p, const SE3& T_c_w, double depth )
{
    return camera2world ( pixel2camera ( p_p, depth ), T_c_w );
}


}