#include "lidar_pose_graph.h"
#include <Eigen/Eigen>
#include <iostream>

using namespace std;
using namespace Eigen;



int edge_pose_graph()
{
    const int n = 100;
    MatrixXd P = MatrixXd::Random(3, n) * 100;
    MatrixXd P_proj = MatrixXd::Zero(3, n);

    Quaterniond q = Quaterniond::UnitRandom();
    if (q.w() < 0) q.coeffs() *= -1;
    Vector3d t = Vector3d::Random();

    Matrix3d R = q.toRotationMatrix();
    for (int i = 0; i < n; i++)
    {
        P_proj.col(i) = R * P.col(i) + t;
    }
    AngleAxisd angle_axis = AngleAxisd(q);

    std::cout << "groundtruth: " << std::endl
              << "angle_axis: " << angle_axis.angle() * angle_axis.axis().transpose() << std::endl
              << "trans: " << t.transpose() << endl;
    double pose[6] = {0, 0, 0, 0, 0, 0};//0-2 for roation and 3-5 for tranlation
    Problem problem;
    for (int i = 0; i < n; i++)
    {
        Vector3d p = P.col(i);
        Vector3d p1 = P_proj.col(i);
        Vector3d p2 = P_proj.col( (i + 1) % n);
        ceres::CostFunction *cost_function = lidar_edge_error::Create(p, p1, p2);
        problem.AddResidualBlock(cost_function,
                                 NULL,//new CauchyLoss(0.5),
                                 pose);
    }
    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_SCHUR;
    options.minimizer_progress_to_stdout = true;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    //std::cout << summary.FullReport() << "\n";

    //printf("result: %lf, %lf, %lf, %lf, %lf, %lf\n", pose[0], pose[1], pose[2], pose[3], pose[4], pose[5]);

    return 0;
}

int planar_pose_graph()
{
    const int n = 100;
    MatrixXd P = MatrixXd::Random(3, n) * 100;
    MatrixXd P_proj = MatrixXd::Zero(3, n);

    Quaterniond q = Quaterniond::UnitRandom();
    if (q.w() < 0)
        q.coeffs() *= -1;
    Vector3d t = Vector3d::Random();

    Matrix3d R = q.toRotationMatrix();
    for (int i = 0; i < n; i++)
    {
        P_proj.col(i) = R * P.col(i) + t;
    }
    AngleAxisd angle_axis = AngleAxisd(q);

    std::cout << "groundtruth: " << std::endl
              << "angle_axis: " << angle_axis.angle() * angle_axis.axis().transpose() << std::endl
              << "trans: " << t.transpose() << endl;
    double pose[6] = {0, 0, 0, 0, 0, 0}; //0-2 for roation and 3-5 for tranlation
    Problem problem;
    for (int i = 0; i < n; i++)
    {
        Vector3d p = P.col(i);
        Vector3d p1 = P_proj.col(i);
        Vector3d p2 = P_proj.col((i + 1) % n);
        Vector3d p3 = P_proj.col((i + 2) % n);

        ceres::CostFunction *cost_function = lidar_planar_error::Create(p, p1, p2, p3);
        problem.AddResidualBlock(cost_function,
                                 NULL,//new CauchyLoss(0.5),
                                 pose);
    }
    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_SCHUR;
    options.minimizer_progress_to_stdout = true;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    std::cout << summary.FullReport() << "\n";

    printf("result: %lf, %lf, %lf, %lf, %lf, %lf\n", pose[0], pose[1], pose[2], pose[3], pose[4], pose[5]);

    return 0;
}

int main(int argc, char **argv)
{
    //edge_pose_graph();
    planar_pose_graph();
    return 0;
}