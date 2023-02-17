//
// Created by dell on 22-7-26.
//
#ifndef SRC_VOXEL_UTIL_H
#define SRC_VOXEL_UTIL_H
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/StdVector>
#include <exception>
#include <pcl/common/io.h>
#include <unordered_map>
#include <string>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include "sensor_data/point_data.h"
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

#define HASH_P 116101
#define MAX_N 10000000000

// TODO: add voxel definate
typedef struct point_ {
    Eigen::Vector3d point;
    double time;
} point_;

class VOXEL_LOC{
public:
    int64_t x, y, z;
    VOXEL_LOC(int64_t vx = 0, int64_t vy = 0, int64_t vz = 0): x(vx), y(vy), z(vz) {}
    bool operator==(const VOXEL_LOC &other) const{
        return (x == other.x && y == other.y && z == other.z);
    }
};

// Hash value
namespace std{
    template <>
    struct hash<VOXEL_LOC>{
        int64_t operator()(const VOXEL_LOC &s) const{
            using std::hash;
            using std::size_t;
            return ((((s.z) * HASH_P) % MAX_N + (s.y)) * HASH_P) % MAX_N + (s.x);
        }
    };
} //using namespace std;

class VoxelShape{
public:
    std::vector<point_> temp_points_; // all points in voxel
    std::vector<point_> new_points_; // new points
    Eigen::Vector3d mean_ = Eigen::Vector3d::Zero();
    Eigen::Matrix3d var_ = Eigen::Matrix3d::Zero();
    Eigen::Vector3d u_ = Eigen::Vector3d::Zero();
    Eigen::Vector3d normal_ = Eigen::Vector3d::Zero();
    Eigen::Vector3d v1_ = Eigen::Vector3d::Zero();
    Eigen::Vector3d v2_ = Eigen::Vector3d::Zero();
    Eigen::Vector3d v3_ = Eigen::Vector3d::Zero();
    Eigen::Vector3d eigen_val_ = Eigen::Vector3d::Zero();
    Eigen::Matrix<double, 6, 1> feat_ = Eigen::VectorXd::Zero(6, 1);
    Eigen::Matrix<double, 7, 1> feat_2 = Eigen::VectorXd::Zero(7, 1);
    
    double time_min_ = DBL_MAX;
    double time_mean_ = 0;
    double time_max_ = DBL_MIN;
    bool is_plane;
    float p_;
    float c_;
    float avg_timestamp;
    float init_flag;
    float voxel_size_;
    int layer_idx;
    int all_points_cnt_;
    int new_points_cnt_;
    float alpha_;
    // VoxelShape(const VoxelShape &voxel) {
    //     std::cout << "copy conduction is jsdhf\n";
    // }
    // VoxelShape(const VoxelShape &voxel){
    // time_min_ = voxel.time_min_;
    // time_mean_ = voxel.time_mean_;
    // time_max_ = voxel.time_max_;
    // is_plane = voxel.is_plane;
    // normal_ = voxel.normal_;
    // p_ = voxel.p_;
    // c_ = voxel.c_;
    // init_flag = voxel.init_flag;
    // voxel_size_ = voxel.voxel_size_;
    // layer_idx = voxel.layer_idx;
    // all_points_cnt_ = voxel.all_points_cnt_;
    // new_points_cnt_ = voxel.new_points_cnt_;
    // eigen_val_ = voxel.eigen_val_;
    // alpha_ = voxel.alpha_;
    // v3_ = voxel.v3_;
    // v2_ = voxel.v2_;
    // v1_ = voxel.v1_;
    // var_ = voxel.var_;
    // mean_ = voxel.mean_;
    // if(!voxel.temp_points_.empty()){
    //     for(auto &p:voxel.temp_points_){
    //         point_ p_new;
    //         p_new.point = p.point;
    //         p_new.time = p.time;
    //         temp_points_.emplace_back(p_new);
    //     }
    // }
    // if(!voxel.new_points_.empty()){
    //     for(auto &p:voxel.new_points_){
    //         point_ p_new;
    //         p_new.point = p.point;
    //         p_new.time = p.time;
    //         new_points_.emplace_back(p_new);
    //     }
    // }

    // }
    VoxelShape(float voxel_size, float alpha)
        :voxel_size_(voxel_size), alpha_(alpha){
        temp_points_.clear();
        all_points_cnt_ = 0;
        new_points_cnt_ = 0;
        is_plane = false;
        init_flag = false;
        avg_timestamp = 0;
        p_ = 0;
        c_ = 0;
    };

