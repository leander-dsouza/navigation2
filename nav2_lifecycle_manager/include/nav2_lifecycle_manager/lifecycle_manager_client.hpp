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

#ifndef NAV2_LIFECYCLE_MANAGER__LIFECYCLE_MANAGER_CLIENT_HPP_
#define NAV2_LIFECYCLE_MANAGER__LIFECYCLE_MANAGER_CLIENT_HPP_

#include <memory>
#include <string>

#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "geometry_msgs/msg/quaternion.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "std_srvs/srv/empty.hpp"
#include "nav2_msgs/srv/manage_lifecycle_nodes.hpp"
#include "std_srvs/srv/trigger.hpp"
#include "nav2_ros_common/lifecycle_node.hpp"
#include "nav2_ros_common/service_client.hpp"

namespace nav2_lifecycle_manager
{
/**
 * @enum nav2_lifecycle_manager::SystemStatus
 * @brief Enum class representing the status of the system.
 */
enum class SystemStatus {ACTIVE, INACTIVE, TIMEOUT};
/**
 * @class nav2_lifecycle_manager::LifeCycleMangerClient
 * @brief The LifecycleManagerClient sends requests to the LifecycleManager to
 * control the lifecycle state of the navigation modules.
 */
class LifecycleManagerClient
{
public:
  /**
   * @brief A constructor for LifeCycleMangerClient
   * @param name Managed node name
   * @param parent_node Node that execute the service calls
   */
  template<typename NodeT>
  explicit LifecycleManagerClient(
    const std::string & name,
    NodeT parent_node)
  {
    manage_service_name_ = name + std::string("/manage_nodes");
    active_service_name_ = name + std::string("/is_active");

    // Use parent node for service call and logging
    logger_ = parent_node->get_logger();

    // Create the service clients
    // Could be using a user rclcpp::Node, so need to use the Nav2 factory to create the
    // subscription to convert nav2::LifecycleNode, rclcpp::Node or rclcpp_lifecycle::LifecycleNode
    manager_client_ = nav2::interfaces::create_client<ManageLifecycleNodes>(
      parent_node, manage_service_name_, true /*creates and spins an internal executor*/);
    is_active_client_ = nav2::interfaces::create_client<std_srvs::srv::Trigger>(
      parent_node, active_service_name_, true /*creates and spins an internal executor*/);
  }

  // Client-side interface to the Nav2 lifecycle manager
  /**
   * @brief Make start up service call
   * @return true or false
   */
  bool startup(const std::chrono::nanoseconds timeout = std::chrono::nanoseconds(-1));
  /**
   * @brief Make shutdown service call
   * @return true or false
   */
  bool shutdown(const std::chrono::nanoseconds timeout = std::chrono::nanoseconds(-1));
  /**
   * @brief Make pause service call
   * @return true or false
   */
  bool pause(const std::chrono::nanoseconds timeout = std::chrono::nanoseconds(-1));
  /**
   * @brief Make resume service call
   * @return true or false
   */
  bool resume(const std::chrono::nanoseconds timeout = std::chrono::nanoseconds(-1));
  /**
   * @brief Make reset service call
   * @return true or false
   */
  bool reset(const std::chrono::nanoseconds timeout = std::chrono::nanoseconds(-1));
  /**
   * @brief Make configure service call
   * @return true or false
   */
  bool configure(const std::chrono::nanoseconds timeout = std::chrono::nanoseconds(-1));
  /**
   * @brief Make cleanup service call
   * @return true or false
   */
  bool cleanup(const std::chrono::nanoseconds timeout = std::chrono::nanoseconds(-1));
  /**
   * @brief Check if lifecycle node manager server is active
   * @return ACTIVE or INACTIVE or TIMEOUT
   */
  SystemStatus is_active(const std::chrono::nanoseconds timeout = std::chrono::nanoseconds(-1));

protected:
  using ManageLifecycleNodes = nav2_msgs::srv::ManageLifecycleNodes;

  /**
   * @brief A generic method used to call startup, shutdown, etc.
   * @param command
   */
  bool callService(
    uint8_t command,
    const std::chrono::nanoseconds timeout = std::chrono::nanoseconds(-1));

  // The node to use for the service call
  rclcpp::Node::SharedPtr node_;
  rclcpp::Logger logger_{rclcpp::get_logger("nav2_lifecycle_manager_client")};

  nav2::ServiceClient<ManageLifecycleNodes>::SharedPtr manager_client_;
  nav2::ServiceClient<std_srvs::srv::Trigger>::SharedPtr is_active_client_;
  std::string manage_service_name_;
  std::string active_service_name_;
};

}  // namespace nav2_lifecycle_manager

#endif  // NAV2_LIFECYCLE_MANAGER__LIFECYCLE_MANAGER_CLIENT_HPP_
