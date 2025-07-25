/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2017, Locus Robotics
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "nav_2d_utils/conversions.hpp"

#include <vector>
#include <string>

#include "geometry_msgs/msg/pose.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/path.hpp"
#include "nav_2d_msgs/msg/twist2_d.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "tf2/utils.hpp"
#pragma GCC diagnostic pop

#include "nav2_util/geometry_utils.hpp"

namespace nav_2d_utils
{
using nav2_util::geometry_utils::orientationAroundZAxis;

geometry_msgs::msg::Twist twist2Dto3D(const nav_2d_msgs::msg::Twist2D & cmd_vel_2d)
{
  geometry_msgs::msg::Twist cmd_vel;
  cmd_vel.linear.x = cmd_vel_2d.x;
  cmd_vel.linear.y = cmd_vel_2d.y;
  cmd_vel.angular.z = cmd_vel_2d.theta;
  return cmd_vel;
}

nav_2d_msgs::msg::Twist2D twist3Dto2D(const geometry_msgs::msg::Twist & cmd_vel)
{
  nav_2d_msgs::msg::Twist2D cmd_vel_2d;
  cmd_vel_2d.x = cmd_vel.linear.x;
  cmd_vel_2d.y = cmd_vel.linear.y;
  cmd_vel_2d.theta = cmd_vel.angular.z;
  return cmd_vel_2d;
}


geometry_msgs::msg::PoseStamped poseToPoseStamped(
  const geometry_msgs::msg::Pose & pose_in,
  const std::string & frame, const rclcpp::Time & stamp)
{
  geometry_msgs::msg::PoseStamped pose;
  pose.header.frame_id = frame;
  pose.header.stamp = stamp;
  pose.pose = pose_in;
  return pose;
}

nav_msgs::msg::Path posesToPath(const std::vector<geometry_msgs::msg::PoseStamped> & poses)
{
  nav_msgs::msg::Path path;
  if (poses.empty()) {
    return path;
  }
  path.poses.resize(poses.size());
  path.header.frame_id = poses[0].header.frame_id;
  path.header.stamp = poses[0].header.stamp;
  for (unsigned int i = 0; i < poses.size(); i++) {
    path.poses[i] = poses[i];
  }
  return path;
}


nav_msgs::msg::Path posesToPath(
  const std::vector<geometry_msgs::msg::Pose> & poses,
  const std::string & frame, const rclcpp::Time & stamp)
{
  nav_msgs::msg::Path path;
  path.poses.resize(poses.size());
  path.header.frame_id = frame;
  path.header.stamp = stamp;
  for (unsigned int i = 0; i < poses.size(); i++) {
    path.poses[i].header.frame_id = frame;
    path.poses[i].header.stamp = stamp;
    path.poses[i].pose = poses[i];
  }
  return path;
}

}  // namespace nav_2d_utils