    void init_voxel_shape_pts(){
        int n = temp_points_.size();
        if (n < 5){ // if pts < min_num , skip this
            return ;
        }
        for(int i = 0;i < n;i++){
            mean_ += temp_points_[i].point;
            var_ += (temp_points_[i].point * temp_points_[i].point.transpose());
        }
        mean_ /= n;
        u_ = mean_;
        var_ = var_ / n - mean_ * mean_.transpose();
        Eigen::EigenSolver<Eigen::Matrix3d> es(var_);
        Eigen::Matrix3cd  evecs = es.eigenvectors();
        Eigen::Vector3d eigen_val = es.eigenvalues().real();
        Eigen::Matrix3f::Index evalsMin, evalsMax;
        eigen_val.rowwise().sum().minCoeff(&evalsMin);
        eigen_val.rowwise().sum().maxCoeff(&evalsMax);
        int evalsMid = 3 - evalsMin - evalsMax;
        double lam1 = eigen_val(evalsMin);
        double lam2 = eigen_val(evalsMid);
        double lam3 = eigen_val(evalsMax);
        eigen_val_(0) = lam1;
        eigen_val_(1) = lam2;
        eigen_val_(2) = lam3;
        c_ = (lam3 - lam2) / (lam1 + lam2 + lam3);
        p_ = 2 * (lam2 - lam1) / (lam1 + lam2 + lam3);
        v1_ = evecs.real().col(evalsMin);
        v2_ = evecs.real().col(evalsMid);
        v3_ = evecs.real().col(evalsMax);
        normal_ = v1_;
        Eigen::Vector3d vec_op = mean_ - Eigen::Vector3d::Zero();
        // if(vec_op.dot(normal_) > 0){
        //     normal_ *= -1.0;
        // }        
        feat_.block<3, 1>(0, 0) = alpha_ * u_;
        feat_.block<3, 1>(3, 0) = p_ * normal_;
        feat_2.block<3, 1>(0, 0) = u_;
        feat_2.block<3, 1>(3, 0) = normal_;
        feat_2(6, 0) = alpha_;
        if(p_ > c_ && p_ > 0.5) is_plane = true;
        temp_points_.clear();
        all_points_cnt_ = n;
        init_flag = true;
    };
    void init_voxel_shape_vxl(){
        int n = temp_points_.size() + all_points_cnt_;
        if(n < 5){
            return ;
        }
        for(int i = 0;i < temp_points_.size();i++){
            mean_ += temp_points_[i].point;
            var_ += (temp_points_[i].point * temp_points_[i].point.transpose());
        }
        mean_ /= n;
        u_ = mean_;
        var_ = var_ / n - mean_ * mean_.transpose();
        Eigen::EigenSolver<Eigen::Matrix3d> es(var_);
        Eigen::Matrix3cd  evecs = es.eigenvectors();
        Eigen::Vector3d eigen_val = es.eigenvalues().real();
        Eigen::Matrix3f::Index evalsMin, evalsMax;
        eigen_val.rowwise().sum().minCoeff(&evalsMin);
        eigen_val.rowwise().sum().maxCoeff(&evalsMax);
        int evalsMid = 3 - evalsMin - evalsMax;
        double lam1 = eigen_val(evalsMin);
        double lam2 = eigen_val(evalsMid);
        double lam3 = eigen_val(evalsMax);
        eigen_val_(0) = lam1;
        eigen_val_(1) = lam2;
        eigen_val_(2) = lam3;
        c_ = (lam3 - lam2) / (lam1 + lam2 + lam3);
        p_ = 2 * (lam2 - lam1) / (lam1 + lam2 + lam3);
        v1_ = evecs.real().col(evalsMin);
        v2_ = evecs.real().col(evalsMid);
        v3_ = evecs.real().col(evalsMax);
        normal_ = v1_;
        Eigen::Vector3d vec_op = mean_ - Eigen::Vector3d::Zero();
        // if(vec_op.dot(normal_) > 0){
        //     normal_ *= -1.0;
        // }        
        feat_.block<3, 1>(0, 0) = alpha_ * u_;
        feat_.block<3, 1>(3, 0) = p_ * normal_;
        feat_2.block<3, 1>(0, 0) = u_;
        feat_2.block<3, 1>(3, 0) = normal_;
        feat_2(6, 0) = alpha_;
        if(p_ > c_ && p_ > 0.5) is_plane = true;
        temp_points_.clear();
        all_points_cnt_ = n;
        init_flag = true;
    }
        void update_feat(){
        feat_.block<3, 1>(0, 0) = alpha_ * u_;
        feat_.block<3, 1>(3, 0) = p_ * normal_;
        feat_2.block<3, 1>(0, 0) = u_;
        feat_2.block<3, 1>(3, 0) = normal_;
        feat_2(6, 0) = alpha_;
    }
    void update_voxel(const VoxelShape &voxel){
        //TODO: need find bug
        // is_plane = false;
        Eigen::Vector3d sum = Eigen::Vector3d::Zero();
        Eigen::Matrix3d var_sum = Eigen::Matrix3d::Zero();
        sum = mean_ * all_points_cnt_;
        var_sum = ((var_ + mean_ * mean_.transpose()) * all_points_cnt_);
        sum += (voxel.mean_ * voxel.all_points_cnt_);
        var_sum += ((voxel.var_ + voxel.mean_ * voxel.mean_.transpose()) * voxel.all_points_cnt_);
        all_points_cnt_ += voxel.all_points_cnt_;
        mean_ = sum / all_points_cnt_;
        var_ = var_sum / all_points_cnt_ - mean_ * mean_.transpose();
        Eigen::EigenSolver<Eigen::Matrix3d> es(var_);
        Eigen::Matrix3cd  evecs = es.eigenvectors();
        Eigen::Vector3d eigen_val = es.eigenvalues().real();
        Eigen::Matrix3f::Index evalsMin, evalsMax;
        eigen_val.rowwise().sum().minCoeff(&evalsMin);
        eigen_val.rowwise().sum().maxCoeff(&evalsMax);
        int evalsMid = 3 - evalsMin - evalsMax;
        double lam1 = eigen_val(evalsMin);
        double lam2 = eigen_val(evalsMid);
        double lam3 = eigen_val(evalsMax);
        eigen_val_(0) = lam1;
        eigen_val_(1) = lam2;
        eigen_val_(2) = lam3;
        c_ = (lam3 - lam2) / (lam1 + lam2 + lam3);
        p_ = 2 * (lam2 - lam1) / (lam1 + lam2 + lam3);
        v1_ = evecs.real().col(evalsMin);
        v2_ = evecs.real().col(evalsMid);
        v3_ = evecs.real().col(evalsMax);
        normal_ = v1_;
        Eigen::Vector3d vec_op = mean_ - Eigen::Vector3d::Zero();
        // if(vec_op.dot(normal_) > 0){
        //     normal_ *= -1.0;
        // }
        if(p_ > c_ && p_ > 0.6) is_plane = true;
        temp_points_.clear();
        init_flag = true;
    }




