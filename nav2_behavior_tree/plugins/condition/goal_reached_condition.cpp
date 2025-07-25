// Copyright (c) 2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>
#include <memory>

#include "nav2_util/robot_utils.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav2_ros_common/node_utils.hpp"

#include "nav2_behavior_tree/plugins/condition/goal_reached_condition.hpp"

namespace nav2_behavior_tree
{

GoalReachedCondition::GoalReachedCondition(
  const std::string & condition_name,
  const BT::NodeConfiguration & conf)
: BT::ConditionNode(condition_name, conf)
{
  auto node = config().blackboard->get<nav2::LifecycleNode::SharedPtr>("node");

  robot_base_frame_ = BT::deconflictPortAndParamFrame<std::string>(
    node, "robot_base_frame", this);
}

GoalReachedCondition::~GoalReachedCondition()
{
  cleanup();
}

void GoalReachedCondition::initialize()
{
  node_ = config().blackboard->get<nav2::LifecycleNode::SharedPtr>("node");

  nav2::declare_parameter_if_not_declared(
    node_, "goal_reached_tol",
    rclcpp::ParameterValue(0.25));
  node_->get_parameter_or<double>("goal_reached_tol", goal_reached_tol_, 0.25);
  tf_ = config().blackboard->get<std::shared_ptr<tf2_ros::Buffer>>("tf_buffer");

  node_->get_parameter("transform_tolerance", transform_tolerance_);
}

BT::NodeStatus GoalReachedCondition::tick()
{
  if (!BT::isStatusActive(status())) {
    initialize();
  }

  if (isGoalReached()) {
    return BT::NodeStatus::SUCCESS;
  }
  return BT::NodeStatus::FAILURE;
}

bool GoalReachedCondition::isGoalReached()
{
  geometry_msgs::msg::PoseStamped goal;
  getInput("goal", goal);

  geometry_msgs::msg::PoseStamped current_pose;
  if (!nav2_util::getCurrentPose(
      current_pose, *tf_, goal.header.frame_id, robot_base_frame_, transform_tolerance_))
  {
    RCLCPP_DEBUG(node_->get_logger(), "Current robot pose is not available.");
    return false;
  }

  double dx = goal.pose.position.x - current_pose.pose.position.x;
  double dy = goal.pose.position.y - current_pose.pose.position.y;

  return (dx * dx + dy * dy) <= (goal_reached_tol_ * goal_reached_tol_);
}

}  // namespace nav2_behavior_tree

#include "behaviortree_cpp/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<nav2_behavior_tree::GoalReachedCondition>("GoalReached");
}
