#include "omni_control/reach_goal.hpp"

#include <cmath>
#include <memory>

#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"

#include "tf2/utils.h"
#include "tf2/LinearMath/Vector3.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

using std::placeholders::_1;

namespace omni_control_1 {

ReachGoal::ReachGoal() : Node("reach_goal")
  , no_goal_received_(false), angular_gain_(1.0), linear_x_gain_(1.0), linear_y_gain_(1.0), tolerance_(0.1), trajectory_point_count(0), cnt(0)
{
  goal_.position.setZero();
  goal_.yaw = 0.0;

  trajectory_subscription_ = this->create_subscription<mm_interfaces::msg::Trajectory2D>(
    "/trajectory", 1, std::bind(&ReachGoal::readTrajectoryCallback, this, _1));

  odom_subscription_ = this->create_subscription<nav_msgs::msg::Odometry>(
    "/gazebo_ground_truth/odom", 1, std::bind(&ReachGoal::readOdometryCallback, this, _1));

  // goal_subscription_ = this->create_subscription<geometry_msgs::msg::Vector3>(
  //   "~/goal", 1, std::bind(&ReachGoal::readGoalPointCallback, this, _1));
  
  cmd_vel_publisher_ = this->create_publisher<geometry_msgs::msg::Twist>(
    "/omnidirectional_controller/cmd_vel_unstamped", 10);

  RCLCPP_INFO(this->get_logger(), "Node %s has started", this->get_logger().get_name());
}

void ReachGoal::setAngularGain(double gain) {
  this->angular_gain_ = gain;
  RCLCPP_INFO(this->get_logger(), "Angular gain set to %.2f", this->angular_gain_);
}

void ReachGoal::setLinearXGain(double gain) {
  this->linear_x_gain_ = gain;
  RCLCPP_INFO(this->get_logger(), "Linear X gain set to %.2f", this->linear_x_gain_);
}

void ReachGoal::setLinearYGain(double gain) {
  this->linear_y_gain_ = gain;
  RCLCPP_INFO(this->get_logger(), "Linear Y gain set to %.2f", this->linear_y_gain_);
}

void ReachGoal::setGoalDistanceTolerance(double tolerance) {
  this->tolerance_ = tolerance;
  RCLCPP_INFO(this->get_logger(), "Distance to goal tolerance set to %.2f", this->tolerance_);
}
 
void ReachGoal::readTrajectoryCallback(const mm_interfaces::msg::Trajectory2D::SharedPtr msg){
  for(auto goal:msg->trajectory){
    TrajectoryPoint trajectory_point;
    trajectory_point.position.setX(goal.waypoint.x);
    trajectory_point.position.setY(goal.waypoint.y);
    trajectory_point.position.setZ(goal.waypoint.z);
    trajectory_point.yaw = goal.yaw;

    trajectory.push_back(trajectory_point);
    trajectory_point_count++;
  }
  RCLCPP_INFO(this->get_logger(), "Trajectory has %d points", trajectory_point_count);
  if(trajectory_point_count > 0){
    no_goal_received_ = true;
  }
}

void ReachGoal::readOdometryCallback(const nav_msgs::msg::Odometry::SharedPtr msg) {
  cmd_vel_.linear.x = 0;
  cmd_vel_.linear.y = 0;
  cmd_vel_.angular.z = 0;

  if (!no_goal_received_) {
    cmd_vel_publisher_->publish(cmd_vel_);
    return;
  }

  RCLCPP_INFO(this->get_logger(), "x: %.2f, y: %.2f, theta: %.2f",msg->pose.pose.position.x, msg->pose.pose.position.y, (msg->pose.pose.orientation.z * 57.295));  

  tf2::Vector3 robot_position;
  double robot_yaw;
  robot_position.setX(msg->pose.pose.position.x);
  robot_position.setY(msg->pose.pose.position.y);
  robot_position.setZ(0);
  robot_yaw = msg->pose.pose.orientation.z;

  double distance_to_goal = this->goal_.position.distance(robot_position);

  if (distance_to_goal < tolerance_) {
    if(cnt == trajectory_point_count){
      RCLCPP_INFO(this->get_logger(), "Omni has reached its final position");
      no_goal_received_ = false;
      cmd_vel_publisher_->publish(cmd_vel_);
    }
    else{
      goal_ = trajectory.at(cnt);
      RCLCPP_INFO(this->get_logger(), "Receiveid goal point: (%.2f, %.2f), yaw: %.2f",
        goal_.position.getX(), goal_.position.getY(), goal_.yaw);
      cnt++;
    }
    return;
  }

  theta_desired = atan2(goal_.position.getY() - robot_position.getY(), goal_.position.getX() - robot_position.getX());

  double yaw_error = goal_.yaw - robot_yaw;

  cmd_vel_.linear.x = linear_x_gain_*(distance_to_goal * cos(theta_desired));
  cmd_vel_.linear.y = linear_y_gain_*(distance_to_goal * sin(theta_desired));
  cmd_vel_.angular.z = angular_gain_* (yaw_error + (theta_desired - tf2::getYaw(msg->pose.pose.orientation)));

  cmd_vel_publisher_->publish(cmd_vel_);
}

void ReachGoal::readGoalPointCallback(const geometry_msgs::msg::Vector3::SharedPtr goal) {
  this->goal_.position.setX(goal->x);
  this->goal_.position.setY(goal->y);
  this->goal_.position.setZ(0);
  no_goal_received_ = true;
  RCLCPP_INFO(this->get_logger(), "Receiveid goal point: (%.2f, %.2f)",
    goal_.position.getX(), goal_.position.getY());
}

ReachGoal::~ReachGoal() {}

}  // namespace omni_control_1