    void update_voxel_shape(){

    };

};

void buildVoxelMap(const std::vector<point_> &input_points,
                   const float voxel_size, const float alpha,
                   std::unordered_map<VOXEL_LOC, VoxelShape *> &shape_map){
    uint pcdsz = input_points.size();
    for(uint i = 0;i < pcdsz; i++){
        //compute voxel index
        const point_ p_voxel = input_points[i];
        float loc_xyz[3];
        //TODO: offset can add in there
        for (int j = 0; j < 3; j++){
            loc_xyz[j] = p_voxel.point[j] / voxel_size;
            if (loc_xyz[j] < 0){
                loc_xyz[j] -= 1.0;
            }
        }
        VOXEL_LOC position((int64_t)loc_xyz[0], (int64_t)loc_xyz[1],
                           (int64_t)loc_xyz[2]);
        auto iter = shape_map.find(position);
        if(iter != shape_map.end()){ // this voxel exists
            iter->second->temp_points_.emplace_back(p_voxel);
        }else{ // this voxel no exists, need new voxel
            VoxelShape *voxelshape = new VoxelShape(voxel_size, alpha);
            shape_map[position] = voxelshape;
            shape_map[position]->temp_points_.emplace_back(p_voxel);
        }
    }
    for(auto iter=shape_map.begin(); iter != shape_map.end(); iter++){
        iter->second->init_voxel_shape_pts();
    }
}


