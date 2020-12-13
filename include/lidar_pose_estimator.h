#ifndef _LIDAR_POSE_ESTIMATOR_H
#define _LIDAR_POSE_ESTIMATOR_H

#include "lidar_preprocessor.h"
#include "lidar_pose_graph.h"
#include <Eigen/Eigen>
#include "math_utils.h"


class lidar_pose_estimator
{
private:
    /* data */
public:
    lidar_preprocessor lidar;
    lidar_preprocessor lidar_prev;

    //feature map in current frame
    pcl::PointCloud<PointType> edge_point_map;
    pcl::PointCloud<PointType> planar_point_map;

    //transform  from current frame to init frame
    Eigen::Quaterniond q;
    Eigen::Vector3d t;
    double timestamp;

    //transform from lidar_prov to lidar
    Eigen::Quaterniond dq;
    Eigen::Vector3d dt;

    lidar_pose_estimator(/* args */);
    ~lidar_pose_estimator();

    void update(const sensor_msgs::PointCloud2ConstPtr &msg);
    void update_feature_map();
    void transform_update();
    void transform_accumulate();
};

lidar_pose_estimator::lidar_pose_estimator(/* args */)
{
    q = Eigen::Quaterniond::Identity();
    t = Eigen::Vector3d::Zero();
    dq = Eigen::Quaterniond::Identity();
    dt = Eigen::Vector3d::Zero();
}

lidar_pose_estimator::~lidar_pose_estimator()
{
}

void lidar_pose_estimator::transform_update()
{
    std::cout << "edge point size prev: " << lidar_prev.edge_points.points.size() << std::endl;
    std::cout << "edge point size: " << lidar.edge_points.points.size() << std::endl;

    //ceres optimization
    double pose[6] = {0, 0, 0, 0, 0, 0}; //0-2 for roation and 3-5 for tranlation
    Problem problem;

    //init paramameter
    double q0[4];
    q0[0] = dq.w();
    q0[1] = dq.x();
    q0[2] = dq.y();
    q0[3] = dq.z();
    ceres::QuaternionToAngleAxis(q0, pose);
    pose[3] = dt(0);
    pose[4] = dt(1);
    pose[5] = dt[2];


    pcl::KdTreeFLANN<PointType> kdtree;
    kdtree.setInputCloud(lidar.edge_points.makeShared());
    int K = 2; // K nearest neighbor search
    std::vector<int> index(K);
    std::vector<float> distance(K);

    Eigen::Matrix3d R = dq.toRotationMatrix();
    //add constraint for edge points
    for (int i = 0; i < lidar_prev.edge_points.points.size(); i++)
    {
        PointType search_point = lidar_prev.edge_points.points[i];
        //project search_point to current frame
        PointType search_point_predict = eigen2point(R * point2eigen(search_point) + dt);
        if (kdtree.nearestKSearch(search_point_predict, K, index, distance) == K)
        {
            //add constraints
            Eigen::Vector3d p = point2eigen(search_point);
            Eigen::Vector3d p1 = point2eigen(lidar.edge_points.points[index[0]]);
            Eigen::Vector3d p2 = point2eigen(lidar.edge_points.points[index[1]]);
            ceres::CostFunction *cost_function = lidar_edge_error::Create(p, p1, p2);
            problem.AddResidualBlock(cost_function,
                                     new CauchyLoss(0.5),
                                     pose);
        }
    }

    //add constraints for planar points
    K = 3;
    kdtree.setInputCloud(lidar.planar_points.makeShared());
    index.resize(K);
    distance.resize(K);
    for (int i = 0; i < lidar_prev.planar_points.points.size(); i++)
    {
        PointType search_point = lidar_prev.planar_points.points[i];
        PointType search_point_predict = eigen2point(R * point2eigen(search_point) + dt);
        if (kdtree.nearestKSearch(search_point_predict, K, index, distance) == K)
        {
            //add constraints
            Eigen::Vector3d p = point2eigen(search_point);
            Eigen::Vector3d p1 = point2eigen(lidar.planar_points.points[index[0]]);
            Eigen::Vector3d p2 = point2eigen(lidar.planar_points.points[index[1]]);
            Eigen::Vector3d p3 = point2eigen(lidar.planar_points.points[index[2]]);
            ceres::CostFunction *cost_function = lidar_planar_error::Create(p, p1, p2, p3);
            problem.AddResidualBlock(cost_function,
                                     new CauchyLoss(0.5),
                                     pose);
        }
    }

    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_SCHUR;
    options.minimizer_progress_to_stdout = false;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    //std::cout << summary.FullReport() << "\n";

    //printf("result: %lf, %lf, %lf, %lf, %lf, %lf\n", pose[0], pose[1], pose[2], pose[3], pose[4], pose[5]);

    double qq[4];
    ceres::AngleAxisToQuaternion(pose, qq);
    dt = Eigen::Vector3d(pose[3], pose[4], pose[5]);

    dq.w() = qq[0];
    dq.x() = qq[1];
    dq.y() = qq[2];
    dq.z() = qq[3];
}

void lidar_pose_estimator::transform_accumulate()
{
    //update transformation
    Eigen::Quaterniond dq_inv = dq.inverse();
    Eigen::Vector3d dt_inv = -dq_inv.toRotationMatrix() * dt;

    t += q.toRotationMatrix() * dt_inv;
    q *= dq_inv;
}

void lidar_pose_estimator::update_feature_map()
{
    //project features to current frame
}

void lidar_pose_estimator::update(const sensor_msgs::PointCloud2ConstPtr &msg)
{
    clock_t start = clock();

    timestamp = msg->header.stamp.toSec();
    if (lidar.lidar_cloud.points.size())
    {
        //copy current lidar data for prev 
        lidar_prev.lidar_cloud = lidar.lidar_cloud;
        lidar_prev.edge_points = lidar.edge_points;
        lidar_prev.planar_points = lidar.planar_points;
    }
    lidar.process(msg);

    if (lidar_prev.lidar_cloud.points.size() && lidar.lidar_cloud.points.size())
    {
        transform_update();
        transform_update();
        transform_accumulate();
    } 

    double dt = ((double)clock() - start) / CLOCKS_PER_SEC;
    printf("lidar update cost %lfs", dt);
}

#endif
 