void buildVoxelMap(const std::unordered_map<VOXEL_LOC, VoxelShape*> &shape_map_in,
                   const float voxel_downtimes, const float alpha,
                   std::unordered_map<VOXEL_LOC, VoxelShape*> &shape_map_out){
    for (auto iter_in=shape_map_in.begin(); iter_in != shape_map_in.end();iter_in++) {
        float loc_in[3] = {float(iter_in->first.x), float(iter_in->first.y), float (iter_in->first.z)}; // input voxel index
        float loc_out[3]; // output voxel index
        for(int j = 0; j < 3; j++){
            loc_out[j] = loc_in[j] / voxel_downtimes;
            if(loc_out[j] < 0){
                loc_out[j] -= 1.0;
            }
        }
        VOXEL_LOC pos_out((int64_t)loc_out[0], (int64_t)loc_out[1], (int64_t)loc_out[2]);
        auto iter_out = shape_map_out.find(pos_out); // find this voxel
        if(iter_out != shape_map_out.end()){ // voxel exists
            if(iter_in->second->init_flag){  // input voxel finish init
                iter_out->second->mean_ += (iter_in->second->mean_ * iter_in->second->all_points_cnt_);
                iter_out->second->var_ += ((iter_in->second->var_ + iter_in->second->mean_ *
                                                                    iter_in->second->mean_.transpose()) * iter_in->second->all_points_cnt_);
                iter_out->second->all_points_cnt_ += iter_in->second->all_points_cnt_;
            }else{ // input voxel no finish
                for(int i = 0; i < iter_in->second->temp_points_.size();i++){
                    iter_out->second->temp_points_.emplace_back(iter_in->second->temp_points_.at(i));
                }
            }

        }else{            // voxel no exists, need new voxel
            VoxelShape *voxelshape = new VoxelShape(iter_in->second->voxel_size_ * voxel_downtimes, alpha);
            shape_map_out[pos_out] = voxelshape;                                    
            if(iter_in->second->init_flag){ // input voxel finish init
                shape_map_out[pos_out]->mean_ = (iter_in->second->mean_ * iter_in->second->all_points_cnt_);
                shape_map_out[pos_out]->var_ = ((iter_in->second->var_ + iter_in->second->mean_ *
                                                        iter_in->second->mean_.transpose()) * iter_in->second->all_points_cnt_);
                shape_map_out[pos_out]->all_points_cnt_ += iter_in->second->all_points_cnt_;
            }else{ // input voxel no finish
                for(int i = 0; i < iter_in->second->temp_points_.size(); i++){
                    shape_map_out[pos_out]->temp_points_.emplace_back(iter_in->second->temp_points_.at(i));
                }
            }
        }
    }
    for(auto iter=shape_map_out.begin(); iter != shape_map_out.end(); iter++){
        iter->second->init_voxel_shape_vxl();
    }
}

void CalcVectQuation(const Eigen::Vector3d &x_vec, const Eigen::Vector3d &y_vec,
                     const Eigen::Vector3d &z_vec,
                     geometry_msgs::Quaternion &q)
{

        Eigen::Vector3d v1{0,0,1};
    Eigen::Vector3d v2{z_vec(0), z_vec(1), z_vec(2)};
    Eigen::Quaterniond eq(Eigen::Quaterniond::FromTwoVectors(v1,v2));
    eq.normalize();
    q.w = eq.w();
    q.x = eq.x();
    q.y = eq.y();
    q.z = eq.z();

}

void mapJet(double v, double vmin, double vmax, uint8_t &r, uint8_t &g,
            uint8_t &b)
{
    r = 255;
    g = 255;
    b = 255;

    if (v < vmin)
    {
        v = vmin;
    }

    if (v > vmax)
    {
        v = vmax;
    }

    double dr, dg, db;

    if (v < 0.1242)
    {
        db = 0.504 + ((1. - 0.504) / 0.1242) * v;
        dg = dr = 0.;
    }
    else if (v < 0.3747)
    {
        db = 1.;
        dr = 0.;
        dg = (v - 0.1242) * (1. / (0.3747 - 0.1242));
    }
    else if (v < 0.6253)
    {
        db = (0.6253 - v) * (1. / (0.6253 - 0.3747));
        dg = 1.;
        dr = (v - 0.3747) * (1. / (0.6253 - 0.3747));
    }
    else if (v < 0.8758)
    {
        db = 0.;
        dr = 1.;
        dg = (0.8758 - v) * (1. / (0.8758 - 0.6253));
    }
    else
    {
        db = 0.;
        dg = 0.;
        dr = 1. - (v - 0.8758) * ((1. - 0.504) / (1. - 0.8758));
    }

    r = (uint8_t)(255 * dr);
    g = (uint8_t)(255 * dg);
    b = (uint8_t)(255 * db);
}


void pub_single_voxel(visualization_msgs::MarkerArray &plane_pub, const int idx, const std::string plane_ns,
                      const Eigen::Vector3d &single_plane, const float alpha,
                      const Eigen::Vector3d rgb, const Eigen::Vector3d v1,
                      const Eigen::Vector3d v2, const Eigen::Vector3d v3, const Eigen::Vector3d lambda){
    visualization_msgs::Marker plane;
    plane.header.frame_id = "aft_mapped";
    plane.header.stamp = ros::Time::now();
    plane.ns = plane_ns;
    plane.id = idx;
    plane.type = visualization_msgs::Marker::CYLINDER;
    plane.action = visualization_msgs::Marker::ADD;
    plane.pose.position.x = single_plane[0];
    plane.pose.position.y = single_plane[1];
    plane.pose.position.z = single_plane[2];
    geometry_msgs::Quaternion q;
    CalcVectQuation(v3, v2, v1, q);
    plane.pose.orientation = q;
    plane.scale.x = 3 * sqrt(lambda(2));
    plane.scale.y = 3 * sqrt(lambda(1));
    plane.scale.z = 2 * sqrt(lambda(0));
    plane.color.a = alpha;
    plane.color.r = rgb(0);
    plane.color.g = rgb(1);
    plane.color.b = rgb(2);
    plane.lifetime = ros::Duration();
    plane_pub.markers.emplace_back(plane);
}

void pub_single_voxel_shift(visualization_msgs::MarkerArray &plane_pub, const int idx, const std::string plane_ns,
                      const Eigen::Vector3d &single_plane, const float alpha,
                      const Eigen::Vector3d rgb, const Eigen::Vector3d v1,
                      const Eigen::Vector3d v2, const Eigen::Vector3d v3, const Eigen::Vector3d lambda){
    visualization_msgs::Marker plane;
    plane.header.frame_id = "a";
    plane.header.stamp = ros::Time::now();
    plane.ns = plane_ns;
    plane.id = idx;
    plane.type = visualization_msgs::Marker::CYLINDER;
    plane.action = visualization_msgs::Marker::ADD;
    plane.pose.position.x = single_plane[0];
    plane.pose.position.y = single_plane[1];
    plane.pose.position.z = single_plane[2] + 10.0;
    geometry_msgs::Quaternion q;
    CalcVectQuation(v3, v2, v1, q);
    plane.pose.orientation = q;
    plane.scale.x = 3 * sqrt(lambda(2));
    plane.scale.y = 3 * sqrt(lambda(1));
    plane.scale.z = 2 * sqrt(lambda(0));
    plane.color.a = alpha;
    plane.color.r = rgb(0);
    plane.color.g = rgb(1);
    plane.color.b = rgb(2);
    plane.lifetime = ros::Duration();
    plane_pub.markers.emplace_back(plane);
}

void pub_single_line(const VoxelShape* source, const VoxelShape* target, const int idx,
                visualization_msgs::MarkerArray &line_pub){
    visualization_msgs::Marker line;
    line.header.frame_id = "a";
    line.header.stamp = ros::Time::now();
    line.action = visualization_msgs::Marker::ADD;
    line.id = idx;
    line.type = visualization_msgs::Marker::LINE_STRIP;
    line.scale.x = 0.1;
    line.color.b = 1.0;
    line.color.a = 1.0;
    geometry_msgs::Point source_p;
    geometry_msgs::Point target_p;
    source_p.x = source->mean_[0];
    source_p.y = source->mean_[1];
    source_p.z = source->mean_[2];
    target_p.x = target->mean_[0];
    target_p.y = target->mean_[1];
    target_p.z = target->mean_[2] + 10.0;
    line.points.emplace_back(source_p);
    line.points.emplace_back(target_p);
    line.lifetime = ros::Duration();
    line_pub.markers.emplace_back(line);
}

void pub_voxel(const std::unordered_map<VOXEL_LOC, VoxelShape*> &shape_map, ros::Publisher &shape_pub, int layer_voxel){

            visualization_msgs::MarkerArray shape_clear;
    shape_clear.markers.reserve(1);
    visualization_msgs::Marker plane;
    plane.header.frame_id = "aft_mapped";
    plane.header.stamp = ros::Time::now();
    plane.type = visualization_msgs::Marker::CYLINDER;
    plane.action = visualization_msgs::Marker::DELETEALL;
    plane.lifetime = ros::Duration( );
    shape_clear.markers.emplace_back(plane);
    shape_pub.publish(shape_clear); 
    
    visualization_msgs::MarkerArray shape_msg;
//    ros::Rate loop(500);
    double max_trace = 0.25;
    double pow_num = 0.2;
    shape_msg.markers.reserve(100000);
    float alpha = 0.8;
    int idx = 0;
    std::vector<Eigen::Matrix<double, 6, 1>> pub_plane_list;
    for (auto iter = shape_map.begin(); iter != shape_map.end(); iter++){
        if(iter->second->is_plane){
            double trace = iter->second->var_.sum();
            if (trace >= max_trace) trace = max_trace;
            trace = trace * (1.0 / max_trace);
            trace = pow(trace, pow_num);
            uint8_t r, g, b;
            mapJet(trace, 0, 1, r, g, b);
            Eigen::Vector3d plane_rgb(r / 256.0, g / 256.0, b / 256.0);    
           switch(layer_voxel)
            {
                case 0:
                    plane_rgb << 255.0, 0.0, 255.0;
                    break;
                case 1:
                    plane_rgb << 255.0, 255.0, 0.0;
                    break;
                case 2:
                    plane_rgb << 0.0, 0.0, 255.0;
                    break;
                case 3:
                    plane_rgb << 255.0, 0.0, 0.0;
                    break;
                case 4:
                    plane_rgb << 0.0, 255.0, 255.0;
                    break;
            }       
            pub_single_voxel(shape_msg, idx, "plane", iter->second->mean_,
                             alpha, plane_rgb, iter->second->v1_,
                             iter->second->v2_, iter->second->v3_, iter->second->eigen_val_);
            idx++;
        }
    }
    std::cout<<"plane voxel size: " <<idx<<std::endl;
    shape_pub.publish(shape_msg);
//    loop.sleep();
}

void pub_voxel(const std::vector<VoxelShape*> &shape_vec_source, const std::vector<VoxelShape*> &shape_vec_target,
               const std::vector<int> match_st,ros::Publisher &shape_pub){
    visualization_msgs::MarkerArray shape_msg;
//    ros::Rate loop(500);
    double max_trace = 0.25;
    double pow_num = 0.2;
    shape_msg.markers.reserve(100000);
    float alpha = 0.8;
    int idx = 0;
    std::vector<Eigen::Matrix<double, 6, 1>> pub_plane_list;
    for (auto iter = shape_vec_source.begin(); iter != shape_vec_source.end(); iter++){
        double trace = (*iter)->var_.sum();
        if (trace >= max_trace) trace = max_trace;
        trace = trace * (1.0 / max_trace);
        trace = pow(trace, pow_num);
        uint8_t r, g, b;
        mapJet(trace, 0, 1, r, g, b);
        Eigen::Vector3d plane_rgb(r / 256.0, g / 256.0, b / 256.0);
        pub_single_voxel(shape_msg, idx, "plane", (*iter)->mean_,
                         alpha, plane_rgb, (*iter)->v1_,
                         (*iter)->v2_, (*iter)->v3_, (*iter)->eigen_val_);
        idx++;
    }
    for (auto iter = shape_vec_target.begin(); iter != shape_vec_target.end(); iter++){
        double trace = (*iter)->var_.sum();
        if (trace >= max_trace) trace = max_trace;
        trace = trace * (1.0 / max_trace);
        trace = pow(trace, pow_num);
        uint8_t r, g, b;
        mapJet(trace, 0, 1, r, g, b);
        Eigen::Vector3d plane_rgb(r / 256.0, g / 256.0, b / 256.0);
        pub_single_voxel_shift(shape_msg, idx, "plane", (*iter)->mean_,
                         alpha, plane_rgb, (*iter)->v1_,
                         (*iter)->v2_, (*iter)->v3_, (*iter)->eigen_val_);
        idx++;
    }
    // for (int i = 0;i < shape_vec_source.size();i++){
    //     if(match_st[i] > -1){
    //         pub_single_line(shape_vec_source.at(i), shape_vec_target.at(match_st[i]), idx, shape_msg);
    //         idx++;
    //     }
    // }
    shape_pub.publish(shape_msg);
//    loop.sleep();
}



#endif //SRC_VOXEL_UTIL_H
